//  This is a modification of the Animated_Eyes_2/ sketch in the TFT_eSPI
// examples library.  Removed user code feature, made work with 240x240
// round LCD (from Waveshare) driven by a XIAO RP2040 controller.

// Wiring:
// Function    rp2040 pin   TFT 1    TFT 2
// --------    ----------   -----    -----
//  MOSI       11 (P3)   -> DIN   -> DIN     // SPI Data In
//  MISO                                     // Not Connected
//  SCLK        9 (P2)   -> SCK   -> SCK     // SPI Clock
//  TFT_DC      5 (P6)   -> DC    -> DC      // Data/Command
//  TFT_RST     6 (P7)   -> RST   -> RST     // Reset
//  TFT1_CS     3 (P28)  -> CS               // Connected to TFT 1 only 
//  TFT2_CS     4 (P29)  ->          CS      // Connected to TFT 2 only 
//  3V3        12 (3V3)  -> 3.3V  -> 3.3V    // Regulated Power Out
//  GND        13 (GND)  -> GND   -> GND     // Ground
//  +5V/VIN    14 (5V)   -> 5V    -> 5V      // Power In

// An adaption of the "UncannyEyes" sketch (see eye_functions tab)
// for the TFT_eSPI library. As written the sketch is for driving
// two TFT displays.

// The size of the displayed eye is determined by the screen size and
// resolution. The eye image is 128 pixels wide. In humans the palpebral
// fissure (open eye) width is about 30mm so a low resolution, large
// pixel size display works best to show a scale eye image. Note that
// display manufacturers usually quote the diagonal measurement, so a
// 128 x 128 1.7" display or 128 x 160 2" display is about right.

// Configuration settings for the eye, eye style, display count,
// chip selects and x offsets can be defined in the sketch "config.h" tab.

// Performance (frames per second = fps) can be improved by using
// DMA (for SPI displays only) on ESP32 and STM32 processors. Use
// as high a SPI clock rate as is supported by the display. 27MHz
// minimum, some displays can be operated at higher clock rates in
// the range 40-80MHz.

// Single defaultEye performance for different processors
//                                  No DMA   With DMA
// ESP8266 (160MHz CPU) 40MHz SPI   36 fps
// ESP32 27MHz SPI                  53 fps     85 fps
// ESP32 40MHz SPI                  67 fps    102 fps
// ESP32 80MHz SPI                  82 fps    116 fps // Note: Few displays work reliably at 80MHz
// STM32F401 55MHz SPI              44 fps     90 fps
// STM32F446 55MHz SPI              83 fps    155 fps
// STM32F767 55MHz SPI             136 fps    197 fps
// XIAO RP2040 ??MHz SPI            40 fps     65 fps

// DMA can be used with RP2040, STM32 and ESP32 processors when the interface
// is SPI, uncomment the next line:
#define USE_DMA

// Load TFT driver library
#include <SPI.h>
#include <TFT_eSPI.h>
TFT_eSPI tft;           // A single instance is used for 1 or 2 displays

// A pixel buffer is used during eye rendering
#define BUFFER_SIZE 1024 // 128 to 1024 seems optimum

#ifdef USE_DMA
    #define BUFFERS 2      // 2 toggle buffers with DMA
#else
    #define BUFFERS 1      // 1 buffer for no DMA
#endif

uint16_t pbuffer[BUFFERS][BUFFER_SIZE]; // Pixel rendering buffer
bool     dmaBuf   = 0;                  // DMA buffer selection

// This struct is populated in config.h
typedef struct {        // Struct is defined before including config.h --
    int8_t  select;       // pin numbers for each eye's screen select line
    int8_t  wink;         // and wink button (or -1 if none) specified there,
    uint8_t rotation;     // also display rotation and the x offset
    int16_t xposition;    // position of eye on the screen
} eyeInfo_t;

#include "config.h"     // ****** CONFIGURATION IS DONE IN HERE ******

#define SCREEN_X_START 0
#define SCREEN_X_END   SCREEN_WIDTH   // Badly named, actually the "eye" width!
#define SCREEN_Y_START 0
#define SCREEN_Y_END   SCREEN_HEIGHT  // Actually "eye" height

// A simple state machine is used to control eye blinks/winks:
#define NOBLINK 0       // Not currently engaged in a blink
#define ENBLINK 1       // Eyelid is currently closing
#define DEBLINK 2       // Eyelid is currently opening
typedef struct {
    uint8_t     state;     // NOBLINK/ENBLINK/DEBLINK
    uint32_t    duration;  // Duration of blink state (micros)
    uint32_t    startTime; // Time (micros) of last state change
} eyeBlink;

struct {  // One-per-eye structure
    int16_t     tft_cs;    // Chip select pin for each display
    eyeBlink    blink;     // Current blink/wink state
    int16_t     xposition; // x position of eye image
} eye[NUM_EYES];

uint32_t startTime;  // For FPS indicator

// INITIALIZATION -- runs once at startup ----------------------------------
void setup(void) {
    delay(1000);
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting");

#if defined(DISPLAY_BACKLIGHT) && (DISPLAY_BACKLIGHT >= 0)
    // Enable backlight pin, initially off
    Serial.println("Backlight turned off");
    pinMode(DISPLAY_BACKLIGHT, OUTPUT);
    digitalWrite(DISPLAY_BACKLIGHT, LOW);
#endif

    // Initialise the eye(s), this will set all chip selects low for the tft.init()
    initEyes();

    // Initialise TFT
    Serial.println("Initialising displays");
    Serial.println("W: " + String(SCREEN_WIDTH) + ", H: " + String(SCREEN_HEIGHT));
    delay(1000);
    tft.init();

#ifdef USE_DMA
    if (tft.initDMA()) {
        Serial.println("DMA enabled");
    } else {
        Serial.println("DMA init failed");
    }
#endif

    // Raise chip select(s) so that displays can be individually configured
    digitalWrite(eye[0].tft_cs, HIGH);
    if (NUM_EYES > 1) digitalWrite(eye[1].tft_cs, HIGH);

    for (uint8_t e = 0; e < NUM_EYES; e++) {
        digitalWrite(eye[e].tft_cs, LOW);
        tft.setRotation(eyeInfo[e].rotation);
        tft.fillScreen(TFT_BLACK);
        //// TMP TMP TMP
        ////tft.fillRect(0, 120, 240, 120, TFT_BLUE);
        digitalWrite(eye[e].tft_cs, HIGH);
    }

#if defined(DISPLAY_BACKLIGHT) && (DISPLAY_BACKLIGHT >= 0)
    Serial.println("Backlight now on!");
    analogWrite(DISPLAY_BACKLIGHT, BACKLIGHT_MAX);
#endif

    startTime = millis(); // For frame-rate calculation
}

// MAIN LOOP -- runs continuously after setup() ----------------------------
void loop() {
    updateEye();
}
