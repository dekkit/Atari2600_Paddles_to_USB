/* DB9 Paddles to USB by dekkit 06/05/2023
 
This was designed to covert C64 and Atari 2600 DB9 connected Paddles to USB.
This is experimental code and does not fully work correctly.

Ardunio Used = Arduino Pro Micro (with Micro Usb conector)

2 x modes of operation:
- GAMEPAD
- MOUSE*

Note: Mouse mode is problematic when using paddles.  They are not responsive in mouse and tend to feel laggy.   It is better to use GAMEPAD Mode and use them x/y analog controls.

It uses the direct access to PORT F of arduino micro (which is referenced by "~PINF")- this is to enable proivde quicker access to the digital pins to reduce latency.
For reference port F directly accesses the digital pins D18,D19,D20,D21,D22,D23 on an arduino micro.  

Explainer for the code:
The original code uses Bitwise left shift operators "<<" to work through if each of the pins are HIGH or LOW on the PORT F.
For example
"(sample & (1 << 7))" is checking if the position 0 in the port, is a "1" or a "0" to indicate if the Up button has been activated by connecting to ground). It is also using is a bit wise "and" operator which is "&"
00000000
00000001
= 00000000

Arduino reference: https://www.arduino.cc/reference/en/language/structure/bitwise-operators/bitshiftleft/

It also uses a Compoundbitwiseor operator "|="  to group the inputs into the variable "combinedButtons" (possible to speed up the code and reduce the amout of memory needed)
Arduino reference: https://reference.arduino.cc/reference/en/language/structure/compound-operators/compoundbitwiseor/


References that were heavily used to develop Atari/C64 Paddle to USB conversion include:
https://hackaday.io/project/170908-control-freak  (Danjovic 2020, Atari 2600 Controller - 25/04/2020)
https://github.com/NicoHood/HID (Arduino HID Library)
https://www.arduino.cc/reference/en/language/functions/usb/mouse/

+----------------------------------------------------------------------------------------+
| ATARI 2600 - The following reference explain what each pins on the Atari DB9 socket    |
+----------------------------------------------------------------------------------------+
Atari Paddle  PINOUT   (DB9 Female Socket)

DB9    Name     Description 
-----------------------------
1       N/A     Not connected
2       N/A     Not connected
3       P1B     Paddle 1 Button (note: same as joystick left)
4       P2B     Paddle 2 Button (note: same as joystick right)
5       P2O     Paddle 2 Pot INPUT
6       N/A     Not connected
7       VCC     VCC for Paddle Pots
8       GND     Ground for Buttons
9       P1O     Paddle 1 Pot INPUT

*/


// DEBUG MODE
#define DEBUG 0 // [1 = ON | 0 = OFF]  DEBUG mode (in the Arduinio IDE use Tools -> Serial Monitor to see messages / also try the Serial Plotter to see it graphed)

//ADDITONAL LIBRARY
#include "HID-Project.h"

// DEFINE PIN of ARDUINO Micro
// Atari 2600 paddles use the same DE-9 with the paddle buttons using Left and Right direction pins for paddle button input.
//                             AVR    Atari 2600
#define atariUp       A3 //    PF4      1 up (not used)
#define atariDown     A2 //    PF5      2 down (not used)
#define atariLeft     A1 //    PF6      3 left  (also paddle 1 fire button)
#define atariRight    A0 //    PF7      4 right (also paddle 2 fire button) 

#define atariPaddle1  15 //    PB1      5 paddle 2 POT input
//                                      7 Vcc
//                                      8 GND
#define atariPaddle2  16 //    PB2      9 paddle 1 POT input


#define MAXCOUNTINGS 250  //OLD VALUE '1023'  - this determines how long to scan for reading the ADC ie long enough to record the bits to get a value.

// MOUSE SENSITIVITY
#define Mouse_DeadZone 2 // 0 for none the amout of turn before the controller registers
#define Mouse_Range 0.8// how sensiive upper and lower ranges



// Control Capacitors charging
#define releaseCapacitors()   DDRB &= ~((1<<1)|(1<<2))
#define resetCapacitors()  do {DDRB|=((1<<1)|(1<<2)); PORTB&=~((1<<1)|(1<<2));} while (0)


//|--------------------|
//|     VARIABLES      |
//|--------------------|
// GAMEPAD or MOUSE mode
int DEVICEMODE = 0;     // [0 = GAMEPAD* | 1 = MOUSE]   Device emulation options are either 'GAMEPAD' (mimics using Left Analog stick of a USB GAMEPAD) or 'MOUSE' (USB MOUSE).   
//
//  NOTE: GAMEPAD mode is preferred method.
//

int responseDelay = 0;        // response delay of the mouse, in ms

uint8_t combinedButtons = 0;
int16_t paddle1AnalogValue, paddle2AnalogValue,OLDpaddle1AnalogValue, OLDpaddle2AnalogValue,DIFpaddle1AnalogValue, DIFpaddle2AnalogValue;   // note max value will be +32,767

uint16_t countDISTANCE, DISTANCEX, DISTANCEY;

int  HASMOVED= 0;  // lets only update the move function if the wheel has changed or moved otherwise stay put



// SETUP                     
void setup() {

  pinMode(atariUp    , INPUT_PULLUP);   // not read but needed to read port F in digital pin mode
  pinMode(atariDown  , INPUT_PULLUP);   // not read but needed to read port F in digital pin mode
  pinMode(atariLeft  , INPUT_PULLUP);     //also paddle 1 fire button
  pinMode(atariRight , INPUT_PULLUP);     //also paddle 2 fire button
  
  resetCapacitors();

// Begin the device
if (DEVICEMODE == 0) {
  // Sends a clean report to the host. This is important on any Arduino type.
  Gamepad.begin();
}else {
  Mouse.begin();
};

// Start with a clean slate and make sure all buttons have been released   DOH!!! this was in the main loop shit shit shit
  if (DEVICEMODE == 0) {
    Gamepad.releaseAll();
  }else{
    Mouse.release(MOUSE_LEFT);
    Mouse.release(MOUSE_RIGHT);
  };

OLDpaddle1AnalogValue =0;  //assume old values are 0 - so that it keeps close to centre
OLDpaddle2AnalogValue =0;  //assume old values are 0 - so that it keeps close to centre

#if defined (DEBUG)
  Serial.begin(9600);
  Serial.println("Let's go...");
#endif
};  //end of setup

//--------------------------
//        MAIN LOOP         |
//--------------------------

void loop() {
uint8_t sample;

//--------------------------
//PORT F = Arduinio Pin number marking on pcb silk screen (Note: this is a Pro Micro)
//--------------------------
// PF0 = n/a (on pro micro)
// PF1 = n/a (on pro micro)
// PF2 = n/a (on pro micro)
// PF3 = n/a (on pro micro)
// PF4 = pin A3 (up)    
// PF5 = pin A2 (down)  
// PF6 = pin A1 (left)  *paddle 1 fire button
// PF7 = pin A0 (right) *paddle 2 fire button
// 

  // Read controller
  // Bit     7  6  5  4  3  2  1  0
  // Button  0  0  p2 p1 1  1  1  1
  

  // Example of "sample" in binary
  //001111  = no buttons pushed
  //011111 = Paddle 1 button pushed but not paddle 2
  //101111 = Paddle 2 button pushed but not paddle 1

// Scan digital controls
 
  combinedButtons = 0;

//Bitwise NOT changes each bit to its opposite: 0 becomes 1, and 1 becomes 0.
// sample is reading Port input F, as it reads this variable it is flipping the bits around.
 
  sample = ~PINF;
  //if (sample & (1 << 7)) combinedButtons |= (1 << 0); // up (this is not needed for paddle mode)
  //if (sample & (1 << 6)) combinedButtons |= (1 << 1); // down (this is not needed for paddle mode)
  if (sample & (1 << 5)) combinedButtons |= (1 << 2); // left (also paddle 1 fire button)   / bit 5  (note 6 digits: 0..5) 
  if (sample & (1 << 4)) combinedButtons |= (1 << 3); // right (also paddle 2 fire button)  / bit 4  (note 6 digits: 0..5)

// Bitwise Operators
 /*                           76543210 
  *  1 << 7   0000001 becomes 01000000
  *  1 << 5   0000001 becomes 00010000
  *  1 << 4   0000001 becomes 00001000
  *  
  *  Code assumes zero values
  *  samples the pins
  *  are specific pins set to high.
  *  updates combinedButtons digit value when pins are confirmed to be high (buttons have not been pressed), it will set it high regardlesso of the current value
  *  
  *  if the digit being tested is 0 (button is activated) then the result will be 0 if the digit is 1 then it will compare with 1 and 1 will be returned.
  *  
  *  https://reference.arduino.cc/reference/en/language/structure/compound-operators/compoundbitwiseor/
  *  https://www.arduino.cc/reference/en/language/structure/bitwise-operators/bitwiseand/
  */
  
 sampleAnalogValues();


// Send input info to USB HID

  if (DEVICEMODE ==0) {     //GAMEPAD MODE - BEGIN
    // Start with a clean slate - this is needed to minimise sticky button scenario
      Gamepad.releaseAll();    


     //Add smoothing?
     // placeholder

     //CONVERT to gamepad range ~ 62,000  using a value between 220-280 
      
     paddle1AnalogValue = (paddle1AnalogValue*260);
     paddle2AnalogValue = (paddle2AnalogValue*260);

    //CONVERT to shift range [-32768 .. +32767]  
    // note it is a int16 value so max is +32,767  
     paddle1AnalogValue = 30000-paddle1AnalogValue;
     paddle2AnalogValue = 30000-paddle2AnalogValue;

    // EDGES #2 - check make sure we don't exceed edges
      //Paddle 1
      if (paddle1AnalogValue > 32767) {
        paddle1AnalogValue= 32767;
      };

      if (paddle1AnalogValue < -32768) {
        paddle1AnalogValue= -32768;
      };
  
      //Paddle 2
       if (paddle2AnalogValue > 32767) { 
         paddle2AnalogValue= 32767;
       };

       if (paddle2AnalogValue < -32768) { 
         paddle2AnalogValue= -32768;
       };

       
    //WRITE  Send collected values to game HID function
      Gamepad.xAxis(paddle1AnalogValue);
      Gamepad.yAxis(paddle2AnalogValue);

      //(swapped directions/paddle buttons!)  
      if (combinedButtons & (1 << 3)) Gamepad.press(1); // Left Paddle   Bit 3
      if (combinedButtons & (1 << 2)) Gamepad.press(2); // Right Paddle  Bit 2

      // This writes the report to the host as a gamepad.
      Gamepad.write();
        
  }else{      //MOUSE MODE - BEGIN
    
    // Mouse library isn't as well defined but its something like this
    // Arduinio Reference:  Mouse.move(xVal, yVal)
    //
    // Official Mouse library range is only -12 to 12 but this library maybe 100.  -127 +127 
    // -12 = extreme left, 0 is middle. +12 = extreme right
    // 
    // xVal: amount to move along the x-axis. Allowed data types: signed char.
    // yVal: amount to move along the y-axis. Allowed data types: signed char.
    // wheel: amount to move scroll wheel. Allowed data types: signed char.


    
      //CONVERT to mouse range ~ using then left shift to supported range ~ [-12 .. +12] 
       //tried 0.1 ~ [-12 .. +12] 
       //tried 0.4 ~ too fasts
      paddle1AnalogValue = (paddle1AnalogValue*Mouse_Range);  //24  tried 0.1 (-12 
      paddle2AnalogValue = (paddle2AnalogValue*Mouse_Range);  //24
      
                  
      // left shift to -12 + 12 note - int range -127  +127
      paddle1AnalogValue = (95-(paddle1AnalogValue));   
      paddle2AnalogValue = (95-(paddle2AnalogValue));

      // DEADZONE CONTROL  
      if(paddle1AnalogValue >-(Mouse_DeadZone) && paddle1AnalogValue <(Mouse_DeadZone)){ 
          paddle1AnalogValue =0;  // create a deadzone in the middle  
      };
      if(paddle2AnalogValue >-(Mouse_DeadZone) && paddle2AnalogValue <(Mouse_DeadZone)){
          paddle2AnalogValue =0; //create a deadzone in the middle 
      };
      

      //Calculate difference  - for relative direction
      DIFpaddle1AnalogValue = paddle1AnalogValue - OLDpaddle1AnalogValue;    
      DIFpaddle2AnalogValue = paddle2AnalogValue - OLDpaddle2AnalogValue;


      /*
      // SET THRESHOLD FOR CHANGES
      if (DIFpaddle1AnalogValue < -5 || DIFpaddle1AnalogValue > 5) {
        DIFpaddle1AnalogValue=paddle1AnalogValue;
      } else {
        DIFpaddle1AnalogValue= OLDpaddle1AnalogValue;
                
      }
      if (DIFpaddle2AnalogValue < -5 || DIFpaddle2AnalogValue > 5) {
        DIFpaddle2AnalogValue=paddle2AnalogValue;
      } else {
        DIFpaddle2AnalogValue= OLDpaddle2AnalogValue;
      }

     */      

      /*
      // EDGES - check make sure we don't exceed edges
      //Paddle 1
      if (DIFpaddle1AnalogValue > 36) {
          DIFpaddle1AnalogValue= 36;
      };
      if (DIFpaddle1AnalogValue < -36) {
          DIFpaddle1AnalogValue= -36;
      };
  
      //Paddle 2
      if (DIFpaddle2AnalogValue > 36) { 
          DIFpaddle2AnalogValue= 36;
      };
      if (DIFpaddle2AnalogValue < -36) { 
          DIFpaddle2AnalogValue= -36;
      };

      */

      // hmmm so each turn will determine how far
      // 0 degrees = stay still
      // +45 degrees = jump right 
      // +90 degrees = jump hard right 

                     
      


        // add condition has mouse moved? only update if it has 
        // HAs moved? 
        
        
        Mouse.move(paddle1AnalogValue, paddle2AnalogValue,0);       

       //store the new values for comparison
      OLDpaddle1AnalogValue = paddle1AnalogValue;
      OLDpaddle2AnalogValue = paddle2AnalogValue;
    
    //logic for release of Mouse buttons (note - they need extra logic to also send the release of button presses)
     if ((combinedButtons & (1 << 3))==8) {
          if (Mouse.isPressed(MOUSE_LEFT)==0) {
              Mouse.press(MOUSE_LEFT); // Left Paddle  
          };
      } else {
        if (Mouse.isPressed(MOUSE_LEFT)==1) Mouse.release(MOUSE_LEFT); // Left Paddle
      };

     if ((combinedButtons & (1 << 2))==4)  {
        if (Mouse.isPressed(MOUSE_RIGHT)==0) { 
          Mouse.press(MOUSE_RIGHT); // Right Paddle   
        };
    } else {
      if (Mouse.isPressed(MOUSE_RIGHT)==1) Mouse.release(MOUSE_RIGHT); // Right Paddle
    };
    
    //delay(responseDelay);   //to avoid false readings might take this off
    
  }; 
//end gamepad / mouse conditions



#if defined (DEBUG)
        
        Serial.print("DEVICEMODE: "); Serial.print(DEVICEMODE);

        //Serial.print("   SAMPLE: "); Serial.print(sample,BIN);
        //Serial.print("   CButtons: "); Serial.print(combinedButtons,BIN);
        
        Serial.print("   OLD: "); Serial.print(paddle1AnalogValue );
        Serial.print("   DIF: "); Serial.print(DIFpaddle1AnalogValue );
        
        Serial.print(" Paddle l : "); Serial.print(paddle1AnalogValue);
        if (combinedButtons & (1 << 3)) Serial.print(" F"); else Serial.print(" -");
        
        
        Serial.print(" / Paddle 2 : "); Serial.print(paddle2AnalogValue);
        if (combinedButtons & (1 << 2)) Serial.print(" F"); else Serial.print(" -");
                
        Serial.println();
#endif

}


//
// Read analog values by measuring time for capacitor to charge.
// return true if none of the channels timed out.
//--------------------------
//PORT B = Arduinio Pin number marking on pcb silk screen (Note: this is a Pro Micro)
//--------------------------
// PB0 = n/c
// PB1 = pin15  *used
// PB2 = pin16  *used 
// PB3 = pin14  *not used
// PB4 = pin8
// PB5 = pin9
// PB6 = pin10
// PB7 = n/c
// 
// Read controller PORT B - LSB (Least significant bit first)
// Bit     7  6  5  4  3  2  1  0
// Button  0  0  0  0  0  aY aX 0


uint8_t sampleAnalogValues() {
  uint8_t sample;
  uint16_t countVar, aX, aY;

    // Release capacitors to charge
  releaseCapacitors();

  // now count time it takes for each input to flip HIGH
  aY = 0; aX = 0, countVar = MAXCOUNTINGS;
  do {
    sample = ~PINB;
    sample >>= 1;            // skip bit 0
    aX += sample & (1 << 0); // sample bit 1 Paddle 1, X
    sample >>= 1;
    aY += sample & (1 << 0); // sample bit 1 Paddle 2, Y
  } while (--countVar);

  resetCapacitors();  // reset external capacitors

  paddle1AnalogValue = (int16_t)aX;
  paddle2AnalogValue = (int16_t)aY;

    
  return;
}
