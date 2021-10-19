// This optional setting causes Encoder to use more optimized code,
// It must be defined before Encoder.h is included.
#define ENCODER_OPTIMIZE_INTERRUPTS

#include <JC_Button.h>
#include <U8g2lib.h>
#include <Encoder.h>

#include "memory.h"

#define ENCODER_A       2   // Must be an interrupt pin for sufficient reliability
#define ENCODER_B       3   // Must be an interrupt pin for sufficient reliability
#define ENCODER_BUTTON  4
#define LONG_PRESS_MS   2000
#define ENCODER_STEP    3
#define BUS_CLOCK_SPEED 400000
#define RELAY_PIN       7

#define SINGLE_DOSE_ADDRESS  0
#define DOUBLE_DOSE_ADDRESS  4

// Display settings with buffer
// U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
// display.setFont(u8g2_font_helvR08_tr);

// Useful:
// u8x8.getRows();
// u8x8.getCols();
// u8x8.setPowerSave(0 | 1);
// display.setContrast(50);

U8X8_SH1106_128X64_NONAME_HW_I2C  display(U8X8_PIN_NONE);
Encoder                           encoder(ENCODER_A, ENCODER_B);
Button                            pushButton(ENCODER_BUTTON);

enum MenuItem {
  SingleDose,
  DoubleDose,
  Settings,
  Statistics
};

enum Screen {
  MenuScreen,
  SettingsScreen,
  Timer,
  StatisticsScreen
};

long           encoderPosition = 0;
unsigned long  oneDoseTimeout  = 0;
unsigned long  twoDoseTimeout  = 0;
Screen         currentScreen   = MenuScreen;
MenuItem       menu[]         = {SingleDose, DoubleDose, Statistics};
MenuItem*       selectedItem    = menu;
short          menuLength      = sizeof(menu)/sizeof(*menu);
bool           ignoreNextPush  = false;

void drawMessage (const char* message, bool inverse = false, uint8_t row = 0) {
  if(inverse)
    display.inverse();
  display.setFont(u8x8_font_8x13B_1x2_r);
  uint8_t line = row * 2;
//  display.clearLine(line);
//  display.clearLine(line + 1);
  display.drawString(0, line, message);
  if(inverse)
    display.noInverse();
}

void showTime (unsigned long milliseconds) {
  unsigned int seconds = milliseconds / 1000;
  byte secondDecimals = (milliseconds % 1000) / 100;

  String padding = seconds < 10 ? " " : "";
  display.setFont(u8x8_font_inr21_2x4_n);
  display.drawString(3, 3, (padding + String(seconds) + ":" + String(secondDecimals)).c_str());
}

const char * getMenuItemString (MenuItem menuItem) {
  switch(menuItem) {
    case SingleDose:
      return "Single dose";
    case DoubleDose:
      return "Double dose";
    case Settings:
      return "Settings";
    case Statistics:
      return "Statistics";
  }
}

void showMenu () {
  currentScreen = MenuScreen;
  for (short i; i < menuLength; i++) {
    String selectionIndicator = *selectedItem == menu[i] ? "> " : "  ";
    String lineText = selectionIndicator + getMenuItemString(menu[i]);
    drawMessage(lineText.c_str(), false, i);
  }
}

void setup () {
  pushButton.begin();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  display.begin();
  display.setBusClock(BUS_CLOCK_SPEED);
  display.setFont(u8x8_font_8x13B_1x2_r);
  display.drawString(1, 2, "May the coffee");
  display.drawString(1, 4, " be with you! ");
  delay(2000);

  display.clearDisplay();
//  writeToMemory(SINGLE_DOSE_ADDRESS, 0);
//  writeToMemory(DOUBLE_DOSE_ADDRESS, 0);
  oneDoseTimeout = readFromMemory(SINGLE_DOSE_ADDRESS);
  twoDoseTimeout = readFromMemory(DOUBLE_DOSE_ADDRESS);
  
  showMenu();
}

void changeTimeout(unsigned long * timeoutPtr, bool isIncrease) {
  if (isIncrease){
    *timeoutPtr += 100;
  } else if (*timeoutPtr >=100) {
    *timeoutPtr -= 100;
  }
  showTime(*timeoutPtr);
}

void handleRotation (bool clockwise) {
  switch(currentScreen) {
    case MenuScreen:
      if (clockwise){
        if (*selectedItem == menu[menuLength - 1]) {
           selectedItem = menu;
        } else {
           selectedItem++;
        }
      } else {
        if (*selectedItem == menu[0]) {
           selectedItem = &menu[menuLength - 1];
        } else {
           selectedItem--;
        }
      }
      showMenu();
      break;
    case SettingsScreen:
      changeTimeout(
        (*selectedItem == SingleDose) ? &oneDoseTimeout : &twoDoseTimeout,
        clockwise
      );
      break;
  }
}

void handleButtonPress () {
  switch(currentScreen) {
    case MenuScreen:
      String loadingBar = "";
      unsigned long timeSincePress = millis() - pushButton.lastChange();
      for (int i = 0; i < (timeSincePress / (LONG_PRESS_MS - 300)) * display.getCols(); i++) {
        loadingBar += " ";
        drawMessage(loadingBar.c_str(), true, 3);
      }
  }
}

void handleButtonPush () {
  if (ignoreNextPush) {
    ignoreNextPush = false;
    return;
  }

  switch(currentScreen) {
    case MenuScreen:
      if (*selectedItem == SingleDose || *selectedItem == DoubleDose){
        display.clear();
        runTimer(*selectedItem);
        break;
      }
      if (*selectedItem == Statistics) {
        // display.clear();
        // showStatistics();
        break;
      }
    case SettingsScreen:
      bool singleDose = *selectedItem == SingleDose;
      writeToMemory(
        singleDose ? SINGLE_DOSE_ADDRESS : DOUBLE_DOSE_ADDRESS,
        singleDose ? oneDoseTimeout : twoDoseTimeout
      );
      display.clear();
      showMenu();
      break;
  }
}

void handleLongButtonPush () {
  switch(currentScreen) {
    case MenuScreen:
      if (*selectedItem == SingleDose || *selectedItem == DoubleDose){
          currentScreen = SettingsScreen;
          display.clear();
          drawMessage(getMenuItemString(*selectedItem));
          showTime(*selectedItem == SingleDose ? oneDoseTimeout : twoDoseTimeout);
        break;
      }
      if (*selectedItem == Statistics) {
        // Reset?
        break;
      }
    default:
      return;
  }
  ignoreNextPush = true;
}

void loop() {
  long newPosition = encoder.read();
  if (newPosition - encoderPosition > ENCODER_STEP) {
    encoderPosition = newPosition;
    handleRotation(false);
  }

  if (newPosition - encoderPosition < -ENCODER_STEP) {
    encoderPosition = newPosition;
    handleRotation(true);
  }

  pushButton.read();
  if (pushButton.isPressed()) {
    handleButtonPress();
  }
  if (pushButton.wasReleased()) {
    handleButtonPush();
  } else if (pushButton.pressedFor(LONG_PRESS_MS)) {
    handleLongButtonPush();
  }
}

void runTimer (MenuItem selectedItem) {
  currentScreen = Timer;
  display.clear();
  drawMessage("Grinding...");
  unsigned long startTime = millis();
  unsigned long millisecondsFromStart = 0;
  bool stopped = false;

  digitalWrite(RELAY_PIN, LOW);

  unsigned long timeout = selectedItem == SingleDose ? oneDoseTimeout : twoDoseTimeout;

  while (millisecondsFromStart <= timeout) {
    pushButton.read();
    if (pushButton.wasReleased()) {
      stopped = true;
      break;
    }

    millisecondsFromStart = millis() - startTime;
    showTime(millisecondsFromStart);
  }
  digitalWrite(RELAY_PIN, HIGH);

  if (stopped) {
    drawMessage("Timer stopped", true);
  } else {
    drawMessage("Done, enjoy!");
  }
  delay(2000);
  display.clear();
  showMenu();
}
