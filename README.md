# Bench_Fan_Controller
Using a PWM PC fan, monitor and control the speed using an SSD1306 GLCD and a rotary encoder. Initial commit works fully on Arduino Uno. Future mods (should) include different display options, and a menu process to save and recall pre-set RPM values, and save them to EEPROM.



I used code and libraries from four external sources:

Adafruit - GFX & SSD1306 LCD driver, & fonts libraries

John Lluch - Encoder library
https://github.com/John-Lluch/Encoder

Tyler Peppy - 25 KHz PWM direct from the 328's registers
https://create.arduino.cc/projecthub/tylerpeppy/25-khz-4-pin-pwm-fan-control-with-arduino-uno-3005a1

Giorgio Aresu - Fan Controller to monitor the fan speed
https://github.com/GiorgioAresu/FanController
