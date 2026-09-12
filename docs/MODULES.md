# Changing module behavior

If you want to change the behavior of a module that doesn't have any options within vial you will need to change the code in the `users/halcyon_modules/splitkb/<module_name>` folder. Within this folder you can change the `.c`, `.h`, `config.h` or `rules.mk` files to your liking. For the actual configuration options, check out the QMK documentation.

If you want to add any custom `.c` files you can do so by adding a `SRC += <file>.c` to your keymaps `rules.mk` or `SRC += $(CURRENT_DIR)/<file>.c` to the modules `rules.mk`

For the modules we added some extra hooks. Mainly the following:

* `void module_post_init_kb(void)`
* `void module_post_init_user(void)`
* `void module_housekeeping_task_kb(void)`
* `void module_housekeeping_task_user(void)`

These four are added because we already use the `keyboard_post_init_kb` and `housekeeping_task_kb` in the main halcyon code. Any module can then use the other hooks to do anything else.  

* `void display_module_housekeeping_task_kb(bool second_display)`
* `void display_module_housekeeping_task_user(bool second_display)`  

Finally we added a display housekeeping task. We added the feature where the keyboard can detect if there are one or two displays connected. Because we always want to show information about layers if we only have one display, where it doesn't matter where the display is located. We use this to determine if the keyboard with display module is the second display or not and draw the content accordingly.

## Display

To customize your display you will need to add some files to your keymap. A complete example can be found in the `examples` directory in our `qmk_userspace` fork. The files in this example can be added to your keymap folder. Some more simple examples can be found below.

Within the example you can see how a display code should be built up. The functions used here are more thoroughly explained in the [Quantum Painter API documentation from QMK](https://docs.qmk.fm/quantum_painter#quantum-painter-api). Because we added some extra hooks which you can read more about [here](#changing-module-behavior), any actual display initialization is already done so you don't have to worry about that. We make use of the surface feature of quantum painter. This makes it so it only draws changed parts of the display. Which is why we use the `qp_surface_draw` feature at the end of the code. 

There are some quirks when using Quantum Painter which we noticed while creating our firmware which can help if you want to create your own display behavior.
* Font size is determined when generating the file using the CLI.
* You need to generate mono2 fonts if you want to recolor the font but this pretty much breaks any aliased font. So a pixel font is recommended.
* Using pixel fonts you can scale them 2 or 4 times larger but somehow they break at a certain point when going too large. We fixed this by just creating images of the fonts and using that.
* If you want to wrap around text, you'll need to create a custom function for that.
* Drawing a full screen image can give the keyboard noticeable lag. For startup this is okay but switching images every couple of seconds could become annoying.
* This also applies for animations. Smaller size animations are fine, from testing the animations could take up around 30% of the screen and still have the keyboard be responsive but when having animations on the entire screen it can slow down the entire keyboard.
* Using large images or animations can eat up the firmware size very quickly so be aware of that.
* Our displays are 240*135 pixels.

You can also look in the `users/halcyon_modules/hlc_tft_display/` folder to see how we implemented the display code.

To load new fonts or images you will need to convert them using the [Quantum Painter CLI tools.](https://docs.qmk.fm/quantum_painter#quantum-painter-cli)


### Example: draw a picture on the second display

First convert your 240*135 image to a QGF file:
`qmk painter-convert-graphics -f rgb565 -i my_image.png`

Copy the generated files to your keymap.

In your `rules.mk` add

```makefile
SRC += my_image.qgf.c
```

And in your keymap.c add:

```c
#include "hlc_tft_display/hlc_tft_display.h"
#include "qp_surface.h"
#include "my_image.qgf.h"

static painter_image_handle_t my_image;

painter_device_t lcd;
painter_device_t lcd_surface;

bool module_post_init_user(void) {
    return false;
}

bool display_module_housekeeping_task_user(bool second_display) {
    static bool display_set = false;

    if(second_display) {
        if (!display_set) {
            my_image = qp_load_image_mem(gfx_my_image); // Get the `gfx_my_image` from the `my_image.qgf.h` file
            qp_drawimage(lcd_surface, 0, 0, my_image);
        }
    }

    if(!second_display) {
        // Re-use the function to display layers and status
        update_display();
    }

    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);

    return false;
}
```


### Example: write some colorful text on the display

First convert your font to an image file:

```bash
qmk painter-make-font-image -s size_of_font -o ./ -f my_font.ttf
```

Now convert the generated font image

```bash
qmk painter-convert-font-image -f mono2 -i my_font.png
```

In your `rules.mk` add

```makefile
SRC += my_font.qff.c
```

And in your keymap.c add:

```c
#include "hlc_tft_display/hlc_tft_display.h"
#include "qp_surface.h"
#include "my_font.qff.h"

static painter_font_handle_t my_font;

painter_device_t lcd;
painter_device_t lcd_surface;

bool module_post_init_user(void) {
    my_font = qp_load_font_mem(font_my_font);
    static const char *text = "Hello from SplitKB!";
    int16_t width = qp_textwidth(my_font, text);
    qp_drawtext_recolor(lcd_surface, (LCD_WIDTH - width), (LCD_HEIGHT - my_font->line_height), my_font, text, HSV_BLUE, HSV_BLACK);
    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    return false;
}

bool display_module_housekeeping_task_user(bool second_display) {
    return false;
}
```

## Encoder

Look at the [QMK documentation for the encoders feature](https://docs.qmk.fm/features/encoders) to see what options are available.

## Cirque

Look at the [QMK documentation for the cirque trackpad](https://docs.qmk.fm/features/pointing_device#cirque-trackpad) to see what options are available.

### Pointing modes: trackpad, trackpoint, joystick

The trackpad module can behave as three different pointing devices, switched at runtime:

| Mode | Behaviour |
| --- | --- |
| `trackpad` | Stock QMK: absolute finger position becomes relative cursor movement, with the Pinnacle tap and circular scroll gestures. |
| `trackpoint` | Pointing stick, like the Lenovo nub: the spot where your finger lands becomes the rest position, and pushing away from it moves the cursor at a speed proportional to the deflection. The cursor keeps moving as long as you hold. |
| `joystick` | HID gamepad: the finger position becomes two absolute analog axes. The keyboard shows up as a gamepad next to the keyboard and mouse. |

The active mode lives on the master half and is pushed to the other half over its own split transaction, `HLC_POINTING_SYNC`, so it does not matter which side the module is plugged into.

Each half runs the firmware built for *its* module, so the two are usually different builds. The split handshake XORs every transaction with `NUM_TOTAL_TRANSACTIONS`, which means both halves must declare the exact same transactions or they stop talking to each other altogether. That is why `HLC_POINTING_SYNC` is declared in `users/halcyon_modules/splitkb/config.h`, for every module, and not in the trackpad module's own `config.h`. Keep any transaction you add there too.

#### Switching modes

Five keycodes are exposed, based at `QK_KB_0` (0x7E00), which is what Vial sends for the first entry of `customKeycodes` -- its `USER00` is `QK_KB_0 + 0`, *not* `QK_USER_0`. `SAFE_RANGE` therefore stays free for your own keymap keycodes. Redefine `HLC_POINTING_KEYCODE_BASE` in your keymap's `config.h` if you need that range for something else.

| Keycode | Alias | Action |
| --- | --- | --- |
| `HLC_PMODE_NEXT` | `PM_NEXT` | cycle trackpad -> trackpoint -> joystick |
| `HLC_PMODE_PAD` | `PM_PAD` | switch to trackpad |
| `HLC_PMODE_TPT` | `PM_TPT` | switch to trackpoint |
| `HLC_PMODE_JOY` | `PM_JOY` | switch to joystick |
| `HLC_PMODE_JCTR` | `PM_JCTR` | toggle how the joystick is centred |

In a `keymap.c`, include the module header and hand your `process_record_user()` over to it:

```c
#ifdef HLC_CIRQUE_TRACKPAD
#    include "hlc_cirque_trackpad/hlc_cirque_trackpad.h"
#else // keep the keycodes usable on builds without the trackpad module
#    define PM_NEXT KC_NO
#    define PM_PAD KC_NO
#    define PM_TPT KC_NO
#    define PM_JOY KC_NO
#    define PM_JCTR KC_NO
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
#ifdef HLC_CIRQUE_TRACKPAD
    if (!hlc_pointing_process_record(keycode, record)) {
        return false;
    }
#endif

    return true;
}
```

Keymaps without a `keymap.c` (json and Vial keymaps) get a default `process_record_user()` from the module and need no code at all. For Vial, the keycodes are the `customKeycodes` of your `vial.json`, in the order below; the Elora `vial_hlc` and `vial_hlc_legacy` keymaps already carry them, other boards need the same array added.

| Vial keycode | Module keycode |
| --- | --- |
| `USER00` | cycle modes |
| `USER01` | trackpad |
| `USER02` | trackpoint |
| `USER03` | joystick |
| `USER04` | joystick centering |

`hlc_pointing_mode()` and `hlc_joystick_center()` return the current state, which is handy to show the mode on a display module.

#### Joystick mode

`JOYSTICK_ENABLE` is turned on by the module. That adds a gamepad interface to the USB device, which also means any key can send `QK_JOYSTICK_BUTTON_0` .. `QK_JOYSTICK_BUTTON_7` while you are at it. Compile with `-e HLC_JOYSTICK=0` to drop the extra USB interface; the module then only cycles between trackpad and trackpoint.

Two ways of centring the stick, toggled with `PM_JCTR`:

* `HLC_JOYSTICK_CENTER_PAD` (default): the centre of the pad is the rest position and the edge is full deflection, like a real stick seen from above.
* `HLC_JOYSTICK_CENTER_TOUCH`: wherever you land becomes the rest position, like the virtual stick of a phone game.

The axes are absolute values, so they have to be sent from the master half. They travel there inside the `x`/`y` fields of the regular mouse report, flagged with a spare button bit, and are unpacked in `halcyon.c` by `hlc_pointing_decode_report()`. Riding on `x`/`y` means the axes get the same rotation and inversion treatment from `pointing_device_adjust_by_defines()` as the cursor, so both modes agree on which way is up. Nothing of that leaks to the host.

#### Tuning

Every value below can be overridden from your keymap's `config.h`; the defaults live in `users/halcyon_modules/splitkb/hlc_cirque_trackpad/config.h`. Distances are in units of `HLC_POINTING_SCALE` (1024) across the pad, so on a 35mm pad one unit is about 0.034mm.

| Define | Default | Meaning |
| --- | --- | --- |
| `HLC_POINTING_MODE_DEFAULT` | `HLC_POINTING_TRACKPAD` | mode after a reset |
| `HLC_TRACKPOINT_DEADZONE` | `35` | slack around the landing point (~1.2mm) |
| `HLC_TRACKPOINT_RANGE` | `300` | deflection that reaches full speed (~10mm) |
| `HLC_TRACKPOINT_SPEED` | `14` | pixels per 10ms poll at full deflection (~1400px/s) |
| `HLC_TRACKPOINT_CURVE` | `2` | 1 linear, 2 quadratic, 3 cubic response |
| `HLC_TRACKPOINT_TAP_DISABLE` | unset | define it to drop tap-to-click in trackpoint mode |
| `HLC_TRACKPOINT_PRESSURE_BOOST` | unset | define it (try `2`) to let finger pressure add to the speed |
| `HLC_JOYSTICK_DEADZONE` | `40` | radial dead zone around the rest position |
| `HLC_JOYSTICK_RANGE` | `430` | pad-centred: distance from the centre for full throw |
| `HLC_JOYSTICK_TOUCH_RANGE` | `250` | touch-centred: radius of the virtual stick |
| `HLC_JOYSTICK_CURVE` | `1` | response curve, linear by default |
| `HLC_JOYSTICK_TAP_ENABLE` | unset | define it to make a tap press `HLC_JOYSTICK_TAP_BUTTON` |
| `HLC_POINTING_TAP_TERM` | `200` | ms, longer contact is not a tap |
| `HLC_POINTING_TAP_RADIUS` | `60` | travel still counted as a tap (~2mm) |
| `HLC_POINTING_Z_THRESHOLD` | `0` | raise it to ignore very light touches |

On Linux, `jstest /dev/input/js0` or `evtest` is enough to check the joystick mode; on any OS, [hardwaretester.com/gamepad](https://hardwaretester.com/gamepad) works in the browser.
