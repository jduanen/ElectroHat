# ElectroHat
Mark3 Tin-Foil Hat

**WIP**

Mark3 hat with goggles:
  * first version: [LdgDisplay](https://github.com/jduanen/LdgDisplay)
  * second version: [PrcDisplay](https://github.com/jduanen/PrcDisplay)

Consists of two major components -- the Hat and the Goggles -- each of which can be operated in stand-alone mode and are interacted with via web interfaces.

## Hat

The Hat lights up the wires wrapping the cones in definable patterns and rates.

The Hat consists of:
  * four cones (each of which is wrapped in a pair of colored EL wires)
  * eight colored EL wires
    - driven by an Alien Font Display driver card (https://github.com/jduanen/alienFontDisplay)
  * ESP8266 WiFi-connected controller that controls the EL wires and offers the Hat's web interface
  * power supply consisting of
    - LiPo battery
    - battery charger: allows charging of battery (via USB connector) and generates 5V for the ESP controller
    - DC-DC converter: converts 5V from battery charger to 12V for HV AC PSU
    - high-voltage AC power supply: for the EL wires

## Googles

The Googles display animated eyes that can be ?

The Goggles consist of:
  * (steampunk-like) googles (with hemispherical lenses and masks)
    - https://www.adafruit.com/product/1577
  * a pair of round LCD displays (Waveshare)
    - 240x240 IPS
    - 32.4mm display diameter (package: 37.5mm diameter, max 40.4mm)
    - 4-wire SPI interface
    - 3.3V and 5V
    - GC9A01 driver
    - Pinout
      * VCC: 3.3V/5V
      * GND: Ground
      * DIN: SPI Data In
      * CLK: SPI Clock In
      * CS: Chip Select (active low)
      * DS: Data/Command select (high=Data, low=Command)
      * RST: Reset (active low)
      * BL: Backlight
    - https://www.waveshare.com/product/displays/lcd-oled/lcd-oled-3/1.28inch-lcd-module.htm
    - https://www.amazon.com/Display-Arduino-Raspberry-240%C3%97240-Embedded/dp/B095X1JDCM/ref=sr_1_15?crid=1DDHRPORF8AHX&keywords=amoled+display+component&qid=1663625753&sprefix=amoled+display%2Caps%2C218&sr=8-15
    - https://www.waveshare.com/product/displays/lcd-oled/lcd-oled-3/1.28inch-lcd-module.htm
    - https://www.waveshare.com/wiki/1.28inch_LCD_Module
    - library and example code:
      * wget https://www.waveshare.com/wiki/File:LCD_Module_RPI_code.zip
  * a XIAO ESP32-C3 controller
    - RISC-V single-core 32b CPU, up to 160MHz
    - 2.4GHz WiFi subsystem and BT5.0
    - 400KB SRAM, 4MB Flash
    - UART, IIC, IIS, SPI, GPIO (11x), PWM, ADC (4x)
    - reset button and boot button
    - 21x17.5mm
    - USB-C
    - 5V @ 200mA
    - pinout
      * GPIO2/A0/D0
      * GPIO3/A1/D1
      * GPIO4/A2/D2
      * GPIO5/A3/D3
      * GPIO6/SDA/D4
      * GPIO7/SCL/D5
      * GPIO21/TX/D6
      * GPIO20/RX/D7
      * GPIO8/SCK/D8
      * GPIO9/MISO/D9
      * GPIO10/MOSI/D10
      * 3V3: regulated output, <=700mA
      * GND: ground
      * 5V: output from USB port, can be used as input (requires diode)
    - https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/
    - https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html
    - https://wiki.seeedstudio.com/XIAO_ESP32C3_WiFi_Usage/
  * a LiPo battery and charger with USB interface

==============================================================================

Notes
* Uncanny Eyes
  - ESP32
    * https://www.hackster.io/laurentslab/halloween-skull-costume-with-uncanny-eyes-on-esp32-376a13
    * rendering takes ~6ms
    * ESP32 library has SPI.writePixels()
      - DMA and endianism-swizzle
    * 240x240 2B/pixel: 115,200 Bytes, 230KB for both displays
    * at 16MHz (divisor=5) SPI clock, ~72msec per display, ~14Hz
      - ~150ms (both displays) ~7Hz -- too slow, want ~20Hz
    * optimization opportunities:
      - overlap rendering and update
      - skip pixels that are still black (20-50% of the frame)
      - use both cores of the CPU

