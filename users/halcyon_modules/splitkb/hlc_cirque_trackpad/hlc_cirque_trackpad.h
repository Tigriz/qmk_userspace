// Copyright 2026 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include QMK_KEYBOARD_H

// The Cirque module can behave as three different pointing devices.
// The active mode is owned by the master half and synced to the slave,
// so it does not matter which half the trackpad module is plugged into.
typedef enum {
    HLC_POINTING_TRACKPAD = 0, // stock QMK behaviour: absolute pad -> relative mouse
    HLC_POINTING_TRACKPOINT,   // Lenovo-style pointing stick: deflection -> velocity
    HLC_POINTING_JOYSTICK,     // HID gamepad: deflection -> absolute analog axes
    HLC_POINTING_MODE_COUNT
} hlc_pointing_mode_t;

// Where the "rest position" of the virtual stick sits in joystick mode.
typedef enum {
    HLC_JOYSTICK_CENTER_PAD = 0, // centre of the pad is neutral, edge is full deflection
    HLC_JOYSTICK_CENTER_TOUCH    // wherever you land becomes neutral (phone-style stick)
} hlc_joystick_center_t;

hlc_pointing_mode_t hlc_pointing_mode(void);
void                hlc_pointing_mode_set(hlc_pointing_mode_t mode);
void                hlc_pointing_mode_step(int8_t offset);

hlc_joystick_center_t hlc_joystick_center(void);
void                  hlc_joystick_center_set(hlc_joystick_center_t center);
void                  hlc_joystick_center_toggle(void);

// Call this from your own process_record_user() if you define one.
// Returns false when the keycode was consumed.
bool hlc_pointing_process_record(uint16_t keycode, keyrecord_t *record);

// Master-side hook, called from halcyon.c on both halves' reports.
report_mouse_t hlc_pointing_decode_report(report_mouse_t mouse_report);

// Keycodes. They sit in the keyboard range rather than on SAFE_RANGE, for two
// reasons: this module is userspace (i.e. keyboard level) code, and Vial maps
// the customKeycodes of vial.json to USER00.. = QK_KB_0 + n, not to QK_USER.
// SAFE_RANGE stays free for your own keymap keycodes. Override
// HLC_POINTING_KEYCODE_BASE in your keymap config.h if you need the room.
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
