// Pin selections here are based on the original Adafruit Learning System
// guide for the Teensy 3.x project.  Some of these pin numbers don't even
// exist on the smaller SAMD M0 & M4 boards, so you may need to make other
// selections:

// GRAPHICS SETTINGS (appearance of eye) -----------------------------------

// Default shape includes the caruncle, creating distinct left/right eyes.

// Enable ONE of these #includes -- HUGE graphics tables for various eyes:
//#include "data/defaultEye.h"      // Standard human-ish hazel eye -OR-
#include "data/eye.h"

// DISPLAY HARDWARE SETTINGS (screen type & connections) -------------------
#define TFT_COUNT         1  // Number of screens (1 or 2)
//#define TFT1_CS          -1  // TFT 1 chip select pin (set to -1 to use TFT_eSPI setup)
//#define TFT2_CS          -1  // TFT 2 chip select pin (set to -1 to use TFT_eSPI setup)
#define TFT_1_ROT         3  // TFT 1 rotation
#define TFT_2_ROT         1  // TFT 2 rotation
#define EYE_1_XPOSITION   24 // x shift for eye 1 image on display
#define EYE_2_XPOSITION   24 // x shift for eye 2 image on display

#define DISPLAY_BACKLIGHT 0  // Pin for backlight control (-1 for none)
#define BACKLIGHT_MAX     255

// EYE LIST ----------------------------------------------------------------

#define NUM_EYES     2 // Number of eyes to display (1 or 2)

#define LH_WINK_PIN -1 // Left wink pin (set to -1 for no pin)
#define RH_WINK_PIN -1 // Right wink pin (set to -1 for no pin)

// This table contains ONE LINE PER EYE.  The table MUST be present with
// this name and contain ONE OR MORE lines.  Each line contains THREE items:
// a pin number for the corresponding TFT/OLED display's SELECT line, a pin
// pin number for that eye's "wink" button (or -1 if not used), a screen
// rotation value (0-3) and x position offset for that eye.

eyeInfo_t eyeInfo[] = {
#if (NUM_EYES == 2)
    { TFT1_CS, LH_WINK_PIN, TFT_1_ROT, EYE_1_XPOSITION }, // LEFT EYE chip select and wink pins, rotation and offset
    { TFT2_CS, RH_WINK_PIN, TFT_2_ROT, EYE_2_XPOSITION }, // RIGHT EYE chip select and wink pins, rotation and offset
#else
    { TFT1_CS, LH_WINK_PIN, TFT_1_ROT, EYE_1_XPOSITION }, // EYE chip select and wink pins, rotation and offset
#endif
};

//#define TRACKING            // If defined, eyelid tracks pupil

#define LIGHT_PIN       A0

#define LIGHT_CURVE     0.33 // Light sensor adjustment curve
#define LIGHT_MIN       0    // Minimum useful reading from light sensor
#define LIGHT_MAX       1023 // Maximum useful reading from sensor

//#define IRIS_SMOOTH         // If enabled, filter input from IRIS_PIN
#if !defined(IRIS_MIN)      // Each eye might have its own MIN/MAX
    #define IRIS_MIN    90  // Iris size (0-1023) in brightest light
#endif
#if !defined(IRIS_MAX)
    #define IRIS_MAX    130 // Iris size (0-1023) in darkest light
#endif

#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   240
