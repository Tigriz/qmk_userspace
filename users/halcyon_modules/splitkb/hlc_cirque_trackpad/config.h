// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

// Any QMK options should go here

#pragma once

#define HLC_CIRQUE_TRACKPAD

#define CIRQUE_PINNACLE_DIAMETER_MM 35
#undef POINTING_DEVICE_CS_PIN
#define POINTING_DEVICE_CS_PIN 13
#define POINTING_DEVICE_ROTATION_180
#define CIRQUE_PINNACLE_CURVED_OVERLAY

#define POINTING_DEVICE_GESTURES_CURSOR_GLIDE_ENABLE
#define CIRQUE_PINNACLE_POSITION_MODE CIRQUE_PINNACLE_ABSOLUTE_MODE
#define CIRQUE_PINNACLE_TAP_ENABLE
#define POINTING_DEVICE_GESTURES_SCROLL_ENABLE

//// Trackpad / trackpoint / joystick modes
// See users/halcyon_modules/splitkb/hlc_cirque_trackpad/hlc_cirque_trackpad.c
// Anything below can be overridden from your keymap's config.h.

// The mode is owned by the master and pushed to the other half over the
// HLC_POINTING_SYNC split transaction, declared for every module in
// users/halcyon_modules/splitkb/config.h so both halves agree on the
// transaction table. See the comment there.

#ifndef HLC_POINTING_MODE_DEFAULT
#    define HLC_POINTING_MODE_DEFAULT HLC_POINTING_TRACKPAD
#endif
#ifndef HLC_POINTING_SYNC_INTERVAL
#    define HLC_POINTING_SYNC_INTERVAL 1000 // ms between mode refreshes to the slave
#endif

// Finger position is scaled to this resolution across the pad, both axes.
// 1024 over a 35mm pad is ~0.034mm per unit; all distances below use it.
#ifndef HLC_POINTING_SCALE
#    define HLC_POINTING_SCALE 1024
#endif
#ifndef HLC_POINTING_Z_THRESHOLD
#    define HLC_POINTING_Z_THRESHOLD 0 // raise to ignore very light touches
#endif
#ifndef HLC_POINTING_TOUCH_TIMEOUT
#    define HLC_POINTING_TOUCH_TIMEOUT 100 // ms without a packet = finger lifted
#endif

// Tap detection, used by trackpoint and joystick modes (trackpad mode keeps
// using the Pinnacle driver's own CIRQUE_PINNACLE_TAP_ENABLE gesture).
#ifndef HLC_POINTING_TAP_TERM
#    define HLC_POINTING_TAP_TERM 200 // ms, longer contact is not a tap
#endif
#ifndef HLC_POINTING_TAP_RADIUS
#    define HLC_POINTING_TAP_RADIUS 60 // ~2mm of travel still counts as a tap
#endif
#ifndef HLC_POINTING_TAP_CLICK_TIME
#    define HLC_POINTING_TAP_CLICK_TIME 50 // ms the resulting click is held
#endif

// Trackpoint mode: deflection from where the finger landed drives a velocity.
#ifndef HLC_TRACKPOINT_DEADZONE
#    define HLC_TRACKPOINT_DEADZONE 35 // ~1.2mm of slack around the landing point
#endif
#ifndef HLC_TRACKPOINT_RANGE
#    define HLC_TRACKPOINT_RANGE 300 // ~10mm of deflection reaches full speed
#endif
#ifndef HLC_TRACKPOINT_SPEED
#    define HLC_TRACKPOINT_SPEED 14 // pixels per poll at full deflection (~1400px/s)
#endif
#ifndef HLC_TRACKPOINT_CURVE
#    define HLC_TRACKPOINT_CURVE 2 // 1 linear, 2 quadratic, 3 cubic response
#endif
#ifndef HLC_TRACKPOINT_TAP_DISABLE
#    define HLC_TRACKPOINT_TAP_ENABLE // tap to left click, like in trackpad mode
#endif
// #define HLC_TRACKPOINT_PRESSURE_BOOST 2 // let finger pressure add to the speed

// Joystick mode: deflection maps straight onto the two analog axes.
#ifndef HLC_JOYSTICK_CENTER_DEFAULT
#    define HLC_JOYSTICK_CENTER_DEFAULT HLC_JOYSTICK_CENTER_PAD
#endif
#ifndef HLC_JOYSTICK_DEADZONE
#    define HLC_JOYSTICK_DEADZONE 40
#endif
// Full throw well before the rim: the reachable area of a 35mm curved overlay
// is much smaller than the Pinnacle's generic window, and a stick that never
// reaches +-1.0 stays under the dead zone of most games (the default ui_*
// actions of Godot need 0.5). Raise it if the axes saturate too early for you.
#ifndef HLC_JOYSTICK_RANGE
#    define HLC_JOYSTICK_RANGE 300 // pad-centred: distance from centre for full throw
#endif
#ifndef HLC_JOYSTICK_TOUCH_RANGE
#    define HLC_JOYSTICK_TOUCH_RANGE 220 // touch-centred: radius of the virtual stick
#endif
#ifndef HLC_JOYSTICK_CURVE
#    define HLC_JOYSTICK_CURVE 1 // games apply their own curve, stay linear
#endif
#ifndef HLC_JOYSTICK_AXIS_X
#    define HLC_JOYSTICK_AXIS_X 0
#endif
#ifndef HLC_JOYSTICK_AXIS_Y
#    define HLC_JOYSTICK_AXIS_Y 1
#endif
#ifndef HLC_JOYSTICK_TAP_BUTTON
#    define HLC_JOYSTICK_TAP_BUTTON 0
#endif
// #define HLC_JOYSTICK_TAP_ENABLE // tap the pad to press that gamepad button

// Spare mouse button bits used to smuggle joystick data from the slave half to
// the master, where the HID gamepad report lives. Move them if you really use
// eight mouse buttons.
#ifndef HLC_JOYSTICK_REPORT_MARKER
#    define HLC_JOYSTICK_REPORT_MARKER (1 << 7)
#endif
#ifndef HLC_JOYSTICK_BUTTON_MARKER
#    define HLC_JOYSTICK_BUTTON_MARKER (1 << 6)
#endif

#ifdef JOYSTICK_ENABLE
// Four axes and sixteen buttons is the conventional gamepad shape; the pad
// drives axes 0/1, the left stick everywhere, and 2/3 stay at rest. Browsers
// still report mapping "n/a" -- they only apply the standard mapping to
// controllers they know by USB vendor/product id -- but engines that index a
// right stick find one.
#    ifndef JOYSTICK_AXIS_COUNT
#        define JOYSTICK_AXIS_COUNT 4
#    endif
#    ifndef JOYSTICK_BUTTON_COUNT
#        define JOYSTICK_BUTTON_COUNT 16 // map QK_JOYSTICK_BUTTON_x on any key
#    endif
#    ifndef JOYSTICK_AXIS_RESOLUTION
#        define JOYSTICK_AXIS_RESOLUTION 8 // axes ride in the 8-bit mouse report
#    endif
#endif
