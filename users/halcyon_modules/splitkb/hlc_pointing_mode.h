// Copyright 2026 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

// Pointing mode of the Cirque module: which of trackpad, trackpoint or joystick
// the pad currently behaves as.
//
// This lives here, outside the trackpad module, because every Halcyon firmware
// needs it: the half carrying the pad reads it to pick its behaviour, the half
// carrying a display reads it to show it, and either half can be the master
// that owns it and syncs it over. The two halves run different firmware, so a
// file compiled into only one of them could not do that.

#pragma once

#include QMK_KEYBOARD_H

typedef enum {
    HLC_POINTING_TRACKPAD = 0, // absolute pad -> relative mouse, with gestures
    HLC_POINTING_TRACKPOINT,   // deflection -> cursor velocity, Lenovo style
    HLC_POINTING_JOYSTICK,     // deflection -> absolute HID gamepad axes
    HLC_POINTING_MODE_COUNT
} hlc_pointing_mode_t;

typedef enum {
    HLC_JOYSTICK_CENTER_PAD = 0, // centre of the pad is neutral
    HLC_JOYSTICK_CENTER_TOUCH    // wherever you land is neutral
} hlc_joystick_center_t;

hlc_pointing_mode_t hlc_pointing_mode(void);
void                hlc_pointing_mode_set(hlc_pointing_mode_t mode);
void                hlc_pointing_mode_step(int8_t offset);

// Short upper-case label for a display module: "TRACKPAD", "TRACKPOINT", "JOYSTICK".
const char *hlc_pointing_mode_name(void);

hlc_joystick_center_t hlc_joystick_center(void);
void                  hlc_joystick_center_set(hlc_joystick_center_t center);
void                  hlc_joystick_center_toggle(void);

// Call from your own process_record_user() if you define one; returns false
// when the keycode was consumed.
bool hlc_pointing_process_record(uint16_t keycode, keyrecord_t *record);

// Called from halcyon.c, on both halves.
void hlc_pointing_mode_init(void);
void hlc_pointing_mode_task(void);

// Implemented by the module that owns the pad, to drop whatever state the
// previous mode had accumulated.
void hlc_pointing_mode_changed_kb(hlc_pointing_mode_t mode);

// Keycodes. They sit in the keyboard range rather than on SAFE_RANGE, for two
// reasons: this is userspace (i.e. keyboard level) code, and Vial maps the
// customKeycodes of vial.json to USER00.. = QK_KB_0 + n, not to QK_USER.
// SAFE_RANGE stays free for your own keymap keycodes.
#ifndef HLC_POINTING_KEYCODE_BASE
#    define HLC_POINTING_KEYCODE_BASE QK_KB_0
#endif

enum hlc_pointing_keycodes {
    HLC_PMODE_NEXT = HLC_POINTING_KEYCODE_BASE, // cycle trackpad -> trackpoint -> joystick
    HLC_PMODE_PAD,                              // force trackpad mode
    HLC_PMODE_TPT,                              // force trackpoint mode
    HLC_PMODE_JOY,                              // force joystick mode
    HLC_PMODE_JCTR,                             // toggle joystick centering (pad / touch)
    HLC_POINTING_KEYCODE_END
};

// Short aliases, easier on keymap grids
#define PM_NEXT HLC_PMODE_NEXT
#define PM_PAD HLC_PMODE_PAD
#define PM_TPT HLC_PMODE_TPT
#define PM_JOY HLC_PMODE_JOY
#define PM_JCTR HLC_PMODE_JCTR
