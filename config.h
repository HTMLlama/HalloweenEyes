// config.h -- everything you are likely to want to change lives here.
#pragma once

// ============================================================================
// Wiring (same as the original HaloweenEyes build)
// ============================================================================
#define OLED1_CS 5     // display 1: the wearer's LEFT eye
#define OLED1_DC 6
#define OLED1_RST 12
#define OLED2_CS 9     // display 2: the wearer's RIGHT eye
#define OLED2_DC 10
#define OLED2_RST 13   // pin 13 is also the Feather's red LED, so it stays lit
#define JOY_X_PIN A0
#define JOY_Y_PIN A1
#define JOY_BTN_PIN 11
#define VBAT_PIN A6    // Feather M4 battery monitor (half of VBAT)

// ============================================================================
// Displays
// ============================================================================
// SPI clock for the OLEDs. The display link is what limits the frame rate on
// the big creature/machine eyes, so use the fastest one your wiring handles:
//   24000000  24 MHz   fastest; needs short, tidy wires (failed on this build)
//   16000000  16.7 MHz tested clean on this build (the default)
//   12000000  12 MHz   what the original sketch used; safest
// Too fast shows as a frozen old image, black screens, speckles or garbage.
#define SPI_HZ 16000000

// Same rotation the original sketch used.
#define EYE1_ROTATION 3
#define EYE2_ROTATION 3

// Which side of each panel is toward your nose, as seen by someone looking at
// you. This mirrors the eyelid shapes. If the "angry" mood looks sad instead,
// swap these two.
#define EYE1_NASAL_ON_LEFT true
#define EYE2_NASAL_ON_LEFT false

// Nudge each eye (in pixels, +x right, +y down) to centre it in the mask.
#define EYE1_OFFSET_X 0
#define EYE1_OFFSET_Y 0
#define EYE2_OFFSET_X 0
#define EYE2_OFFSET_Y 0

// Brightness steps cycled by a triple click (SSD1351 master current, 0-15).
#define BRIGHTNESS_LEVELS {15, 10, 6}

// Load a gamma curve into the OLEDs so colours and shading match the preview
// images (the panels are linear by default, which washes out mid-tones).
// Toggle live over USB serial with 'g' to compare.
#define GAMMA_CORRECT 1

// ============================================================================
// Joystick
// ============================================================================
#define JOYSTICK_ENABLED 1
#define JOY_INVERT_X 1   // both flipped for how the stick is mounted in the frame
#define JOY_INVERT_Y 1
#define JOY_SWAP_XY 0
#define JOY_DEADZONE 0.12f  // fraction of full throw ignored around centre
#define JOY_THROW 0.85f     // fraction of the ADC half-range that counts as "full"

// Click while holding the stick hard left/right to wink instead of changing
// style.
#define CLICK_DEFLECTED_WINKS 1

// ============================================================================
// Behaviour
// ============================================================================
// Style shown at power-up: -1 = random, otherwise an index (send 'l' over
// serial for the list).
#define BOOT_STYLE -1

// Styles in the click rotation (shuffled, no repeats until all have shown).
// {-1} means every style. Example: {0, 2, 6, 18} for just a favourite few.
#define STYLE_PLAYLIST {-1}

// 1 = "PLEASE STANDBY" TV channel-change between styles,
// 0 = quick blink-and-swap.
#define TRANSITION_TV 1

// Change style automatically after this many minutes without a button press
// (0 = never).
#define AUTO_CYCLE_MINUTES 0

// ============================================================================
// Power
// ============================================================================
#define LOW_BATTERY_PCT 15       // standby card shows a warning below this
#define CRITICAL_BATTERY_PCT 5   // eyes turn sleepy and dim below this

// ============================================================================
// Debug
// ============================================================================
#define SERIAL_BAUD 115200
#define PRINT_STATS 0  // print fps / timing every 2 s (toggle with 'f')
