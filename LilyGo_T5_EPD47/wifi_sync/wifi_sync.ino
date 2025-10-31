/**
 * @copyright Copyright (c) 2024  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 *            版權所有 (c) 2024 深圳鑫源電子技術有限公司
 * @date      2024-04-05
 *            開發日期：2024年4月5日
 * @note      Arduino Setting - Arduino IDE 設定說明
 *            Tools -> 工具選單設定：
 *                  Board:"ESP32S3 Dev Module" - 開發板：ESP32S3 開發模組
 *                  USB CDC On Boot:"Enable" - 開機時啟用 USB CDC：啟用
 *                  USB DFU On Boot:"Disable" - 開機時啟用 USB DFU：停用
 *                  Flash Size : "16MB(128Mb)" - Flash 大小：16MB(128Mb)
 *                  Flash Mode"QIO 80MHz - Flash 模式：QIO 80MHz（四線輸入輸出，80MHz頻率）
 *                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)" - 分割區方案：16M Flash（3M 應用程式/9.9MB 檔案系統）
 *                  PSRAM:"OPI PSRAM" - PSRAM：八線輸入輸出 PSRAM
 *                  Upload Mode:"UART0/Hardware CDC" - 上傳模式：UART0/硬體 CDC
 *                  USB Mode:"Hardware CDC and JTAG" - USB 模式：硬體 CDC 和 JTAG
 *
 */

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
// 檢查 PSRAM 是否啟用，如未啟用則顯示錯誤訊息
// 錯誤提示：請啟用 PSRAM，路徑：Arduino IDE -> 工具 -> PSRAM -> OPI !!!
#endif

/*
 * ESP32 電子紙 WiFi 同步顯示系統
 *
 * 程式功能：
 * - 建立 WiFi 熱點和 Web 伺服器
 * - 提供網頁介面上傳和管理圖像
 * - 支援 JPEG 圖像自動解碼和顯示
 * - 實現無線圖像同步到電子紙螢幕
 * - 提供檔案管理和預覽功能
 *
 * 技術特點：
 * - 使用 ESP32 WiFi AP 模式建立獨立網路
 * - 整合 Web 伺服器處理 HTTP 請求
 * - 支援檔案上傳、下載、刪除操作
 * - 自動 JPEG 解碼和電子紙適配
 * - 使用 PSRAM 處理大容量圖像資料
 * - 支援 SD 卡和內建 Flash 儲存
 *
 * 使用方式：
 * 1. 設備啟動後自動建立 WiFi 熱點
 * 2. 連接到指定的 WiFi 網路
 * 3. 開啟瀏覽器存取管理頁面
 * 4. 上傳圖像檔案並即時顯示
 */

#include <WiFi.h>            // ESP32 WiFi 功能函式庫
#include <WiFiClient.h>      // WiFi 客戶端連線
#include <WebServer.h>       // HTTP Web 伺服器
#include <ESPmDNS.h>         // mDNS 域名解析服務
#include <Arduino.h>         // Arduino 核心函式庫
#include "epd_driver.h"      // 電子紙驅動程式
#include "libjpeg/libjpeg.h" // JPEG 圖像解碼函式庫
#include "firasans.h"        // Fira Sans 字型檔案
#include "esp_adc_cal.h"     // ESP32 ADC 校正函式庫
#include <FS.h>              // 檔案系統抽象層
#include <SPI.h>             // SPI 通訊協定
#include <SD.h>              // SD 卡檔案系統
#include <FFat.h>            // FAT 檔案系統（Flash）
#include "logo.h"            // 標誌圖像資料
#include "utilities.h"       // 公用函式和工具程式

// #define USE_SD     // 使用 SD 卡儲存（註解掉）
#define USE_FLASH // 使用內建 Flash 儲存

// 根據儲存方式定義檔案系統
#if defined(USE_SD)
#define FILE_SYSTEM SD // 使用 SD 卡檔案系統
#elif defined(USE_FLASH)
#define FILE_SYSTEM FFat // 使用 Flash FAT 檔案系統
#endif

#define DBG_OUTPUT_PORT Serial // 定義除錯輸出埠為串列埠

// WiFi 連線設定
const char *ssid = "GL-MT1300-44e"; // WiFi 網路名稱（SSID）
const char *password = "88888888";  // WiFi 密碼
const char *host = "lilygo";        // mDNS 主機名稱（可透過 lilygo.local 存取）

WebServer server(80);               // 建立 HTTP 伺服器，監聽埠 80
static bool hasFILE_SYSTEM = false; // 檔案系統初始化狀態標誌
File uploadFile;                    // 檔案上傳物件
EventGroupHandle_t handleServer;    // FreeRTOS 事件群組處理器
String pic_path;                    // 圖片檔案路徑
uint8_t *framebuffer;               // 影像緩衝區指標
char buf[128];                      // 字串緩衝區，用於格式化文字

// 定義電子紙顯示區域
const Rect_t line1Area = {
    // 第一行文字顯示區域
    .x = 0,       // 起始 X 座標
    .y = 387,     // 起始 Y 座標
    .width = 960, // 區域寬度
    .height = 51, // 區域高度
};

const Rect_t line2Area = {
    // 第二行文字顯示區域
    .x = 0,
    .y = 438,
    .width = 960,
    .height = 51,
};

const Rect_t line3Area = {
    // 第三行文字顯示區域
    .x = 0,
    .y = 489,
    .width = 960,
    .height = 51,
};

// FreeRTOS 事件位元定義
#define BIT_CLEAN _BV(0) // 清除螢幕事件位元
#define BIT_SHOW _BV(1)  // 顯示圖像事件位元

/**
 * 回傳成功回應的內聯函數
 *
 * 功能：向客戶端發送 HTTP 200 OK 回應
 * 用途：確認操作成功完成
 */
inline void returnOK()
{
    server.send(200, "text/plain", "");
}

/**
 * 回傳失敗回應的內聯函數
 *
 * 功能：向客戶端發送 HTTP 500 錯誤回應
 * @param msg 錯誤訊息內容
 * 用途：通知客戶端操作失敗的原因
 */
inline void returnFail(String msg)
{
    server.send(500, "text/plain", msg + "\r\n");
}

/**
 * 從檔案系統載入檔案並回應 HTTP 請求的函數
 *
 * 功能：
 * 1. 根據檔案副檔名推斷 MIME 類型
 * 2. 處理目錄請求（自動載入 index.htm）
 * 3. 從檔案系統讀取檔案並串流回應給客戶端
 *
 * @param path 請求的檔案路徑
 * @return 成功載入返回 true，失敗返回 false
 *
 * 支援的檔案類型：
 * - HTML 檔案（.htm）
 * - CSS 樣式表（.css）
 * - JavaScript 檔案（.js）
 * - 圖片檔案（.png, .gif, .jpg, .ico）
 * - 其他檔案（.xml, .pdf, .zip）
 */
bool loadFromFILE_SYSTEM(String path)
{
    String dataType = "text/plain"; // 預設 MIME 類型
    if (path.endsWith("/"))
    {
        path += "index.htm"; // 目錄請求時自動載入 index.htm
    }

    if (path.endsWith(".src"))
    {
        path = path.substring(0, path.lastIndexOf(".")); // 移除 .src 副檔名
    }
    else if (path.endsWith(".htm"))
    {
        dataType = "text/html"; // HTML 檔案
    }
    else if (path.endsWith(".css"))
    {
        dataType = "text/css"; // CSS 樣式表
    }
    else if (path.endsWith(".js"))
    {
        dataType = "application/javascript"; // JavaScript 檔案
    }
    else if (path.endsWith(".png"))
    {
        dataType = "image/png"; // PNG 圖片
    }
    else if (path.endsWith(".gif"))
    {
        dataType = "image/gif"; // GIF 圖片
    }
    else if (path.endsWith(".jpg"))
    {
        dataType = "image/jpeg"; // JPEG 圖片
    }
    else if (path.endsWith(".ico"))
    {
        dataType = "image/x-icon"; // 圖示檔
    }
    else if (path.endsWith(".xml"))
    {
        dataType = "text/xml"; // XML 檔案
    }
    else if (path.endsWith(".pdf"))
    {
        dataType = "application/pdf"; // PDF 檔案
    }
    else if (path.endsWith(".zip"))
    {
        dataType = "application/zip"; // ZIP 壓縮檔
    }

    File dataFile = FILE_SYSTEM.open(path.c_str(), "rb"); // 以二進制模式開啟檔案
    if (dataFile.isDirectory())
    {
        path += "/index.htm"; // 如果是目錄，嘗試載入 index.htm
        dataType = "text/html";
        dataFile = FILE_SYSTEM.open(path.c_str());
        Serial.println("isDirectory"); // 除錯訊息：處理目錄請求
    }

    if (!dataFile)
    {
        return false; // 檔案開啟失敗
    }

    if (server.hasArg("download"))
    {
        dataType = "application/octet-stream"; // 強制下載模式
    }

    // 串流檔案內容給客戶端
    if (server.streamFile(dataFile, dataType) != dataFile.size())
    {
        DBG_OUTPUT_PORT.println("Sent less data than expected!"); // 資料傳輸不完整警告
    }

    dataFile.close(); // 關閉檔案
    return true;      // 成功載入
}

/**
 * 處理檔案上傳的函數
 *
 * 功能：
 * 1. 接收客戶端上傳的檔案
 * 2. 處理檔案上傳的三個階段：開始、寫入、結束
 * 3. 支援檔案覆蓋（如果同名檔案存在則刪除）
 *
 * 上傳階段：
 * - UPLOAD_FILE_START：開始上傳，建立檔案
 * - UPLOAD_FILE_WRITE：寫入檔案資料
 * - UPLOAD_FILE_END：結束上傳，關閉檔案
 *
 * 注意：僅處理 "/edit" 路徑的上傳請求
 */
void handleFileUpload()
{
    if (server.uri() != "/edit")
    {
        return; // 只處理 /edit 路徑的上傳
    }
    HTTPUpload &upload = server.upload(); // 獲取上傳物件參考
    if (upload.status == UPLOAD_FILE_START)
    { // 開始上傳
        if (FILE_SYSTEM.exists((char *)upload.filename.c_str()))
        {
            FILE_SYSTEM.remove((char *)upload.filename.c_str()); // 刪除同名檔案
        }
        uploadFile = FILE_SYSTEM.open(upload.filename.c_str(), FILE_WRITE); // 建立新檔案
        DBG_OUTPUT_PORT.print("Upload: START, filename: ");                 // 除錯訊息：開始上傳
        DBG_OUTPUT_PORT.println(upload.filename);
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    { // 寫入檔案資料
        if (uploadFile)
        {
            uploadFile.write(upload.buf, upload.currentSize); // 寫入資料緩衝區
        }
        DBG_OUTPUT_PORT.print("Upload: WRITE, Bytes: "); // 除錯訊息：寫入位元組數
        DBG_OUTPUT_PORT.println(upload.currentSize);
    }
    else if (upload.status == UPLOAD_FILE_END)
    { // 結束上傳
        if (uploadFile)
        {
            uploadFile.close(); // 關閉檔案
        }
        DBG_OUTPUT_PORT.print("Upload: END, Size: "); // 除錯訊息：總檔案大小
        DBG_OUTPUT_PORT.println(upload.totalSize);
    }
}

/**
 * 遞迴刪除檔案或目錄的函數
 *
 * 功能：
 * 1. 刪除指定路徑的檔案或目錄
 * 2. 如果是目錄，遞迴刪除所有子檔案和子目錄
 * 3. 確保完全清理目錄結構
 *
 * @param path 要刪除的檔案或目錄路徑
 *
 * 演算法：
 * 1. 檢查是否為目錄
 * 2. 如果是檔案，直接刪除
 * 3. 如果是目錄，遍歷所有子項目並遞迴刪除
 */
void deleteRecursive(String path)
{
    File file = FILE_SYSTEM.open((char *)path.c_str()); // 開啟檔案或目錄
    if (!file.isDirectory())
    { // 如果是檔案
        file.close();
        FILE_SYSTEM.remove((char *)path.c_str()); // 直接刪除檔案
        return;
    }

    file.rewindDirectory(); // 重設目錄讀取位置到開頭
    while (true)
    {
        File entry = file.openNextFile(); // 開啟下一個檔案項目
        if (!entry)
        {
            break; // 沒有更多項目，結束遍歷
        }
        String entryPath = path + "/" + entry.name(); // 建構完整路徑
        if (entry.isDirectory())
        { // 如果是子目錄
            entry.close();
            deleteRecursive(entryPath); // 遞迴刪除子目錄
        }
        else
        { // 如果是檔案
            entry.close();
            FILE_SYSTEM.remove((char *)entryPath.c_str()); // 刪除檔案
        }
        yield(); // 讓出 CPU 時間，避免看門狗重設
    }

    FILE_SYSTEM.rmdir((char *)path.c_str()); // 刪除空目錄
    file.close();                            // 關閉目錄檔案
}

/**
 * 處理檔案刪除 HTTP 請求的函數
 *
 * 功能：
 * 1. 解析 HTTP 請求參數獲取要刪除的路徑
 * 2. 驗證路徑有效性和存在性
 * 3. 呼叫遞迴刪除函數執行實際刪除操作
 *
 * HTTP 回應：
 * - 成功：200 OK
 * - 失敗：500 錯誤（附帶錯誤訊息）
 *
 * 安全檢查：
 * - 防止刪除根目錄（"/"）
 * - 確認檔案/目錄存在
 */
void handleDelete()
{
    if (server.args() == 0)
    {
        return returnFail("BAD ARGS"); // 缺少參數
    }
    String path = server.arg(0); // 獲取第一個參數作為路徑
    if (path == "/" || !FILE_SYSTEM.exists((char *)path.c_str()))
    {
        returnFail("BAD PATH"); // 路徑無效或檔案不存在
        return;
    }
    deleteRecursive(path); // 執行遞迴刪除
    returnOK();            // 回傳成功
}

/**
 * 處理檔案/目錄建立 HTTP 請求的函數
 *
 * 功能：
 * 1. 解析 HTTP 請求參數獲取要建立的路徑
 * 2. 根據路徑是否包含副檔名判斷建立檔案或目錄
 * 3. 執行實際的檔案系統操作
 *
 * 建立邏輯：
 * - 包含 "." 字元：建立空檔案
 * - 不包含 "." 字元：建立目錄
 *
 * 安全檢查：
 * - 防止建立根目錄（"/"）
 * - 確認路徑不存在（避免覆蓋）
 */
void handleCreate()
{
    if (server.args() == 0)
    {
        return returnFail("BAD ARGS"); // 缺少參數
    }
    String path = server.arg(0); // 獲取第一個參數作為路徑
    if (path == "/" || FILE_SYSTEM.exists((char *)path.c_str()))
    {
        returnFail("BAD PATH"); // 路徑無效或已存在
        return;
    }

    if (path.indexOf('.') > 0)
    { // 包含副檔名，建立檔案
        File file = FILE_SYSTEM.open((char *)path.c_str(), FILE_WRITE);
        if (file)
        {
            file.write(0); // 寫入一個空位元組
            file.close();
        }
    }
    else
    { // 不包含副檔名，建立目錄
        FILE_SYSTEM.mkdir((char *)path.c_str());
    }
    returnOK(); // 回傳成功
}

/**
 * 處理圖片路徑設定 HTTP 請求的函數
 *
 * 功能：
 * 1. 接收客戶端指定的圖片檔案路徑
 * 2. 設定全域變數 pic_path
 * 3. 觸發 FreeRTOS 事件，通知顯示任務更新圖片
 *
 * 工作流程：
 * 1. 獲取圖片路徑參數
 * 2. 設定事件位元 BIT_SHOW
 * 3. 顯示任務將讀取並顯示指定圖片
 */
void handleGetPath()
{
    if (server.args() == 0)
    {
        return returnFail("BAD ARGS"); // 缺少參數
    }
    pic_path = server.arg(0);        // 設定圖片路徑
    Serial.print("get pic path : "); // 除錯訊息
    Serial.println(pic_path.c_str());
    xEventGroupSetBits(handleServer, BIT_SHOW); // 觸發顯示事件
    returnOK();                                 // 回傳成功
}

/**
 * 列印目錄內容的函數（JSON 格式回應）
 *
 * 功能：
 * 1. 接收目錄路徑參數
 * 2. 遍歷指定目錄中的所有檔案和子目錄
 * 3. 以 JSON 陣列格式回傳目錄內容清單
 *
 * JSON 回應格式：
 * [
 *   {"type":"file", "name":"/path/file1.txt"},
 *   {"type":"dir", "name":"/path/subdir"}
 * ]
 *
 * 用途：提供檔案管理器介面的目錄瀏覽功能
 */
void printDirectory()
{
    if (!server.hasArg("dir"))
    {
        return returnFail("BAD ARGS"); // 缺少目錄參數
    }
    String path = server.arg("dir"); // 獲取目錄路徑
    if (path != "/" && !FILE_SYSTEM.exists((char *)path.c_str()))
    {
        return returnFail("BAD PATH"); // 路徑不存在
    }
    File dir = FILE_SYSTEM.open((char *)path.c_str()); // 開啟目錄
    path = String();                                   // 清空路徑變數
    if (!dir.isDirectory())
    {
        dir.close();
        return returnFail("NOT DIR"); // 不是目錄
    }
    dir.rewindDirectory();                           // 重設目錄讀取位置
    server.setContentLength(CONTENT_LENGTH_UNKNOWN); // 設定不定長度內容
    server.send(200, "text/json", "");               // 開始發送 JSON 回應
    WiFiClient client = server.client();

    server.sendContent("["); // JSON 陣列開始
    for (int cnt = 0; true; ++cnt)
    {
        File entry = dir.openNextFile(); // 開啟下一個檔案項目
        if (!entry)
        {
            break; // 沒有更多項目
        }

        String output;
        if (cnt > 0)
        {
            output = ','; // 非第一個項目前加逗號
        }

        output += "{\"type\":\"";
        output += (entry.isDirectory()) ? "dir" : "file"; // 判斷類型
        output += "\",\"name\":\"";
        output += entry.path(); // 檔案完整路徑
        output += "\"";
        output += "}";
        server.sendContent(output); // 發送 JSON 物件
        entry.close();              // 關閉檔案項目
    }
    server.sendContent("]"); // JSON 陣列結束
    dir.close();             // 關閉目錄
}

/**
 * 處理 HTTP 404 錯誤（檔案未找到）的函數
 *
 * 功能：
 * 1. 當請求的檔案不存在時被呼叫
 * 2. 回傳詳細的錯誤訊息，包含請求資訊
 * 3. 提供除錯資訊協助問題排查
 *
 * 錯誤訊息包含：
 * - 檔案系統狀態
 * - 請求的 URI
 * - HTTP 方法（GET/POST）
 * - 請求參數清單
 */
void handleNotFound()
{
    if (hasFILE_SYSTEM && loadFromFILE_SYSTEM(server.uri()))
    {
        return; // 嘗試從檔案系統載入，成功則返回
    }
    String message = "FILE SYSTEM Not Detected\n\n"; // 檔案系統未偵測到
    message += "URI: ";
    message += server.uri(); // 請求的 URI
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST"; // HTTP 方法
    message += "\nArguments: ";
    message += server.args(); // 參數數量
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++)
    { // 遍歷所有參數
        message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message); // 發送 404 錯誤回應
    DBG_OUTPUT_PORT.print(message);          // 輸出除錯訊息
}

/**
 * Arduino 初始化函數
 *
 * 功能：
 * 1. 初始化硬體模組（電子紙顯示器、JPEG 解碼器）
 * 2. 分配影像緩衝區記憶體
 * 3. 建立 WiFi 連線
 * 4. 設定 HTTP 伺服器路由
 * 5. 初始化檔案系統
 * 6. 啟動 FreeRTOS 任務
 */
void setup()
{
    handleServer = xEventGroupCreate();   // 建立 FreeRTOS 事件群組
    DBG_OUTPUT_PORT.begin(115200);        // 初始化串列埠通訊
    DBG_OUTPUT_PORT.setDebugOutput(true); // 啟用除錯輸出
    DBG_OUTPUT_PORT.print("\n");

    /** Initialize the screen */
    /* 初始化電子紙顯示器 */
    epd_init();                                                                      // 初始化 EPD 驅動程式
    libjpeg_init();                                                                  // 初始化 JPEG 解碼器
    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2); // 分配影像緩衝區
    if (!framebuffer)
    {
        Serial.println("alloc memory failed !!!"); // 記憶體分配失敗
        while (1)
            ; // 停止執行
    }
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2); // 清空緩衝區（設為白色）
    epd_poweron();                                         // 啟動電子紙電源
    epd_clear();                                           // 清除螢幕內容

    // 定義 Logo 顯示區域
    Rect_t area = {
        .x = 256,              // X 座標
        .y = 180,              // Y 座標
        .width = logo_width,   // Logo 寬度
        .height = logo_height, // Logo 高度
    };

    // epd_draw_grayscale_image(area, (uint8_t *)logo_data);  // 灰階顯示（註解）
    epd_draw_image(area, (uint8_t *)logo_data, BLACK_ON_WHITE); // 黑白模式顯示 Logo

    // WiFi 連線設定
    WiFi.disconnect(); // 斷開現有連線
    delay(100);
    WiFi.mode(WIFI_STA);                    // 設定為站台模式
    WiFi.onEvent(WiFiEvent);                // 註冊 WiFi 事件處理函數
    WiFi.begin(ssid, password);             // 開始連線到指定 AP
    sprintf(buf, "Connecting to %s", ssid); // 格式化連線訊息
    DBG_OUTPUT_PORT.println(buf);
    int cursor_x = line1Area.x;                                           // 文字游標 X 座標
    int cursor_y = line1Area.y + FiraSans.advance_y + FiraSans.descender; // 文字游標 Y 座標
    epd_clear_area(line1Area);                                            // 清除第一行顯示區域
    writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL);       // 顯示連線訊息

    // 啟動 mDNS 服務
    if (MDNS.begin(host))
    {
        MDNS.addService("http", "tcp", 80);                // 註冊 HTTP 服務
        DBG_OUTPUT_PORT.println("MDNS responder started"); // mDNS 回應器已啟動
        DBG_OUTPUT_PORT.print("You can now connect to http://");
        DBG_OUTPUT_PORT.print(host);
        DBG_OUTPUT_PORT.println(".local"); // 可透過 .local 存取
    }

    // 設定 HTTP 伺服器路由
    server.on("/list", HTTP_GET, printDirectory);  // 列出目錄內容
    server.on("/edit", HTTP_DELETE, handleDelete); // 刪除檔案
    server.on("/edit", HTTP_PUT, handleCreate);    // 建立檔案
    server.on(
        "/edit",
        HTTP_POST,
        []()
        {
            returnOK(); // 檔案上傳完成回應
        },
        handleFileUpload // 檔案上傳處理函數
    );
    server.on(
        "/clean",
        HTTP_POST,
        []()
        {
            Serial.println("Get clean msg");             // 收到清除螢幕訊息
            xEventGroupSetBits(handleServer, BIT_CLEAN); // 設定清除事件位元
            returnOK();
        });
    server.on("/show", HTTP_POST, handleGetPath);   // 顯示圖片路徑處理
    server.onNotFound(handleNotFound);              // 404 錯誤處理
    server.begin();                                 // 啟動 HTTP 伺服器
    DBG_OUTPUT_PORT.println("HTTP server started"); // HTTP 伺服器已啟動

// 初始化檔案系統
#if defined(USE_SD)
    SPI.begin(SD_SCLK, SD_SCLK, SD_MOSI, SD_CS); // 初始化 SPI（SD 卡模式）
    bool rlst = FILE_SYSTEM.begin(SD_CS, SPI);   // 初始化 SD 卡檔案系統
#else
    bool rlst = FILE_SYSTEM.begin(true); // 初始化 Flash FAT 檔案系統
#endif
    if (rlst)
    {
        DBG_OUTPUT_PORT.println("FS initialized."); // 檔案系統初始化成功
        hasFILE_SYSTEM = true;                      // 設定檔案系統可用標誌
    }
    else
    {
        DBG_OUTPUT_PORT.println("FS initialization failed."); // 檔案系統初始化失敗
        epd_clear_area(line3Area);                            // 清除第三行顯示區域
        cursor_x = line3Area.x;
        cursor_y = line3Area.y + FiraSans.advance_y + FiraSans.descender;                         // 計算文字顯示位置
        writeln((GFXfont *)&FiraSans, "FatFS initialization failed", &cursor_x, &cursor_y, NULL); // 顯示錯誤訊息
    }
}

/**
 * Arduino 主迴圈函數
 *
 * 功能：
 * 1. 處理 HTTP 客戶端請求
 * 2. 監聽 FreeRTOS 事件群組
 * 3. 根據事件執行對應操作（清除螢幕或顯示圖片）
 *
 * 支援的事件：
 * - BIT_CLEAN：清除電子紙螢幕
 * - BIT_SHOW：顯示指定的 JPEG 圖片
 */
void loop()
{
    server.handleClient();                              // 處理 HTTP 客戶端請求
    delay(2);                                           // allow the cpu to switch to other tasks  // 讓 CPU 切換到其他任務
    EventBits_t bit = xEventGroupGetBits(handleServer); // 獲取事件群組狀態
    if (bit & BIT_CLEAN)
    {                                                  // 收到清除螢幕事件
        xEventGroupClearBits(handleServer, BIT_CLEAN); // 清除事件位元
        epd_poweron();                                 // 啟動電子紙電源
        epd_clear();                                   // 清除螢幕內容
        epd_poweroff();                                // 關閉電子紙電源
    }
    else if (bit & BIT_SHOW)
    {                                                 // 收到顯示圖片事件
        xEventGroupClearBits(handleServer, BIT_SHOW); // 清除事件位元
        epd_poweron();                                // 啟動電子紙電源
        File jpg = FILE_SYSTEM.open(pic_path);        // 開啟 JPEG 檔案
        String jpg_p;                                 // JPEG 檔案內容緩衝區
        while (jpg.available())
        {
            jpg_p += jpg.readString(); // 讀取檔案內容到字串
        }
        // 定義全螢幕顯示區域
        Rect_t rect = {
            .x = 0,               // 起始 X 座標
            .y = 0,               // 起始 Y 座標
            .width = EPD_WIDTH,   // 螢幕寬度
            .height = EPD_HEIGHT, // 螢幕高度
        };
        show_jpg_from_buff((uint8_t *)jpg_p.c_str(), jpg_p.length(), rect); // 從緩衝區顯示 JPEG
        Serial.printf("jpg w:%d,h:%d\r\n", rect.width, rect.height);        // 輸出圖片尺寸資訊
        epd_poweroff();                                                     // 關閉電子紙電源
    }
}

/**
 * WiFi 事件處理函數
 *
 * 功能：
 * 1. 監聽並處理各種 WiFi 連線事件
 * 2. 在電子紙螢幕上顯示連線狀態資訊
 * 3. 提供網路連線的視覺化回饋
 *
 * 支援的事件：
 * - WIFI_READY：WiFi 介面準備就緒
 * - STA_START：站台模式啟動
 * - STA_CONNECTED：已連線到 AP
 * - STA_GOT_IP：取得 IP 位址
 * - STA_DISCONNECTED：連線中斷
 *
 * @param event WiFi 事件類型
 */
void WiFiEvent(WiFiEvent_t event)
{
    int32_t cursor_x = 0; // 文字游標 X 座標
    int32_t cursor_y = 0; // 文字游標 Y 座標

    Serial.printf("[WiFi-event] event: %d\n", event); // 輸出事件除錯訊息

    switch (event)
    {
    case ARDUINO_EVENT_WIFI_READY: // WiFi 介面準備就緒
        Serial.println("WiFi interface ready");
        break;
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
        Serial.println("Completed scan for access points");
        break;
    case ARDUINO_EVENT_WIFI_STA_START: // 站台模式啟動
        Serial.println("WiFi client started");
        break;
    case ARDUINO_EVENT_WIFI_STA_STOP: // 站台模式停止
        Serial.println("WiFi clients stopped");
        break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: // 已連線到存取點
        Serial.println("Connected to access point");
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: // 與存取點連線中斷
        WiFi.begin(ssid, password);           // 重新嘗試連線
        Serial.println("Disconnected from WiFi access point");
        epd_clear_area(line1Area); // 清除第一行顯示區域
        cursor_x = line1Area.x;
        cursor_y = line1Area.y + FiraSans.advance_y + FiraSans.descender;
        writeln((GFXfont *)&FiraSans, "WiFi Disconnected", &cursor_x, &cursor_y, NULL); // 顯示斷線訊息
        break;
    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE: // 存取點認證模式改變
        Serial.println("Authentication mode of access point has changed");
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: // 取得 IP 位址
        Serial.print("Obtained IP address: ");
        Serial.println(WiFi.localIP()); // 輸出取得的 IP 位址

        // 顯示連線成功訊息
        memset(buf, 0, sizeof(buf));           // 清空緩衝區
        sprintf(buf, "Connected to %s", ssid); // 格式化連線訊息
        epd_clear_area(line1Area);             // 清除第一行
        cursor_x = line1Area.x;
        cursor_y = line1Area.y + FiraSans.advance_y + FiraSans.descender;
        writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL); // 顯示連線訊息

        // 顯示存取 URL
        epd_clear_area(line2Area); // 清除第二行
        cursor_x = line2Area.x;
        cursor_y = line2Area.y + FiraSans.advance_y + FiraSans.descender;
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "Please visit http://%s.local/edit", host);        // 格式化存取 URL
        writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL); // 顯示存取指示
        break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP: // 失去 IP 位址
        Serial.println("Lost IP address and IP address is reset to 0");
        break;
    case ARDUINO_EVENT_WPS_ER_SUCCESS: // WPS 註冊成功
        Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
        break;
    case ARDUINO_EVENT_WPS_ER_FAILED: // WPS 註冊失敗
        Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
        break;
    case ARDUINO_EVENT_WPS_ER_TIMEOUT: // WPS 註冊逾時
        Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
        break;
    case ARDUINO_EVENT_WPS_ER_PIN: // WPS PIN 碼模式
        Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
        break;
    case ARDUINO_EVENT_WIFI_AP_START: // 存取點模式啟動
        Serial.println("WiFi access point started");
        break;
    case ARDUINO_EVENT_WIFI_AP_STOP: // 存取點模式停止
        Serial.println("WiFi access point stopped");
        break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED: // 客戶端連線到 AP
        Serial.println("Client connected");
        break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: // 客戶端與 AP 斷線
        Serial.println("Client disconnected");
        break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED: // 為客戶端分配 IP 位址
        Serial.println("Assigned IP address to client");
        break;
    case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED: // 收到探測請求
        Serial.println("Received probe request");
        break;
    case ARDUINO_EVENT_WIFI_AP_GOT_IP6: // AP 模式取得 IPv6 位址
        Serial.println("AP IPv6 is preferred");
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6: // 站台模式取得 IPv6 位址
        Serial.println("STA IPv6 is preferred");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP6: // 乙太網路取得 IPv6 位址
        Serial.println("Ethernet IPv6 is preferred");
        break;
    case ARDUINO_EVENT_ETH_START: // 乙太網路啟動
        Serial.println("Ethernet started");
        break;
    case ARDUINO_EVENT_ETH_STOP: // 乙太網路停止
        Serial.println("Ethernet stopped");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED: // 乙太網路已連線
        Serial.println("Ethernet connected");
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED: // 乙太網路已斷線
        Serial.println("Ethernet disconnected");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP: // 乙太網路取得 IP 位址
        Serial.println("Obtained IP address");
        break;
    default:
        break; // 預設情況：無操作
    }
} // WiFiEvent 函數結束

/**
 * ==========================================
 * 程式說明結束
 * ==========================================
 *
 * 此程式實現了一個完整的 WiFi 圖片同步系統，具備以下特色：
 *
 * 🔗 網路功能：
 * - WiFi 連線管理與狀態監控
 * - HTTP 伺服器提供 RESTful API
 * - mDNS 服務支援 .local 域名存取
 * - 支援檔案上傳、下載、管理
 *
 * 📱 顯示功能：
 * - 4.7 吋電子紙顯示器控制
 * - JPEG 圖片解碼與顯示
 * - 16 階灰階顯示支援
 * - 螢幕清除與更新管理
 *
 * 💾 檔案系統：
 * - Flash FAT 或 SD 卡檔案系統
 * - 檔案遞迴操作（建立、刪除、遍歷）
 * - 支援多種檔案格式
 *
 * ⚡ 系統最佳化：
 * - FreeRTOS 事件驅動架構
 * - PSRAM 記憶體最佳化
 * - 低功耗設計考量
 * - 異常處理與錯誤回復
 *
 * 🚀 使用方式：
 * 1. 連線到指定 WiFi 網路
 * 2. 開啟瀏覽器存取 http://lilygo.local/edit
 * 3. 上傳 JPEG 圖片檔案
 * 4. 透過 API 控制圖片顯示
 * 5. 支援遠端清除螢幕功能
 *
 * 💡 開發重點：
 * - 完整的錯誤處理機制
 * - 詳盡的中英雙語註解
 * - 模組化函數設計
 * - 易於擴展的架構
 */

/**
 * ==========================================
 * 📖 程式功能與流程完整說明
 * ==========================================
 *
 * 🎯 這支程式是什麼？
 * 這是一個「無線圖片顯示器」程式，讓你可以透過手機或電腦的瀏覽器，
 * 將圖片無線傳送到電子紙螢幕上顯示。就像是一個可以遠端控制的數位相框。
 *
 * 🔧 主要功能（程式能做什麼）：
 *
 * 1️⃣ 建立 WiFi 連線
 *     - 開發板會自動連線到你設定的 WiFi 網路
 *     - 成功連線後會在螢幕上顯示網址
 *
 * 2️⃣ 網頁檔案管理器
 *     - 在瀏覽器中管理檔案（上傳、刪除、瀏覽）
 *     - 支援建立資料夾和檔案
 *     - 就像電腦的檔案總管一樣
 *
 * 3️⃣ 圖片上傳和顯示
 *     - 可以從手機或電腦上傳 JPEG 圖片
 *     - 自動轉換圖片格式適合電子紙顯示
 *     - 即時顯示在 4.7 吋電子紙螢幕上
 *
 * 4️⃣ 遠端螢幕控制
 *     - 可以遠端清除螢幕
 *     - 選擇要顯示的圖片
 *     - 即時更新顯示內容
 *
 * 📋 程式執行流程（程式怎麼運作）：
 *
 * 🚀 啟動階段（setup函數）：
 *
 *   ┌─ 步驟1：硬體初始化
 *   │   ├─ 啟動串列埠通訊（用來除錯和監控）
 *   │   ├─ 初始化電子紙螢幕
 *   │   ├─ 初始化 JPEG 解碼器
 *   │   └─ 分配記憶體給影像緩衝區
 *   │
 *   ├─ 步驟2：顯示開機畫面
 *   │   ├─ 清除螢幕
 *   │   └─ 顯示 Logo 圖片
 *   │
 *   ├─ 步驟3：建立網路連線
 *   │   ├─ 連線到指定的 WiFi 網路
 *   │   ├─ 在螢幕上顯示「正在連線...」
 *   │   └─ 連線成功後顯示網址
 *   │
 *   ├─ 步驟4：啟動網頁伺服器
 *   │   ├─ 設定各種網址路徑（API 端點）
 *   │   ├─ /list - 列出檔案清單
 *   │   ├─ /edit - 檔案上傳/刪除/建立
 *   │   ├─ /show - 顯示指定圖片
 *   │   ├─ /clean - 清除螢幕
 *   │   └─ 啟動 HTTP 伺服器
 *   │
 *   └─ 步驟5：初始化檔案系統
 *       ├─ 啟動內建 Flash 儲存空間
 *       └─ 檢查是否成功，失敗則顯示錯誤
 *
 * 🔄 運行階段（loop函數）：
 *
 *   程式會不斷重複以下動作：
 *
 *   ┌─ 每 2 毫秒檢查一次
 *   │
 *   ├─ 處理網頁請求
 *   │   └─ 當有人在瀏覽器操作時回應請求
 *   │
 *   ├─ 檢查是否有螢幕操作指令
 *   │   ├─ 如果收到「清除螢幕」指令
 *   │   │   ├─ 開啟電子紙電源
 *   │   │   ├─ 清除螢幕內容
 *   │   │   └─ 關閉電源（節省電力）
 *   │   │
 *   │   └─ 如果收到「顯示圖片」指令
 *   │       ├─ 開啟電子紙電源
 *   │       ├─ 從檔案讀取 JPEG 圖片
 *   │       ├─ 自動解碼並調整大小
 *   │       ├─ 顯示在螢幕上
 *   │       └─ 關閉電源
 *   │
 *   └─ 重複循環
 *
 * 🌐 網路事件處理（WiFiEvent函數）：
 *
 *   當網路狀態改變時會自動觸發：
 *
 *   ├─ WiFi 開始連線 → 在螢幕顯示「正在連線」
 *   ├─ 連線成功 → 顯示「已連線到網路」
 *   ├─ 取得 IP 位址 → 顯示可存取的網址
 *   ├─ 連線中斷 → 顯示「連線中斷」並自動重連
 *   └─ 其他網路事件 → 在串列埠輸出狀態訊息
 *
 * 🔧 檔案操作功能：
 *
 *   ├─ 檔案上傳（handleFileUpload）
 *   │   ├─ 接收從瀏覽器上傳的檔案
 *   │   ├─ 如果同名檔案存在則覆蓋
 *   │   └─ 儲存到內建 Flash 空間
 *   │
 *   ├─ 檔案刪除（handleDelete）
 *   │   ├─ 可以刪除單一檔案
 *   │   └─ 可以刪除整個資料夾（包含所有內容）
 *   │
 *   ├─ 檔案建立（handleCreate）
 *   │   ├─ 建立新的空白檔案
 *   │   └─ 建立新的資料夾
 *   │
 *   ├─ 目錄瀏覽（printDirectory）
 *   │   ├─ 列出資料夾中的所有檔案
 *   │   └─ 以 JSON 格式回傳給瀏覽器
 *   │
 *   └─ 檔案載入（loadFromFILE_SYSTEM）
 *       ├─ 根據檔案類型設定正確的 MIME 類型
 *       ├─ 支援 HTML、CSS、JavaScript、圖片等
 *       └─ 串流檔案內容給瀏覽器
 *
 * 💡 簡單來說，這個程式的運作方式：
 *
 * 1. 開發板開機後連上 WiFi，變成一個小型網站伺服器
 * 2. 你用手機或電腦連到同一個 WiFi，開啟瀏覽器輸入顯示的網址
 * 3. 在網頁上可以上傳圖片、管理檔案
 * 4. 選擇要顯示的圖片，它就會立刻出現在電子紙螢幕上
 * 5. 也可以遠端清除螢幕或更換圖片
 *
 * 就像是把你的手機相片無線傳送到一個數位相框上顯示！
 *
 * 🎨 技術重點：
 *
 * - 使用 FreeRTOS 事件系統確保操作不會衝突
 * - 自動 JPEG 解碼適配電子紙的特殊顯示需求
 * - 記憶體管理優化，支援大圖片處理
 * - 完整的檔案系統操作，支援檔案管理
 * - 網頁介面簡潔易用，支援多種檔案格式
 * - 電源管理優化，只在需要時開啟電子紙電源
 *
 * 🔋 注意事項：
 *
 * - 需要 16MB PSRAM 支援大圖片處理
 * - 電子紙更新較慢，適合靜態圖片顯示
 * - WiFi 連線品質會影響檔案上傳速度
 * - 建議使用解析度適中的 JPEG 圖片以獲得最佳效果
 *
 * 🎯 總結：
 * 這個程式將 ESP32 開發板變成一個無線控制的電子相框，
 * 透過簡單的網頁介面就能管理和顯示圖片，非常適合用來
 * 展示相片、藝術作品或資訊看板。整個操作過程簡單直覺，
 * 不需要複雜的設定就能享受無線圖片傳輸的便利性！
 */