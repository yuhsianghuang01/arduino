#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// GC9A01A 連接定義
#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// 圖片接收緩衝區
#define BUFFER_SIZE 512
uint8_t rxBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// 圖片參數
uint16_t imageWidth = 240;
uint16_t imageHeight = 240;
uint16_t totalPixels = imageWidth * imageHeight;
uint32_t pixelsReceived = 0;

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);
  
  Serial.println("Ready to receive image");
}

void loop() {
  if (Serial.available()) {
    receiveImageData();
  }
}

void receiveImageData() {
  while (Serial.available() && bufferIndex < BUFFER_SIZE) {
    rxBuffer[bufferIndex++] = Serial.read();
  }
  
  // 當緩衝區滿了或接收完畢時處理資料
  if (bufferIndex >= BUFFER_SIZE || pixelsReceived + bufferIndex/2 >= totalPixels) {
    processPixelData();
    bufferIndex = 0;
  }
}

void processPixelData() {
  // 每2個bytes組成一個16位RGB565像素
  for (int i = 0; i < bufferIndex - 1; i += 2) {
    uint16_t pixel = (rxBuffer[i] << 8) | rxBuffer[i + 1];
    
    // 計算像素座標
    uint16_t x = pixelsReceived % imageWidth;
    uint16_t y = pixelsReceived / imageWidth;
    
    tft.drawPixel(x, y, pixel);
    pixelsReceived++;
    
    if (pixelsReceived >= totalPixels) {
      Serial.println("Image received completely");
      pixelsReceived = 0;
      break;
    }
  }
}
