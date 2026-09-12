POST_CONFIG_H += $(USER_PATH)/splitkb/hlc_cirque_trackpad/config.h
SRC += $(USER_PATH)/splitkb/hlc_cirque_trackpad/hlc_cirque_trackpad.c

# HID gamepad, for the joystick mode of the trackpad.
# Compile with `-e HLC_JOYSTICK=0` to drop the extra USB interface; the module
# then only cycles between trackpad and trackpoint.
HLC_JOYSTICK ?= 1
ifeq ($(strip $(HLC_JOYSTICK)), 1)
  JOYSTICK_ENABLE = yes
  JOYSTICK_DRIVER = digital
endif
