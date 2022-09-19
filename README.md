# ElectroHat
Third version of tin-foil hat

**WIP**

Mark3 hat with goggles:
  * first version: [LdgDisplay](https://github.com/jduanen/LdgDisplay)
  * second version: [PrcDisplay](https://github.com/jduanen/PrcDisplay)

Consists of two major components -- the Hat and the Goggles -- each of which can be operated in stand-alone mode and are interacted with via web interfaces.

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

The Goggles consist of:
  * (steampunk-like) googles
  * a pair of ??mm round LCD displays
  * a ? controller
  * a LiPo battery and charger with USB interface
