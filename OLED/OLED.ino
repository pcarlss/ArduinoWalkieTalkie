#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RF24.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    4
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BATTERY_PIN A0
const float MAX_VOLTAGE = 3.5;
int batteryLevel = 100;
int channelNumber = 5;
String status = "IDLE";
bool radioConnected = true;

// Timing
unsigned long lastUptimeUpdate = 0;
unsigned long startMillis;
unsigned long lastChannelUpdate = 0;
const unsigned long channelCycleInterval = 2000;
unsigned long lastDisplayReset = 0;
const unsigned long displayResetInterval = 5000; // 5 seconds

// NRF24L01 Setup
RF24 radio(9, 10);

// -------- Booting Animation --------
void showBootAnimation() {
  display.clearDisplay();
  for (int i = 0; i < 3; i++) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 20);
    display.print("BOOTING");
    display.display();
    delay(300);
    display.clearDisplay();
    display.display();
    delay(200);
  }

  display.setTextSize(2);
  display.setCursor(15, 20);
  display.println("WALKIE");
  display.setCursor(30, 40);
  display.println("TALKIE");
  display.display();
  delay(1000);

  display.clearDisplay();
  display.drawRect(14, 30, 100, 10, SSD1306_WHITE);
  for (int i = 0; i <= 100; i += 5) {
    display.fillRect(15, 31, i, 8, SSD1306_WHITE);
    display.display();
    delay(50);
  }

  display.setTextSize(1);
  display.setCursor(40, 48);
  display.print("System Ready");
  display.display();
  delay(1000);
}

// -------- Battery Icon --------
void drawBatteryIcon(int x, int y, int level) {
  display.drawRect(x, y, 18, 8, SSD1306_WHITE);
  display.fillRect(x + 18, y + 2, 2, 4, SSD1306_WHITE);
  int fillWidth = map(level, 0, 100, 0, 16);
  display.fillRect(x + 1, y + 1, fillWidth, 6, SSD1306_WHITE);
}

// -------- Format Uptime --------
String formatUptime(unsigned long ms) {
  unsigned long totalSecs = ms / 1000;
  int hours = totalSecs / 3600;
  int minutes = (totalSecs % 3600) / 60;
  int seconds = totalSecs % 60;

  char buffer[9];
  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buffer);
}

// -------- Read Battery --------
int readBatteryPercentage() {
  int raw = analogRead(BATTERY_PIN);
  float voltage = (raw / 1023.0) * 5.0;
  if (voltage < 0.2) return 0;

  int percent = int((voltage / MAX_VOLTAGE) * 100);
  return constrain(percent, 0, 100);
}

// -------- Main UI --------
void showWalkieTalkieMenu() {
  display.clearDisplay();

  // Channel (top-left)
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("CH ");
  display.print(channelNumber);

  // Battery icon (top-right)
  drawBatteryIcon(104, 0, batteryLevel);

  // Uptime (top-center)
  String uptime = formatUptime(millis() - startMillis);
  int16_t ux, uy;
  uint16_t uw, uh;
  display.getTextBounds(uptime, 0, 0, &ux, &uy, &uw, &uh);
  display.setCursor((SCREEN_WIDTH - uw) / 2, 0);
  display.print(uptime);

  // Status / Warning / Radio Disconnect (center)
  display.setTextSize(2);
  String centerText;
  if (!radioConnected) {
    centerText = "NO RADIO";
  } else if (batteryLevel <= 20) {
    centerText = "LOW POWER";
  } else {
    centerText = status;
  }

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(centerText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 24);
  display.print(centerText);

  // Menu options (bottom)
  display.setTextSize(1);
  display.setCursor(10, 48);
  display.print("> Volume");

  display.setCursor(10, 56);
  display.print("  Scan");

  display.setCursor(70, 56);
  display.print("  Settings");

  display.display();
}

void reinitDisplay() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
}



// -------- Setup --------
void setup() {
  Serial.begin(9600);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  analogReference(DEFAULT);
  startMillis = millis();

  if (!radio.begin()) {
    radioConnected = false;
  } else {
    radioConnected = radio.isChipConnected();
    if (radioConnected) {
      radio.setChannel(125);
      radio.setPALevel(RF24_PA_HIGH);
      radio.openWritingPipe(0xF0F0F0F0E1LL);
      radio.openReadingPipe(1, 0xF0F0F0F0D2LL);
      radio.startListening();
    }
  }

  showBootAnimation();
  showWalkieTalkieMenu();
}

// -------- Loop --------
void loop() {
  unsigned long currentTime = millis();

  // Periodically reset the display every 5 seconds
  if (currentTime - lastDisplayReset >= displayResetInterval) {
    lastDisplayReset = currentTime;
    reinitDisplay(); // Reset the OLED display
    showWalkieTalkieMenu(); // Redraw menu after reset
  }

  // Read battery level from A0
  batteryLevel = readBatteryPercentage();
  Serial.print("Battery %: ");
  Serial.println(batteryLevel);

  // Cycle through channels
  if (currentTime - lastChannelUpdate >= channelCycleInterval) {
    lastChannelUpdate = currentTime;
    channelNumber++;
    if (channelNumber > 124) {
      channelNumber = 0;
    }
    showWalkieTalkieMenu(); // Update display
  }

  // Refresh every second
  if (currentTime - lastUptimeUpdate >= 1000) {
    lastUptimeUpdate = currentTime;
    showWalkieTalkieMenu();
  }
}
