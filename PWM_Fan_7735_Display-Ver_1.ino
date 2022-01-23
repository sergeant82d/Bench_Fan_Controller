/* Visual Micro is in vMicro>General>Tutorial Mode
* 
*   Name:       PWM_Fan___Reader.ino
*   Created:	1/16/2022 7:41:57 AM
*   Author:     BRAD-LAB\serge
* 
*//////////////////////////////////////////////////////////////////////////////
// Include the libraries

#include <Adafruit_GFX.h>                   // Core graphics library
#include <Adafruit_ST7735.h>                // Hardware-specific library for ST7735
#include <Fonts/FreeSansBoldOblique9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <SPI.h>
#include <FanController.h>
#include <Encoder.h>

/////////////////////////////////////////////////////////////////////////////
// Function Prototypes

void    DISPLAY_Fan_Speed(uint16_t);        // wrapper for library function
void    DISPLAY_Redraw();                   // screen refresh
void    DISPLAY_Error_(void);
uint8_t FAN_SpeedProcess(uint16_t);         // limits and calculations
void    FAN_Speed_Set(uint16_t);            // for rotary encoder
void    ENCODER_Switch(bool);
void    PWM_Timer_Setup();
void    (*resetFunc) (void) = 0;            // software reset in error handling function


//#define DEBUG

/////////////////////////////////////////////////////////////////////////////
// Hardware Assignments

struct MYPINS {
    const uint8_t TFT_ChipSelect        = 10;
    const uint8_t TFT_Data_Command      = 8;  // "AO" on my 7735 display - BDH
    const uint8_t TFT_Reset             = 7;  // Or set to -1 and connect to Arduino RESET pin
    const uint8_t FAN_SpeedSensor       = 2;
    const uint8_t PWM_25K_Output        = 9;
    const uint8_t ENCODER_A             = 6;
    const uint8_t ENCODER_B             = 7;
    const uint8_t ENCODER_PushSwitch    = 5;
    const uint8_t TEMPORARY_TEST_LED    = 12;
} pin;

/////////////////////////////////////////////////////////////////////////////
// color definitions

struct COLORS {
    const uint16_t  Black               = 0x0000;
    const uint16_t  Blue                = 0x001F;
    const uint16_t  Red                 = 0xF800;
    const uint16_t  Green               = 0x07E0;
    const uint16_t  Cyan                = 0x07FF;
    const uint16_t  Magenta             = 0xF81F;
    const uint16_t  Yellow              = 0xFFE0;
    const uint16_t  White               = 0xFFFF;
} Color;

/////////////////////////////////////////////////////////////////////////////
// Global Variables
// Never a good idea, but I'm too lazy to get rid of them, so at least make them obvious

struct GLOBALS {
    bool            isDisplayVisible            = false;
    bool            FAN_Power_State             = false;
    uint16_t        Current_Text_Color          = Color.Black;
    uint16_t        Current_Backround_Color     = Color.Cyan;
    const uint16_t  PWM_FREQ_HZ                 = 25000;    //Change this value to adjust the PWM frequency in Hz
    const uint16_t  TCNT1_TOP                   = (16000000 / (2 * PWM_FREQ_HZ));
    const uint16_t  fanSpeedReadThreshold       = 1000;     // not sure if this is nbr of readings
                                                            // or time, or something else - see header
} global;

/////////////////////////////////////////////////////////////////////////////
// Initialize libraries

FanController fan(pin.FAN_SpeedSensor, global.fanSpeedReadThreshold);
Encoder encoder(pin.ENCODER_A, pin.ENCODER_B, pin.ENCODER_PushSwitch);
Adafruit_ST7735 tft = Adafruit_ST7735(pin.TFT_ChipSelect, pin.TFT_Data_Command, pin.TFT_Reset);

/////////////////////////////////////////////////////////////////////////////
// Setup function - start things up
/////////////////////////////////////////////////////////////////////////////

void setup(void)
{
#ifdef DEBUG
    Serial.begin(115200);
    Serial.println("Fan Controller Library Demo");
#endif // DEBUG

    pinMode(pin.ENCODER_A, INPUT_PULLUP);
    pinMode(pin.ENCODER_B, INPUT_PULLUP);
    pinMode(pin.ENCODER_PushSwitch, INPUT_PULLUP);
    pinMode(pin.PWM_25K_Output, OUTPUT);
    pinMode(pin.TEMPORARY_TEST_LED, OUTPUT);

    tft.initR(INITR_BLACKTAB);                  // Init ST7735S chip, black tab - see Adafruit headers
    tft.setSPISpeed(40000000);                  // comment out if screen blacks out

    PWM_Timer_Setup();                          // Clear Timer1 control and count registers

    fan.begin();                                // read and write to PWM Fan
    EncoderInterrupt.begin(&encoder);           // read user interface rotary encoder

    DISPLAY_Redraw();                           // default setup

    digitalWrite(pin.TEMPORARY_TEST_LED, LOW);  // temporary, testing use
}

/////////////////////////////////////////////////////////////////////////////
// Main function, get and show the temperature
/////////////////////////////////////////////////////////////////////////////

void loop(void)
{
    static uint16_t encValue = 0;
    uint16_t delta = 0;
    uint16_t rpms = 0;
    
    ENCODER_Switch(encoder.button() );          // push button switch state

    rpms = fan.getSpeed();                      // Send the command to get RPM
    DISPLAY_Fan_Speed(rpms);

    delta = encoder.delta();                    // get any encoder change from lib func
    encValue += delta;                          // adjust total encoder value

    if ((encValue >= 3) && (encValue <= 400)) { // limit value here to avoid rollovers
        FAN_Speed_Set(encValue);                // send to PWM module
    }                                           // (encoder has four increments per knob tick)

} // END Main Loop

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void FAN_Speed_Set(uint16_t input) {
    static uint8_t oldTarget = 0;
    uint8_t target = 0;
    
    target = (input / 4); // encoder has four increments per knob tick - remove them here
    PWM_Set_Duty(target);

#ifdef DEBUG
    // Get duty cycle
    Serial.print("Setting duty cycle to: ");
    Serial.println(target);
#endif // DEBUG

    if (oldTarget == target) {                  // most of the time, it won't change, 
        ;                                       // so skip the display redraw
    }
    else {                                      // only redraw screen if it needs updating
        tft.setRotation(3);
        tft.fillRect(30, 22, 132, 106, global.Current_Backround_Color);
        tft.setFont(&FreeSansBold18pt7b);
        tft.setTextColor(global.Current_Text_Color);

        if ((target > 9) && (target < 100)) {   // set cursor location to right-align the numbers
            tft.setCursor(78, 90);              // always start with the most likely outcome
        }
        else if (target < 10) {
            tft.setCursor(115, 90);
        }
        else if (target == 100) {
            tft.setCursor(40, 90);
        }
        else if ((target < 0) || (target > 100)) {      // handle unexpected issues
            target = 0;                                 // turn off fan
            DISPLAY_Error_();                           // reset the Arduino

#ifdef DEBUG
    Serial.println("Error occurred in ");
    Serial.println(__FILE__);
    Serial.println(__FUNCTION__);
    Serial.println(__LINE__);
    Serial.end();                               // keep from overflowing your PC...
    resetFunc();                                // reset the Arduino
#endif // DEBUG
        }
        
        tft.print(target);                      // update the display
    }

    oldTarget = target;                         // update reference to avoid re-drawing every time through
}   // END setSpdEnc

/////////////////////////////////////////////////////////////////////////////

void DISPLAY_Fan_Speed(uint16_t rpms) {
    static uint16_t oldRPMs = 0;

    if (oldRPMs == rpms) {                      // most of the time, it won't change, 
        ;                                       // so skip the display redraw
    }
    else {
        tft.setRotation(3);
        tft.setFont();
        tft.setTextSize(2);
        tft.setTextColor(global.Current_Text_Color);
        tft.setCursor(80, 2);
        tft.fillRect(80, 0, 80, 19, Color.Red);

        tft.print(rpms);
    }

    oldRPMs = rpms;

#ifdef DEBUG
    Serial.print(rpms);
    Serial.println(" RPM");
#endif // DEBUG
}

/////////////////////////////////////////////////////////////////////////////

void DISPLAY_Error_() {
    tft.setCursor(45, 45);
    tft.setTextColor(Color.Red);                // Red always says "Emergency"
    tft.print("ERROR");
    tft.setCursor(45, 95);
    tft.print("Rebooting");                     // let user know what's happening
    delay(1000);
    tft.setTextColor(global.Current_Text_Color);
    
    resetFunc();                                // reset the Arduino
}

/////////////////////////////////////////////////////////////////////////////

void DISPLAY_Redraw() {
    global.isDisplayVisible = false;            // display off...
    delay(100);
    global.isDisplayVisible = true;             // and back on, with settling time
    delay(100);

    tft.setRotation(3);                         // my display is on it's left side
    tft.setTextWrap(false);

    tft.setFont(&FreeSansBoldOblique9pt7b);
    tft.fillScreen(global.Current_Backround_Color);
    tft.setTextColor(global.Current_Text_Color);
    tft.fillRect(0, 0, 160, 19, Color.Red);
    tft.setCursor(2, 15);
    tft.print("RPM");                           // label for fan rpm

    tft.fillRoundRect(2, 22, 28, 104, 5, Color.Magenta);
    tft.setTextColor(global.Current_Text_Color);
    tft.setCursor(8, 44);                       // Vertical block of text w/ magenta background
    tft.print("S");                             // "Speed" - for user encoder label
    tft.setCursor(9, 62);
    tft.print("p");
    tft.setCursor(9, 80);
    tft.print("e");
    tft.setCursor(9, 98);
    tft.print("e");
    tft.setCursor(9, 116);
    tft.print("d");

    tft.setTextSize(2);

    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextColor(global.Current_Text_Color);
    tft.setCursor(115, 90);
    tft.print(0);                               // speed setting is blank on startup without this...

}

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
}
/////////////////////////////////////////////////////////////////////////////

void PWM_Timer_Setup(void) {
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    TCCR1A |= (1 << COM1A1) | (1 << WGM11);
    TCCR1B |= (1 << WGM13) | (1 << CS10);
    ICR1 = global.TCNT1_TOP;
}

/////////////////////////////////////////////////////////////////////////////
// 25 KHz routine
void PWM_Set_Duty(byte duty) {

    OCR1A = (word)(duty * global.TCNT1_TOP) / 100;     // write to chip register

#ifdef DEBUG
    Serial.println(duty);
#endif
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// EOF (that's "End Of File"... 8>)