// Copyright 2026 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "hlc_pointing_mode.h"
#include "transactions.h"
#include "split_util.h"
#include "timer.h"

typedef struct {
    uint8_t mode;
    uint8_t center;
} hlc_pointing_sync_t;

static hlc_pointing_mode_t   pointing_mode   = HLC_POINTING_MODE_DEFAULT;
static hlc_joystick_center_t joystick_center = HLC_JOYSTICK_CENTER_DEFAULT;
static bool                  sync_dirty      = true;

__attribute__((weak)) void hlc_pointing_mode_changed_kb(hlc_pointing_mode_t mode) {}

hlc_pointing_mode_t hlc_pointing_mode(void) {
    return pointing_mode;
}

const char *hlc_pointing_mode_name(void) {
    switch (pointing_mode) {
        case HLC_POINTING_TRACKPOINT:
            return "TRACKPOINT";
        case HLC_POINTING_JOYSTICK:
            return "JOYSTICK";
        case HLC_POINTING_TRACKPAD:
        default:
            return "TRACKPAD";
    }
}

void hlc_pointing_mode_set(hlc_pointing_mode_t mode) {
    if (mode >= HLC_POINTING_MODE_COUNT || mode == pointing_mode) {
        return;
    }
#ifndef JOYSTICK_ENABLE
    // Only the master sends HID reports, so a half built without the gamepad
    // interface cannot offer joystick mode while it holds the USB cable.
    if (mode == HLC_POINTING_JOYSTICK) {
        return;
    }
#endif

    pointing_mode = mode;
    hlc_pointing_mode_changed_kb(pointing_mode);
    sync_dirty = true;
}

void hlc_pointing_mode_step(int8_t offset) {
#ifdef JOYSTICK_ENABLE
    const int8_t count = HLC_POINTING_MODE_COUNT;
#else
    const int8_t count = HLC_POINTING_MODE_COUNT - 1; // joystick compiled out
#endif
    int8_t next = ((int8_t)pointing_mode + offset) % count;

    if (next < 0) {
        next += count;
    }
    hlc_pointing_mode_set((hlc_pointing_mode_t)next);
}

hlc_joystick_center_t hlc_joystick_center(void) {
    return joystick_center;
}

void hlc_joystick_center_set(hlc_joystick_center_t center) {
    if (center == joystick_center) {
        return;
    }
    joystick_center = center;
    hlc_pointing_mode_changed_kb(pointing_mode);
    sync_dirty = true;
}

void hlc_joystick_center_toggle(void) {
    hlc_joystick_center_set(joystick_center == HLC_JOYSTICK_CENTER_PAD ? HLC_JOYSTICK_CENTER_TOUCH : HLC_JOYSTICK_CENTER_PAD);
}

// ------------------------------------------------------------ split sync ----

static void hlc_pointing_sync_handler(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    if (initiator2target_buffer_size != sizeof(hlc_pointing_sync_t)) {
        return;
    }

    hlc_pointing_sync_t state;
    memcpy(&state, initiator2target_buffer, sizeof(state));

    joystick_center = (hlc_joystick_center_t)state.center;

    // Assigned rather than passed through hlc_pointing_mode_set(): the master
    // has the last word, including modes this half would refuse to pick itself.
    if (state.mode != pointing_mode && state.mode < HLC_POINTING_MODE_COUNT) {
        pointing_mode = (hlc_pointing_mode_t)state.mode;
        hlc_pointing_mode_changed_kb(pointing_mode);
    }
}

void hlc_pointing_mode_init(void) {
    transaction_register_rpc(HLC_POINTING_SYNC, hlc_pointing_sync_handler);
}

void hlc_pointing_mode_task(void) {
    static uint32_t last_sync = 0;

    if (!is_keyboard_master() || !is_transport_connected()) {
        return;
    }
    if (!sync_dirty && timer_elapsed32(last_sync) < HLC_POINTING_SYNC_INTERVAL) {
        return;
    }

    hlc_pointing_sync_t state = {.mode = (uint8_t)pointing_mode, .center = (uint8_t)joystick_center};

    if (transaction_rpc_send(HLC_POINTING_SYNC, sizeof(state), &state)) {
        sync_dirty = false;
        last_sync  = timer_read32();
    }
}

// --------------------------------------------------------------- keycodes ----

bool hlc_pointing_process_record(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) {
        return true;
    }

    switch (keycode) {
        case HLC_PMODE_NEXT:
            hlc_pointing_mode_step(1);
            return false;
        case HLC_PMODE_PAD:
            hlc_pointing_mode_set(HLC_POINTING_TRACKPAD);
            return false;
        case HLC_PMODE_TPT:
            hlc_pointing_mode_set(HLC_POINTING_TRACKPOINT);
            return false;
        case HLC_PMODE_JOY:
            hlc_pointing_mode_set(HLC_POINTING_JOYSTICK);
            return false;
        case HLC_PMODE_JCTR:
            hlc_joystick_center_toggle();
            return false;
    }

    return true;
}

// Used when the keymap does not define process_record_user() itself (json and
// Vial keymaps). A keymap that does define it should call
// hlc_pointing_process_record() from there.
__attribute__((weak)) bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    return hlc_pointing_process_record(keycode, record);
}
