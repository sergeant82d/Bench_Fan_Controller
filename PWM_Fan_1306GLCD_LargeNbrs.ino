/*
* Name:		PWM_Fan_1306GLCD_LargeNbrs.ino
* Created:	1/18/2022 7:41:38 PM
* Author:	Brad Hedges
* 
* Visual Micro is in vMicro>General>Tutorial Mode
*
*//////////////////////////////////////////////////////////////////////////////
// Include the libraries

#include <Adafruit_GFX.h>                       // Core graphics library
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBoldOblique9pt7b.h>
#include <Fonts/FreeSansBoldOblique24pt7b.h>
#include <FanController.h>
#include <Encoder.h>

/////////////////////////////////////////////////////////////////////////////
// Function Prototypes

void    DISPLAY_Encoder_Setting(uint8_t);       // for rotary encoder
void    DISPLAY_Fan_RPM(uint16_t);              // wrapper for library function
void    DISPLAY_Setup(void);
void    DISPLAY_Write_Numbers(void);
void    DISPLAY_Redraw(void);                   // screen refresh
void    DISPLAY_Error_(void);
uint8_t ENCODER_Speed_Set(uint16_t);            // limits and calculations
void    ENCODER_Switch(bool);
void    PWM_Timer_Setup(void);
void    PWM_Set_Duty(byte);
void    (*resetFunc) (void) = 0;                // software reset in error handling function


//#define DEBUG                                 // un-comment to print to serial port

/////////////////////////////////////////////////////////////////////////////
// Hardware Assignments

struct MYPINS {
    const uint8_t FAN_SpeedSensor       = 2;
    const uint8_t PWM_25K_Output        = 9;
    const uint8_t ENCODER_A             = 6;
    const uint8_t ENCODER_B             = 7;
    const uint8_t ENCODER_PushSwitch    = 8;
    const uint8_t TEMPORARY_TEST_LED    = 12;
} pin;

/////////////////////////////////////////////////////////////////////////////
// Global Variables
// Never a good idea, but I'm too lazy to get rid of them, so at least make them obvious

struct GLOBALS {
    const uint16_t  DISPLAY_ADDRESS         = 0x3c;     // I2C address for display
    uint16_t        Current_Text_Color      = WHITE;    // My SSD1306 only accepts Black & White
    const int16_t   ENCODER_MAX_VAL         = 400;      // 100 percent, times 4 encoder increments per knob click
    const int16_t   ENCODER_MIN_VAL         = 0;
    const int16_t   TARGET_MAX_VAL          = 100;      // 100 percent
    const int16_t   TARGET_MIN_VAL          = 0;
    const uint16_t  PWM_FREQ_HZ             = 25000;    //Change this value to adjust the PWM frequency in Hz
    const uint16_t  TCNT1_TOP               = ((16000000L) / (2 * PWM_FREQ_HZ)); // crystal freq - PWM timer setup
    bool            FAN_Power_State         = false;
    const uint16_t  fanSpeedReadThreshold   = 1000;     // this is number of millis() elapsed - see library header
} global;

/////////////////////////////////////////////////////////////////////////////
// Initialize libraries

FanController fan(pin.FAN_SpeedSensor, global.fanSpeedReadThreshold);
Encoder encoder(pin.ENCODER_A, pin.ENCODER_B, pin.ENCODER_PushSwitch);
Adafruit_SSD1306 display(128, 64);

/////////////////////////////////////////////////////////////////////////////
// Setup function - start things up
/////////////////////////////////////////////////////////////////////////////

void setup(void)
{
#ifdef DEBUG
    Serial.begin(115200);
    Serial.println("PWM Fan Controller with RPM Readout");
#endif // DEBUG

    pinMode(pin.ENCODER_A, INPUT_PULLUP);
    pinMode(pin.ENCODER_B, INPUT_PULLUP);
    pinMode(pin.ENCODER_PushSwitch, INPUT_PULLUP);
    pinMode(pin.PWM_25K_Output, OUTPUT);
    pinMode(pin.TEMPORARY_TEST_LED, OUTPUT);

    digitalWrite(pin.TEMPORARY_TEST_LED, LOW);      // temporary, testing use

    PWM_Timer_Setup();                              // Clear Timer1 control and count registers
    fan.begin();                                    // read speed from PWM Fan
    EncoderInterrupt.begin(&encoder);               // read user interface rotary encoder

    DISPLAY_Setup();                                // default setup
    DISPLAY_Redraw();                               // essentially works to test display by filling it
    delay(2000);
    DISPLAY_Write_Numbers();                        // fill with starting values
}   // END setup

/////////////////////////////////////////////////////////////////////////////
// Main function, get and show the fan speed
/////////////////////////////////////////////////////////////////////////////

void loop(void) {
    uint16_t encValue = 0;                          // intermediate variable to pass values

    ENCODER_Switch(encoder.button());               // monitor push button switch state (lib func)

    encValue = ENCODER_Speed_Set(encoder.delta());  // using library function, get the input changes
    DISPLAY_Encoder_Setting(encValue);              // and display user control setting
    DISPLAY_Fan_RPM(fan.getSpeed());                // Read fan RPM with library function

}   // END Main Loop

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/*
* TODO:
* Idea is to make the fan not run until button is pushed, and turn off when pushed
* again. I.E., after applying power, you can set the speed, but it won't start
* turning until specifically instructed. Also, later, to add EEPROM saving option.
*/
void ENCODER_Switch(bool pb) {
    static bool oldPowerState = 0;
    global.FAN_Power_State = pb;

    if (oldPowerState == global.FAN_Power_State) {
        ;
    }
    else {
        if (global.FAN_Power_State == false) {
            digitalWrite(pin.TEMPORARY_TEST_LED, LOW);
        }
        if (global.FAN_Power_State == true) {
            digitalWrite(pin.TEMPORARY_TEST_LED, HIGH);
            resetFunc();                        // reset the Arduino - just for testing!
        }

    }
    oldPowerState = global.FAN_Power_State;
}   // END ENCODER_Switch

/////////////////////////////////////////////////////////////////////////////

uint8_t ENCODER_Speed_Set(int16_t delta) {
    static int16_t      encValue    = 0;        // intermediate variable for calculations
    uint8_t             target      = 0;        // sent to PWM and display

    /* Library will return both positive and negative numbers, of a value which depends
    *  on the rate (speed) that you turn the encoder knob. Therefor, we should test
    *  the value of 'delta' and compare that to 'encValue', instead of just to
    *  the 'encValue' MIN and MAX limits.
    */
    if ( (delta < global.ENCODER_MIN_VAL) && (encValue < (abs(delta)) ) ) {
        delta = 0;                              // prevent us from going below zero, or abov MAX
    }
    if ( (delta > global.ENCODER_MIN_VAL) && (encValue > (global.ENCODER_MAX_VAL - delta) ) ) {
        delta = 0;
    }

    encValue += delta;                          // adjust total encoder value

    target = (encValue / 4); // encoder has four increments per knob tick - remove them here

    if ( (target >= global.TARGET_MIN_VAL) && (target <= global.TARGET_MAX_VAL) ) {   // limit value here to avoid rollovers
        PWM_Set_Duty(target);
    }

#ifdef DEBUG
    // Get duty cycle
    Serial.print("Setting duty cycle to: ");
    Serial.println(target);
#endif // DEBUG

    return target;
}   // END ENCODER_Speed_Set

/////////////////////////////////////////////////////////////////////////////

void DISPLAY_Encoder_Setting(uint8_t target) {
    static uint8_t oldTarget = 0;

    if (oldTarget == target) {                  // most of the time, it won't change, 
        ;                                       // so skip the display redraw
    }
    else {                                      // only redraw screen if it needs updating
        if ((target > 9) && (target < 100)) {   // set cursor location to right-align the numbers
            display.setCursor(65, 61);          // always start with the most likely outcome
        }
        else if ((target >= 0) && (target < 10)) {
            display.setCursor(90, 61);
        }
        else if (target >= 100) {
            target = 100;
            display.setCursor(39, 61);
        }
        else if (target <= 0) {                 // these two checks are redundant,
            target = 0;                         // but better safe than sorry...
            display.setCursor(90, 61);
        }
        else {                                  // handle unexpected issues
            target = 0;                         // turn off fan
#ifdef DEBUG
            Serial.println("Error occurred in ");
            Serial.println(__FILE__);
            Serial.println(__FUNCTION__);
            Serial.println(__LINE__);
            Serial.end();                       // keep from overflowing your PC...
#endif // DEBUG
            DISPLAY_Error_();                   // let user know, and
            resetFunc();                        // reset the Arduino
        }
        // blank out old, and draw new value
        display.setFont(&FreeSansBoldOblique24pt7b);
        display.fillRect(33, 20, 95, 44, BLACK);
        display.print(target);
        display.display();
    }

    oldTarget = target;                         // update reference to avoid re-drawing every time through
}   // END DISPLAY_Encoder_Setting

/////////////////////////////////////////////////////////////////////////////

void DISPLAY_Fan_RPM(uint16_t rpms) {
    static uint16_t oldRPMs = 0;

    if (oldRPMs == rpms) {                      // most of the time, it won't change, 
        ;                                       // so skip the display redraw
    }
    else {
        display.fillRect(64, 0, 64, 15, BLACK); // blank out the old numbers before writing new
        display.setFont(&FreeSansBoldOblique9pt7b);
        display.setCursor(70, 14);
        display.print(rpms);
        display.display();
    }

    oldRPMs = rpms;

#ifdef DEBUG
    Serial.print(rpms);
    Serial.println(" RPM");
#endif // DEBUG
}   // END DISPLAY_Fan_RPM

/////////////////////////////////////////////////////////////////////////////

void DISPLAY_Error_() {
    display.clearDisplay();

    display.setFont();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("ERROR");
    display.setCursor(0, 30);
    display.print("Rebooting");                 // let user know what's happening
    display.display();

    delay(2000);

}   // END DISPLAY_Error_

/////////////////////////////////////////////////////////////////////////////

void DISPLAY_Redraw() {
    display.clearDisplay();
    display.setTextWrap(false);
    delay(50);
    display.display();

    display.setFont(&FreeSansBoldOblique9pt7b);
    display.setCursor(1, 14);
    display.print("RPM");                       // label for fan rpm

    display.setCursor(1, 58);
    display.print("Set");                       // user setting label

    display.setCursor(70, 14);                  // fan RPM digit test
    display.print(9999);

    display.setFont(&FreeSansBoldOblique24pt7b);
    display.setCursor(39, 61);
    display.print(0);                           // speed setting is blank on startup without this...
    display.setCursor(65, 61);                  // font spacing test - helps establish positions
    display.print(0);
    display.setCursor(90, 61);
    display.print(0);

    display.display();
}   // END DISPLAY_Redraw

/////////////////////////////////////////////////////////////////////////////

void DISPLAY_Write_Numbers() {
    // RPM display
    display.fillRect(64, 0, 64, 15, BLACK);     // blank out the old numbers before writing new
    display.setFont(&FreeSansBoldOblique9pt7b); // after clearing the numbers, set display to zero
    display.setCursor(70, 14);
    display.print(0);

    // User setting display
    display.fillRect(33, 16, 95, 48, BLACK);    // blank out the old numbers before writing new
    display.setFont(&FreeSansBoldOblique24pt7b);// after clearing the numbers, set display to zero
    display.setCursor(90, 61);
    display.print(0);
    
    display.display();
}   // END DISPLAY_WRITE_INITIAL

/////////////////////////////////////////////////////////////////////////////

void DISPLAY_Setup() {
    display.begin(SSD1306_SWITCHCAPVCC, global.DISPLAY_ADDRESS);
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setRotation(0);
    display.setTextWrap(false);
}   // END DISPLAY_Setup

/////////////////////////////////////////////////////////////////////////////

void PWM_Timer_Setup(void) {                            // this gives us the 25 KHz PWM we need
    TCCR1A = 0;                                         // write to chip register
    TCCR1B = 0;
    TCNT1 = 0;
    TCCR1A |= (1 << COM1A1) | (1 << WGM11);
    TCCR1B |= (1 << WGM13) | (1 << CS10);
    ICR1 = global.TCNT1_TOP;
}   // END PWM_Timer_Setup

/////////////////////////////////////////////////////////////////////////////

void PWM_Set_Duty(byte duty) {                          // 25 KHz routine

    OCR1A = (word)(duty * global.TCNT1_TOP) / 100;      // write to chip register

#ifdef DEBUG
    Serial.println(duty);
#endif
}   // END PWM_Set_Duty

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// EOF - that's "End Of File"... 8>D
