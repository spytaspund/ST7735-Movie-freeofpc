#include "Adafruit_GFX.h"
#include "Adafruit_ST7735.h"
#include <FS.h>
#include <SD.h>
#include "Adafruit_Keypad.h"

#define TFT_CS 5
#define TFT_RST 4
#define TFT_DC 2
#define SD_CS 22

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
uint16_t picture[120][160];

char keys[4][4] = { { '5', 'E', '1', 'P' },    //Play, pAuse, List, Stop
                    { 'D', 'O', 'U', 'A' },    //xz, Up, xz, xz
                    { '6', 'W', '2', 'L' },    //East(left), Ok, West(right), xz
                    { '7', '4', '3', 'S' } };  //xz, Down, xz, xz
byte rowPins[4] = { 27, 14, 12, 13 };          //R(1, 2, 3, 4)
byte colPins[4] = { 26, 25, 33, 32 };          //C(1, 2, 3, 4)

Adafruit_Keypad kp = Adafruit_Keypad(makeKeymap(keys), rowPins, colPins, 4, 4);

String menuFiles[60];
int menuIDs[60];
int menuCursor = 24;

uint32_t d_timer = 0;
#define d_PERIOD 320

void setup(void) {
  SD.begin(SD_CS);
  kp.begin();
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  drawHelloMsg();
  Serial.println();
  Serial.println("Enter \"list\", \"play\", \"pause\", or \"stop\" for control");
  Serial.println();
}

void loop() {
  kp.tick();
  keypadEvent e = kp.read();
  static File file;
  static bool play = false;
  static bool pause = false;
  static bool list = false;
  if (kp.isPressed('L')) {
    listDir(file, play, pause);
    list = true;
  }

  if (millis() - d_timer >= d_PERIOD) {
    if (kp.isPressed('D')) {
      if (list) {
        menuCursor += 9;
        tft.fillRect(0, 24, 5, 104, ST7735_BLACK);
      }
    }
    if (kp.isPressed('U')) {
      if (list) {
        menuCursor -= 9;
        tft.fillRect(0, 24, 5, 104, ST7735_BLACK);
      }
    }
    if (kp.isPressed('P')) {
      String filename = menuFiles[(menuCursor - 25) / 9];
      if (!pause) {
        filename.trim();
        if (filename.indexOf('.') != -1) {
          file = SD.open(filename);
          tft.fillScreen(ST7735_BLACK);
        }
      }
      play = true;
      pause = false;
      list = false;
      tft.setRotation(2);
    }
    if (kp.isPressed('A')) {
      if (play) {
        play = false;
        pause = true;
        tft.fillRect(0, 0, 24, 24, ST7735_BLACK);
        tft.fillRect(4, 4, 18, 6, ST7735_WHITE);
        tft.fillRect(4, 14, 18, 6, ST7735_WHITE);
      }
    }
    if (kp.isPressed('S')) {
      play = false;
      list = false;
      pause = false;
      file.close();
      drawHelloMsg();
    }
    do {
      d_timer += d_PERIOD;
      if (d_timer < d_PERIOD) break;
    } while (d_timer < millis() - d_PERIOD);
  }

  if (list) {
    if (menuCursor >= 114) {
      menuCursor = 114;
    }
    if (menuCursor < 24) {
      menuCursor = 24;
    }
    updateSelection(menuCursor);
  }
  if (play) {
    if (!list) {
      file.read((uint8_t*)picture, 120 * 160 * 2);
      tft.fillImage(picture, 0, 4, 160, 120);
      if (!file.available()) {
        play = false;
        pause = false;
        file.close();
      }
    }
  }
}

void listDir(File file, bool ply, bool pas) {
  tft.setRotation(3);
  tft.fillScreen(ST7735_BLACK);
  if (ply || pas) {
    ply = false;
    pas = false;
    file.close();
    listDir(file, ply, pas);
  }
  tft.printString(2, 2, "esp32 $ ls", ST7735_WHITE, ST7735_BLACK, 2, 2);
  tft.drawFastHLine(5, 20, 150, ST7735_WHITE);
  tft.setCursor(0, 24);
  File root = SD.open("/");
  file = root.openNextFile();
  int fileCounter = 0;
  while (file) {
    if (file.isDirectory()) {
      if (String(file.name()).length() > 26) {
        String locname = String(file.name()).substring(0, 11) + "..." + String(file.name()).substring(String(file.name()).length() - 11, String(file.name()).length()) + "\n";
        tft.tprint(locname, ST7735_BLUE, ST7735_BLACK, 1, 1);
      } else {
        tft.tprint(String(file.name()) + "\n", ST7735_BLUE, ST7735_BLACK, 1, 1);
      }
    } else {
      char filesize[16];
      menuFiles[fileCounter] = String(file.name());
      formatBytes(file.size(), filesize);
      if (String(file.name()).length() > 12) {
        String locname = String(file.name()).substring(0, 6) + "..." + String(file.name()).substring(String(file.name()).length() - 6, String(file.name()).length());
        tft.tprint(locname + " | " + String(filesize) + "\n", ST7735_WHITE, ST7735_BLACK, 1, 1);
      } else {
        tft.tprint(String(file.name()) + " | " + String(filesize) + "\n", ST7735_WHITE, ST7735_BLACK, 1, 1);
      }
      menuIDs[fileCounter] = tft.getY() - 8;
      ++fileCounter;
    }
    file = root.openNextFile();
  }
  tft.tprint(" That's all for now. u_u", ST7735_GRAY, ST7735_BLACK, 1, 1);
  //tft.printString(0, 110, menuFiles[0] + " | " + String(menuIDs[0]), ST7735_WHITE, ST7735_BLACK, 1, 1);
  tft.fillRect(0, 24, 5, 104, ST7735_BLACK);
}

void updateSelection(int mcur) {
  //don't ask me why tf i am using this giant function to draw char
  uint32_t curoff_timer = 0;
  int curoff_PERIOD = 200;
  if (millis() - curoff_timer >= curoff_PERIOD) {
    tft.drawChar(0, mcur, '>', ST7735_RED, ST7735_BLACK, 1, 1);
    do {
      curoff_timer += curoff_PERIOD;
      if (curoff_timer < curoff_PERIOD) break;
    } while (curoff_timer < millis() - curoff_PERIOD);
  }
}

void drawHelloMsg() {
  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(3);
  tft.printString(44, 56, "Hello!", 0xFFFF, 0x0000, 2, 2);
  tft.printString(0, 120, "  Play  Pause  List  Stop", ST7735_WHITE, ST7735_BLACK, 1, 1);
}

void formatBytes(uint64_t bytes, char* buf) {
  const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };

  int suffixIndex = 0;
  double val = bytes;

  while (val >= 1024 && suffixIndex < 4) {
    suffixIndex++;
    val /= 1024;
  }

  sprintf(buf, "%.2f %s", val, suffixes[suffixIndex]);
}