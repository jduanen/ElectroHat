/*
* NeoPixel LED Ring library
*/

#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>


#define NEO_PIXEL_RING_VERSION  "1.0"

#define LED_PIN             15
#define LED_COUNT           16
#define DEF_LED_BRIGHTNESS  50  // max = 255

#define PATTERN_FUNC(func)  [](NeoPixelRing* npr) { return npr->func();}

class NeoPixelRing;
using PatternFunc = void (*)(NeoPixelRing*);

typedef struct {
    PatternFunc func;
    char *name;
} Patterns;

typedef struct {
    uint32_t startColor;
    uint32_t endColor;
} ColorRange;


class NeoPixelRing {
public:
    String libVersion = NEO_PIXEL_RING_VERSION;

    enum LED_TEST {
        ALL_WHITE = 0,
        ALL_RED   = 1,
        ALL_GREEN = 2,
        ALL_BLUE  = 3,
        ALL_OFF   = 4,
        ALL_BLINK = 5
    };

    NeoPixelRing();
    NeoPixelRing(byte brightness);
    void test(byte test);
    void clear();
    unsigned long run();
    uint32_t makeColor(byte r, byte g, byte b);
    void setColor(uint32_t color);
    uint32_t getColor();
    void setDelay(uint32_t delay);
    uint32_t getDelay();
    byte getNumPatterns();
    byte getPatternNames(char *namePtrs[], byte number);
    byte getSelectedPattern();
    bool selectPattern(byte patternNum);
    uint32_t getEnabledCustomPixels();
    void getCustomPixels(uint32_t pixels, ColorRange colorRanges[], int size);
    void disableCustomPixels(uint32_t pixels);
    void enableCustomPixels(uint32_t pixels, uint32_t startColor, uint32_t endColor);
    uint32_t getCustomDelta();
    void setCustomDelta(uint32_t delta);
    bool getCustomBidir();
    void setCustomBidir(bool bidir);

private:
    Adafruit_NeoPixel *_ring = new Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

    static byte const _NUM_PATTERNS = 6;    //// FIXME

    byte _pixelNum = 0;
    uint32_t _color = 0x000000;
    uint32_t _delay = 0;
    byte _patternNum = 0;
    unsigned long _nextRunTime = 0;
    uint32_t _firstPixelHue = 0;
    unsigned long _loopCnt = 0;
    ColorRange _colorRanges[LED_COUNT];
    uint32_t _customPixelEnables = 0;
    int _customDelta = 64;
    bool _customBidir = false;

    void _create(byte brightness);

    void _colorWipe();
    void _colorFill();
    void _marquee();
    void _rainbowMarquee();
    void _rainbow();
    void _custom();

    //// FIXME
    const Patterns _patterns[_NUM_PATTERNS] = {
        {PATTERN_FUNC(_colorWipe), "Color Wipe"},
        {PATTERN_FUNC(_colorFill), "Color Fill"},
        {PATTERN_FUNC(_marquee), "Marquee"},
        {PATTERN_FUNC(_rainbowMarquee), "Rainbow Marquee"},
        {PATTERN_FUNC(_rainbow), "Rainbow"},
        {PATTERN_FUNC(_custom), "Custom"}
    };
};

#include "NeoPixelRing.hpp"
