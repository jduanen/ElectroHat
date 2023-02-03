/***************************************************************************
 *
 * NeoPixel LED Ring library
 *
 ***************************************************************************/

#define LED_FUDGE_FACTOR    10  // let things be up to 10msec late without complaining

#define GET_RED(c)          ((c >> 16) & 0xFF)
#define GET_GREEN(c)        ((c >> 8) & 0xFF)
#define GET_BLUE(c)         (c & 0xFF)

#define RED_COMP            0x00FF0000
#define GREEN_COMP          0x0000FF00
#define BLUE_COMP           0x000000FF

// ASSERT: 0 < NUM_COMETS < (NUM_COMETS * COMET_LENGTH)
// ASSERT: (NUM_COMETS * COMET_LENGTH) <= NUM_LEDS)
#define COMET_LENGTH        5
#define NUM_COMETS          2

#define DECAY_MULTIPLIER    1
#define DECAY_RED(i, c)     (((c & RED_COMP) >> (i * DECAY_MULTIPLIER)) & RED_COMP)
#define DECAY_GREEN(i, c)   (((c & GREEN_COMP) >> (i * DECAY_MULTIPLIER)) & GREEN_COMP)
#define DECAY_BLUE(i, c)    (((c & BLUE_COMP)>> (i * DECAY_MULTIPLIER)) & BLUE_COMP)

#define DECAY_COLOR(i, c)   (DECAY_RED(i, c) | DECAY_GREEN(i, c) | DECAY_BLUE(i, c))


template<uint8_t numLeds>
NeoPixelRing<numLeds>::NeoPixelRing() {
    _create(DEF_LED_BRIGHTNESS);
};

template<uint8_t numLeds>
NeoPixelRing<numLeds>::NeoPixelRing(byte brightness) {
    _create(brightness);
};

template<uint8_t numLeds>
byte NeoPixelRing<numLeds>::getNumLeds() {
    return numLeds;
};

template<uint8_t numLeds>
byte NeoPixelRing<numLeds>::getBrightness() {
    return _brightness;
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::setBrightness(byte brightness) {
    _brightness = brightness;
    ring->setBrightness(brightness);
};

template<uint8_t numLeds>
uint32_t NeoPixelRing<numLeds>::getColor() {
    return _color;
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::setColor(uint32_t color) {
    _color = color;
};

template<uint8_t numLeds>
uint32_t NeoPixelRing<numLeds>::getDelay() {
    return _patternDelay;
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::setDelay(uint32_t delay) {
    _patternDelay = delay;
};

template<uint8_t numLeds>
byte NeoPixelRing<numLeds>::getNumPatterns() {
    return _numPatterns;
};

//// FIXME use the right kind of args to capture both dims
template<uint8_t numLeds>
byte NeoPixelRing<numLeds>::getPatternNames(char *namePtrs[], byte number) {
    int i = 0;
    for (i = 0; (i < min(number, _numPatterns)); i++) {
        namePtrs[i] = _patterns[i].name;
    }
    return i;
};

template<uint8_t numLeds>
byte NeoPixelRing<numLeds>::getSelectedPattern() {
    return _patternNum;
};

template<uint8_t numLeds>
bool NeoPixelRing<numLeds>::selectPattern(byte patternNum) {
    if (patternNum >= _numPatterns) {
        return true;
    }
    _patternNum = patternNum;
    return false;
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::enableRandomPattern(bool enable) {
    _randomPattern = enable;
    _nextRunTime = millis() + 1;
}

template<uint8_t numLeds>
bool NeoPixelRing<numLeds>::randomPattern() {
    return _randomPattern;
}

// Return which pixels are enabled for custom patterns.
template<uint8_t numLeds>
uint32_t NeoPixelRing<numLeds>::getEnabledCustomPixels() {
    return _customPixelEnables;
};

// Enable pixels for custom display mode and provide a pair of colors to be
//  cycled between.
// Give a bit-vector where a 1 means to disable the corresponding pixel.
template<uint8_t numLeds>
void NeoPixelRing<numLeds>::enableCustomPixels(uint32_t pixels, uint32_t startColor, uint32_t endColor) {
    for (int i = 0; (i < numLeds); i++) {
        if (pixels & (1 << i)) {
            _colorRanges[i].startColor = startColor;
            _colorRanges[i].endColor = endColor;
            _customPixelEnables |= (1 << i);
        }
    }
};

// Disable pixels for custom display mode.
// Give a bit-vector where a 1 means to disable the corresponding pixel.
template<uint8_t numLeds>
void NeoPixelRing<numLeds>::disableCustomPixels(uint32_t pixels) {
    _customPixelEnables &= ~pixels;
};

// Return the color ranges for the selected pixel positions.
template<uint8_t numLeds>
void NeoPixelRing<numLeds>::getCustomPixels(uint32_t pixels, ColorRange colorRanges[], int size) {
    int n = 0;
    assert(numLeds < 32);
    for (int i = 0; (i < numLeds); i++) {
        if (pixels & (1 << i)) {
            assert(n < size);
            colorRanges[n] = _colorRanges[i];
//            Serial.println("$$: " + String(n) + ", 0x" + String(colorRanges[n].startColor, HEX) + "0x" + String(colorRanges[n].endColor, HEX));
            n += 1;
        }
    }
};

template<uint8_t numLeds>
uint32_t NeoPixelRing<numLeds>::getCustomDelta() {
    return _customDelta;
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::setCustomDelta(uint32_t delta) {
    assert(delta > 0);
    _customDelta = delta;
};

template<uint8_t numLeds>
bool NeoPixelRing<numLeds>::getCustomBidir() {
    return _customBidir;
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::setCustomBidir(bool bidir) {
    _customBidir = bidir;
};

//// FIXME use the right kind of args to capture both dims
template<uint8_t numLeds>
void NeoPixelRing<numLeds>::getPatternDelays(uint16_t minDelays[], uint16_t maxDelays[], uint16_t defDelays[], byte number) {
    for (int i = 0; (i < min(number, _numPatterns)); i++) {
        if (minDelays != NULL) {
            minDelays[i] = _patterns[i].minDelay;
        }
        if (maxDelays != NULL) {
            maxDelays[i] = _patterns[i].maxDelay;
        }
        if (defDelays != NULL) {
            defDelays[i] = _patterns[i].defDelay;
        }
    }
};

template<uint8_t numLeds>
uint32_t NeoPixelRing<numLeds>::makeColor(byte r, byte g, byte b) {
    return ring->Color(r, g, b);
};

// Fill all pixels with the selected color
// Doesn't clear the pixels first.
template<uint8_t numLeds>
void NeoPixelRing<numLeds>::fill(uint32_t color) {
    ring->fill(color, 0, numLeds);
    ring->show();
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::clear() {
    ring->clear();
    ring->show();
};

template<uint8_t numLeds>
unsigned long NeoPixelRing<numLeds>::run() {
    unsigned long now = millis();
    if (now > (_nextRunTime + LED_FUDGE_FACTOR)) {
        Serial.println("WARNING LED: " + String(now - _nextRunTime) + " msec late");
    } else {
        if (_nextRunTime > now) {
            return ((_nextRunTime - now) - 1);  // deduct a msec to account for overheads
        }
    }
    if (_randomPattern) {
        _generateRandomPixel();
    } else {
        _patterns[_patternNum].methodPtr(this);
    }
    _loopCnt += 1;
    _pixelNum = _loopCnt % numLeds;
    return 0L;
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::_create(byte brightness) {
    assert((ring->numPixels() <= 32) && (numLeds <= 32) && (numLeds <= ring->numPixels()));
    _brightness = brightness;
    ring->begin();
    ring->clear();
    ring->show();  // turn off all pixels
    ring->setBrightness(brightness);
    ring->setPixelColor(1, 255, 0, 255);

    // use floating input as source of randomness
    randomSeed(analogRead(UNUSED_ANALOG));
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::_generateRandomPixel() {
    ring->setPixelColor(random(0, numLeds),
                         random(0, 0xFF), random(0, 0xFF), random(0, 0xFF));
    ring->show();
    _nextRunTime = millis() + _patternDelay;
}

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::_splitColor(byte *rPtr, byte *gPtr, byte *bPtr, uint32_t c) {
    *rPtr = GET_RED(c);
    *gPtr = GET_GREEN(c);
    *bPtr = GET_RED(c);
};

// Movie-marquee-like chasing rainbow lights that precess
template<uint8_t numLeds>
void NeoPixelRing<numLeds>::_rainbowMarquee() {
    _nextRunTime = millis() + _patternDelay;
    ring->clear();
    int n = numLeds;
    for (int i = (_loopCnt % 3); (i < n); i += 3) {
        int hue = _firstPixelHue + i * 65536L / n;
        uint32_t rgbColor = ring->gamma32(ring->ColorHSV(hue));
//        Serial.println("> " + String(i) + ", 0x" + String(rgbColor, HEX));
        ring->setPixelColor(i, rgbColor);
        ring->show();
    }
    _firstPixelHue += 256;
};

// Loop through color wheel over all pixels.  Just changes Hue value and keeps
//  Saturation and Brightness at max (i.e., 255).  Bumps starting pixel's Hue
//  by 256 each iteration.
template<uint8_t numLeds>
void NeoPixelRing<numLeds>::_rainbow() {
    ring->clear();
    ring->rainbow(_firstPixelHue);
    ring->show();
    _firstPixelHue += 256;
    _nextRunTime = millis() + _patternDelay;
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::_colorWipe() {
    if (_pixelNum == 0) {
        ring->clear();
    }
    if (_pixelNum < numLeds) {
        ring->setPixelColor(_pixelNum, _color);
        ring->show();
    }
    _nextRunTime = millis() + _patternDelay;
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::_colorFill() {
    ring->clear();
    for (int i = 0; (i < numLeds); i++) {
        ring->setPixelColor(i, _color);
    }
    ring->show();
    _nextRunTime = millis() + _patternDelay;
};

template<uint8_t numLeds>
void NeoPixelRing<numLeds>::_comet() {
    // each comet starts with an LED of the selected brightness and COMET_LENGTH LEDs with decreasing brightness
    // brightness of comet tail is exponentially decreasing from the starting pixel component values
    uint32_t c;
    ring->clear();
    int cometSpan = numLeds / NUM_COMETS;
    for (int i = 0; (i < numLeds); i++) {
        int j = (cometSpan - (i % cometSpan)) - 1;
        if (j >= COMET_LENGTH) {
            c = 0x0000000;
        } else {
            c = DECAY_COLOR(j, _color);
        }
        ring->setPixelColor(((i + _pixelNum) % numLeds), c);
    }
    ring->show();
    _nextRunTime = millis() + _patternDelay;
};

// Movie-marquee-like chasing rainbow lights in the selected color
template<uint8_t numLeds>
void NeoPixelRing<numLeds>::_marquee() {
    ring->clear();
    for (int i = (_loopCnt % 3); (i < numLeds); i += 3) {
        ring->setPixelColor(i, _color);
        ring->show();
    }
    _firstPixelHue += 256;
    _nextRunTime = millis() + _patternDelay;
};

// Cycle pixels between a pair of colors.
template<uint8_t numLeds>
void NeoPixelRing<numLeds>::_custom() {
    int incr;
    byte sR, sG, sB, eR, eG, eB, r, g, b;
    ring->clear();
    for (int i = 0; (i < 32); i++) {
        if ((_customPixelEnables & (1 << i)) != 0) {
            _splitColor(&sR, &sG, &sB, _colorRanges[i].startColor);
            _splitColor(&eR, &eG, &eB, _colorRanges[i].endColor);
            if (_customBidir == true) {
                incr = _loopCnt % ((_customDelta + 1) * 2);
                if (incr >= _customDelta) {
                    incr = (((_customDelta + 1) * 2) - 1) - incr;
                }
            } else {
                incr = _loopCnt % (_customDelta + 1);
            }
            r = sR + (((eR - sR) * incr) / _customDelta);
            g = sG + (((eG - sG) * incr) / _customDelta);
            b = sB + (((eB - sB) * incr) / _customDelta);
            ring->setPixelColor(i, r, g, b);
        }
    }
    ring->show();
    _nextRunTime = millis() + _patternDelay;
};
