/*
* NeoPixel LED Ring library
*/

NeoPixelRing::NeoPixelRing() {
    _create(DEF_NUM_LEDS, DEF_LED_BRIGHTNESS);
};

NeoPixelRing::NeoPixelRing(byte numLeds) {
    _create(numLeds, DEF_LED_BRIGHTNESS);
};

NeoPixelRing::NeoPixelRing(byte brightness) {
    _create(DEF_NUM_LEDS, brightness);
};

NeoPixelRing::NeoPixelRing(byte numLeds, byte brightness) {
    _create(numLeds, brightness);
};

void NeoPixelRing::_create(byte numLeds, byte brightness) {
    assert(_ring->numPixels() < 32);
    assert(numLeds < 32);
    _numLeds = numLeds;
    _ring->begin();
    _ring->clear();
    _ring->show();  // turn off all pixels
    _ring->setBrightness(brightness);
    _ring->setPixelColor(1, 255, 0, 255);

    // use floating input as source of randomness
    randomSeed(analogRead(UNUSED_ANALOG));
};

byte getNumLeds() {
    return _numLeds;
}

void NeoPixelRing::test(byte test) {
    uint32_t c;

    switch (test) {
    case NeoPixelRing::ALL_WHITE:
        c = _ring->Color(255, 255, 255);
        _ring->fill(c, 0, LED_COUNT);
        _ring->show();
        break;
    case NeoPixelRing::ALL_RED:
        _ring->fill(_ring->Color(255, 0, 0), 0, LED_COUNT);
        _ring->show();
        break;
    case NeoPixelRing::ALL_GREEN:
        _ring->fill(_ring->Color(0, 255, 0), 0, LED_COUNT);
        _ring->show();
        break;
    case NeoPixelRing::ALL_BLUE:
        _ring->fill(_ring->Color(0, 0, 255), 0, LED_COUNT);
        _ring->show();
        break;
    case NeoPixelRing::ALL_OFF:
        _ring->clear();
        _ring->show();
        break;
    case NeoPixelRing::ALL_BLINK:
        int i;
        uint32_t c[LED_COUNT];
        for (i = 0; (i < LED_COUNT); i++) {
            c[i] = _ring->getPixelColor(i);
        }
        for (int n = 0; (n < 60); n++) {
            _ring->clear();
            delay(500);
            for (i = 0; (i < LED_COUNT); i++) {
                _ring->setPixelColor(i, c[i]);
            }
            _ring->show();
            delay(1000);
        }
        break;
    default:
        Serial.println("Invalid test: " + String(test));
        return;
    }
};

void NeoPixelRing::clear() {
    _ring->clear();
    _ring->show();
};

// Return which pixels are enabled for custom patterns.
uint32_t NeoPixelRing::getEnabledCustomPixels() {
    return _customPixelEnables;
};

// Return the color ranges for the selected pixel positions.
void NeoPixelRing::getCustomPixels(uint32_t pixels, ColorRange colorRanges[], int size) {
    int n = 0;
    for (int i = 0; (i < 32); i++) {
        if (pixels & (1 << i)) {
            assert(n < size);
            colorRanges[n] = _colorRanges[i];
//            Serial.println("$$: " + String(n) + ", 0x" + String(colorRanges[n].startColor, HEX) + "0x" + String(colorRanges[n].endColor, HEX));
            n += 1;
        }
    }
};

// Disable pixels for custom display mode.
// Give a bit-vector where a 1 means to disable the corresponding pixel.
void NeoPixelRing::disableCustomPixels(uint32_t pixels) {
    _customPixelEnables &= ~pixels;
};

// Enable pixels for custom display mode and provide a pair of colors to be
//  cycled between.
// Give a bit-vector where a 1 means to disable the corresponding pixel.
void NeoPixelRing::enableCustomPixels(uint32_t pixels, uint32_t startColor, uint32_t endColor) {
    for (int i = 0; (i < _numLeds); i++) {
        if (pixels & (1 << i)) {
            _colorRanges[i].startColor = startColor;
            _colorRanges[i].endColor = endColor;
            _customPixelEnables |= (1 << i);
        }
    }
};

// Fill all pixels with the selected color
// Doesn't clear the pixels first.
void NeoPixelRing::fill(uint32_t color) {
    _ring->fill(color, 0, _numLeds);
    _ring->show();
};

// Fill pixels sequentially with the selected color, with the selected delay
//  time (in msec) between each illumination.
// Clears the pixels at the start of every cycle.
void NeoPixelRing::_colorWipe() {
    if (_pixelNum == 0) {
        _ring->clear();
    }
    _ring->setPixelColor(_pixelNum, _color);
    _ring->show();
    _nextRunTime = millis() + _patternDelay;
};

// Fill all pixels with the selected color
// Doesn't clear the pixels first.
void NeoPixelRing::_colorFill() {
    _ring->fill(_color, 0, _numLeds);
    _ring->show();
    _nextRunTime = millis() + _patternDelay;
};

// Movie-marquee-like chasing rainbow lights in the selected color
void NeoPixelRing::_marquee() {
    _ring->clear();
    int n = _numLeds;
    for (int i = (_loopCnt % 3); (i < n); i += 3) {
        _ring->setPixelColor(i, _color);
        _ring->show();
    }
    _firstPixelHue += 256;
    _nextRunTime = millis() + _patternDelay;
};

// Movie-marquee-like chasing rainbow lights that precess
void NeoPixelRing::_rainbowMarquee() {
    _nextRunTime = millis() + _patternDelay;
    _ring->clear();
    int n = _numLeds;
    for (int i = (_loopCnt % 3); (i < n); i += 3) {
        int hue = _firstPixelHue + i * 65536L / n;
        uint32_t rgbColor = _ring->gamma32(_ring->ColorHSV(hue));
//        Serial.println("> " + String(i) + ", 0x" + String(rgbColor, HEX));
        _ring->setPixelColor(i, rgbColor);
        _ring->show();
    }
    _firstPixelHue += 256;
};

// Loop through color wheel over all pixels.  Just changes Hue value and keeps
//  Saturation and Brightness at max (i.e., 255).  Bumps starting pixel's Hue
//  by 256 each iteration.
void NeoPixelRing::_rainbow() {
    _ring->clear();
//    Serial.println("R: " + String(_firstPixelHue));
    _ring->rainbow(_firstPixelHue);
    _ring->show();
    _firstPixelHue += 256;
    _nextRunTime = millis() + _patternDelay;
};

uint32_t NeoPixelRing::getCustomDelta() {
    return _customDelta;
};

void NeoPixelRing::setCustomDelta(uint32_t delta) {
    _customDelta = delta;
};

bool NeoPixelRing::getCustomBidir() {
    return _customBidir;
};

void NeoPixelRing::setCustomBidir(bool bidir) {
    _customBidir = bidir;
};

void NeoPixelRing::_splitColor(byte *rPtr, byte *gPtr, byte *bPtr, uint32_t c) {
    *rPtr = ((c >> 16) & 0xFF);
    *gPtr = ((c >> 8) & 0xFF);
    *bPtr = (c & 0xFF);
};

// Cycle pixels between a pair of colors.
void NeoPixelRing::_custom() {
    int incr;
    byte sR, sG, sB, eR, eG, eB, r, g, b;
    _ring->clear();
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
            _ring->setPixelColor(i, r, g, b);
        }
    }
    _ring->show();
    _nextRunTime = millis() + _patternDelay;
};

#define LED_FUDGE_FACTOR        10  // let things be up to 10msec late without complaining

unsigned long NeoPixelRing::run() {
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
        _patterns[_patternNum].func(this);
    }
    _loopCnt += 1;
    _pixelNum = _loopCnt % _numLeds;
    return 0L;
};

void NeoPixelRing::_generateRandomPixel() {
    _ring->setPixelColor(random(0, _numLeds),
                         random(0, 0xFF), random(0, 0xFF), random(0, 0xFF));
    _ring->show();
    _nextRunTime = millis() + _patternDelay;
}


uint32_t NeoPixelRing::makeColor(byte r, byte g, byte b) {
    return _ring->Color(r, g, b);
};

void NeoPixelRing::setColor(uint32_t color) {
    _color = color;
};

uint32_t NeoPixelRing::getColor() {
    return _color;
};

void NeoPixelRing::setDelay(uint32_t delay) {
    _patternDelay = delay;
};

uint32_t NeoPixelRing::getDelay() {
    return _patternDelay;
};

byte NeoPixelRing::getNumPatterns() {
    return _NUM_PATTERNS;
};

//// FIXME use the right kind of args to capture both dims
byte NeoPixelRing::getPatternNames(char *namePtrs[], byte number) {
    int i = 0;
    for (i = 0; (i < min(number, _NUM_PATTERNS)); i++) {
        namePtrs[i] = _patterns[i].name;
    }
    return i;
};

//// FIXME use the right kind of args to capture both dims
void NeoPixelRing::getPatternDelays(uint16_t minDelays[], uint16_t maxDelays[], byte number) {
    for (int i = 0; (i < min(number, _NUM_PATTERNS)); i++) {
        if (minDelays != NULL) {
            minDelays[i] = _patterns[i].minDelay;
        }
        if (maxDelays != NULL) {
            maxDelays[i] = _patterns[i].maxDelay;
        }
    }
};

byte NeoPixelRing::getSelectedPattern() {
    return _patternNum;
};

bool NeoPixelRing::selectPattern(byte patternNum) {
    if (patternNum >= _NUM_PATTERNS) {
        return true;
    }
    _patternNum = patternNum;
    return false;
};

bool NeoPixelRing::randomPattern() {
    return _randomPattern;
}

void NeoPixelRing::enableRandomPattern(bool enable) {
    _randomPattern = enable;
    _nextRunTime = millis() + 1;
}
