
//This function check the voltage in pin A3, this check in done AFTER the voltage divider
//and is used to signal how much more battery left 12.6-10.8=green, 10.8-9.6=blue, 9.6> = fast red blink
void checkVoltage()
{
	//Serial.println(analogRead(pinVoltage));
	//delay(500);
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
