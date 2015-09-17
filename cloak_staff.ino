#define RF69_COMPAT 0  // define this to use the RF69 driver i.s.o. RF12
#include <JeeLib.h>  //from jeelabs.org
#include "aux_func.c"

#define myNodeID 10          //node ID of tx (range 0-30)
#define network     42      //network group (can be in the range 1-250).
#define RF_freq RF12_433MHZ     //Freq of RF12B can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. Match freq to module

#define com_types 6 // define the amount of commands that are use (light LED, change colors, patterns... etc)
#define com_len 2 //defines the length of the reeds command

//comm vars
//data1 = red,
//data2 = green,
//data3 = blue,
//data4 = function - bit 0(flicker, solid), bit1(low, high), bit2(random color, different color),
//                   bit3(Patern1, Patern2), bit4(Patern3, Patern4)
typedef struct { int data1, data2, data3, data4; } PayloadTX;      // create structure - a neat way of packaging data for RF comms
PayloadTX emontx;

int pinR = A0;
int pinG = A2;
int pinB = A1;

// states the current state of the RGB LED
int pinRstate = LOW;
int pinGstate = LOW;
int pinBstate = LOW;

int cycRGB = 0; // 0 = red, 1=green, 2=blue

int pinMag1 = 7;
int pinMag2 = 8;
int pinMag3 = 4;
int pinMag4 = 3;

int pinVoltage = A3;

int pinOutLED = 6;
int pinOutFairy = 5;

long previousMillisVoltageLED = 0;        // will store last time LED was updated

long VoltageOnTime = 150; //the time in ms in which the RCG LED it going to blink
long VoltageOffTime = 4850; //the time in ms in which the RCG LED it going to blink

// voltage reference for main power supply after voltage divider - LiPo 3S - 12.6v-9.6V -> 5v-0v
const int vHigh = 820; // = 10.8v
const int vMedium = 610; // = 9.6v

//reed sensor debouncing setup handling 
int stateMag1;             // the current reading from the input pin
int stateMag2;             // the current reading from the input pin
int stateMag3;             // the current reading from the input pin
int stateMag4;             // the current reading from the input pin
int lastMag1State = LOW;   // the previous reading from the input pin
int lastMag2State = LOW;   // the previous reading from the input pin
int lastMag3State = LOW;   // the previous reading from the input pin
int lastMag4State = LOW;   // the previous reading from the input pin

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTimeMag1 = 0;  // the last time the output pin was toggled
long lastDebounceTimeMag2 = 0;  // the last time the output pin was toggled
long lastDebounceTimeMag3 = 0;  // the last time the output pin was toggled
long lastDebounceTimeMag4 = 0;  // the last time the output pin was toggled
long debounceDelay = 100;    // the debounce time; increase if the output flickers

// in order to not transmit all the time..
boolean statusChanged = false;

//counter for LED high power shutdown
unsigned long countShutdown;
int statusLED = 0; //LED status: 0 = off, 1=on, 2=10%

// while reading the read sensors we want to give the user an indication that a successful read was made
// if we "in-readin" to don't do voltage check. 1 = do voltage check, 0 = don't do;
bool statusVoltage = 1;

//fairy lights
int statusFairy = 0; // 0 = off, 1 = on , 2=fader
int valueFairy;
long timeFairy=0;

int periodeFairy = 2000;
int displaceFairy = 500;

int reedCommands[com_types][com_len] = {
	{1,2}, // toggle LED low (on-off)
	{1,3}, // turn LED high (on for 4 seconds)
	{2,1}, // cycle cloak RGB
	{2,3}, // random color
	{3,1}, // cycle patterns
	{3,2}  // random pattern or fairy lights - TBD
};

int arrCommand[2] = {0,0};

//because we expect 2 sequence command we need to count the command in order to know the order of them.
int curCmd = 0;

void setup() {
  Serial.begin(9600);
  rf12_initialize(myNodeID,RF_freq,network);   //Initialize RFM12 with settings defined above  
  
  //Magnetic latch setup (reed sensors)
  pinMode(pinMag1,INPUT_PULLUP);
  pinMode(pinMag2,INPUT_PULLUP);
  pinMode(pinMag3,INPUT_PULLUP);
  pinMode(pinMag4,INPUT_PULLUP);
  
  //RGB indication LED setup
  pinMode(pinR,OUTPUT);
  pinMode(pinG,OUTPUT);
  pinMode(pinB,OUTPUT);
  
  //NPN control (pin 5,6)
  pinMode(pinOutLED,OUTPUT); 
  pinMode(pinOutFairy,OUTPUT); 
  
  //sets the voltage measure pin as input
  pinMode(pinVoltage,INPUT);

  //kill RGB led, fairy leds and main led
  digitalWrite(pinR,LOW);
  digitalWrite(pinG,LOW);
  digitalWrite(pinB,LOW);
  
  digitalWrite(pinOutLED,LOW);
  digitalWrite(pinOutFairy,LOW);
}

void loop() 
{
	// if 2 or reed sensor were toggles than check the command
	if (curCmd >= 2)
	{
		if (statusChanged)
		{
			Serial.println("transmitting...");
			Serial.println(emontx.data1);
			Serial.println(emontx.data2);
			Serial.println(emontx.data3);
			Serial.println(emontx.data4);
			delay(100);
			//send the data
			rf12_sendNow(0, &emontx, sizeof emontx);
			rf12_sendWait(4);
			statusChanged = false;	//zeros the var because the data was sent	
		}
		curCmd = 0;
		arrCommand[0] = 0;
		arrCommand[1] = 0;
		statusVoltage = 1;
	}
	//delay(1);
	debMag();
	//delay(1);
	checkCommand();
	//delay(1);
	if (statusVoltage)
		checkVoltage();
	
	//delay(1);
	// high LED usage	
	unsigned long currentcountShutdown = millis();
	
	if ((abs(currentcountShutdown-countShutdown) >=4000) && statusLED == 1)
	{
		statusLED= 0;
		analogWrite(pinOutLED,0);
		////Serial.println("LED OFF");
	}
	
	// fairy cycle usage
	if (statusFairy == 3)
	{
		timeFairy = millis();
		valueFairy = 128+127*cos(2*PI/periodeFairy*timeFairy);
		analogWrite(pinOutFairy, valueFairy);           // sets the value (range from 0 to 255) 
	}
}

/*
0 - {1,2}, // toggle LED low (on-off) 
1 - {1,3}, // turn LED high (on for 4 seconds)
2 - {2,1}, // cycle cloak RGB
3 - {2,3}, // random color
4 - {3,1}, // cycle patterns
5 - {3,2}  // random pattern or fairy lights - TBD
*/
//after getting reed sensors data (4 reads) it checks if it suits any known command
// return 0 if invalid command
// return 1 if command OK to transmit
// return 2 if command OK but not to transmit (LED or fairy)
int checkCommand()
{
	if (reedCommands[0][0] == arrCommand[0] && reedCommands[0][1] == arrCommand[1]) // toggle LED low (on-off) 1-2
	{
		//LED status: 0 = off, 1=on, 2=10%
		if (statusLED != 2)
		{
			statusLED = 2; //because it can go from HIGH also.. not just from OFF
			analogWrite(pinOutLED, 4);
		}
		else
		{
			analogWrite(pinOutLED, 0);
			statusLED = 0;
		}
		
		// do voltage check
		statusVoltage = 1;
		//zeros the array
		arrCommand[0] = 0;
		arrCommand[1] = 0;
		return 2;
	}
	
	if (reedCommands[1][0] == arrCommand[0] && reedCommands[1][1] == arrCommand[1]) // turn LED high (on for 4 seconds) 1-3
		{
			analogWrite(pinOutLED, 255);	
			countShutdown = millis();
			//LED status: 0 = off, 1=on, 2=10%
			statusLED = 1;
			// do voltage check
			statusVoltage = 1;
			//zeros the array
			arrCommand[0] = 0;
			arrCommand[1] = 0;
			return 2;
		}
	
	if (reedCommands[2][0] == arrCommand[0] && reedCommands[2][1] == arrCommand[1]) // cycle RGB 2-1
	{
		//Serial.println("RGB");
		//delay(100);
		//make green
		if (cycRGB == 0)
		{
			emontx.data1 = 0;
			emontx.data2 = 255;
			emontx.data3 = 0;
			cycRGB++;
			//Serial.println("green");
			//delay(100);
		}
		// make blue
		else if (cycRGB == 1)
		{
			emontx.data1 = 0;
			emontx.data2 = 0;
			emontx.data3 = 255;
			cycRGB++;
			//Serial.println("blue");
			//delay(100);
		}
		else //make red - it means that we either in blue or other color
		{
			emontx.data1 = 255;
			emontx.data2 = 0;
			emontx.data3 = 0;
			cycRGB = 0;
			//Serial.println("red");
			//delay(100);
		}
		//prepare it to transmission
		statusChanged = true;
		// do voltage check
		statusVoltage = 1;
		//zeros the array
		arrCommand[0] = 0;
		arrCommand[1] = 0;
		return 1;
	}
	if (reedCommands[3][0] == arrCommand[0] && reedCommands[3][1] == arrCommand[1]) // cycle patterns 2-3
	{
		emontx.data1 = random(0,255);
		emontx.data2 = random(0,255);
		emontx.data3 = random(0,255);
		//prepare it to transmission
		statusChanged = true;
		// do voltage check
		statusVoltage = 1;
		//zeros the array
		arrCommand[0] = 0;
		arrCommand[1] = 0;
		return 1;
	}
	if (reedCommands[4][0] == arrCommand[0] && reedCommands[4][1] == arrCommand[1]) // random color 3-1
	{
		emontx.data4 = emontx.data4 + 1;	//cycle thru the different states
		if (emontx.data4 > 3)
			emontx.data4 = 0;
		//prepare it to transmission
		statusChanged = true;
		// do voltage check
		statusVoltage = 1;
		//zeros the array
		arrCommand[0] = 0;
		arrCommand[1] = 0;
		return 1;
	}
	if (reedCommands[5][0] == arrCommand[0] && reedCommands[5][1] == arrCommand[1]) // toggle fairy on of 3-2
	{
		periodeFairy = random(3500,7000);
		displaceFairy = random(250,750);
		// toggle status of fairy lights
		statusFairy=statusFairy+1;
		//cycle through the states
		if (statusFairy > 3)
			statusFairy = 0;
		// if off kill the lights
		if (statusFairy == 0)
			analogWrite(pinOutFairy,0);
		else if (statusFairy == 1)
			analogWrite(pinOutFairy,80);
		else if (statusFairy == 2)
			analogWrite(pinOutFairy,255);
		//Serial.println("Fairy gtl");
		// do voltage check
		statusVoltage = 1;
		//zeros the array
		arrCommand[0] = 0;
		arrCommand[1] = 0;
		return 1;
	}
	return 0;
}

void debMag()
{
	// read the state of the switch into a local variable:
	int readingMag1 = digitalRead(pinMag1);
	int readingMag2 = digitalRead(pinMag2);
	int readingMag3 = digitalRead(pinMag3);
	int readingMag4 = digitalRead(pinMag4);

	// If the switch changed, due to noise or pressing:
	if (readingMag1 != lastMag1State) {
		// reset the debouncing timer
		lastDebounceTimeMag1 = millis();
	}
	
	// If the switch changed, due to noise or pressing:
	if (readingMag2 != lastMag2State) {
		// reset the debouncing timer
		lastDebounceTimeMag2 = millis();
	}
	
	// If the switch changed, due to noise or pressing:
	if (readingMag3 != lastMag3State) {
		// reset the debouncing timer
		lastDebounceTimeMag3 = millis();
	}
	
	// If the switch changed, due to noise or pressing:
	if (readingMag4 != lastMag4State) {
		// reset the debouncing timer
		lastDebounceTimeMag4 = millis();
	}

	//mag1
	if ((millis() - lastDebounceTimeMag1) > debounceDelay) {
		
		// if the reed state has changed:
		if (readingMag1 != stateMag1) {
			stateMag1 = readingMag1;

			//check is it LOW (beacuse of pull-up use)
			if (stateMag1 == LOW) 
			{
				//sets the read in the array command
				arrCommand[curCmd] = 1;
				curCmd++;//for the next reed reading
				statusVoltage = 0; //don't do voltage reading
				// indicates that the reed command was successfully, RED = mag1 reed 
				digitalWrite(pinR, HIGH);
				digitalWrite(pinG, LOW);
				digitalWrite(pinB, LOW);
				//Serial.println("mag1");
				//delay(100);
			}
		}
	}
	
	//mag2
	if ((millis() - lastDebounceTimeMag2) > debounceDelay) {
		
		// if the reed state has changed:
		if (readingMag2 != stateMag2) {
			stateMag2 = readingMag2;

			//makes random color and effect(?)
			if (stateMag2 == LOW)
			{
				//sets the read in the array command
				arrCommand[curCmd] = 2;
				curCmd++;//for the next reed reading
				statusVoltage = 0; //don't do voltage reading
				// indicates that the reed command was successfully, RED = mag1 reed
				digitalWrite(pinR, LOW);
				digitalWrite(pinG, HIGH);
				digitalWrite(pinB, LOW);
				//Serial.println("mag2");
				//delay(100);
			}
		}
	}
	
	//mag3
	if ((millis() - lastDebounceTimeMag3) > debounceDelay) {
		
		// if the reed state has changed:
		if (readingMag3 != stateMag3) {
			stateMag3 = readingMag3;

			//makes random color and effect(?)
			if (stateMag3 == LOW)
			{
				//sets the read in the array command
				arrCommand[curCmd] = 3;
				curCmd++;//for the next reed reading
				statusVoltage = 0; //don't do voltage reading
				// indicates that the reed command was successfully, RED = mag1 reed
				digitalWrite(pinR, LOW);
				digitalWrite(pinG, LOW);
				digitalWrite(pinB, HIGH);
				//Serial.println("mag3");
				//delay(100);
			}
		}
	}
	
	//mag4
	if ((millis() - lastDebounceTimeMag4) > debounceDelay) {
		
		// if the reed state has changed:
		if (readingMag4 != stateMag4) {
			stateMag4 = readingMag4;

			//TBD - free reed sensor
			if (stateMag4 == LOW)
			{
				
			}
		}
	}
	// save the reading.  Next time through the loop,
	lastMag1State = readingMag1;
	lastMag2State = readingMag2;
	lastMag3State = readingMag3;
	lastMag4State = readingMag4;
}

//This function check the voltage in pin A3, this check in done AFTER the voltage divider
//and is used to signal how much more battery left 12.6-10.8=green, 10.8-9.6=blue, 9.6> = fast red blink
void checkVoltage()
{
	////Serial.println(analogRead(pinVoltage));
	////delay(500);
	if (analogRead(pinVoltage) > vHigh)
	{
		pinBstate = LOW;
		pinRstate = LOW;
		digitalWrite(pinB, pinBstate);
		digitalWrite(pinR, pinRstate);
		// check to see if it's time to change the state of the LED
		unsigned long currentMillis2 = millis();
		
		if((pinGstate == HIGH) && (currentMillis2 - previousMillisVoltageLED >= VoltageOnTime))
		{
			pinGstate = LOW;  // Turn it off
			previousMillisVoltageLED = currentMillis2;  // Remember the time
			digitalWrite(pinG, pinGstate);  // Update the actual LED
		}
		else if ((pinGstate == LOW) && (currentMillis2 - previousMillisVoltageLED >= VoltageOffTime))
		{
			pinGstate = HIGH;  // turn it on
			previousMillisVoltageLED = currentMillis2;   // Remember the time
			digitalWrite(pinG, pinGstate);	  // Update the actual LED
		}
	} else if (analogRead(pinVoltage) > vMedium)
	{
		pinRstate = LOW;
		pinGstate = LOW;
		digitalWrite(pinR, pinRstate);
		digitalWrite(pinG, pinGstate);
		// check to see if it's time to change the state of the LED
		unsigned long currentMillis2 = millis();
		
		if((pinBstate == HIGH) && (currentMillis2 - previousMillisVoltageLED >= VoltageOnTime))
		{
			pinBstate = LOW;  // Turn it off
			previousMillisVoltageLED = currentMillis2;  // Remember the time
			digitalWrite(pinB, pinBstate);  // Update the actual LED
		}
		else if ((pinBstate == LOW) && (currentMillis2 - previousMillisVoltageLED >= VoltageOffTime))
		{
			pinBstate = HIGH;  // turn it on
			previousMillisVoltageLED = currentMillis2;   // Remember the time
			digitalWrite(pinB, pinBstate);	  // Update the actual LED
		}
	} else
	{
		pinBstate = LOW;
		pinGstate = LOW;
		digitalWrite(pinB, pinBstate);
		digitalWrite(pinG, pinGstate);
		// check to see if it's time to change the state of the LED
		unsigned long currentMillis2 = millis();
		
		if((pinRstate == HIGH) && (currentMillis2 - previousMillisVoltageLED >= VoltageOnTime))
		{
			pinRstate = LOW;  // Turn it off
			previousMillisVoltageLED = currentMillis2;  // Remember the time
			digitalWrite(pinR, pinRstate);  // Update the actual LED
		}
		else if ((pinRstate == LOW) && (currentMillis2 - previousMillisVoltageLED >= VoltageOffTime/5))
		{
			pinRstate = HIGH;  // turn it on
			previousMillisVoltageLED = currentMillis2;   // Remember the time
			digitalWrite(pinR, pinRstate);	  // Update the actual LED
		}
	}
}
