// LCD Library
#include <LiquidCrystal.h>

// Libraries for the DS18B20 Temperature Sensor
#include <OneWire.h>
#include <DallasTemperature.h>

// So we can save and retrieve settings
#include <EEPROM.h>

// EEPROM addresses for persisted data
const int SpAddress = 0;

// Display Variables and constants
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
const int analogInPin = A0; 
int sensorValue = 0; 

double Setpoint;
double Input;
float increment = 0.1;

// define the degree symbol
byte degree[8] = {
  B00110, 
  B01001, 
  B01001, 
  B00110, 
  B00000,
  B00000, 
  B00000, 
  B00000 
};

// Define the registered symbol (R)
byte registeredChar[8] = {
 B11111,
 B10001,
 B10101,
 B10001,
 B10011,
 B10101,
 B11111,
 B00000
};



// ************************************************
// Pin definitions
// ************************************************

// Output Relay
#define RelayPin 52

// One-Wire Temperature Sensor
// (Use GPIO pins for power/ground to simplify the wiring)
#define ONE_WIRE_BUS 22
#define ONE_WIRE_PWR 24
#define ONE_WIRE_GND 26


// window temperature
double window = 0.3; 

unsigned long lastInput = 0;
unsigned long lastDriveOutput = millis();

const int logInterval = 1000; // log every 10 seconds
long lastLogTime = 0;

// ************************************************
// States for state machine
// ************************************************
enum operatingState { OFF = 0, SETP, RUN, TUNE_P, TUNE_I, TUNE_D, AUTO };
enum buttonState { BUTTON_UP, BUTTON_DOWN, BUTTON_LEFT, BUTTON_RIGHT, BUTTON_SHIFT, BUTTON_NONE };
buttonState buttonState = BUTTON_NONE;
operatingState opState = OFF;

// ************************************************
// Sensor Variables and constants
// ************************************************

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress tempSensor;

// ************************************************
// Setup and diSplay initial screen
// ************************************************
void setup() {
  Serial.begin(9600);

  // Initialize Relay Control:

  pinMode(RelayPin, OUTPUT);    // Output mode to drive relay
  digitalWrite(RelayPin, HIGH);  // make sure it is off to start

  // Set up Ground & Power for the sensor from GPIO pins

  pinMode(ONE_WIRE_GND, OUTPUT);
  digitalWrite(ONE_WIRE_GND, LOW);

  pinMode(ONE_WIRE_PWR, OUTPUT);
  digitalWrite(ONE_WIRE_PWR, HIGH);

  // Initialize LCD DiSplay 

  lcd.begin(16, 2);
  lcd.createChar(1, degree); // create degree symbol from the binary
  lcd.createChar(2, registeredChar); // create registered symbol from the binary

  // Start up the DS18B20 One Wire Temperature Sensor

  sensors.begin();
  if(!sensors.getAddress(tempSensor, 0)) {
    //lcd.setCursor(0, 1);
    //lcd.print(F("Sensor Error"));
  }
  sensors.setResolution(tempSensor, 12);
  sensors.setWaitForConversion(false);

  // delay(3000);  // Splash screen

  // Initialize the PID and related variables
  LoadParameters();

}



// ************************************************
// Main Control Loop
//
// All state changes pass through here
// ************************************************
void loop() {
  lcd.clear();
  switch (opState) {
  case OFF:
    Off();
    break;
  case RUN:
    Run();
    break;
  }
}

// ************************************************
// Initial State - press RIGHT to enter setpoint
// ************************************************
void Off() {
  // Make sure it is off
  digitalWrite(RelayPin, HIGH);  
  
  // Start an asynchronous temperature reading
  sensors.requestTemperatures();
  
  // Prepare for the transition
  lcd.setCursor(2, 0);
  lcd.print(F("Sous Vide! "));
  lcd.write(2);
  delay(3000);

  opState = RUN;
}

// ************************************************
//
//
//*************************************************
void readButtons() {
  sensorValue = analogRead(analogInPin);
  if(sensorValue < 1000 ||  sensorValue > 1100) {
    delay(250);
    setPressedButton(sensorValue);
    lastInput = millis();
  }
}

void setPressedButton(int sensor) {
  if(sensor > 400 && sensor < 500) {
    buttonState = BUTTON_LEFT;
    return;
  }

  if(sensor > 600 && sensor < 750) {
    // SELECT BUTTON
    buttonState = BUTTON_SHIFT;
    return;
  }

  if(sensor > 300 && sensor < 350) {
    buttonState = BUTTON_DOWN;
    return;
  }

  if(sensor > 100 && sensor < 150) {
    buttonState = BUTTON_UP;
    return;
  }

  if(sensor < 10) {
    buttonState = BUTTON_RIGHT;
    return;
  }
}

void Run() {
   while(true) {
    readButtons();
    
    if(buttonState == BUTTON_UP) {
      Setpoint += increment;
      buttonState = BUTTON_NONE;
      delay(200);
     SaveParameters(); 

    } else if(buttonState == BUTTON_DOWN) {
      Setpoint -= increment;
      buttonState = BUTTON_NONE;
      delay(200);
      SaveParameters();
    } else if(buttonState == BUTTON_SHIFT) {
      if(increment == 0.1) {
        increment = 1.0;  
      } else if (increment == 1.0) {
        increment = 10.0;
      } else if (increment == 10.0) {
        increment = 0.1;
      } 
      buttonState = BUTTON_NONE;
    }
    lcd.setCursor(0,0);
    lcd.print(F("Sp:    "));
    lcd.print(Setpoint);
    lcd.print(F(" "));
    lcd.write(1);
    lcd.print(F("C"));
    
    DoControl();

    lcd.setCursor(0,1);
    lcd.print(F("Temp:  "));
    lcd.print(Input);
    lcd.print(F(" "));
    lcd.write(1);
    lcd.print(F("C"));
    
    DriveOutput();
    
    delay(100);
  }
}

void DoControl() {
    Input = sensors.getTempC(tempSensor);
    sensors.requestTemperatures();
}

void DriveOutput() {
  if((Input < (Setpoint + 0.1) && (millis() - lastDriveOutput < 3000))
    || (Input < Setpoint - 1)) {
    digitalWrite(RelayPin, LOW);
  } else if(digitalRead(RelayPin) == LOW) {
    digitalWrite(RelayPin, HIGH);
    delay(3000);
    lastDriveOutput = millis();
  } else {
    lastDriveOutput = millis();
  }
}

// ************************************************
// Save any parameter changes to EEPROM
// ************************************************
void SaveParameters() {
  if(Setpoint != EEPROM_readDouble(SpAddress)) {
    EEPROM_writeDouble(SpAddress, Setpoint);
  }
}

// ************************************************
// Load parameters from EEPROM
// ************************************************
void LoadParameters() {
  // Load from EEPROM
  Setpoint = EEPROM_readDouble(SpAddress);

  // Use defaults if EEPROM values are invalid
  if(isnan(Setpoint)) {
    Setpoint = 80;
  }
}

// ************************************************
// Write floating point values to EEPROM
// ************************************************
void EEPROM_writeDouble(int address, double value) {
  byte* p = (byte*)(void*)&value;
  for (int i = 0; i < sizeof(value); i++) {
    EEPROM.write(address++, *p++);
  }
}

// ************************************************
// Read floating point values from EEPROM
// ************************************************
double EEPROM_readDouble(int address) {
  double value = 0.0;
  byte* p = (byte*)(void*)&value;
  for (int i = 0; i < sizeof(value); i++) {
    *p++ = EEPROM.read(address++);
  }
  return value;
}
