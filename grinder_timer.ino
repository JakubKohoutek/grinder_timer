// This optional setting causes Encoder to use more optimized code,
// It must be defined before Encoder.h is included.
#define ENCODER_OPTIMIZE_INTERRUPTS

#include <JC_Button.h>
#include <U8g2lib.h>
#include <Encoder.h>

#define ENCODER_A       2   // Must be an interrupt pin for sufficient reliability
#define ENCODER_B       3   // Must be an interrupt pin for sufficient reliability
#define ENCODER_BUTTON  4
#define ENCODER_STEP    3
#define BUS_CLOCK_SPEED 400000
#define RELAY_PIN       7

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

long encoderPosition = 0;
unsigned long oneDoseTimeout = 0;

void drawMessage (const char* message, bool inverse = false, uint8_t row = 0) {
  if(inverse)
    display.inverse();
  display.setFont(u8x8_font_8x13B_1x2_r);
  uint8_t line = row * 2;
  display.clearLine(line);
  display.clearLine(line + 1);
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


  drawMessage("Jedna", false, 0);
  delay(500);
  drawMessage("Dva", false, 1);
  delay(500);
  drawMessage("Tri", false, 2);
  delay(500);
  drawMessage("Ctyri", false, 3);
  delay(500);
  display.clearDisplay();
}

void loop() {
    long newPosition = encoder.read();

    if (newPosition - encoderPosition > ENCODER_STEP) {
      encoderPosition = newPosition;
      if (oneDoseTimeout >=100)
        oneDoseTimeout -= 100;
      showTime(oneDoseTimeout);
    }

    if (newPosition - encoderPosition < -ENCODER_STEP) {
      encoderPosition = newPosition;
      oneDoseTimeout += 100;
      showTime(oneDoseTimeout);
    }

    pushButton.read();
    if (pushButton.wasPressed()) {
      runTimer();
    }
}


void runTimer () {
  drawMessage("Grinding...");
  unsigned long startTime = millis();
  unsigned long millisecondsFromStart = 0;

  digitalWrite(RELAY_PIN, LOW);

  while (millisecondsFromStart <= oneDoseTimeout) {
    pushButton.read();
    if (pushButton.wasPressed()) {
      drawMessage("Timer stopped", true);
      break;
    }

    millisecondsFromStart = millis() - startTime;
    showTime(millisecondsFromStart);
  }

  digitalWrite(RELAY_PIN, HIGH);
}
