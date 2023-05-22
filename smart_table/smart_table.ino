/*
  Smart-Table software
  Software to control the smart software table

  Hardware:
    M5StackFire
    RFID RC522
    SK6812 LED strip
    HUB
    Ultrasonic sensor


  Software references:
  https://github.com/m5stack/M5Stack/blob/master/examples/Basics/Speaker/Speaker.ino
  https://github.com/m5stack/M5Stack/blob/master/examples/Unit/RFID_RC522/RFID_RC522.ino  
  https://github.com/m5stack/M5Stack/blob/master/examples/Unit/RGB_LED_SK6812/display_rainbow/display_rainbow.ino
  https://github.com/m5stack/M5Unit-Sonic/blob/master/examples/Unit_SonicI2C_M5Core2/Unit_SonicI2C_M5Core2.ino

  May 2023
*/

#include <M5Stack.h>
#include <FastLED.h>
#include <MFRC522_I2C.h>
#include <Unit_Sonic.h>

#define DELAY 200

#define NUM_LEDS 30
#define DATA_PIN 17
#define BRIGHTNESS 50

byte currColor[] = {0, 0, 0};
byte systemColor[] = { 0, 0, 0};
CRGB leds[NUM_LEDS];
MFRC522_I2C mfrc522(0x28, 0);

#define WAIT_SHORT 3*1000
#define WAIT_LONG 60*1000

long transCount = 0;
long transTotal = WAIT_SHORT/DELAY;

String TAG_ID[] = {"e566ed3c", "6868ed3c", "1465ed3c", "a768ed3c", "0167ed3c"};

#define TAG_COUNT 5

#define DIST_THRESHOLD 800

SONIC_I2C distSensor;

/*
  System status sheet:
  0 available green default status
  1 occupied/reserved red
  2 request waiter
  3 request bill yellow
  4 no disturb white
  5 client at the table long transition blue
  9 fallback status black/off
*/

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.lcd.setTextSize(2);
  M5.Lcd.println("Smart table ver.0.1");
  
  FastLED.addLeds<WS2811,DATA_PIN,GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  M5.Speaker.setVolume(1);
  M5.Speaker.update();
  
  Wire.begin();
  mfrc522.PCD_Init();
  M5.Lcd.setCursor(0, 80);
  M5.Lcd.println("RFID UID:");

  distSensor.begin();
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.println("Ultrasonic Distance:");
}

void loop() {
  M5.update();
  static byte systemStatus = 9;
  static byte newStatus = 0;

  static float newDistance = 0;
  static int distCount = 0;
  
  newDistance = distSensor.getDistance();
  
  M5.Lcd.fillRect(0, 40, 320, 20, BLACK);
  M5.Lcd.setCursor(0,40);
  M5.Lcd.println(newDistance);

  if (systemStatus == 0) {
    if (newDistance > DIST_THRESHOLD && distCount < 10) {
      distCount++;

    } else if ( distCount == 10) {
      newStatus = 5;
      distCount = 0;

    } else {
      distCount = 0;

    }
  }

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = readRFID(mfrc522);
    newStatus = getNewStatus(uid);
    M5.Lcd.fillRect(0, 100, 320, 20, BLACK);
    M5.Lcd.setCursor(0,100);
    M5.Lcd.println(uid);
    M5.Lcd.println(systemColor[0]);
  }

  if (systemStatus != newStatus) {
    systemStatus = updateStatus(systemStatus, newStatus);
  }

  transLEDS();

  delay(DELAY);
}

/**
  Updates system status
  @param currentStatus byte
  @param newStatus byte
  @return byte
*/
byte updateStatus(byte currentStatus, byte newStatus) {
  M5.Lcd.fillRect(0, 140, 320, 20, BLACK);
  M5.Lcd.setCursor(0,140);
  M5.Lcd.printf("System status: %i\n", newStatus);
  
  transCount = (transCount > transTotal)? transTotal: transCount;

  updateLEDS(leds, newStatus);

  M5.Lcd.fillRect(0, 220, 320, 20, BLACK);
  M5.Lcd.setCursor(0,220);
  M5.Lcd.printf("R: %i G: %i B: %i T: %i\n", currColor[0], currColor[1], currColor[2], transCount);

  transCount = 0;
  transTotal = WAIT_SHORT/DELAY;

  if (newStatus == 5) {
    transTotal = WAIT_LONG/DELAY;
  }

  M5.Speaker.tone(200, 100);

  return newStatus;
}

/**
  Updates current LED strip color
  
  @return void
*/
void transLEDS() {
  if (transCount <= transTotal) {
    byte red = (float) (transCount * abs(currColor[0] - systemColor[0]))/transTotal;
    byte green = (float) (transCount * abs(currColor[1] - systemColor[1]))/transTotal;
    byte blue = (float) (transCount * abs(currColor[2] - systemColor[2]))/transTotal;

    red = (systemColor[0] > currColor[0])? currColor[0] + red: currColor[0] - red;
    green = (systemColor[1] > currColor[1])? currColor[1] + green: currColor[1] - green;
    blue = (systemColor[2] > currColor[2])? currColor[2] + blue: currColor[2] - blue;
    
    fill_solid(leds, NUM_LEDS, {red, green , blue});
    FastLED.show();

    M5.Lcd.fillRect(0, 180, 320, 20, BLACK);
    M5.Lcd.setCursor(0,180);
    M5.Lcd.printf("R: %i G: %i B: %i\n", red, green, blue);
    M5.Lcd.println(transCount);
    transCount++;
  }
}

/**
  Defines target LED colors
  
  @param leds[] CRGB
  @param currentStatus byte
  @return void
*/
void updateLEDS(CRGB leds[], byte currentStatus) {
  byte red = (float) (transCount * abs(currColor[0] - systemColor[0]))/transTotal;
  byte green = (float) (transCount * abs(currColor[1] - systemColor[1]))/transTotal;
  byte blue = (float) (transCount * abs(currColor[2] - systemColor[2]))/transTotal;

  currColor[0] = (systemColor[0] > currColor[0])? currColor[0] + red: currColor[0] - red;
  currColor[1] = (systemColor[1] > currColor[1])? currColor[1] + green: currColor[1] - green;
  currColor[2] = (systemColor[2] > currColor[2])? currColor[2] + blue: currColor[2] - blue;

  if (currentStatus == 0) {
    M5.Lcd.fillRect(0, 160, 320, 40, GREEN);
    systemColor[0] = 0;
    systemColor[1] = 255;
    systemColor[2] = 0;
  } else if (currentStatus == 1) {
    M5.Lcd.fillRect(0, 160, 320, 40, RED);
    systemColor[0] = 255;
    systemColor[1] = 0;
    systemColor[2] = 0;
  } else if (currentStatus == 2 || currentStatus == 5) {
    M5.Lcd.fillRect(0, 160, 320, 40, BLUE);
    systemColor[0] = 0;
    systemColor[1] = 0;
    systemColor[2] = 255;
  } else if (currentStatus == 3) {
    M5.Lcd.fillRect(0, 160, 320, 40, YELLOW);
    systemColor[0] = 255;
    systemColor[1] = 255;
    systemColor[2] = 0;
  } else if (currentStatus == 4) {
    M5.Lcd.fillRect(0, 160, 320, 40, WHITE);
    systemColor[0] = 255;
    systemColor[1] = 255;
    systemColor[2] = 255;
  } else {
    M5.Lcd.fillRect(0, 160, 320, 40, BLACK);
    systemColor[0] = 0;
    systemColor[1] = 0;
    systemColor[2] = 0;
  }
}


/**
  Extracs the UID from the current RFID read
  
  @param rFIDReader MFRC522 instance
  @return UID from RFID read.
*/
String readRFID(MFRC522_I2C rFIDReader){
  String uid;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(mfrc522.uid.uidByte[i],HEX);
  }
  return uid;
}

/**
  Pairs a UID with a status from arrar
  
  @param uid string
  @return status code.
*/
byte getNewStatus(String uid) {
  for(byte i = 0; i < TAG_COUNT; i++) {
    if (strcmp(uid.c_str(), TAG_ID[i].c_str()) == 0){
      return i;
    }
  }
  return 9;
}