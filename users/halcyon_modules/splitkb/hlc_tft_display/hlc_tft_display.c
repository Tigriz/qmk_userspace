// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "halcyon.h"
#include "hlc_tft_display.h"

#include "hardware/structs/rosc.h"

#include "hlc_pointing_mode.h"

// Fonts mono2
#include "graphics/fonts/gohufont.qff.h"

// Background artwork, 135x240 rgb565
#include "graphics/sayaka.qgf.h"

// Numbers mono2
#include "graphics/numbers/0.qgf.h"
#include "graphics/numbers/1.qgf.h"
#include "graphics/numbers/2.qgf.h"
#include "graphics/numbers/3.qgf.h"
#include "graphics/numbers/4.qgf.h"
#include "graphics/numbers/5.qgf.h"
#include "graphics/numbers/6.qgf.h"
#include "graphics/numbers/7.qgf.h"
#include "graphics/numbers/8.qgf.h"
#include "graphics/numbers/9.qgf.h"
#include "graphics/numbers/undef.qgf.h"

static const char *caps =        "CAPS";
static const char *num =         "NUM";
static const char *scroll =      "SCRL";

static painter_font_handle_t  status_font;
static painter_image_handle_t background;

static uint8_t lcd_surface_fb[SURFACE_REQUIRED_BUFFER_BYTE_SIZE(135, 240, 16)];

int color_value = 0;

painter_device_t lcd;
painter_device_t lcd_surface;

led_t last_led_usb_state = {0};
layer_state_t last_layer_state = {0};

#define GRID_WIDTH 27
#define GRID_HEIGHT 48
#define CELL_SIZE 4  // Cell size excluding outline
#define OUTLINE_SIZE 1

// Define the probability factor for initial alive cells
#define INITIAL_ALIVE_PROBABILITY 0.2  // 20% chance of being alive

bool grid[GRID_HEIGHT][GRID_WIDTH];  // Current state
bool new_grid[GRID_HEIGHT][GRID_WIDTH];  // Next state
bool changed_grid[GRID_HEIGHT][GRID_WIDTH]; // Tracks changed cells

uint32_t get_random_32bit(void) {
    uint32_t random_value = 0;
    for (int i = 0; i < 32; i++) {
        wait_ms(1);
        random_value = (random_value << 1) | (rosc_hw->randombit & 1);
    }
    return random_value;
}

void init_grid() {
    // Initialize grid with alive cells
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[y][x] = (rand() < INITIAL_ALIVE_PROBABILITY * RAND_MAX);  // Use probability factor
            changed_grid[y][x] = true;      // Mark all as changed initially
        }
    }
}

void draw_grid() {
    uint8_t hue = 0;  // Hue for alive cells
    uint8_t sat = 0;  // Saturation for alive cells
    uint8_t val_dead = 0;  // Brightness for dead cells

    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (changed_grid[y][x]) { // Only update changed cells
                uint16_t left = x * (CELL_SIZE + OUTLINE_SIZE);
                uint16_t top = y * (CELL_SIZE + OUTLINE_SIZE);
                uint16_t right = left + CELL_SIZE + OUTLINE_SIZE;
                uint16_t bottom = top + CELL_SIZE + OUTLINE_SIZE;

                // Draw the outline
                qp_rect(lcd_surface, left, top, right, bottom, hue, sat, val_dead, true);

                // Draw the filled cell inside the outline if it's alive
                if (grid[y][x]) {
                    switch (color_value) {
                    case 0:
                        qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_0, true);
                        break;
                    case 1:
                        qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_1, true);
                        break;
                    case 2:
                        qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_2, true);
                        break;
                    case 3:
                        qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_3, true);
                        break;
                    case 4:
                        qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_4, true);
                        break;
                    case 5:
                        qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_5, true);
                        break;
                    case 6:
                        qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_6, true);
                        break;
                    case 7:
                        qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_7, true);
                        break;
                    default:
                        qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_UNDEF, true);
                    }
                }
            }
        }
    }
}

void update_grid() {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int alive_neighbors = 0;

            // Count alive neighbors
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dy == 0 && dx == 0) continue;  // Skip the current cell
                    int ny = y + dy;
                    int nx = x + dx;
                    if (ny >= 0 && ny < GRID_HEIGHT && nx >= 0 && nx < GRID_WIDTH) {
                        alive_neighbors += grid[ny][nx];
                    }
                }
            }

            // Apply the rules of the Game of Life
            if (grid[y][x]) {
                // Any live cell with two or three live neighbours survives.
                new_grid[y][x] = (alive_neighbors == 2 || alive_neighbors == 3);
            } else {
                // Any dead cell with exactly three live neighbours becomes a live cell.
                new_grid[y][x] = (alive_neighbors == 3);
            }

            // Track changed cells
            changed_grid[y][x] = (grid[y][x] != new_grid[y][x]);
        }
    }

    // Copy new grid state to current grid
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[y][x] = new_grid[y][x];
        }
    }
}

// Function to add a cluster of cells at a random position
void add_cell_cluster() {
    int cluster_size = 3;  // Size of the cluster (3x3)
    int x = rand() % (GRID_WIDTH - cluster_size);
    int y = rand() % (GRID_HEIGHT - cluster_size);

    for (int dy = 0; dy < cluster_size; dy++) {
        for (int dx = 0; dx < cluster_size; dx++) {
            bool is_alive = rand() % 2; // Randomly choose between 0 and 1
            grid[y + dy][x + dx] = is_alive;  // Set the cell to be alive
            changed_grid[y + dy][x + dx] = true; // Mark the cell as changed
        }
    }
}

// Layout of the status block at the bottom of the artwork. Everything is
// expressed in line heights, so swapping the font for a taller or shorter one
// rearranges the block instead of breaking it.
#define STATUS_LINES 3
#define STATUS_PAD 3
#define STATUS_LEFT 4

typedef struct {
    uint8_t h, s, v;
} status_color_t;

static const status_color_t mode_colors[HLC_POINTING_MODE_COUNT] = {
    [HLC_POINTING_TRACKPAD]   = {HSV_MODE_TRACKPAD},
    [HLC_POINTING_TRACKPOINT] = {HSV_MODE_TRACKPOINT},
    [HLC_POINTING_JOYSTICK]   = {HSV_MODE_JOYSTICK},
};

static uint16_t status_line_y(uint8_t line) {
    return LCD_HEIGHT - (STATUS_LINES * status_font->line_height + 2 * STATUS_PAD) + STATUS_PAD + line * status_font->line_height;
}

// Text is drawn opaque, so a shorter string would leave the tail of the
// previous one behind: wipe the line first.
static void clear_status_line(uint8_t line) {
    uint16_t top = status_line_y(line);

    qp_rect(lcd_surface, 0, top, LCD_WIDTH - 1, top + status_font->line_height - 1, HSV_BLACK, true);
}

void update_display(void) {
    static bool                  drawn       = false;
    static hlc_pointing_mode_t   last_mode   = HLC_POINTING_MODE_COUNT;
    static hlc_joystick_center_t last_center = HLC_JOYSTICK_CENTER_PAD;

    if (!drawn) {
        status_font = qp_load_font_mem(font_gohufont);
        background  = qp_load_image_mem(gfx_sayaka);

        if (!status_font) {
            return;
        }

        qp_drawimage(lcd_surface, 0, 0, background);

        // Backdrop for the status block, so the text stays readable whatever
        // the artwork does behind it.
        qp_rect(lcd_surface, 0, status_line_y(0) - STATUS_PAD, LCD_WIDTH - 1, LCD_HEIGHT - 1, HSV_BLACK, true);
    }

    // Line 0: active layer
    if (last_layer_state != layer_state || !drawn) {
        static char layer_text[] = "LAYER 0";
        uint8_t     layer        = get_highest_layer(layer_state | default_layer_state);

        layer_text[6] = '0' + (layer % 10);

        clear_status_line(0);
        switch (layer) {
            case 0: qp_drawtext_recolor(lcd_surface, STATUS_LEFT, status_line_y(0), status_font, layer_text, HSV_LAYER_0, HSV_BLACK); break;
            case 1: qp_drawtext_recolor(lcd_surface, STATUS_LEFT, status_line_y(0), status_font, layer_text, HSV_LAYER_1, HSV_BLACK); break;
            case 2: qp_drawtext_recolor(lcd_surface, STATUS_LEFT, status_line_y(0), status_font, layer_text, HSV_LAYER_2, HSV_BLACK); break;
            case 3: qp_drawtext_recolor(lcd_surface, STATUS_LEFT, status_line_y(0), status_font, layer_text, HSV_LAYER_3, HSV_BLACK); break;
            case 4: qp_drawtext_recolor(lcd_surface, STATUS_LEFT, status_line_y(0), status_font, layer_text, HSV_LAYER_4, HSV_BLACK); break;
            case 5: qp_drawtext_recolor(lcd_surface, STATUS_LEFT, status_line_y(0), status_font, layer_text, HSV_LAYER_5, HSV_BLACK); break;
            case 6: qp_drawtext_recolor(lcd_surface, STATUS_LEFT, status_line_y(0), status_font, layer_text, HSV_LAYER_6, HSV_BLACK); break;
            case 7: qp_drawtext_recolor(lcd_surface, STATUS_LEFT, status_line_y(0), status_font, layer_text, HSV_LAYER_7, HSV_BLACK); break;
            default: qp_drawtext_recolor(lcd_surface, STATUS_LEFT, status_line_y(0), status_font, layer_text, HSV_LAYER_UNDEF, HSV_BLACK); break;
        }

        last_layer_state = layer_state;
    }

    // Line 1: what the Cirque module currently behaves as. The mode is owned by
    // the master and synced over, so this works from the half without the pad.
    if (last_mode != hlc_pointing_mode() || last_center != hlc_joystick_center() || !drawn) {
        last_mode   = hlc_pointing_mode();
        last_center = hlc_joystick_center();

        const char    *name  = hlc_pointing_mode_name();
        uint16_t       y     = status_line_y(1);
        status_color_t color = mode_colors[last_mode < HLC_POINTING_MODE_COUNT ? last_mode : HLC_POINTING_TRACKPAD];

        // A stick centred on the touch point rather than on the pad shifts the
        // hue of the whole line and adds a star after the label.
        bool relative = (last_mode == HLC_POINTING_JOYSTICK && last_center == HLC_JOYSTICK_CENTER_TOUCH);
        if (relative) {
            color.h += HLC_MODE_RELATIVE_HUE_SHIFT; // wraps around the colour wheel
        }

        clear_status_line(1);
        qp_drawtext_recolor(lcd_surface, STATUS_LEFT, y, status_font, name, color.h, color.s, color.v, HSV_BLACK);
        if (relative) {
            qp_drawtext_recolor(lcd_surface, STATUS_LEFT + qp_textwidth(status_font, name) + 2, y, status_font, "*", color.h, color.s, color.v, HSV_BLACK);
        }
    }

    // Line 2: the three lock keys, side by side instead of stacked
    if (last_led_usb_state.raw != host_keyboard_led_state().raw || !drawn) {
        led_t    led_usb_state = host_keyboard_led_state();
        uint16_t y             = status_line_y(2);

        // Spread the three labels across the width from their actual size, so a
        // wider font pushes them apart instead of off the panel.
        int16_t x_caps = STATUS_LEFT;
        int16_t x_num  = (LCD_WIDTH - qp_textwidth(status_font, num)) / 2;
        int16_t x_scrl = LCD_WIDTH - STATUS_LEFT - qp_textwidth(status_font, scroll);

        clear_status_line(2);
        led_usb_state.caps_lock   ? qp_drawtext_recolor(lcd_surface, x_caps, y, status_font, caps, HSV_CAPS_ON, HSV_BLACK)
                                  : qp_drawtext_recolor(lcd_surface, x_caps, y, status_font, caps, HSV_CAPS_OFF, HSV_BLACK);
        led_usb_state.num_lock    ? qp_drawtext_recolor(lcd_surface, x_num, y, status_font, num, HSV_NUM_ON, HSV_BLACK)
                                  : qp_drawtext_recolor(lcd_surface, x_num, y, status_font, num, HSV_NUM_OFF, HSV_BLACK);
        led_usb_state.scroll_lock ? qp_drawtext_recolor(lcd_surface, x_scrl, y, status_font, scroll, HSV_SCROLL_ON, HSV_BLACK)
                                  : qp_drawtext_recolor(lcd_surface, x_scrl, y, status_font, scroll, HSV_SCROLL_OFF, HSV_BLACK);

        last_led_usb_state = led_usb_state;
    }

    drawn = true;
}

// Called from halcyon.c
void module_suspend_power_down_kb(void) {
    qp_power(lcd, false);
}

// Called from halcyon.c
void module_suspend_wakeup_init_kb(void) {
    qp_power(lcd, true);
}

// Called from halcyon.c
bool module_post_init_kb(void) {
    // Turn on backlight
    backlight_enable();

    // Make the devices
    lcd = qp_st7789_make_spi_device(LCD_WIDTH, LCD_HEIGHT, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN, LCD_SPI_DIVISOR, LCD_SPI_MODE);
    lcd_surface = qp_make_rgb565_surface(LCD_WIDTH, LCD_HEIGHT, lcd_surface_fb);

    // Initialise the LCD
    qp_init(lcd, LCD_ROTATION);
    qp_set_viewport_offsets(lcd, LCD_OFFSET_X, LCD_OFFSET_Y);
    qp_clear(lcd);
    qp_rect(lcd, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, HSV_BLACK, true);
    qp_power(lcd, true);
    qp_flush(lcd);

    // Initialise the LCD surface
    qp_init(lcd_surface, LCD_ROTATION);
    qp_rect(lcd_surface, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, HSV_BLACK, true);
    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    qp_flush(lcd);

    if(!module_post_init_user()) { return false; }

    return true;
}

// Called from halcyon.c
bool display_module_housekeeping_task_kb(bool second_display) {
    if(!display_module_housekeeping_task_user(second_display)) { return false; }

    if(second_display) {
        static uint32_t last_draw = 0;
        static bool second_display_set = false;
        static uint32_t previous_matrix_activity_time = 0;

        if(!second_display_set) {
            srand(get_random_32bit());
            init_grid();
            color_value = rand() % 8;
            second_display_set = true;
        }

        if (timer_elapsed32(last_draw) >= 100) { // Throttle to 10 fps
            draw_grid();
            update_grid();

            if (previous_matrix_activity_time != last_matrix_activity_time()) {
                color_value = rand() % 8;
                add_cell_cluster();
                previous_matrix_activity_time = last_matrix_activity_time();
            }

            last_draw = timer_read32();
        }
    }

    // Update display information (layers, numlock, etc.)
    if(!second_display) {
        update_display();
    }

    // Move surface to lcd
    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    qp_flush(lcd);

    return true;
}
