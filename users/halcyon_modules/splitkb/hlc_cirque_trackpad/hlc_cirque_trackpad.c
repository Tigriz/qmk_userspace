// Copyright 2026 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

// Turns the Halcyon Cirque module into three pointing devices in one:
//
//   * trackpad   : stock QMK behaviour (absolute pad -> relative mouse + gestures)
//   * trackpoint : Lenovo-style pointing stick, finger deflection drives a velocity
//   * joystick   : HID gamepad, finger deflection drives absolute analog axes
//
// Everything is done inside a replacement pointing device driver, so the mode
// also applies when the module sits on the slave half. Joystick axes are HID
// reports that only the master can send, so they travel to the master inside the
// x/y fields of the regular mouse report (flagged with a spare button bit) and
// are unpacked in halcyon.c, on the master, by hlc_pointing_decode_report().
// Riding on x/y means the axes get the same rotation/inversion treatment from
// pointing_device_adjust_by_defines() as the cursor does, so both modes agree on
// which way is "up" whichever half the module is on.

#include QMK_KEYBOARD_H
#include "hlc_cirque_trackpad/hlc_cirque_trackpad.h"
#include "pointing_device.h"
#include "split_util.h"
#include "timer.h"
#ifdef JOYSTICK_ENABLE
#    include "joystick.h"
#endif

#if CIRQUE_PINNACLE_POSITION_MODE != CIRQUE_PINNACLE_ABSOLUTE_MODE
#    error "hlc_cirque_trackpad needs CIRQUE_PINNACLE_ABSOLUTE_MODE to read finger position"
#endif

#if defined(JOYSTICK_ENABLE) && !defined(MOUSE_EXTENDED_REPORT) && (JOYSTICK_MAX_VALUE > 127)
#    error "Joystick axes travel in the 8-bit mouse report: use JOYSTICK_AXIS_RESOLUTION 8 or enable MOUSE_EXTENDED_REPORT"
#endif

// ---------------------------------------------------------------- state ----

typedef struct {
    bool     touching;
    uint16_t x, y, z; // scaled to HLC_POINTING_SCALE
} hlc_touch_t;

static hlc_touch_t touch;
static uint32_t    touch_last_sample;

static uint16_t origin_x, origin_y; // finger landing point
static bool     origin_valid;

static int32_t accum_x, accum_y; // trackpoint sub-pixel remainder, 8.8 fixed point

static uint32_t touch_start;
static int32_t  touch_travel;     // largest distance from the landing point
static uint32_t tap_click_start;  // when the click produced by a tap was pressed
static bool     tap_click_active; // that click is still held
static bool     trackpad_resync;  // drop the first trackpad delta after a mode change

static void hlc_pointing_reset(void);

// ----------------------------------------------------------- math bits ----

static uint32_t hlc_isqrt(uint32_t value) {
    uint32_t rest = value, result = 0, bit = 1UL << 30;

    while (bit > rest) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (rest >= result + bit) {
            rest -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

// Maps a deflection vector to an output vector: radial dead zone, response
// curve on the magnitude, circular clamp at `range`, direction preserved.
static void hlc_apply_curve(int32_t dx, int32_t dy, int32_t deadzone, int32_t range, int32_t out_max, uint8_t curve, int32_t *out_x, int32_t *out_y) {
    *out_x = 0;
    *out_y = 0;

    int32_t magnitude = (int32_t)hlc_isqrt((uint32_t)(dx * dx + dy * dy));
    if (magnitude <= deadzone) {
        return;
    }
    if (range <= deadzone) {
        range = deadzone + 1;
    }

    int32_t span   = range - deadzone;
    int32_t effort = magnitude - deadzone;
    if (effort > span) {
        effort = span;
    }

    int32_t normalized = (effort * 1024) / span; // 0..1024
    for (uint8_t i = 1; i < curve; i++) {
        normalized = (normalized * normalized) / 1024;
    }

    int32_t output = (normalized * out_max) / 1024;
    *out_x         = (dx * output) / magnitude;
    *out_y         = (dy * output) / magnitude;
}

// --------------------------------------------------------- sensor input ----

// Keeps the last known finger state: the Pinnacle only produces a packet every
// ~10ms, but a stick has to keep pushing the cursor on every poll in between.
static void hlc_sample_touch(void) {
    pinnacle_data_t data = cirque_pinnacle_read_data();

    if (!data.valid) {
        // Safety net: a lost lift-off packet must not leave the stick pushed.
        if (touch.touching && timer_elapsed32(touch_last_sample) > HLC_POINTING_TOUCH_TIMEOUT) {
            touch.touching = false;
        }
        return;
    }

    touch_last_sample = timer_read32();

    if (!data.touchDown
#if HLC_POINTING_Z_THRESHOLD > 0
        || data.zValue < HLC_POINTING_Z_THRESHOLD
#endif
    ) {
        touch.touching = false;
        touch.z        = 0;
        return;
    }

    cirque_pinnacle_scale_data(&data, HLC_POINTING_SCALE, HLC_POINTING_SCALE);

    touch.x        = data.xValue;
    touch.y        = data.yValue;
    touch.z        = data.zValue;
    touch.touching = true;
}

// Tracks landing point and travel, and reports a tap on release.
static bool hlc_track_touch(int32_t *dx, int32_t *dy) {
    bool tapped = false;

    if (!touch.touching) {
        if (origin_valid) {
            tapped       = (timer_elapsed32(touch_start) <= HLC_POINTING_TAP_TERM) && (touch_travel <= HLC_POINTING_TAP_RADIUS);
            origin_valid = false;
        }
        *dx = 0;
        *dy = 0;
        return tapped;
    }

    if (!origin_valid) {
        origin_x     = touch.x;
        origin_y     = touch.y;
        origin_valid = true;
        touch_start  = timer_read32();
        touch_travel = 0;
        accum_x      = 0;
        accum_y      = 0;
    }

    *dx = (int32_t)touch.x - (int32_t)origin_x;
    *dy = (int32_t)touch.y - (int32_t)origin_y;

    int32_t travel = (int32_t)hlc_isqrt((uint32_t)((*dx) * (*dx) + (*dy) * (*dy)));
    if (travel > touch_travel) {
        touch_travel = travel;
    }

    return false;
}

// ------------------------------------------------------------- trackpad ----

static report_mouse_t hlc_trackpad_report(report_mouse_t mouse_report) {
    if (trackpad_resync) {
        // The stock driver keeps the previous finger position in a static; after
        // a stint in another mode that stale position would produce one huge
        // jump. Feeding it a zero scale once makes it discard that delta.
        uint16_t scale = cirque_pinnacle_get_scale();

        cirque_pinnacle_set_scale(0);
        mouse_report = cirque_pinnacle_get_report(mouse_report);
        cirque_pinnacle_set_scale(scale);

        mouse_report.x  = 0;
        mouse_report.y  = 0;
        trackpad_resync = false;
        return mouse_report;
    }

    return cirque_pinnacle_get_report(mouse_report);
}

// ----------------------------------------------------------- trackpoint ----

static report_mouse_t hlc_trackpoint_report(report_mouse_t mouse_report) {
    int32_t dx, dy;

    hlc_sample_touch();
    bool tapped = hlc_track_touch(&dx, &dy);

#ifdef HLC_TRACKPOINT_TAP_ENABLE
    if (tapped) {
        tap_click_start  = timer_read32();
        tap_click_active = true;
    } else if (tap_click_active && timer_elapsed32(tap_click_start) >= HLC_POINTING_TAP_CLICK_TIME) {
        tap_click_active = false;
    }
    mouse_report.buttons = pointing_device_handle_buttons(mouse_report.buttons, tap_click_active, POINTING_DEVICE_BUTTON1);
#else
    (void)tapped;
#endif

    if (!touch.touching) {
        accum_x = 0;
        accum_y = 0;
        return mouse_report;
    }

    int32_t vx, vy;
    hlc_apply_curve(dx, dy, HLC_TRACKPOINT_DEADZONE, HLC_TRACKPOINT_RANGE, (int32_t)HLC_TRACKPOINT_SPEED << 8, HLC_TRACKPOINT_CURVE, &vx, &vy);

#ifdef HLC_TRACKPOINT_PRESSURE_BOOST
    // Pressing harder pushes the stick harder, like a real strain gauge.
    int32_t boost = 256 + ((int32_t)touch.z * HLC_TRACKPOINT_PRESSURE_BOOST);
    vx            = (vx * boost) / 256;
    vy            = (vy * boost) / 256;
#endif

    accum_x += vx;
    accum_y += vy;

    int32_t step_x = accum_x >> 8; // arithmetic shift: floors, remainder stays positive
    int32_t step_y = accum_y >> 8;
    accum_x -= step_x << 8;
    accum_y -= step_y << 8;

    mouse_report.x = CONSTRAIN_HID_XY(step_x);
    mouse_report.y = CONSTRAIN_HID_XY(step_y);

    return mouse_report;
}

// ------------------------------------------------------------- joystick ----

#ifdef JOYSTICK_ENABLE
static report_mouse_t hlc_joystick_report(report_mouse_t mouse_report) {
    int32_t dx, dy;
    int32_t ax = 0, ay = 0;

    hlc_sample_touch();
    bool tapped = hlc_track_touch(&dx, &dy);

#    ifdef HLC_JOYSTICK_TAP_ENABLE
    if (tapped) {
        tap_click_start  = timer_read32();
        tap_click_active = true;
    } else if (tap_click_active && timer_elapsed32(tap_click_start) >= HLC_POINTING_TAP_CLICK_TIME) {
        tap_click_active = false;
    }
#    else
    (void)tapped;
#    endif

    if (touch.touching) {
        if (hlc_joystick_center() == HLC_JOYSTICK_CENTER_TOUCH) {
            hlc_apply_curve(dx, dy, HLC_JOYSTICK_DEADZONE, HLC_JOYSTICK_TOUCH_RANGE, JOYSTICK_MAX_VALUE, HLC_JOYSTICK_CURVE, &ax, &ay);
        } else {
            hlc_apply_curve((int32_t)touch.x - (HLC_POINTING_SCALE / 2), (int32_t)touch.y - (HLC_POINTING_SCALE / 2), HLC_JOYSTICK_DEADZONE, HLC_JOYSTICK_RANGE, JOYSTICK_MAX_VALUE, HLC_JOYSTICK_CURVE, &ax, &ay);
        }
    }

    mouse_report.x = ax;
    mouse_report.y = ay;
    mouse_report.h = 0;
    mouse_report.v = 0;
    mouse_report.buttons |= HLC_JOYSTICK_REPORT_MARKER;
    if (tap_click_active) {
        mouse_report.buttons |= HLC_JOYSTICK_BUTTON_MARKER;
    }

    return mouse_report;
}

static void hlc_joystick_release(void) {
    if (!is_keyboard_master()) {
        return;
    }
    joystick_set_axis(HLC_JOYSTICK_AXIS_X, 0);
    joystick_set_axis(HLC_JOYSTICK_AXIS_Y, 0);
#    ifdef HLC_JOYSTICK_TAP_ENABLE
    unregister_joystick_button(HLC_JOYSTICK_TAP_BUTTON);
#    endif
    joystick_flush();
}
#endif // JOYSTICK_ENABLE

report_mouse_t hlc_pointing_decode_report(report_mouse_t mouse_report) {
    if (!(mouse_report.buttons & HLC_JOYSTICK_REPORT_MARKER)) {
        return mouse_report;
    }

    // The master only re-reads the other half's report when it changes, so a
    // report from joystick mode can still sit in the shared buffer for a few
    // polls after a mode change. Dropping it here keeps a stale deflection from
    // sticking to the axes, which nothing would clear afterwards.
    if (hlc_pointing_mode() == HLC_POINTING_JOYSTICK) {
#ifdef JOYSTICK_ENABLE
        joystick_set_axis(HLC_JOYSTICK_AXIS_X, mouse_report.x);
        joystick_set_axis(HLC_JOYSTICK_AXIS_Y, mouse_report.y);
#    ifdef HLC_JOYSTICK_TAP_ENABLE
        if (mouse_report.buttons & HLC_JOYSTICK_BUTTON_MARKER) {
            register_joystick_button(HLC_JOYSTICK_TAP_BUTTON);
        } else {
            unregister_joystick_button(HLC_JOYSTICK_TAP_BUTTON);
        }
#    endif
        joystick_flush();
#endif
    }

    // Nothing of this belongs in the mouse report the host gets.
    mouse_report.x = 0;
    mouse_report.y = 0;
    mouse_report.h = 0;
    mouse_report.v = 0;
    mouse_report.buttons &= ~(HLC_JOYSTICK_REPORT_MARKER | HLC_JOYSTICK_BUTTON_MARKER);

    return mouse_report;
}

// --------------------------------------------------------------- driver ----

static report_mouse_t hlc_pointing_get_report(report_mouse_t mouse_report) {
    mouse_report.buttons &= ~(HLC_JOYSTICK_REPORT_MARKER | HLC_JOYSTICK_BUTTON_MARKER);

    switch (hlc_pointing_mode()) {
        case HLC_POINTING_TRACKPOINT:
            return hlc_trackpoint_report(mouse_report);
#ifdef JOYSTICK_ENABLE
        case HLC_POINTING_JOYSTICK:
            return hlc_joystick_report(mouse_report);
#endif
        case HLC_POINTING_TRACKPAD:
        default:
            return hlc_trackpad_report(mouse_report);
    }
}

// clang-format off
static const pointing_device_driver_t hlc_pointing_driver = {
    .init       = cirque_pinnacle_init,
    .get_report = hlc_pointing_get_report,
    .set_cpi    = cirque_pinnacle_set_cpi,
    .get_cpi    = cirque_pinnacle_get_cpi
};
// clang-format on

extern const pointing_device_driver_t *pointing_device_driver;

// ------------------------------------------------------------ mode state ----

static void hlc_pointing_reset(void) {
    touch.touching   = false;
    origin_valid     = false;
    accum_x          = 0;
    accum_y          = 0;
    touch_travel     = 0;
    tap_click_active = false;
    trackpad_resync  = true;
}

// The mode itself lives in hlc_pointing_mode.c, which every module compiles;
// here we only care about dropping whatever the previous mode had accumulated.
void hlc_pointing_mode_changed_kb(hlc_pointing_mode_t mode) {
#ifdef JOYSTICK_ENABLE
    if (mode != HLC_POINTING_JOYSTICK) {
        hlc_joystick_release();
    }
#endif

    hlc_pointing_reset();
}

// ----------------------------------------------------------------- hooks ----

void pointing_device_init_kb(void) {
    // The stock cirque driver has already been initialised at this point; we
    // only take over the per-poll report generation.
    pointing_device_driver = &hlc_pointing_driver;

    hlc_pointing_reset();
}
