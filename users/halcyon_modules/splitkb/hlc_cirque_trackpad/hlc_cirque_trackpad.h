// Copyright 2026 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include QMK_KEYBOARD_H

// The pointing mode itself, its keycodes and its split sync live one level up,
// in hlc_pointing_mode.h: every Halcyon firmware needs them, not just this one.
#include "hlc_pointing_mode.h"

// Master-side hook, called from halcyon.c on both halves' reports: unpacks the
// joystick axes that joystick mode hides in the mouse report.
report_mouse_t hlc_pointing_decode_report(report_mouse_t mouse_report);
