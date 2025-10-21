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

// ===== WiFi AP Config =====
const char *ssid = "EPD-Controller";
const char *password = "12345678"; // 至少8碼

// ===== Web Server =====
WebServer server(80);

// ===== Framebuffer =====
uint8_t *framebuffer = NULL;
const int FB_SIZE = EPD_WIDTH * EPD_HEIGHT / 2; // 2-bit grayscale

// ===== Helper: Draw text on EPD with full control =====
void epd_draw_text_advanced(const char *text, int x, int y, uint8_t textColor, uint8_t bgColor, int fontSize, uint8_t *fb)
{
    // 使用 epd_driver 的文字繪製函式（如果可用）
    // 顏色值：0=黑色，15=白色，中間值為灰階
    if (fb && text && strlen(text) > 0)
    {
        // 根據字體大小計算尺寸
        int charWidth = 6 + (fontSize * 2);  // 基礎寬度 + 大小調整
        int charHeight = 8 + (fontSize * 4); // 基礎高度 + 大小調整
        int text_width = strlen(text) * charWidth;
        int text_height = charHeight;

        // 確保座標在有效範圍內
        if (x >= 0 && y >= 0 && x + text_width < EPD_WIDTH && y + text_height < EPD_HEIGHT)
        {
            // 繪製背景矩形
            if (bgColor != 255) // 255 表示透明背景
            {
                epd_fill_rect(x - 2, y - 2, text_width + 4, text_height + 4, bgColor, fb);
            }

            // 繪製文字邊框（模擬文字）
            epd_draw_rect(x, y, text_width, text_height, textColor, fb);

            // 根據字體大小繪製不同粗細的線條來模擬文字
            for (int i = 0; i < fontSize + 1; i++)
            {
                epd_draw_hline(x + 3, y + text_height / 3 + i, text_width - 6, textColor, fb);
                epd_draw_hline(x + 3, y + 2 * text_height / 3 + i, text_width - 6, textColor, fb);
            }

            // 繪製字符分隔線
            for (int i = 1; i < strlen(text); i++)
            {
                int sepX = x + i * charWidth;
                epd_draw_vline(sepX, y + text_height / 4, text_height / 2, textColor, fb);
            }

            // 如果您有實際的字體函式，請替換上述代碼為：
            // 例如：epd_write_string(font_array[fontSize], text, &x, &y, fb);
            // 或：epd_draw_string_with_font(x, y, text, textColor, bgColor, fontSize, fb);

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
          alert('文字已繪製: ' + data);
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
    // 使用我們的進階文字繪製功能顯示 IP
    // 參數：文字內容, x座標, y座標, 文字顏色(0=黑), 背景顏色(255=透明), 字體大小(2=中等)
    epd_draw_text_advanced(ipStr.c_str(), 10, 10, 0, 255, 2, framebuffer);

    // 在 IP 下方顯示 SSID 資訊
    String ssidInfo = "SSID: " + String(ssid);
    epd_draw_text_advanced(ssidInfo.c_str(), 10, 35, 0, 255, 1, framebuffer);

    // 更新 EPD 顯示
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();

    // 設定 Web Server 路由
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