#define RF69_COMPAT 0  // define this to use the RF69 driver i.s.o. RF12
#include <JeeLib.h>  //from jeelabs.org

#define myNodeID 10          //node ID of tx (range 0-30)
#define network     210      //network group (can be in the range 1-250).
#define RF_freq RF12_433MHZ     //Freq of RF12B can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. Match freq to module


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
const int vHigh = 520; // = 10.8v
const int vMedium = 461; // = 9.6v

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

void setup() {
  //Serial.begin(9600);
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
	
	if (statusChanged)
	{
		//send the data
		rf12_sendNow(0, &emontx, sizeof emontx);
		rf12_sendWait(2);
		statusChanged = false;	//zeros the var because the data was sent
	}
	
	debMag();
	if (statusLED != 2)
		checkVoltage();
}

void debMag()
{
	// read the state of the switch into a local variable:
	int readingMag1 = digitalRead(pinMag1);
	int readingMag2 = digitalRead(pinMag2);
	int readingMag3 = digitalRead(pinMag3);
	int readingMag4 = digitalRead(pinMag4);

	// check to see if you just pressed the button
	// (i.e. the input went from LOW to HIGH),  and you've waited
	// long enough since the last press to ignore any noise:

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

			// cycles thru RGB colors
			if (stateMag1 == LOW) 
			{
				//make green
				if (emontx.data1 == 255 && emontx.data2 == 0 && emontx.data3 == 0)
				{
					emontx.data1 = 0;
					emontx.data2 = 255;
					emontx.data3 = 0;
				}
				// make blue
				else if (emontx.data1 == 0 && emontx.data2 == 255 && emontx.data3 == 0)
				{
					emontx.data1 = 0;
					emontx.data2 = 0;
					emontx.data3 = 255;
				}
				else //make red - it means that we either in blue or other color
				{
					emontx.data1 = 255;
					emontx.data2 = 0;
					emontx.data3 = 0;
				}
				
				statusChanged = true;
				statusLED = 0;
				digitalWrite(pinR, LOW);
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
				emontx.data1 = random(0,255);
				emontx.data2 = random(0,255);
				emontx.data3 = random(0,255);
				statusChanged = true;
				if (statusLED == 0)
				{
					statusLED = 2;
					digitalWrite(pinR, HIGH);
				}
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
				emontx.data4 = emontx.data4+1;	//cycle thru the different states
				if (emontx.data4 > 4)
					emontx.data4 =0;
				statusChanged = true;
				statusLED = 0;
				digitalWrite(pinR, LOW);
				//Serial.println(emontx.data4);
			}
		}
	}
	
	//mag4
	if ((millis() - lastDebounceTimeMag4) > debounceDelay) {
		
		// if the reed state has changed:
		if (readingMag4 != stateMag4) {
			stateMag4 = readingMag4;

			//makes random color and effect(?)
			if (stateMag4 == LOW)
			{
				if (statusLED == 0)
				{
					statusLED = 1;
					analogWrite(pinOutLED,4);
				}
				if (statusLED == 1)
				{
					statusLED = 0;
					analogWrite(pinOutLED,0);
				}
				if (statusLED == 2)
				{
					analogWrite(pinOutLED, 255);
					countShutdown = millis();	
				}
				
				digitalWrite(pinR, LOW);
			}
		}
	}
	// save the reading.  Next time through the loop,
	lastMag1State = readingMag1;
	lastMag2State = readingMag2;
	lastMag3State = readingMag3;
	lastMag4State = readingMag4;
	
	unsigned long currentcountShutdown = millis();
	
	if ((abs(currentcountShutdown-countShutdown) >=4000) && statusLED == 2)
	{
		statusLED= 0;
		analogWrite(pinOutLED,0);
		//Serial.println("LED OFF");
	}
}
