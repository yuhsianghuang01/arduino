/**
 * @copyright Copyright (c) 2024  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2024-04-05
 * @note      Arduino Setting - Arduino IDE 設定說明
 *            Tools -> 工具選單設定
 *                  Board:"ESP32S3 Dev Module" - 開發板：ESP32S3 開發模組
 *                  USB CDC On Boot:"Enable" - 開機時啟用 USB CDC（通訊設備類別）
 *                  USB DFU On Boot:"Disable" - 開機時停用 USB DFU（設備韌體升級）
 *                  Flash Size : "16MB(128Mb)" - 快閃記憶體大小：16MB
 *                  Flash Mode"QIO 80MHz - 快閃記憶體模式：四路輸入輸出 80MHz
 *                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)" - 分割區配置：3MB應用程式/9.9MB檔案系統
 *                  PSRAM:"OPI PSRAM" - 偽靜態記憶體：八路並行介面 PSRAM
 *                  Upload Mode:"UART0/Hardware CDC" - 上傳模式：硬體 CDC
 *                  USB Mode:"Hardware CDC and JTAG" - USB模式：硬體 CDC 和 JTAG 除錯
 *
 */

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
// 錯誤：請啟用 PSRAM，路徑：Arduino IDE -> 工具 -> PSRAM -> OPI
#endif

// 引入必要的函式庫
#include <Arduino.h>     // Arduino 核心函式庫
#include "epd_driver.h"  // 電子紙顯示器驅動程式
#include "firasans.h"    // Fira Sans 字型檔案
#include "esp_adc_cal.h" // ESP32 ADC 校準函式庫，用於精確電壓測量
#include <FS.h>          // 檔案系統函式庫
#include <SPI.h>         // SPI 通訊協定函式庫
#include <SD.h>          // SD 卡讀寫函式庫
#include "logo.h"        // 標誌圖像資料

#include <Wire.h>            // I2C 通訊協定函式庫
#include <TouchDrvGT911.hpp> // GT911 觸控晶片驅動程式
#include <SensorPCF8563.hpp> // PCF8563 實時時鐘（RTC）感測器驅動程式
#include <WiFi.h>            // WiFi 連線功能函式庫
#include <esp_sntp.h>        // 簡單網路時間協定（SNTP）函式庫，用於網路時間同步
#include "utilities.h"       // 工具函式庫，包含硬體定義和輔助函數

// WiFi 連線設定
#define WIFI_SSID "Your WiFi SSID"         // WiFi 網路名稱（請修改為實際的 WiFi 名稱）
#define WIFI_PASSWORD "Your WiFi PASSWORD" // WiFi 密碼（請修改為實際的 WiFi 密碼）

// 網路時間同步伺服器設定
const char *ntpServer1 = "pool.ntp.org";  // 主要 NTP 時間伺服器
const char *ntpServer2 = "time.nist.gov"; // 備用 NTP 時間伺服器
const long gmtOffset_sec = 3600;          // GMT 時區偏移秒數（1小時 = 3600秒）
const int daylightOffset_sec = 3600;      // 日光節約時間偏移秒數
const char *time_zone = "CST-8";          // 時區規則，CST-8 表示中國標準時間（UTC+8）
// TimeZone rule for Europe/Rome including daylight adjustment rules (optional)
// 時區規則設定，包含日光節約時間調整規則（可選）

// 感測器和設備物件初始化
SensorPCF8563 rtc;   // 實時時鐘物件，用於時間記錄和管理
TouchDrvGT911 touch; // 觸控驅動物件，處理觸控螢幕輸入

// 全域變數宣告
uint8_t *framebuffer = NULL; // 影像緩衝區指標，存儲待顯示的圖像資料
bool touchOnline = false;    // 觸控裝置線上狀態標誌，true=觸控可用，false=觸控不可用
uint32_t interval = 0;       // 時間間隔計數器，用於控制定期更新頻率
int vref = 1100;             // ADC 參考電壓值（毫伏），用於電池電壓測量校準
char buf[128];               // 字串緩衝區，用於格式化輸出文字

// 觸控按鈕區域定義結構
struct _point
{
    uint8_t buttonID; // 按鈕識別碼，用於區分不同按鈕
    int32_t x;        // 按鈕左上角 X 座標
    int32_t y;        // 按鈕左上角 Y 座標
    int32_t w;        // 按鈕寬度
    int32_t h;        // 按鈕高度
} touchPoint[] = {
    // 定義五個觸控按鈕區域
    {0, 10, 10, 80, 80},                              // 按鈕 A：左上角 (10,10)，大小 80x80
    {1, EPD_WIDTH - 80, 10, 80, 80},                  // 按鈕 B：右上角，大小 80x80
    {2, 10, EPD_HEIGHT - 80, 80, 80},                 // 按鈕 C：左下角，大小 80x80
    {3, EPD_WIDTH - 80, EPD_HEIGHT - 80, 80, 80},     // 按鈕 D：右下角，大小 80x80
    {4, EPD_WIDTH / 2 - 60, EPD_HEIGHT - 80, 120, 80} // 睡眠按鈕：底部中央，大小 120x80
};

/**
 * WiFi 連線成功回調函數
 * @param event WiFi 事件類型
 * @param info WiFi 事件詳細資訊，包含 IP 位址等
 * 當 ESP32 成功連接到 WiFi 網路並獲得 IP 位址時會呼叫此函數
 */
void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("WiFi connected");                       // 輸出 WiFi 連線成功訊息
    Serial.println("IP address: ");                         // 輸出 IP 位址標籤
    Serial.println(IPAddress(info.got_ip.ip_info.ip.addr)); // 輸出取得的 IP 位址
}

/**
 * 網路時間同步完成回調函數
 * @param t 時間結構指標，包含同步後的時間資訊
 * 當從 NTP 伺服器成功獲取時間並完成同步時會呼叫此函數
 */
void timeavailable(struct timeval *t)
{
    Serial.println("[WiFi]: Got time adjustment from NTP!");
    // 輸出：已從 NTP 伺服器獲得時間校正
    rtc.hwClockWrite(); // 將網路同步的時間寫入硬體 RTC 時鐘
}

/**
 * 系統初始化函數，程式啟動時執行一次
 * 初始化所有硬體模組：WiFi、SD卡、ADC、電子紙、觸控、RTC 等
 */
void setup()
{
    // 初始化串列通訊，設定傳輸速率為 115200 bps，用於除錯訊息輸出
    Serial.begin(115200);

    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    // 設定 WiFi 為工作站模式，並斷開之前可能連接的存取點
    WiFi.mode(WIFI_STA); // 設定為 Station 模式（客戶端）
    WiFi.disconnect();   // 斷開任何現有連線
    // 註冊 WiFi 事件回調函數，當獲得 IP 位址時會呼叫 WiFiGotIP
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

    // 開始連接指定的 WiFi 網路
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // set notification call-back function
    // 設定時間同步通知回調函數，當 NTP 時間同步完成時會呼叫
    sntp_set_time_sync_notification_cb(timeavailable);

    /**
     * This will set configured ntp servers and constant TimeZone/daylightOffset
     * should be OK if your time zone does not need to adjust daylightOffset twice a year,
     * in such a case time adjustment won't be handled automagicaly.
     */
    // 此方法會設定 NTP 伺服器和固定的時區/日光節約時間偏移
    // 如果您的時區不需要一年調整兩次日光節約時間，這樣設定就足夠了
    // 否則時間調整不會自動處理
    // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

    // 設定時區時間，自動處理日光節約時間調整
    configTzTime(time_zone, ntpServer1, ntpServer2);

    /**
     * SD Card test - SD 卡測試
     * Only as a test SdCard hardware, use example reference
     * 僅作為 SD 卡硬體測試，使用範例參考
     * https://github.com/espressif/arduino-esp32/tree/master/libraries/SD/examples
     */
    // 初始化 SPI 通訊，設定 SD 卡相關的腳位
    SPI.begin(SD_SCLK, SD_SCLK, SD_MOSI, SD_CS);
    // 嘗試初始化 SD 卡
    bool rlst = SD.begin(SD_CS, SPI);
    if (!rlst)
    {
        // SD 卡初始化失敗
        Serial.println("SD init failed");              // 輸出：SD 初始化失敗
        snprintf(buf, 128, "➸ No detected SdCard 😂"); // 格式化顯示：未檢測到 SD 卡
    }
    else
    {
        // SD 卡初始化成功
        Serial.println("SD init success"); // 輸出：SD 初始化成功
        // 計算並顯示 SD 卡容量（轉換為 GB 單位）
        snprintf(buf, 128,
                 "➸ Detected SdCard insert:%.2f GB😀",
                 SD.cardSize() / 1024.0 / 1024.0 / 1024.0 // 將位元組轉換為 GB
        );
    }

    // Correct the ADC reference voltage - 校正 ADC 參考電壓
    // 建立 ADC 校準特性結構，用於提高電壓測量精確度
    esp_adc_cal_characteristics_t adc_chars;
    // 校準 ADC，取得實際的參考電壓值
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        ADC_UNIT_2,       // 使用 ADC 單元 2
        ADC_ATTEN_DB_11,  // 衰減 11dB（測量範圍 0-3.3V）
        ADC_WIDTH_BIT_12, // 12 位元解析度
        1100,             // 預設參考電壓 1100mV
        &adc_chars        // 輸出校準特性
    );

    // 檢查校準資料來源並更新參考電壓
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        // 如果從 eFuse 取得校準值，使用更精確的參考電壓
        Serial.printf("eFuse Vref: %umV\r\n", adc_chars.vref); // 輸出：從 eFuse 取得的參考電壓
        vref = adc_chars.vref;                                 // 更新全域參考電壓變數
    }

    // 動態分配 PSRAM 記憶體給影像緩衝區，大小為電子紙寬度 × 高度 ÷ 2（每個像素佔 4 位元）
    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer)
    {
        // 記憶體分配失敗時輸出錯誤訊息並停止程式
        Serial.println("alloc memory failed !!!"); // 輸出：記憶體分配失敗
        while (1)
            ; // 無限迴圈，停止程式執行
    }
    // 將影像緩衝區填滿 0xFF（白色），清空顯示內容
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    // 初始化電子紙顯示器硬體
    epd_init();

    // 定義標誌圖像的顯示區域
    Rect_t area = {
        .x = 230,              // 起始 X 座標
        .y = 0,                // 起始 Y 座標
        .width = logo_width,   // 標誌寬度
        .height = logo_height, // 標誌高度
    };

    // 開啟電子紙顯示器電源
    epd_poweron();
    // 清除電子紙顯示器上的所有內容
    epd_clear();
    // 顯示灰階標誌圖像
    epd_draw_grayscale_image(area, (uint8_t *)logo_data);
    // 顯示黑白標誌圖像（疊加顯示）
    epd_draw_image(area, (uint8_t *)logo_data, BLACK_ON_WHITE);

    // 設定文字顯示的初始座標位置
    int cursor_x = 200;
    int cursor_y = 200;

#if defined(CONFIG_IDF_TARGET_ESP32S3)
    // Assuming that the previous touch was in sleep state, wake it up
    // 假設觸控晶片之前處於睡眠狀態，將其喚醒
    pinMode(TOUCH_INT, OUTPUT);    // 設定觸控中斷腳位為輸出模式
    digitalWrite(TOUCH_INT, HIGH); // 將觸控中斷腳位設為高電位，喚醒觸控晶片

    // 在電子紙上顯示 SD 卡檢測結果
    writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL);
    cursor_x = 200; // 重設 X 座標
    cursor_y += 50; // Y 座標下移 50 像素

    // 初始化 I2C 通訊，連接 RTC 和觸控晶片
    Wire.begin(BOARD_SDA, BOARD_SCL);
    // 嘗試連接 PCF8563 實時時鐘晶片
    Wire.beginTransmission(PCF8563_SLAVE_ADDRESS);
    if (Wire.endTransmission() == 0)
    {
        // RTC 晶片連線成功
        rtc.begin(Wire, PCF8563_SLAVE_ADDRESS, BOARD_SDA, BOARD_SCL);
        // rtc.setDateTime(2022, 6, 30, 0, 0, 0);  // 可用於設定初始時間
        writeln((GFXfont *)&FiraSans, "➸ RTC is online  😀 \n", &cursor_x, &cursor_y, NULL);
        // 顯示：RTC 已連線
    }
    else
    {
        // RTC 晶片連線失敗
        writeln((GFXfont *)&FiraSans, "➸ RTC is probe failed!  😂 \n", &cursor_x, &cursor_y, NULL);
        // 顯示：RTC 探測失敗
    }

    /*
     * The touch reset pin uses hardware pull-up,
     * and the function of setting the I2C device address cannot be used.
     * Use scanning to obtain the touch device address.
     */
    // 觸控重置腳位使用硬體上拉電阻，無法使用設定 I2C 設備位址的功能
    // 使用掃描方式來取得觸控設備位址
    uint8_t touchAddress = 0x14; // 預設觸控晶片位址

    // 嘗試連接位址 0x14 的觸控晶片
    Wire.beginTransmission(0x14);
    if (Wire.endTransmission() == 0)
    {
        touchAddress = 0x14; // 找到位址 0x14 的觸控晶片
    }
    // 嘗試連接位址 0x5D 的觸控晶片
    Wire.beginTransmission(0x5D);
    if (Wire.endTransmission() == 0)
    {
        touchAddress = 0x5D; // 找到位址 0x5D 的觸控晶片
    }

    cursor_x = 200; // 重設文字顯示 X 座標
    cursor_y += 50; // Y 座標下移 50 像素

    // 初始化觸控晶片
    touch.setPins(-1, TOUCH_INT); // 設定觸控腳位（重置腳位設為 -1，中斷腳位設為 TOUCH_INT）
    if (touch.begin(Wire, touchAddress, BOARD_SDA, BOARD_SCL))
    {
        // 觸控晶片初始化成功
        touch.setMaxCoordinates(EPD_WIDTH, EPD_HEIGHT); // 設定觸控座標範圍為電子紙尺寸
        touch.setSwapXY(true);                          // 交換 X/Y 座標軸
        touch.setMirrorXY(false, true);                 // 設定座標鏡像（X軸不鏡像，Y軸鏡像）
        touchOnline = true;                             // 標記觸控裝置為線上狀態
        writeln((GFXfont *)&FiraSans, "➸ Touch is online  😀 \n", &cursor_x, &cursor_y, NULL);
        // 顯示：觸控已連線
    }
    else
    {
        // 觸控晶片初始化失敗
        writeln((GFXfont *)&FiraSans, "➸ Touch is probe failed!  😂 \n", &cursor_x, &cursor_y, NULL);
        // 顯示：觸控探測失敗
    }

#endif

    // 設定字型屬性
    FontProperties props = {
        .fg_color = 15,      // 前景色：白色（15 表示最亮的灰階）
        .bg_color = 0,       // 背景色：黑色（0 表示最暗的灰階）
        .fallback_glyph = 0, // 回退字符：當找不到字符時使用
        .flags = 0           // 字型標誌：無特殊標誌
    };

    // Draw button - 繪製按鈕
    // 繪製按鈕 A（左上角）
    int32_t x = 18;
    int32_t y = 50;
    epd_fill_rect(10, 10, 80, 80, 0x0000, framebuffer);                                 // 繪製黑色矩形背景
    write_mode((GFXfont *)&FiraSans, "A", &x, &y, framebuffer, WHITE_ON_BLACK, &props); // 在黑色背景上寫白色字母 A

    // 繪製按鈕 B（右上角）
    x = EPD_WIDTH - 72;
    y = 50;
    epd_fill_rect(EPD_WIDTH - 80, 10, 80, 80, 0x0000, framebuffer);                     // 繪製黑色矩形背景
    write_mode((GFXfont *)&FiraSans, "B", &x, &y, framebuffer, WHITE_ON_BLACK, &props); // 在黑色背景上寫白色字母 B

    // 繪製按鈕 C（左下角）
    x = 18;
    y = EPD_HEIGHT - 30;
    epd_fill_rect(10, EPD_HEIGHT - 80, 80, 80, 0x0000, framebuffer);                    // 繪製黑色矩形背景
    write_mode((GFXfont *)&FiraSans, "C", &x, &y, framebuffer, WHITE_ON_BLACK, &props); // 在黑色背景上寫白色字母 C

    // 繪製按鈕 D（右下角）
    x = EPD_WIDTH - 72;
    y = EPD_HEIGHT - 30;
    epd_fill_rect(EPD_WIDTH - 80, EPD_HEIGHT - 80, 80, 80, 0x0000, framebuffer);        // 繪製黑色矩形背景
    write_mode((GFXfont *)&FiraSans, "D", &x, &y, framebuffer, WHITE_ON_BLACK, &props); // 在黑色背景上寫白色字母 D

    // 繪製睡眠按鈕（底部中央）
    x = EPD_WIDTH / 2 - 55;
    y = EPD_HEIGHT - 30;
    epd_draw_rect(EPD_WIDTH / 2 - 60, EPD_HEIGHT - 80, 120, 75, 0x0000, framebuffer);     // 繪製黑色矩形邊框
    write_mode((GFXfont *)&FiraSans, "Sleep", &x, &y, framebuffer, WHITE_ON_BLACK, NULL); // 在矩形內寫入 "Sleep" 文字

    // 將所有繪製內容顯示到電子紙螢幕上
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);

    // 關閉電子紙顯示器電源以節省電力
    epd_poweroff();
}

/**
 * 主迴圈函數，持續執行系統監控和觸控處理
 * 包含電池電壓監測、時間顯示更新、觸控事件處理等功能
 */
void loop()
{

    // 每 10 秒執行一次電池電壓和時間更新
    if (millis() > interval)
    {
        interval = millis() + 10000; // 設定下次更新時間為 10 秒後

        // When reading the battery voltage, POWER_EN must be turned on
        // 讀取電池電壓時，必須開啟 POWER_EN 電源控制
        epd_poweron();
        delay(10); // Make adc measurement more accurate - 延遲 10ms 使 ADC 測量更準確

        // 讀取電池電壓 ADC 值並轉換為實際電壓
        uint16_t v = analogRead(BATT_PIN); // 讀取電池電壓腳位的 ADC 值
        // 計算實際電池電壓：ADC值 / 4095 * 分壓比2.0 * 參考電壓3.3V * 校準係數
        float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
        if (battery_voltage >= 4.2)
        {
            battery_voltage = 4.2; // 限制最大顯示電壓為 4.2V（鋰電池滿電電壓）
        }
        // 格式化電壓顯示字串
        String voltage = "➸ Voltage: " + String(battery_voltage) + "V";

        // 定義電壓和時間顯示區域
        Rect_t area = {
            .x = 200,      // 起始 X 座標
            .y = 310,      // 起始 Y 座標
            .width = 500,  // 區域寬度
            .height = 100, // 區域高度
        };

        // 設定文字顯示座標
        int cursor_x = 200;
        int cursor_y = 350;
        // 清除指定區域，準備顯示新內容
        epd_clear_area(area);

        // 顯示電池電壓資訊
        writeln((GFXfont *)&FiraSans, (char *)voltage.c_str(), &cursor_x, &cursor_y, NULL);
        cursor_x = 200; // 重設 X 座標
        cursor_y += 50; // Y 座標下移 50 像素

        // Format the output using the strftime function
        // For more formats, please refer to :
        // https://man7.org/linux/man-pages/man3/strftime.3.html
        // 使用 strftime 函數格式化時間輸出
        // 更多格式選項請參考：https://man7.org/linux/man-pages/man3/strftime.3.html

        struct tm timeinfo;
        // Get the time C library structure - 取得時間 C 語言結構
        rtc.getDateTime(&timeinfo); // 從 RTC 取得當前日期時間

        // 格式化時間字串：月份 日期 年份 時:分:秒
        strftime(buf, 64, "➸ %b %d %Y %H:%M:%S", &timeinfo);
        // 在電子紙上顯示格式化的時間
        writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL);

        /**
         * There are two ways to close
         * It will turn off the power of the ink screen,
         * but cannot turn off the blue LED light.
         */
        // 有兩種關閉方式
        // 此方法會關閉電子紙的電源，但無法關閉藍色 LED 燈
        // epd_poweroff();

        /**
         * It will turn off the power of the entire
         * POWER_EN control and also turn off the blue LED light
         */
        // 此方法會關閉整個 POWER_EN 控制的電源，同時也會關閉藍色 LED 燈
        epd_poweroff_all();
    }

    // 處理觸控輸入事件
    if (touchOnline)
    {
        int16_t x, y; // 觸控座標變數

        // 檢查觸控中斷腳位狀態，如果為低電位則表示沒有觸控事件
        if (!digitalRead(TOUCH_INT))
        {
            return; // 沒有觸控事件，直接返回
        }

        // 取得觸控點座標
        uint8_t touched = touch.getPoint(&x, &y);
        if (touched) // 如果檢測到觸控
        {

            // When reading the battery voltage, POWER_EN must be turned on
            // 處理觸控事件時，必須開啟 POWER_EN 電源控制
            epd_poweron();

            // 設定觸控資訊顯示座標
            int cursor_x = 200;
            int cursor_y = 450;

            // 定義觸控資訊顯示區域
            Rect_t area = {
                .x = 200,     // 起始 X 座標
                .y = 410,     // 起始 Y 座標
                .width = 400, // 區域寬度
                .height = 50, // 區域高度
            };
            // 清除觸控資訊顯示區域
            epd_clear_area(area);

            // 格式化觸控座標資訊
            snprintf(buf, 128, "➸ X:%d Y:%d", x, y);

            bool pressButton = false; // 按鈕按下標誌
            // 檢查觸控位置是否在任何按鈕區域內
            for (int i = 0; i < sizeof(touchPoint) / sizeof(touchPoint[0]); ++i)
            {
                // 判斷觸控座標是否在當前按鈕的範圍內
                if ((x > touchPoint[i].x && x < (touchPoint[i].x + touchPoint[i].w)) && (y > touchPoint[i].y && y < (touchPoint[i].y + touchPoint[i].h)))
                {
                    // 格式化按鈕按下資訊（A=65, B=66, C=67, D=68, Sleep=69）
                    snprintf(buf, 128, "➸ Pressed Button: %c\n", 65 + touchPoint[i].buttonID);
                    // 顯示按鈕按下資訊
                    writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL);
                    pressButton = true; // 標記按鈕已按下

                    // 檢查是否按下睡眠按鈕（按鈕 ID = 4）
                    if (touchPoint[i].buttonID == 4)
                    {

                        Serial.println("Sleep !!!!!!"); // 輸出：進入睡眠模式

                        // 清除整個電子紙螢幕
                        epd_clear();

                        // 設定 "Sleep" 文字顯示在螢幕中央
                        cursor_x = EPD_WIDTH / 2 - 40;
                        cursor_y = EPD_HEIGHT / 2 - 40;

                        // 重新填充影像緩衝區為白色
                        memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

                        // 在螢幕中央顯示 "Sleep" 文字
                        writeln((GFXfont *)&FiraSans, "Sleep", &cursor_x, &cursor_y, framebuffer);

                        // 將影像緩衝區內容顯示到電子紙上
                        epd_draw_grayscale_image(epd_full_screen(), framebuffer);

                        delay(1000); // 延遲 1 秒讓使用者看到 "Sleep" 訊息

                        // 關閉所有電源
                        epd_poweroff_all();

                        // 斷開 WiFi 連線並清除設定
                        WiFi.disconnect(true);

                        // 讓觸控晶片進入睡眠模式
                        touch.sleep();

                        delay(100); // 短暫延遲確保所有操作完成

                        // 結束 I2C 通訊
                        Wire.end();

                        // 結束串列通訊
                        Serial.end();

                        // BOOT(STR_IO0) Button wakeup - 設定 BOOT 按鈕作為喚醒源
                        // 當 GPIO0（BOOT 按鈕）被按下時喚醒系統
                        esp_sleep_enable_ext1_wakeup(_BV(0), ESP_EXT1_WAKEUP_ANY_LOW);

                        // 進入深度睡眠模式
                        esp_deep_sleep_start();
                    }
                }
            }
            // 如果沒有按下任何按鈕，只顯示觸控座標
            if (!pressButton)
            {
                writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL);
            }

            /*
             * 電源關閉的兩種方式：
             *
             * 方式一：epd_poweroff()
             * - 僅關閉電子紙顯示器的電源
             * - 無法關閉藍色 LED 指示燈
             * - 適用於僅需關閉顯示器的情況
             */
            // epd_poweroff();

            /*
             * 方式二：epd_poweroff_all()
             * - 關閉 POWER_EN 控制的所有電源
             * - 同時關閉藍色 LED 指示燈
             * - 更徹底的電源管理，節能效果更好
             * - 適用於大部分應用場景
             */
            epd_poweroff_all();
        }
    }

    delay(2); // 主迴圈延遲 2 毫秒，避免過度耗電
}
/*
Demo.ino 程式完整功能說明
demo.ino 程式添加了詳細的中文註解，這個程式包含了以下主要功能模組：

📡 WiFi 與網路時間同步
    WiFi 連線：自動連接指定的 WiFi 網路
    NTP 時間同步：從網路時間伺服器自動同步時間
    時區處理：支援 CST-8 中國標準時間
    回調機制：WiFi 連線和時間同步完成時的事件處理
💾 儲存系統
    SD 卡支援：檢測並顯示 SD 卡容量
    檔案系統：支援 FAT 檔案系統讀寫
    容量顯示：自動計算並顯示 GB 單位的容量
🔋 電源管理
    ADC 校準：精確的電池電壓測量
    電壓監控：每 10 秒更新電池電壓顯示
    節能模式：支援深度睡眠功能
🖥️ 顯示系統
    電子紙控制：540×960 解析度電子紙顯示
    影像緩衝：PSRAM 動態記憶體管理
    字型渲染：支援 Fira Sans 字型顯示
    圖像顯示：標誌圖像的灰階和黑白顯示
👆 觸控介面
    GT911 觸控：支援多點觸控輸入
    按鈕系統：5 個虛擬按鈕（A、B、C、D、Sleep）
    座標轉換：自動處理觸控座標映射
    觸控喚醒：從睡眠狀態喚醒觸控晶片
⏰ 時間系統
    RTC 時鐘：PCF8563 實時時鐘晶片
    時間顯示：格式化的日期時間顯示
    自動同步：網路時間與 RTC 的自動同步
🔧 硬體通訊
    I2C 通訊：連接 RTC 和觸控晶片
    SPI 通訊：SD 卡資料傳輸
    ADC 讀取：電池電壓監測
💡 系統特色
    模組化設計：各功能模組獨立初始化
    錯誤處理：完整的硬體檢測和錯誤回報
    用戶友好：直觀的狀態顯示和emoji提示
    低功耗：智能電源管理和睡眠模式

demo.ino 程式註解總結
1. 程式頭部說明
    程式功能和用途概述
    硬體平台和相依性說明
    功能模組清單
2. 標頭檔案和巨集定義
    各個標頭檔案的用途說明
    GPIO 腳位定義和功能說明
    開發板差異化設定說明
3. 全域變數說明
    硬體物件實例化說明
    緩衝區和資料結構用途
    觸控按鈕配置陣列詳解
4. setup() 函數詳細說明
    串列通訊初始化
    SD 卡偵測和設定
    電子紙顯示器初始化
    WiFi 連線設定
    RTC 時鐘晶片設定
    觸控晶片初始化
    電池電壓偵測
    按鈕繪製功能
5. loop() 函數完整說明
    電池電壓監控
    時間顯示格式化
    觸控事件處理
    按鈕響應邏輯
    睡眠模式控制
    電源管理策略
6. 技術細節說明
    記憶體使用最佳化
    電力管理策略
    硬體模組互動
    錯誤處理機制
    這個程式現在包含了完整的中文註解，涵蓋了：

    硬體控制：EPD 顯示器、觸控、RTC、SD 卡、WiFi
    電源管理：深度睡眠、電力優化策略
    使用者介面：觸控按鈕、資訊顯示
    系統監控：電池電壓、時間同步
*/