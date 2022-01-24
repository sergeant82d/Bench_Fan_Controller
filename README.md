# Bench_Fan_Controller
Using an SSD1306 GLCD and a rotary encoder to monitor and control the speed of a PWM (4-pin) PC fan. Initial commit works fully on Arduino Uno. Future mods (should) include different display options, and a menu process to save and recall pre-set RPM values, and save them to EEPROM.



I used code and libraries from four external sources:

Adafruit - GFX & SSD1306 LCD driver, & fonts libraries. I also used their fontconverter to reduce the character set of the 24pt FreeSansBoldOblique24Pt7b.h down to only the numbers. This reduced the file size from ~10KB to only ~1KB. Totally worth the effort.

John Lluch - Encoder library
https://github.com/John-Lluch/Encoder

Tyler Peppy - 25 KHz PWM direct from the 328's registers
https://create.arduino.cc/projecthub/tylerpeppy/25-khz-4-pin-pwm-fan-control-with-arduino-uno-3005a1

Giorgio Aresu - Fan Controller to monitor the fan speed
https://github.com/GiorgioAresu/FanController
