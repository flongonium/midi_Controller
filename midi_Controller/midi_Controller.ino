#include "Arduino.h"
//The setup function is called once at startup of the sketch
/*
 *  MIDI Serial speed: 31250 - For debugging use 38400
 *
 *  Control Change Table for Meris Polymoon:
 *  CC# 04 - Exp. pedal    CC# 22 - early modulation
 *  CC# 09 - dottet 8th    CC# 23 - feedback filter
 *  CC# 14 - bypass        CC# 24 - delay level
 *  CC# 15 - tempo         CC# 25 - late modulation
 *  CC# 16 - time          CC# 26 - dyn flanger mode
 *  CC# 17 - feedback      CC# 27 - dyn flanger speed
 *  CC# 18 - mix           CC# 28 - tap
 *  CC# 19 - multiply      CC# 29 - phaser mode
 *  CC# 20 - dimension     CC# 30 - flanger feedback
 *  CC# 21 - dynamics      CC# 31 - half speed
 *
 *  CC# 00 - bank select
 *
 *  Note on  = 0x90
 *  Note off = 0x80
 *  127, HEX 7F, 1111111
 *
 *
 *  ********HOW DOES CONTROL CHANGE WORK?*******
 *  0xnc, 0xcc, 0xvv
      n is the status (0xB)
      c is the MIDI channel (e.g. 0)
      cc is the controller number (0-127)
      vv is the controller value (0-127)


 *  ********HOW DOES PROGRAM CHANGE WORK?*******
 *  0xnc, 0xpp
      n is the status (0xC)
      c is the channel (e.g. 0)
      pp is the patch number (0-127)
 */

/*-------------------------------------- Variables declaration --------------------------------------*/

// arduino inputs
const int merisPedal  = A2;
const int expPedal    = A1;
const int constSwitch = 7;
const int leds[]      = {2, 3, 4, 5};

struct input
{
	const int switchInput;
	int switchState;
	int switchStateOld;
	int msgType;
};

struct input inputs[] =
{
	 {12, 0, 0, 1} //input 2, add 4
	,{11, 0, 0, 2} //input 3, substract 4
	,{0, 0, 0, 0} //end of list marker
};


// program change value represents layer
int pcValue     = 0;

// expression pedal stuff
int expValue          = 0;
int expValueAnalog    = 0;
int expValueAnalogOld = 0;

// channel selection (currently only 2 channels)
int channelVal    = 0;
int channelValOld = 0;

// half speed CC
int speedVal    = 0;
int speedValOld = 0;

// midi channels
byte midiChannelCC = 0xB0;     // 176, HEX B0 -> this is channel 0, must match with midi device
byte midiChannelPC = 0xC0;     // 192, HEX C0 -> this is preset 0, let's start always with this preset

/*-------------------------------------- SETUP --------------------------------------*/

void setup()
{
	Serial.begin(31250); //MIDI rate = 31250

	//set up switches and leds
	pinsSetup();

	delay(50);
}

/*-------------------------------------- MAIN PROGRAM--------------------------------------*/

void loop()
{
	pcValue = readChangeLayer(pcValue);
	readMeris();
	changeChannel();
	readExpressionPedal();
	setLedState(pcValue);
}

/*-------------------------------------- SETUPS --------------------------------------*/

void pinsSetup()
{
	for (int i=0; inputs[i].switchInput != 0; i++)
	{
		pinMode(inputs[i].switchInput, INPUT_PULLUP);
	}
  pinMode(constSwitch, INPUT_PULLUP);

  for (unsigned int i=0; i > sizeof(leds); i++)
  {
    pinMode(leds[i], OUTPUT);
  }
}

/*-------------------------------------- MIDI MAIN MESSAGES --------------------------------------*/

void midiSend_CC(byte midiChannel, byte controllerNumber , byte controllerVal) // midiChannel = number of MIDI channel, controllerNumber = CC Message, controllerVal = data Value between 0-127
{
  Serial.write(midiChannel);
  Serial.write(controllerNumber);
  Serial.write(controllerVal);
}

void midiSend_PC(byte midiChannel, byte patchNumber) // midiChannel = number of MIDI channel, patchNumber = preset number between 0-127 (Meris can handle only 16)
{
  Serial.write(midiChannel);
  Serial.write(patchNumber);
}

/*-------------------------------------- ANALOG READING --------------------------------------*/

// Case values depend on resistors values in preset switch pedal, so check them first.
// other approach: get mean of n values and compare
void readMeris()
{
	int val = analogRead(merisPedal);
	switch (val)
	{
		case 490 ... 510:
			midiSend_PC(midiChannelPC, pcValue);
			break;
		case 760 ... 775:
			midiSend_PC(midiChannelPC, pcValue+1);
			break;
		case 895 ... 915:
			midiSend_PC(midiChannelPC, pcValue+2);
			break;
		case 230 ... 250:
			midiSend_PC(midiChannelPC, pcValue+3);
			break;
		default:
			break;
	}
}

// expression pedal
void readExpressionPedal()
{
  int expValueAnalog = analogRead(expPedal);
  if(expValueAnalog != expValueAnalogOld)
  {
    expValue = map(expValueAnalog, 0, 1024, 0, 255);
    //midiSend_CC(midiChannelCC, 4, expValue);
  }
  expValueAnalogOld = expValueAnalog;
}

/*-------------------------------------- SWICHT EVENTS --------------------------------------*/

// changes the layer of the bank
int readChangeLayer(int val)
{
  for(int i=0; inputs[i].switchInput != 0; ++i)
  {
    inputs[i].switchState = digitalRead(inputs[i].switchInput);

    if(inputs[i].switchState != inputs[i].switchStateOld)
    {
      if(inputs[i].switchState == HIGH)
      {
        if(inputs[i].msgType == 1)
        {
          val = val+4;
          if(val > 12 || val < 0)
          {
            val = 0;
          }
        }
        else if(inputs[i].msgType == 2)
        {
          val = val-4;
          if(val > 12 || val < 0)
          {
            val = 0;
          }
        }
        else
        {
          break;
        }
      }
      inputs[i].switchStateOld = inputs[i].switchState;
    }
  }
  return val;
}


// change channel to send same commands to another device
void changeChannel()
{
  channelVal = digitalRead(constSwitch);
  if(channelVal != channelValOld)
  {
    if(channelVal == HIGH)
    {
      midiChannelCC = 0xB0;
      midiChannelPC = 0xC0;
    }
    else if (channelVal == LOW)
    {
      midiChannelCC = 0xB1;
      midiChannelPC = 0xC1;
    }
  }
  channelValOld = channelVal;
}

// sets permanently half speed on or off
void setHalfSpeed()
{
  int speedVal_1 = digitalRead(inputs[0].switchInput);
  int speedVal_2 = digitalRead(inputs[1].switchInput);

  if(speedVal_1 && speedVal_2 == HIGH)
  {
    if(speedValOld == HIGH) // This is marked as channel 1 on my device
    {
      midiSend_CC(midiChannelCC, 31, 0);
    }
    else if (speedValOld == LOW)
    {
      midiSend_CC(midiChannelCC, 31, 127);
    }
  }
  speedValOld = speedVal_1;
}
/*-------------------------------------- LED STUFF --------------------------------------*/

// get som fnacy lights in here
void setLedState(int val)
{
  switch (val)
  {
    case 0:
      digitalWrite(leds[3], LOW);
      digitalWrite(leds[2], LOW);
      digitalWrite(leds[1], LOW);
      digitalWrite(leds[0], HIGH);
      break;
    case 4:
      digitalWrite(leds[3], HIGH);
      digitalWrite(leds[2], LOW);
      digitalWrite(leds[1], LOW);
      digitalWrite(leds[0], HIGH);
      break;
    case 8:
      digitalWrite(leds[3], HIGH);
      digitalWrite(leds[2], HIGH);
      digitalWrite(leds[1], LOW);
      digitalWrite(leds[0], HIGH);
      break;
    case 12:
      digitalWrite(leds[3], HIGH);
      digitalWrite(leds[2], HIGH);
      digitalWrite(leds[1], HIGH);
      digitalWrite(leds[0], HIGH);
      break;
  }
}
