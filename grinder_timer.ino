#include <JC_Button.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>

#include "memory.h"
#include "rotary_encoder.h"
#include "voltage.h"

#define ENCODER_A       14   // Must be an interrupt pin for sufficient reliability
#define ENCODER_B       12   // Must be an interrupt pin for sufficient reliability
#define ENCODER_BUTTON  13
#define LONG_PRESS_MS   2000
#define BUS_CLOCK_SPEED 400000
#define PERIPHERY_PIN   16
#define RELAY_PIN       15

#define SINGLE_DOSE_ADDRESS  0
#define DOUBLE_DOSE_ADDRESS  4
#define STATISTICS_ADDRESS   8

#define OLED         U8G2_SH1106_128X64_NONAME_F_HW_I2C
#define SMALL_FONT   u8g2_font_8x13B_tf // XE font group
#define LARGE_FONT   u8g2_font_fur30_tn // Free universal group

OLED          display(U8G2_R0, U8X8_PIN_NONE);
Button        pushButton(ENCODER_BUTTON);

// TODO:
// calculation for text centering
// sleep after period of inactivity
// read voltage during startup, show warning if low

enum MenuItem {
  SingleDose,
  DoubleDose,
  Statistics,
  Battery
};

enum Screen {
  MenuScreen,
  SettingsScreen,
  Timer,
  StatisticsScreen,
  BatteryScreen
};

long           encoderPosition = 0;
unsigned long  oneDoseTimeout  = 0;
unsigned long  twoDoseTimeout  = 0;
unsigned long  grindedDosesCount = 0;
Screen         currentScreen   = MenuScreen;
MenuItem       menu[]          = {SingleDose, DoubleDose, Statistics, Battery};
MenuItem*      selectedItem    = menu;
short          menuLength      = sizeof(menu) / sizeof(*menu);
bool           ignoreNextPush  = false;

void drawMessage (const char* message, uint8_t row = 0) {
  display.firstPage();
  do {
    display.setFont(SMALL_FONT);
    display.drawStr(0, 15 * (row + 1), message);
  } while ( display.nextPage() );
}

void showTime (unsigned long milliseconds, const char * title) {
  unsigned int seconds = milliseconds / 1000;
  byte secondDecimals = (milliseconds % 1000) / 100;

  String padding = seconds < 10 ? " " : "";
  display.firstPage();
  do {
    display.setFont(SMALL_FONT);
    display.drawStr(0, 15, title);
    display.setFont(LARGE_FONT);
    display.drawStr(25, 55, (padding + String(seconds) + ":" + String(secondDecimals)).c_str());
  } while ( display.nextPage() );
}

const char * getMenuItemString (MenuItem menuItem) {
  switch (menuItem) {
    case SingleDose:
      return "Single dose";
    case DoubleDose:
      return "Double dose";
    case Statistics:
      return "Statistics";
    case Battery:
      return "Battery";
  }
}

void showStatistics () {
  currentScreen = StatisticsScreen;
  String padding = grindedDosesCount < 10 ? " " : "";
  display.firstPage();
  do {
    display.setFont(SMALL_FONT);
    display.drawStr(0, 15, "Coffees grinded:");
    display.setFont(LARGE_FONT);
    display.drawStr(30, 55, (padding + String(grindedDosesCount)).c_str());
  } while ( display.nextPage() );
}

void showBattery () {
  currentScreen = BatteryScreen;
  float voltage = readVoltage();
  display.firstPage();
  do {
    display.setFont(SMALL_FONT);
    display.drawStr(0, 15, "Voltage:");
    display.setFont(LARGE_FONT);
    display.drawStr(30, 55, String(voltage, 1).c_str());
  } while ( display.nextPage() );
}

void showMenu () {
  currentScreen = MenuScreen;
  display.firstPage();
  do {
    display.setFont(SMALL_FONT);
    for (short i = 0; i < menuLength; i++) {
      String selectionIndicator = *selectedItem == menu[i] ? "> " : "  ";
      String lineText = selectionIndicator + getMenuItemString(menu[i]);
      display.drawStr(0, 15 * (i + 1), lineText.c_str());
    }
  } while ( display.nextPage() );
}

void setup () {
  #ifdef DEBUG
    Serial.begin(115200);
  #endif

  // Turn off the WiFi as it's not needed now
  WiFi.disconnect();
  WiFi.forceSleepBegin();

  pinMode(ENCODER_BUTTON, INPUT);
  pinMode(RELAY_PIN,      OUTPUT);
  pinMode(PERIPHERY_PIN,  OUTPUT);

  digitalWrite(RELAY_PIN,     LOW);
  digitalWrite(PERIPHERY_PIN, LOW);

  initiateMemory();
  initRotaryEncoder(ENCODER_A, ENCODER_B);

  display.begin();
  display.setBusClock(BUS_CLOCK_SPEED);

  //  writeToMemory(SINGLE_DOSE_ADDRESS, 0);
  //  writeToMemory(DOUBLE_DOSE_ADDRESS, 0);
  //  writeToMemory(STATISTICS_ADDRESS, 0);
  oneDoseTimeout = readFromMemory(SINGLE_DOSE_ADDRESS);
  twoDoseTimeout = readFromMemory(DOUBLE_DOSE_ADDRESS);
  grindedDosesCount = readFromMemory(STATISTICS_ADDRESS);

  showMenu();
}

void changeTimeout(unsigned long * timeoutPtr, bool up, const char * title) {
  if (up) {
    *timeoutPtr += 100;
  } else if (*timeoutPtr >= 100) {
    *timeoutPtr -= 100;
  }
  showTime(*timeoutPtr, title);
}

void handleRotation (bool clockwise) {
  switch (currentScreen) {
    case MenuScreen:
      if (clockwise) {
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
        clockwise,
        getMenuItemString(*selectedItem)
      );
      break;
  }
}

void handleButtonPush () {
  if (ignoreNextPush) {
    ignoreNextPush = false;
    return;
  }

  switch (currentScreen) {
    case MenuScreen: {
      if (*selectedItem == SingleDose || *selectedItem == DoubleDose) {
        runTimer(*selectedItem);
        break;
      }
      if (*selectedItem == Statistics) {
        showStatistics();
        break;
      }
      if (*selectedItem == Battery) {
        showBattery();
        break;
      }
    }
    case StatisticsScreen: {
      showMenu();
      break;
    }
    case SettingsScreen: {
      bool singleDose = *selectedItem == SingleDose;
      writeToMemory(
        singleDose ? SINGLE_DOSE_ADDRESS : DOUBLE_DOSE_ADDRESS,
        singleDose ? oneDoseTimeout : twoDoseTimeout
      );
      showMenu();
      break;
    }
    case BatteryScreen: {
      showMenu();
      break;
    }
  }
}

void handleLongButtonPush () {
  switch (currentScreen) {
    case MenuScreen:
      if (*selectedItem == SingleDose || *selectedItem == DoubleDose) {
        currentScreen = SettingsScreen;
        drawMessage(getMenuItemString(*selectedItem));
        showTime(*selectedItem == SingleDose ? oneDoseTimeout : twoDoseTimeout, getMenuItemString(*selectedItem));
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
  long newPosition = readRotaryEncoder();
  if (newPosition != encoderPosition) {
    handleRotation(newPosition - encoderPosition < 0);
    encoderPosition = newPosition;
  }

  pushButton.read();
  if (pushButton.wasReleased()) {
    handleButtonPush();
  } else if (pushButton.pressedFor(LONG_PRESS_MS)) {
    handleLongButtonPush();
  }
}

void sleep () {
  digitalWrite(PERIPHERY_PIN, HIGH);
  ESP.deepSleep(0);
}

void runTimer (MenuItem selectedItem) {
  currentScreen = Timer;
  unsigned long startTime = millis();
  unsigned long millisecondsFromStart = 0;
  bool stopped = false;

  digitalWrite(RELAY_PIN, HIGH);

  unsigned long timeout = selectedItem == SingleDose ? oneDoseTimeout : twoDoseTimeout;

  while (millisecondsFromStart <= timeout) {
    pushButton.read();
    if (pushButton.wasReleased()) {
      stopped = true;
      break;
    }

    millisecondsFromStart = millis() - startTime;
    showTime(millisecondsFromStart, "Grinding...");
    yield();
  }
  digitalWrite(RELAY_PIN, LOW);

  if (stopped) {
    showTime(millisecondsFromStart, "Timer stopped...");
  } else {
    showTime(millisecondsFromStart, "Done, enjoy!");
    writeToMemory(
      STATISTICS_ADDRESS,
      grindedDosesCount += (selectedItem == SingleDose ? 1 : 2)
    );
  }
  delay(2000);
  showMenu();
}
