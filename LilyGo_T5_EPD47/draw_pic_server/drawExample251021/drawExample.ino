/**
 * EPD Web Controller via Wi-Fi AP
 * - Starts AP mode
 * - Shows IP:Port on EPD top-left
 * - Web interface to draw shapes or upload image
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "epd_driver.h"
#include "utilities.h"

// 嘗試包含字體檔案（如果可用）
// 常見的 EPD 字體檔案
#ifdef EPD_FONT_OPENSANS18B_H
#include "opensans18b.h"
#endif
#ifdef EPD_FONT_OPENSANS12B_H
#include "opensans12b.h"
#endif

// 如果沒有字體檔案，我們將使用圖形方式顯示數字和字母

// ===== WiFi AP Config =====
const char *ssid = "EPD-Controller";
const char *password = "12345678"; // 至少8碼

// ===== Web Server =====
WebServer server(80);

// ===== Framebuffer =====
uint8_t *framebuffer = NULL;
const int FB_SIZE = EPD_WIDTH * EPD_HEIGHT / 2; // 2-bit grayscale

// ===== 簡易 ASCII 字體 (5x7 點陣) =====
// 基本的 ASCII 字符點陣數據
const uint8_t ascii_font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 空格 (32)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // ! (33)
    {0x00, 0x07, 0x00, 0x07, 0x00}, // " (34)
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // # (35)
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $ (36)
    {0x23, 0x13, 0x08, 0x64, 0x62}, // % (37)
    {0x36, 0x49, 0x55, 0x22, 0x50}, // & (38)
    {0x00, 0x05, 0x03, 0x00, 0x00}, // ' (39)
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // ( (40)
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // ) (41)
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // * (42)
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // + (43)
    {0x00, 0x50, 0x30, 0x00, 0x00}, // , (44)
    {0x08, 0x08, 0x08, 0x08, 0x08}, // - (45)
    {0x00, 0x60, 0x60, 0x00, 0x00}, // . (46)
    {0x20, 0x10, 0x08, 0x04, 0x02}, // / (47)
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0 (48)
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1 (49)
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2 (50)
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3 (51)
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4 (52)
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5 (53)
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6 (54)
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7 (55)
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8 (56)
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9 (57)
    {0x00, 0x36, 0x36, 0x00, 0x00}, // : (58)
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ; (59)
    {0x00, 0x08, 0x14, 0x22, 0x41}, // < (60)
    {0x14, 0x14, 0x14, 0x14, 0x14}, // = (61)
    {0x41, 0x22, 0x14, 0x08, 0x00}, // > (62)
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ? (63)
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @ (64)
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A (65)
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B (66)
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C (67)
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D (68)
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E (69)
    {0x7F, 0x09, 0x09, 0x01, 0x01}, // F (70)
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G (71)
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H (72)
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I (73)
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J (74)
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K (75)
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L (76)
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M (77)
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N (78)
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O (79)
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P (80)
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q (81)
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R (82)
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S (83)
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T (84)
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U (85)
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V (86)
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W (87)
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X (88)
    {0x03, 0x04, 0x78, 0x04, 0x03}, // Y (89)
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z (90)
};

// ===== 繪製單個字符的函式 =====
void draw_char_5x7(int x, int y, char c, uint8_t color, uint8_t *fb)
{
  if (c < 32 || c > 90)
    return; // 只支援基本 ASCII

  int char_index = c - 32;

  for (int col = 0; col < 5; col++)
  {
    uint8_t column = ascii_font_5x7[char_index][col];
    for (int row = 0; row < 7; row++)
    {
      if (column & (1 << row))
      {
        int px = x + col;
        int py = y + row;
        if (px >= 0 && px < EPD_WIDTH && py >= 0 && py < EPD_HEIGHT)
        {
          // 使用 1x1 矩形來繪製像素點
          epd_fill_rect(px, py, 1, 1, color, fb);
        }
      }
    }
  }
}

// ===== 簡化的 IP 顯示函式（只顯示數字和點號）=====
void draw_ip_simple(int x, int y, const char *ip_str, uint8_t color, uint8_t *fb)
{
  int current_x = x;
  int char_spacing = 8; // 字符間距

  for (int i = 0; i < strlen(ip_str); i++)
  {
    char c = ip_str[i];

    // 只繪製數字、點號、冒號和部分字母
    if ((c >= '0' && c <= '9') || c == '.' || c == ':' || c == '/' ||
        (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
    {

      // 將小寫字母轉為大寫來使用我們的字體
      if (c >= 'a' && c <= 'z')
      {
        c = c - 'a' + 'A';
      }

      draw_char_5x7(current_x, y, c, color, fb);
      current_x += char_spacing;
    }
  }
}

// ===== Helper: Draw text on EPD with full control =====
void epd_draw_text_advanced(const char *text, int x, int y, uint8_t textColor, uint8_t bgColor, int fontSize, uint8_t *fb)
{
  if (fb && text && strlen(text) > 0)
  {
    int char_width = 6;   // 5像素寬度 + 1像素間距
    int char_height = 7;  // 7像素高度
    int scale = fontSize; // 縮放倍數 (1-5)

    int text_width = strlen(text) * char_width * scale;
    int text_height = char_height * scale;

    // 確保座標在有效範圍內
    if (x >= 0 && y >= 0 && x + text_width < EPD_WIDTH && y + text_height < EPD_HEIGHT)
    {
      // 繪製背景矩形
      if (bgColor != 255) // 255 表示透明背景
      {
        epd_fill_rect(x, y, text_width, text_height, bgColor, fb);
      }

      // 逐字符繪製
      for (int i = 0; i < strlen(text); i++)
      {
        char c = text[i];
        int char_x = x + i * char_width * scale;

        // 將小寫轉大寫
        if (c >= 'a' && c <= 'z')
        {
          c = c - 'a' + 'A';
        }

        if (scale == 1)
        {
          // 原始大小直接繪製
          draw_char_5x7(char_x, y, c, textColor, fb);
        }
        else
        {
          // 放大繪製 - 繪製多個相同字符來模擬放大
          for (int sx = 0; sx < scale; sx++)
          {
            for (int sy = 0; sy < scale; sy++)
            {
              draw_char_5x7(char_x + sx, y + sy, c, textColor, fb);
            }
          }
        }
      }

      Serial.printf("Drawing text '%s' at (%d,%d) size:%d textColor:%d bgColor:%d\n",
                    text, x, y, fontSize, textColor, bgColor);
    }
    else
    {
      Serial.printf("Text position out of bounds: (%d,%d) size:(%d,%d)\n", x, y, text_width, text_height);
    }
  }
}

// ===== Web Handlers =====

void handleRoot()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>EPD Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; padding: 10px; }
    button { margin:5px; padding:10px; font-size:16px; }
    .upload, .text-control { margin-top:20px; padding:15px; border:1px solid #ccc; border-radius:5px; }
    .form-row { margin:10px 0; }
    label { display:inline-block; width:80px; }
    input[type="text"], input[type="number"] { padding:5px; margin:5px; }
    input[type="range"] { width:200px; }
    .color-value { font-weight:bold; }
  </style>
</head>
<body>
  <h2>EPD Web Controller</h2>
  <button onclick="fetch('/clear')">Clear Screen</button>
  <button onclick="fetch('/draw/line')">Draw Random Line</button>
  <button onclick="fetch('/draw/rect')">Draw Random Rect</button>
  <button onclick="fetch('/draw/circle')">Draw Random Circle</button>

  <div class="text-control">
    <h3>文字控制</h3>
    <div class="form-row">
      <label>文字:</label>
      <input type="text" id="text" placeholder="輸入文字" value="Hello EPD">
    </div>
    <div class="form-row">
      <label>X座標:</label>
      <input type="number" id="x" min="0" max="%WIDTH%" value="50">
      <label>Y座標:</label>
      <input type="number" id="y" min="0" max="%HEIGHT%" value="50">
    </div>
    <div class="form-row">
      <label>字體大小:</label>
      <input type="range" id="fontSize" min="1" max="5" value="2" oninput="updateFontSizeValue()">
      <span class="color-value" id="fontSizeValue">2</span>
    </div>
    <div class="form-row">
      <label>文字顏色:</label>
      <input type="range" id="textColor" min="0" max="15" value="0" oninput="updateTextColorValue()">
      <span class="color-value" id="textColorValue">0 (黑色)</span>
    </div>
    <div class="form-row">
      <label>背景顏色:</label>
      <input type="range" id="bgColor" min="0" max="16" value="16" oninput="updateBgColorValue()">
      <span class="color-value" id="bgColorValue">透明</span>
    </div>
    
    <div class="form-row">
      <label>預設樣式:</label>
      <button onclick="setStyle('title')" style="margin:2px; padding:5px;">標題</button>
      <button onclick="setStyle('normal')" style="margin:2px; padding:5px;">正文</button>
      <button onclick="setStyle('highlight')" style="margin:2px; padding:5px;">強調</button>
      <button onclick="setStyle('subtitle')" style="margin:2px; padding:5px;">副標題</button>
    </div>
    
    <button onclick="drawText()">繪製文字</button>
    
    <h4>多行文字</h4>
    <div class="form-row">
      <label>多行文字:</label>
      <textarea id="multiText" placeholder="輸入多行文字，用分號(;)分隔" rows="3" style="width:300px;">第一行文字;第二行文字;第三行文字</textarea>
    </div>
    <div class="form-row">
      <label>起始X:</label>
      <input type="number" id="startX" min="0" max="%WIDTH%" value="50">
      <label>起始Y:</label>
      <input type="number" id="startY" min="0" max="%HEIGHT%" value="100">
    </div>
    <div class="form-row">
      <label>行高:</label>
      <input type="number" id="lineHeight" min="10" max="100" value="25">
    </div>
    <button onclick="drawMultiText()">繪製多行文字</button>
  </div>

  <div class="upload">
    <h3>Upload 1-bit or 2-bit RAW Image</h3>
    <p>Size: %WIDTH%x%HEIGHT% (bytes: %BYTES%)</p>
    <form method="POST" action="/upload" enctype="multipart/form-data">
      <input type="file" name="image" accept=".bin,.raw">
      <input type="submit" value="Upload">
    </form>
  </div>

  <script>
    function updateTextColorValue() {
      const color = document.getElementById('textColor').value;
      const colorNames = ['黑色', '深灰', '中灰', '淺灰', '白色'];
      let colorName;
      if (color == 0) colorName = '黑色';
      else if (color <= 3) colorName = '深灰';
      else if (color <= 7) colorName = '中灰';
      else if (color <= 11) colorName = '淺灰';
      else colorName = '白色';
      
      document.getElementById('textColorValue').textContent = color + ' (' + colorName + ')';
    }
    
    function updateBgColorValue() {
      const color = document.getElementById('bgColor').value;
      let colorName;
      if (color == 16) {
        colorName = '透明';
      } else {
        if (color == 0) colorName = '黑色';
        else if (color <= 3) colorName = '深灰';
        else if (color <= 7) colorName = '中灰';
        else if (color <= 11) colorName = '淺灰';
        else colorName = '白色';
        colorName = color + ' (' + colorName + ')';
      }
      
      document.getElementById('bgColorValue').textContent = colorName;
    }
    
    function updateFontSizeValue() {
      const size = document.getElementById('fontSize').value;
      const sizeNames = ['', '極小', '小', '中', '大', '極大'];
      document.getElementById('fontSizeValue').textContent = size + ' (' + sizeNames[size] + ')';
    }
    
    function setStyle(styleName) {
      const styles = {
        'title': { fontSize: 4, textColor: 0, bgColor: 16 },      // 大字黑色，透明背景
        'normal': { fontSize: 2, textColor: 0, bgColor: 16 },     // 中字黑色，透明背景
        'highlight': { fontSize: 3, textColor: 15, bgColor: 0 },  // 大字白色，黑色背景
        'subtitle': { fontSize: 3, textColor: 5, bgColor: 16 }    // 大字灰色，透明背景
      };
      
      if (styles[styleName]) {
        document.getElementById('fontSize').value = styles[styleName].fontSize;
        document.getElementById('textColor').value = styles[styleName].textColor;
        document.getElementById('bgColor').value = styles[styleName].bgColor;
        
        // 更新顯示值
        updateFontSizeValue();
        updateTextColorValue();
        updateBgColorValue();
      }
    }
    
    function drawText() {
      const text = document.getElementById('text').value;
      const x = document.getElementById('x').value;
      const y = document.getElementById('y').value;
      const textColor = document.getElementById('textColor').value;
      const bgColor = document.getElementById('bgColor').value;
      const fontSize = document.getElementById('fontSize').value;
      
      if (!text.trim()) {
        alert('請輸入文字');
        return;
      }
      
      // 16 表示透明背景，轉換為 255
      const actualBgColor = bgColor == 16 ? 255 : bgColor;
      
      const url = '/draw/text?text=' + encodeURIComponent(text) + 
                  '&x=' + x + '&y=' + y + 
                  '&textColor=' + textColor + '&bgColor=' + actualBgColor + 
                  '&fontSize=' + fontSize;
      
      fetch(url)
        .then(response => response.text())
        .then(data => {
          console.log(data);
          
        })
        .catch(error => {
          console.error('Error:', error);
          alert('繪製失敗');
        });
    }
    
    function drawMultiText() {
      const texts = document.getElementById('multiText').value;
      const startX = document.getElementById('startX').value;
      const startY = document.getElementById('startY').value;
      const lineHeight = document.getElementById('lineHeight').value;
      const textColor = document.getElementById('textColor').value;
      const bgColor = document.getElementById('bgColor').value;
      const fontSize = document.getElementById('fontSize').value;
      
      if (!texts.trim()) {
        alert('請輸入多行文字');
        return;
      }
      
      // 16 表示透明背景，轉換為 255
      const actualBgColor = bgColor == 16 ? 255 : bgColor;
      
      const url = '/draw/multitext?texts=' + encodeURIComponent(texts) + 
                  '&startX=' + startX + '&startY=' + startY + 
                  '&lineHeight=' + lineHeight + 
                  '&textColor=' + textColor + '&bgColor=' + actualBgColor + 
                  '&fontSize=' + fontSize;
      
      fetch(url)
        .then(response => response.text())
        .then(data => {
          console.log(data);
          alert('多行文字已繪製: ' + data);
        })
        .catch(error => {
          console.error('Error:', error);
          alert('繪製失敗');
        });
    }
    
    // 初始化所有顯示值
    updateTextColorValue();
    updateBgColorValue();
    updateFontSizeValue();
  </script>
</body>
</html>
)rawliteral";

  html.replace("%WIDTH%", String(EPD_WIDTH));
  html.replace("%HEIGHT%", String(EPD_HEIGHT));
  html.replace("%BYTES%", String(FB_SIZE));

  server.send(200, "text/html", html);
}

void handleClear()
{
  if (framebuffer)
  {
    memset(framebuffer, 0xFF, FB_SIZE);
    epd_poweron();
    epd_clear();
    epd_poweroff();
  }
  server.send(200, "text/plain", "OK");
}

void handleDrawLine()
{
  if (framebuffer)
  {
    epd_poweron();
    epd_draw_hline(10, random(10, EPD_HEIGHT), EPD_WIDTH - 20, 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
  }
  server.send(200, "text/plain", "Line drawn");
}

void handleDrawRect()
{
  if (framebuffer)
  {
    epd_poweron();
    epd_draw_rect(10, random(10, EPD_HEIGHT), random(10, 60), random(10, 120), 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
  }
  server.send(200, "text/plain", "Rect drawn");
}

void handleDrawCircle()
{
  if (framebuffer)
  {
    epd_poweron();
    epd_draw_circle(random(10, EPD_WIDTH), random(10, EPD_HEIGHT), 20, 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
  }
  server.send(200, "text/plain", "Circle drawn");
}

void handleDrawText()
{
  if (!framebuffer)
  {
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  // 獲取參數
  String text = server.arg("text");
  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();
  int textColor = server.arg("textColor").toInt();
  int bgColor = server.arg("bgColor").toInt();
  int fontSize = server.arg("fontSize").toInt();

  // 驗證參數
  if (text.length() == 0)
  {
    server.send(400, "text/plain", "Text parameter required");
    return;
  }

  // 限制顏色值範圍 (0-15，0=黑色，15=白色)
  if (textColor < 0)
    textColor = 0;
  if (textColor > 15)
    textColor = 15;
  if (bgColor < 0)
    bgColor = 255; // 255 = 透明背景
  if (bgColor > 15 && bgColor != 255)
    bgColor = 15;

  // 限制字體大小範圍 (1-5)
  if (fontSize < 1)
    fontSize = 1;
  if (fontSize > 5)
    fontSize = 5;

  // 限制座標範圍
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= EPD_WIDTH)
    x = EPD_WIDTH - 1;
  if (y >= EPD_HEIGHT)
    y = EPD_HEIGHT - 1;

  epd_poweron();
  epd_draw_text_advanced(text.c_str(), x, y, textColor, bgColor, fontSize, framebuffer);
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  String response = "Text drawn: '" + text + "' at (" + String(x) + "," + String(y) +
                    ") size:" + String(fontSize) + " textColor:" + String(textColor) +
                    " bgColor:" + (bgColor == 255 ? "transparent" : String(bgColor));
  server.send(200, "text/plain", response);
}

void handleDrawMultiText()
{
  if (!framebuffer)
  {
    server.send(400, "text/plain", "Framebuffer not available");
    return;
  }

  // 獲取參數（用分號分隔多行文字）
  String texts = server.arg("texts");
  int startX = server.arg("startX").toInt();
  int startY = server.arg("startY").toInt();
  int lineHeight = server.arg("lineHeight").toInt();
  int textColor = server.arg("textColor").toInt();
  int bgColor = server.arg("bgColor").toInt();
  int fontSize = server.arg("fontSize").toInt();

  if (texts.length() == 0)
  {
    server.send(400, "text/plain", "Texts parameter required");
    return;
  }

  if (lineHeight <= 0)
    lineHeight = 20 + (fontSize * 5); // 根據字體大小調整預設行高

  // 限制顏色值範圍
  if (textColor < 0)
    textColor = 0;
  if (textColor > 15)
    textColor = 15;
  if (bgColor < 0)
    bgColor = 255; // 255 = 透明背景
  if (bgColor > 15 && bgColor != 255)
    bgColor = 15;

  // 限制字體大小範圍
  if (fontSize < 1)
    fontSize = 1;
  if (fontSize > 5)
    fontSize = 5;

  epd_poweron();

  // 分割文字並逐行繪製
  int currentY = startY;
  int lineCount = 0;
  int startIndex = 0;

  for (int i = 0; i <= texts.length(); i++)
  {
    if (i == texts.length() || texts[i] == ';')
    {
      if (i > startIndex)
      {
        String line = texts.substring(startIndex, i);
        line.trim();

        if (line.length() > 0 && currentY < EPD_HEIGHT)
        {
          epd_draw_text_advanced(line.c_str(), startX, currentY, textColor, bgColor, fontSize, framebuffer);
          currentY += lineHeight;
          lineCount++;
        }
      }
      startIndex = i + 1;
    }
  }

  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff();

  String response = "Multi-text drawn: " + String(lineCount) + " lines starting at (" +
                    String(startX) + "," + String(startY) + ") size:" + String(fontSize) +
                    " textColor:" + String(textColor) + " bgColor:" +
                    (bgColor == 255 ? "transparent" : String(bgColor));
  server.send(200, "text/plain", response);
}

void handleUpload()
{
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    Serial.printf("Upload: %s\n", upload.filename.c_str());
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (framebuffer && upload.currentSize <= FB_SIZE)
    {
      memcpy(framebuffer + upload.totalSize - upload.currentSize, upload.buf, upload.currentSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (framebuffer)
    {
      epd_poweron();
      epd_draw_grayscale_image(epd_full_screen(), framebuffer);
      epd_poweroff();
    }
    Serial.printf("Upload complete, size: %d\n", upload.totalSize);
  }
  server.send(200, "text/plain", "Upload complete");
}

void notFound()
{
  server.send(404, "text/plain", "Not found");
}

// ===== Setup =====
void setup()
{
  Serial.begin(115200);
  delay(1000);

  // 初始化 framebuffer
  framebuffer = (uint8_t *)ps_calloc(1, FB_SIZE);
  if (!framebuffer)
  {
    Serial.println("PSRAM alloc failed!");
    while (1)
      delay(100);
  }
  memset(framebuffer, 0xFF, FB_SIZE);

  // 初始化 EPD
  epd_init();
  epd_poweron();
  epd_clear();
  epd_poweroff();

  // 啟動 Wi-Fi AP
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  String ipStr = "http://" + IP.toString() + ":80";

  Serial.println("AP started");
  Serial.print("IP address: ");
  Serial.println(ipStr);

  // 在 EPD 左上角顯示 IP 地址
  epd_poweron();

  // 方法1: 使用簡化的 IP 顯示（推薦）
  String simpleIP = IP.toString() + ":80";
  draw_ip_simple(10, 10, simpleIP.c_str(), 0, framebuffer);

  // 在下方顯示 SSID
  draw_ip_simple(10, 25, ssid, 0, framebuffer);

  // 方法2: 如果上面的方法不行，使用大號矩形顯示
  // 用矩形繪製 IP 的每個數字（備用方案）
  /*
  String ipOnly = IP.toString();
  epd_draw_rect(10, 40, ipOnly.length() * 12, 16, 0, framebuffer);
  for (int i = 0; i < ipOnly.length(); i++) {
      if (ipOnly[i] == '.') {
          epd_fill_rect(22 + i * 12, 50, 2, 2, 0, framebuffer);
      } else if (ipOnly[i] >= '0' && ipOnly[i] <= '9') {
          // 用矩形表示數字
          int digit = ipOnly[i] - '0';
          for (int j = 0; j < digit % 4; j++) {
              epd_draw_hline(12 + i * 12, 42 + j * 3, 8, 0, framebuffer);
          }
      }
  }
  */

  // 更新 EPD 顯示
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff(); // 設定 Web Server 路由
  server.on("/", HTTP_GET, handleRoot);
  server.on("/clear", HTTP_GET, handleClear);
  server.on("/draw/line", HTTP_GET, handleDrawLine);
  server.on("/draw/rect", HTTP_GET, handleDrawRect);
  server.on("/draw/circle", HTTP_GET, handleDrawCircle);
  server.on("/draw/text", HTTP_GET, handleDrawText);
  server.on("/draw/multitext", HTTP_GET, handleDrawMultiText);
  server.on("/upload", HTTP_POST, []()
            { server.send(200); }, handleUpload);

  server.onNotFound(notFound);
  server.begin();
}

// ===== Loop =====
void loop()
{
  server.handleClient();
  delay(1); // 讓出 CPU
}