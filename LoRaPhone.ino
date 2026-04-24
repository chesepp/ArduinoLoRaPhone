#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RYLR_LoRaAT.h"

RYLR_LoRaAT rylr; 

int screenWidth = 128;
int screenHeight = 64;
int oledReset = -1;

#define SCREEN_ADDRESS 0x3C
#define CLK 3
#define DT 4
#define SW 5 //select character
#define SW2 6 //backspace
#define SW3 7 //switch keyboard page
#define SW4 8 //enter
#define SW5 9 //Lora send/recieve

Adafruit_SSD1306 oled(screenWidth, screenHeight, &Wire, oledReset);

int lastCLK;
int lastDT;
unsigned long lastDebounceTime = 0;
const int debounceDelay = 5;

// --- Button ---
bool lastButtonState = HIGH;
bool buttonPressed = false;
unsigned long lastButtonTime = 0;
const int buttonDebounce = 50;

int lastEncoded = 0;
long encoderValue = 0;
int cursorX = 0;
int cursorY = 0;

unsigned long lastFrame = 0;
const int frameDelay = 30;

const char keysPage0[7][6] = {
  {'A','B','C','D','E','F'},
  {'G','H','I','J','K','L'},
  {'M','N','O','P','Q','R'},
  {'S','T','U','V','W','X'},
  {'Y','Z','0','1','2','3'},
  {'4','5','6','7','8','9'},
  {'<','>','#',' ',' ',' '}
};

const char keysPage1[7][6] = {
  {'!','@','#','$','%','^'},
  {'&','*','(',')','-','+'},
  {'[',']','{','}',';',':'},
  {'\'','"','\\','/','|','~'},
  {',','.','?','`','_',' '},
  {' ',' ',' ',' ',' ',' '}
};
enum ScreenMode { KEYBOARD, TERMINAL };
ScreenMode currentScreen = KEYBOARD;
int currentPage = 0;

#define MAX_MSGS 6
String messages[MAX_MSGS];
int msgIndex = 0;

const char (*getKeys())[6] {
  if (currentPage == 0) return keysPage0;
  return keysPage1;
}

const int rows = 7;
const int cols = 6;

String inputText = "";

//LORA message form AT+SEND=<address>,<length>,<message>
void drawUI() {
  oled.clearDisplay();

  if (currentScreen == KEYBOARD) {
    drawKeyboard();
  } else {
    drawTerminal();
  }

  oled.display();
}

void drawKeyboard() {
  oled.setCursor(0, 0);
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.print(inputText);

  int startY = 16;
  const char (*keys)[6] = getKeys();

  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {

      int px = x * 18;
      int py = startY + y * 9;

      if (x == cursorX && y == cursorY) {
        oled.fillRect(px, py, 20, 10, WHITE);
        oled.setTextColor(BLACK);
      } else {
        oled.setTextColor(WHITE);
      }

      char c = keys[y][x];

      oled.setCursor(px + 2, py);
      oled.print(c == ' ' ? ' ' : c);
    }
  }
}

void drawTerminal() {
  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  oled.setCursor(0, 0);
  oled.print("LoRa Terminal");

  // show last messages
  for (int i = 0; i < MAX_MSGS; i++) {
    int idx = (msgIndex + i) % MAX_MSGS;

    oled.setCursor(0, 10 + i * 9);
    oled.print(messages[idx]);
  }
}
void handleButtons();
void handleEncoder();
void moveRight();
void moveLeft();


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial1.begin(115200);
  rylr.setSerial(&Serial1);
  // Basic setup (only needs to run once)
  rylr.setAddress(1);      // this device's address

  pinMode(CLK,INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW3, INPUT_PULLUP);
  pinMode(SW4, INPUT_PULLUP);
  pinMode(SW5, INPUT_PULLUP);
  lastCLK = digitalRead(CLK);
  lastDT = digitalRead(DT);
  oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  oled.clearDisplay();
}

void loop() {
  if (currentScreen == KEYBOARD) {
  handleEncoder();   // only move cursor in keyboard mode
  }

  handleButtons();     // buttons always work

  if (millis() - lastFrame > frameDelay) {
    drawUI();
    lastFrame = millis();
  }
  RYLR_LoRaAT_Message *message;

  if ((message = rylr.checkMessage())) {

    String msg = String(message->from_address) + ": " + String(message->data);

    Serial.println("RX: " + msg);

    // store message (scrolling buffer)
    messages[msgIndex] = msg;
    msgIndex = (msgIndex + 1) % MAX_MSGS;
  }
}

void handleEncoder() {
  int clkState = digitalRead(CLK);
  int dtState  = digitalRead(DT);

  // only act on CLK change
  if (clkState != lastCLK && clkState == LOW) {

    // direction check
    if (dtState != clkState) {
      moveRight();
      Serial.println("RIGHT");
    } else {
      moveLeft();
      Serial.println("LEFT");
    }
  }

  lastCLK = clkState;
  lastDT = dtState;
}
void moveRight() {
  cursorX++;
  if (cursorX >= cols) {
    cursorX = 0;
    cursorY++;
    if (cursorY >= rows) cursorY = 0;
  }
}

void moveLeft() {
  cursorX--;
  if (cursorX < 0) {
    cursorX = cols - 1;
    cursorY--;
    if (cursorY < 0) cursorY = rows - 1;
  }
}
void handleButtons() {

  static bool lastSW5 = HIGH;
  bool sw5 = digitalRead(SW5);

  static bool lastSW  = HIGH;
  static bool lastSW2 = HIGH;
  static bool lastSW3 = HIGH;
  static bool lastSW4 = HIGH;

  bool sw  = digitalRead(SW);
  bool sw2 = digitalRead(SW2);
  bool sw3 = digitalRead(SW3);
  bool sw4 = digitalRead(SW4);

  const char (*keys)[6] = getKeys();

  // --- SELECT CHARACTER (SW) ---
  if (lastSW == HIGH && sw == LOW) {
    char selected = keys[cursorY][cursorX];

    if (selected != ' ') {
      inputText += selected;
    }
  }

  // --- BACKSPACE (SW2) ---
  if (lastSW2 == HIGH && sw2 == LOW) {
    if (inputText.length() > 0) {
      inputText.remove(inputText.length() - 1);
    }
  }

  // --- SWITCH PAGE (SW3) ---
  if (lastSW3 == HIGH && sw3 == LOW) {
    currentPage = (currentPage + 1) % 2;

    // reset cursor so it doesn't land on empty space
    cursorX = 0;
    cursorY = 0;
  }

  // --- SEND MESSAGE (SW4) ---
  if (lastSW4 == HIGH && sw4 == LOW) {

    if (inputText.length() > 0) {

      Serial.println("Sending: " + inputText);

      // Send via RYLR_LoRaAT library
      const char* msg = inputText.c_str();
      rylr.startTxMessage();
      rylr.addTxData(msg);
      rylr.sendTxMessage(0);   // 0 = destination address

      inputText = "";
    }
  }
  static unsigned long lastToggleTime = 0;
  const int toggleDebounce = 200;

  // --- TOGGLE SCREEN (SW5) ---

  if (sw5 == HIGH && lastSW5 == LOW) {
    if (millis() - lastToggleTime > toggleDebounce) {
      currentScreen = (currentScreen == KEYBOARD) ? TERMINAL : KEYBOARD;
      lastToggleTime = millis();

      Serial.println("TOGGLED SCREEN"); // debug
    }
    drawUI();
  }

  // update states
  lastSW  = sw;
  lastSW2 = sw2;
  lastSW3 = sw3;
  lastSW4 = sw4;
  lastSW5 = sw5;
}


