#include <SoftwareSerial.h>
#include <EEPROM.h>
#include "Adafruit_FONA.h"

//sms initialization
#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 4
char replybuffer[255];
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);

//bluetooth initialization
String string;
SoftwareSerial BTMode(10,11); // rx, tx

//commands and registered Number
String numList[9];
String commands[] = {"LGHT1_ON", "LGHT1_OFF",
                       "APP1_ON", "APP1_OFF",
                       "APP2_ON", "APP2_OFF"
                      };

int regNumSize = sizeof(numList) / sizeof(String);
int commandSize = sizeof(commands) / sizeof(String);

//additional
uint8_t smsn = 1;
int8_t smsnum;

//relay initialization
int relayPin1 = 12;
int relayPin2 = 13;

//potentiometer initialization
int potPin = A0;
int potValue = 0;

boolean isUseSMS = false;

void setup() {
  Serial.begin(9600);
  BTMode.begin(9600);
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  
  //FONA setup
  //Serial.println(F("FONA basic test"));
  //Serial.println(F("Initializing....(May take 3 seconds)"));
  //gsm begin
  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    //Serial.println(F("Couldn't find FONA"));
    while (1);
  }
  //Serial.println(F("FONA is OK"));

  //display registered number
  if(EEPROM.read(1021) == 0){
    switchAppliances("APP1_OFF");
  } else if(EEPROM.read(1021) == 1){
    switchAppliances("APP1_ON");
  }
  if(EEPROM.read(1022) == 0){
    switchAppliances("APP2_OFF");
  } else if(EEPROM.read(1022) == 1){
    switchAppliances("APP2_ON");
  }
  //reset();
  getNumber();
  fonaSS.write("AT+CNUM");
}

void loop() {
  potValue=analogRead(potPin);
  if(potValue >= 600 && potValue <= 1020){
    //bt
    if(EEPROM.read(1023) != 1){
      EEPROM.write(1023, 1);
      Serial.println(F("Bluetooth Mode Activated"));
    }
  } else if(potValue >= 5 && potValue <= 599){
    //sms
    if(EEPROM.read(1023) != 0){
      EEPROM.write(1023, 0);
      Serial.println(F("SMS Mode Activated"));
    }
  }
  
  if(EEPROM.read(1023) == 0){
    smsMode();
  } else {
    btMode();
  }
  //Serial.print("Potentiometer Value: "); Serial.println(potValue);
}

void smsMode(){
  isUseSMS = true;
  //read the number of SMS's!
  fonaSS.listen();
  smsnum = fona.getNumSMS();
  if (smsnum > 0) {
    checkPhoneNumber(smsnum);
  } else {
    smsn = 1;
  }
}

void btMode(){
  isUseSMS = false;
  BTMode.listen();
  if (BTMode.available()) {
    string = "";
    string = BTMode.readString();
    for (int i = 0; i < commandSize; i++) {
      if (string == commands[i]) {
        switchAppliances(string);
      }
    }
    if (string.startsWith("Create")) {
      String strNum = string.substring(21);
      char prompt[7];
      if(regNumber(strNum)){
        BTMode.write(strcpy(prompt, "save"));
      } else {
        BTMode.write(strcpy(prompt, "failed"));
      }
      //Serial.println("Registered Success!");
    } else if (string == "Request_Number"){
      String numListStr = "";
      for(int i=0;i<commandSize;i++){
        numListStr += numList[i];
      }
      //send data to bluetooth
      int strLen = numListStr.length() + 1;
      if(strLen == 1){
        BTMode.write("No Number");
      } else {
        char charNum[strLen];
        numListStr.toCharArray(charNum, strLen);
        BTMode.write(charNum);
      }
      getNumber();
    } else if (string == "Reset") {
      reset();
      for(int i=0;i<regNumSize;i++){
        numList[i] = "";
      }
    } else if(string == "Request SMS Mode" || string == "Request SMS ModeRequest SMS Mode"){
      Serial.println(F("SMS Mode activated"));
      EEPROM.write(1023, 0);
    }
  }
}

void checkPhoneNumber(int smsCount) {
  boolean match = false;
  if (! fona.getSMSSender(smsn, replybuffer, 250)) {
    Serial.println(F("Failed!"));
    smsn = 1;
  }
  String senderNum = replybuffer;
  for (int i = 0; i < regNumSize; i++) {
    if (senderNum == numList[i]) {
      //Serial.println("Sender Numbber and Registered Number Found!");
      match = true;
    }
  }
  
  if (match) {
    // Retrieve SMS value.
    uint16_t smslen;
    if (! fona.readSMS(smsn, replybuffer, 250, &smslen)) { // pass in buffer and max len!
      Serial.println(F("Error getting SMS Content"));
    }
    String simMessage = replybuffer;
    for (int i = 0; i < commandSize; i++) {
      if (simMessage == commands[i]) {
        switchAppliances(simMessage);
      }
    }
    if(simMessage == "Syncing"){
      switchAppliances("");
    } else if(simMessage == "Request BT Mode") {
      if (fona.deleteSMS(smsn)) {
        Serial.print(F("Deleted #")); Serial.println(smsn);
      }
      EEPROM.write(1023, 1);
    }
    if(smsn > 1){
      smsn++;
    }
  } else {
    // delete unknown sender sim message
    if (fona.deleteSMS(smsn)) {
      Serial.print(F("Deleted #")); Serial.println(smsn);
      smsn++;
    }
  }
}

void switchAppliances(String stat) {
  //on
  if (stat == "APP1_ON") {
    EEPROM.write(1021, 1);
    digitalWrite(relayPin1, LOW);
  } else if (stat == "APP2_ON") {
    EEPROM.write(1022, 1);
    digitalWrite(relayPin2, LOW);
  }
  
  //off
  if (stat == "APP1_OFF") {
    EEPROM.write(1021, 0);
    digitalWrite(relayPin1, HIGH);
  } else if (stat == "APP2_OFF") {
    EEPROM.write(1022, 0);
    digitalWrite(relayPin2, HIGH);
  }
  
  if(isUseSMS){
    // delete sim message
    if (fona.deleteSMS(smsn)) {
      Serial.print(F("Deleted #")); Serial.println(smsn);
      smsn++;
    }
    for(int i=0;i<regNumSize;i++){
      if (numList[i] != "") {
        char numChar[21];
        numList[i].toCharArray(numChar, 21);
        sendConfirmation(numChar);
      }
    }
  }
}

void sendConfirmation(char sendto[21]){
  // send an SMS!
  String message = "";
  if(EEPROM.read(1021) == 0){
    message += "\nOutlet 1 is now turned off";
  } else if(EEPROM.read(1021) == 1){
    message += "\nOutlet 1 is now turned on";
  }
  if(EEPROM.read(1022) == 0){
    message += "\nOutlet 2 is now turned off";
  } else if(EEPROM.read(1022) == 1){
    message += "\nOutlet 2 is now turned on";
  }
  //strcpy(text, message);
  int strLen = message.length() + 1;
  char text[strLen];
  message.toCharArray(text, strLen);
  fona.sendSMS(sendto, text);
  delay(3000);
}

void getNumber() {
  String strNum;
  for (int x = 0; x < 109; x++) {
    int eepromValue = EEPROM.read(x);
    if (eepromValue != 0) {
      strNum += String(EEPROM.read(x) - 48);
    }
  }
  int numCount = strNum.length() / 11;
  for (int i = 0; i < numCount; i++) {
    String newNum = strNum.substring(i*11, (i*11)+11);
    numList[i] = "+63" + newNum.substring(1);
  }
  /*for (int i = 0; i < commandSize; i++) {
    if (numList[i] != "") {
      //Serial.print("Registered "); Serial.print(i+1); Serial.print(" #");
      Serial.println(numList[i]);
    }
  }*/
}

boolean regNumber(String number) {
  boolean isSave = false;
  boolean isDone = false;
  for (int i = 0; i < 109; i++) {
    if (EEPROM.read(i) == 0) {
      for (int x = 0; x < number.length(); x++) {
        if(i > 109){
          isSave = false;
        } else {
          EEPROM.write(i, number.charAt(x));
          i++;
          isDone = true;
          isSave = true;
        }
      }
      if(isDone){
        break;
      }
    }
  }
  return isSave;
}

void reset() {
  for (int i = 0; i < 109; i++) {
    if (EEPROM.read(i) != 0) {
      EEPROM.write(i, 0);
    }
  }
}
