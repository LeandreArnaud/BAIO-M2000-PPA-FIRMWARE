#include <Joystick.h>
#include "Adafruit_LEDBackpack.h"
#define DCSBIOS_DEFAULT_SERIAL
#include "DcsBios.h"


/********* PINs SETUP **********/
const int PPA_SDA = 2;
const int PPA_SCL = 3;
const int PPAlatchPin = 7;
const int PPAdataPin = 4;
const int PPAclockPin = 6;
const byte PPAbacklightPin = 5;

const bool debug = false;



/********* INIT VALUES AND CST **********/
byte PPAvalues[2] = {255, 255};
const byte registreLength = 8;
int bombeNumber = 0;
const byte maxBombeNumber = 18;
int bombeMeters = 0;
const byte maxBombeMeters = 28;

bool isDCSinmission = false;

bool mis = false;
bool misp = false;
bool mag = false;
bool magp = false;
bool par = false;

Joystick_ Joystick(
  0x05, // id of the gamepad, icrease in other pads to avoid conflict
  0x05, //Gamepad
  16, // button
  0, // hat
  false, // X
  false, // Y
  false, // Z
  false, // Rx
  false, // Ry
  false, // Rz
  false, // rudder
  false, // throttle
  false, // rudder
  false, // rudder
  false // steering
);

Adafruit_LEDBackpack matrix = Adafruit_LEDBackpack();








/********* SETUP **********/
void setup() {
  DcsBios::setup();

  matrix.begin(0x70);  // pass in the address 70 or 74
  matrix.setBrightness(5);
  matrix.setDisplayState(true);

  pinMode(PPAbacklightPin, OUTPUT);

  pinMode(PPAlatchPin, OUTPUT);
  pinMode(PPAclockPin, OUTPUT);
  pinMode(PPAdataPin, INPUT);

  Joystick.begin();

  if (debug) Serial.begin(9600);

  // init digits
  for (uint8_t i=1; i<8; i++) { // mdl, hg, bg, b, bd, hd, h
    matrix.displaybuffer[i] = 0b0000000000000001; // bd, bg, hd, hg 0b0000000000011111
    delay(100);
  }
  matrix.writeDisplay();
  analogWrite(PPAbacklightPin, 255);

  delay(1000);
}







/********* DCS BIOS **********/
// PAR
void onPpaGunRocketPapChange(unsigned int newValue) {
  par = newValue;
}
DcsBios::IntegerBuffer ppaGunRocketPapBuffer(0x72e4, 0x0100, 8, onPpaGunRocketPapChange);

// MAG
void onPpaMagicMisChange(unsigned int newValue) {
  mag = newValue;
}
DcsBios::IntegerBuffer ppaMagicMisBuffer(0x72e0, 0x8000, 15, onPpaMagicMisChange);

// MAGP
void onPpaMagicPChange(unsigned int newValue) {
  magp = newValue;
}
DcsBios::IntegerBuffer ppaMagicPBuffer(0x72e0, 0x4000, 14, onPpaMagicPChange);

// MIS
void onPpaS530MisChange(unsigned int newValue) {
  mis = newValue;
}
DcsBios::IntegerBuffer ppaS530MisBuffer(0x72e0, 0x0800, 11, onPpaS530MisChange);

// MISP
void onPpaS530PChange(unsigned int newValue) {
  misp = newValue;
}
DcsBios::IntegerBuffer ppaS530PBuffer(0x72e0, 0x0400, 10, onPpaS530PChange);


// BOMBE NUMBER
void onPpaQtyDispChange(char* newValue) {
    // convert char to int
    bombeNumber = (newValue[0] - '0') * 10 + (newValue[1] - '0');
}
DcsBios::StringBuffer<2> ppaQtyDispBuffer(0x7308, onPpaQtyDispChange);

// BOMBE INTERVAL
void onPpaIntDispChange(char* newValue) {
    bombeMeters = (newValue[0] - '0') * 10 + (newValue[1] - '0');
}
DcsBios::StringBuffer<2> ppaIntDispBuffer(0x730a, onPpaIntDispChange);


// CONSOLE BACKLIGHT
void onDashPanelLgtKnobChange(unsigned int newValue) {
    if (newValue <= 2000) {
        analogWrite(PPAbacklightPin, 0);
    } else {
        analogWrite(PPAbacklightPin, newValue/256);    
    }
}
DcsBios::IntegerBuffer dashPanelLgtKnobBuffer(0x729c, 0xffff, 0, onDashPanelLgtKnobChange);
// DIGIT DISPLAY BACKLIGHT
void onCautAdvLgtChange(unsigned int newValue) {
  if (newValue <= 2000) {
        matrix.setBrightness(0);
        matrix.setDisplayState(false);
    } else {
        matrix.setBrightness(newValue/4096);
        matrix.setDisplayState(true);
    }
    
}
DcsBios::IntegerBuffer cautAdvLgtBuffer(0x72a2, 0xffff, 0, onCautAdvLgtChange);

void onAircraftNameChange(char* newValue) {
  if(strcmp(newValue,""))
    isDCSinmission = true;
  else
    isDCSinmission = false;
}
DcsBios::StringBuffer<24> AcftNameBuffer(0x0000, onAircraftNameChange);





/********* LOOP **********/
void loop() {
  DcsBios::loop();

  scanPPA();
  delay(10);
  // writePPADigits(bombeNumber, bombeMeters, mis, misp, mag, magp, par);
  if (!isDCSinmission && bitRead(PPAvalues[0], 7)) {
    writePPADigits(88, 88, 1, 1, 1, 1, 1);
  } else {
    writePPADigits(bombeNumber, bombeMeters, mis, misp, mag, magp, par);
  }
}


void scanPPA() {
  digitalWrite(PPAlatchPin,1);
  delayMicroseconds(20);
  digitalWrite(PPAlatchPin,0);

  for (int k=0; k<2; k++) {
    for (int i=7; i>=0; i--) {
      digitalWrite(PPAclockPin, 0);
      delayMicroseconds(2);
      bool value = digitalRead(PPAdataPin);
      
      // if value has changed
      if (value != bitRead(PPAvalues[k], i)) {
        bitWrite(PPAvalues[k], i, value);

        Joystick.setButton(registreLength*k+i, value);
        // debug
        // if (debug) Serial.println(registreLength*k+i);
        if (debug) Serial.println(k);
        if (debug) Serial.println(i);
        if (debug) Serial.println(value);
        if (debug) Serial.println("----");

        // change led state is dcs bios is not connected
        // keep changing digit if dcs bios is connected to reduce the visible lag on the displays
        if (value) {
          if (k == 0 && i == 2) {
            incrementPPABombeNumber();
          } else if (k == 0 && i == 3) {
            decrementPPABombeNumber();
          } else if (k == 0 && i == 4) {
            incrementPPABombeMeters();
          } else if (k == 0 && i == 5) {
            decrementPPABombeMeters();
          }
        }
        if (!isDCSinmission && value) { 
          if (k == 1 && i == 4) {
            magp = !magp;
            mag = !mag;
          } else if (k == 1 && i == 2) {
            misp = !misp;
            mis = !mis;
          } else if (k == 1 && i == 5) {
            par = !par;
          }
        }
      }

      digitalWrite(PPAclockPin, 1);
      delayMicroseconds(2);
    }
  }

  // RET position
  if (!bitRead(PPAvalues[1], 0) && !bitRead(PPAvalues[1], 1)) {
    Joystick.setButton(14, 1);
  } else {
    Joystick.setButton(14, 0);
  }

  // AUTO D/G position
  if (!bitRead(PPAvalues[0], 1) && !bitRead(PPAvalues[0], 0)) {
    Joystick.setButton(15, 1);
  } else {
    Joystick.setButton(15, 0);
  }
}

//            mdl hg bg b bd hd h dot
bool zero[] = {0, 1, 1, 1, 1, 1, 1, 0};
bool one[] = {0, 0, 0, 0, 1, 1, 0, 0};
bool two[] = {1, 0, 1, 1, 0, 1, 1, 0};
bool three[] = {1, 0, 0, 1, 1, 1, 1, 0};
bool four[] = {1, 1, 0, 0, 1, 1, 0, 0};
bool five[] = {1, 1, 0, 1, 1, 0, 1, 0};
bool six[] = {1, 1, 1, 1, 1, 0, 1, 0};
bool seven[] = {0, 0, 0, 0, 1, 1, 1, 0};
bool eight[] = {1, 1, 1, 1, 1, 1, 1, 0};
bool nine[] = {1, 1, 0, 1, 1, 1, 1, 0};

bool getLedValueFromNumber(int number, int ledNumber) {
  switch (number) {
    case 0:
      return zero[ledNumber];
    case 1:
      return one[ledNumber];
    case 2:
      return two[ledNumber];
    case 3:
      return three[ledNumber];
    case 4:
      return four[ledNumber];
    case 5:
      return five[ledNumber];
    case 6:
      return six[ledNumber];
    case 7:
      return seven[ledNumber];
    case 8:
      return eight[ledNumber];
    case 9:
      return nine[ledNumber];
  }
}

void writePPADigits(int bombe, int dist, bool mis, bool misp, bool mag, bool magp, bool par) { // , bool evfLed, bool testLed
  for (uint8_t i=0; i<8; i++) {
    byte valToWrite = 0b0000000000000000;
    bitWrite(valToWrite, 0, getLedValueFromNumber(bombe/10, i));
    bitWrite(valToWrite, 1, getLedValueFromNumber(bombe%10, i));
    bitWrite(valToWrite, 2, getLedValueFromNumber(dist/10, i));
    bitWrite(valToWrite, 3, getLedValueFromNumber(dist%10, i));
    if (i == 0) {
      bitWrite(valToWrite, 4, misp);
    }
    if (i == 1) {
      bitWrite(valToWrite, 4, mis);
    }
    if (i == 2) {
      bitWrite(valToWrite, 4, magp);
    }
    if (i == 3) {
      bitWrite(valToWrite, 4, mag);
    }
    if (i == 4) {
      bitWrite(valToWrite, 4, par);
    }

    matrix.displaybuffer[i] = valToWrite;
    delay(10);
  }
  matrix.writeDisplay();
}

void incrementPPABombeNumber() {
  // if dcs bios is not connected
  if (!isDCSinmission) {
    if (bombeNumber == maxBombeNumber) {
      bombeNumber = 0;
    } else {
      bombeNumber++;
    }
  }
}

void decrementPPABombeNumber() {
  // if dcs bios is not connected
  if (!isDCSinmission) {
    if (bombeNumber == 0) {
      bombeNumber = maxBombeNumber;
    } else {
      bombeNumber--;
    }
  }
}

void incrementPPABombeMeters() {
  // if dcs bios is not connected
  if (!isDCSinmission) {
    if (bombeMeters == maxBombeMeters) {
      bombeMeters = 0;
    } else {
      bombeMeters++;
    }
  }
}

void decrementPPABombeMeters() {
  // if dcs bios is not connected
  if (!isDCSinmission) {
    if (bombeMeters == 0) {
      bombeMeters = maxBombeMeters;
    } else {
      bombeMeters--;
    }
  }
}
