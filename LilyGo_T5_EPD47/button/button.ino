/**
 * @copyright Copyright (c) 2024  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2024-04-05
 * @note      Arduino Setting
 *            Tools ->
 *                  Board:"ESP32S3 Dev Module"
 *                  USB CDC On Boot:"Enable"
 *                  USB DFU On Boot:"Disable"
 *                  Flash Size : "16MB(128Mb)"
 *                  Flash Mode"QIO 80MHz
 *                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)"
 *                  PSRAM:"OPI PSRAM"
 *                  Upload Mode:"UART0/Hardware CDC"
 *                  USB Mode:"Hardware CDC and JTAG"
 *
 */

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

// 引入必要的函式庫
#include <Arduino.h>    // Arduino 核心函式庫
#include "epd_driver.h" // 電子紙顯示器驅動程式
#include "firasans.h"   // Fira Sans 字型檔案
#include "Button2.h"    // 按鈕處理函式庫
#include "lilygo.h"     // LilyGo 標誌圖像資料
#include "logo.h"       // 其他標誌圖像資料

// 按鈕物件初始化
Button2 btn1(BUTTON_1); // 主按鈕，所有板型都支援

#if defined(CONFIG_IDF_TARGET_ESP32)
Button2 btn2(BUTTON_2); // 按鈕 2，僅 ESP32 版本支援
Button2 btn3(BUTTON_3); // 按鈕 3，僅 ESP32 版本支援
#endif

// 全域變數宣告
uint8_t *framebuffer; // 影像緩衝區指標，用於存儲待顯示的圖像資料
int vref = 1100;      // 參考電壓值（毫伏），用於電壓測量校準
int cursor_x = 20;    // 文字顯示的 X 座標游標位置
int cursor_y = 60;    // 文字顯示的 Y 座標游標位置
int state = 0;        // 當前顯示狀態，控制顯示不同內容頁面

/* 記憶體極致優化策略說明：
 *
 * 🚀 FLASH 記憶體優化 (16MB)：
 * - 當前使用：存儲程式碼、字型檔案、圖像常數
 * - 極致策略：
 *   1. 使用 PROGMEM 將大型資料存於 Flash（如圖片、字型）
 *   2. 實作圖片壓縮演算法，提高存儲密度
 *   3. 建立多頁面圖像庫，支援動態載入
 *   4. 使用 SPIFFS/LittleFS 檔案系統存儲配置和圖片
 *
 * ⚡ SRAM 記憶體優化 (520KB)：
 * - 當前使用：影像緩衝區、變數、堆疊
 * - 極致策略：
 *   1. 實作雙緩衝技術，前景/背景交替更新
 *   2. 動態記憶體池管理，避免記憶體碎片
 *   3. 局部更新演算法，只重繪變更區域
 *   4. 快取常用字型點陣，加速文字渲染
 *
 * 🔋 PSRAM 記憶體優化 (16MB)：
 * - 當前使用：主要影像緩衝區分配
 * - 極致策略：
 *   1. 多層級緩存系統（L1:SRAM, L2:PSRAM）
 *   2. 預載入下一頁內容，實現無縫切換
 *   3. 大型圖片分塊存儲和串流載入
 *   4. 歷史頁面緩存，支援快速返回
 *   5. 實作圖片解壓縮緩衝區
 */

// 定義主要顯示區域的矩形範圍
Rect_t area1 = {
    .x = 10,                      // 起始 X 座標：距離左邊緣 10 像素
    .y = 20,                      // 起始 Y 座標：距離上邊緣 20 像素
    .width = EPD_WIDTH - 20,      // 寬度：螢幕寬度減去左右邊距
    .height = EPD_HEIGHT / 2 + 80 // 高度：螢幕高度的一半再加 80 像素
};

// 概覽文字內容陣列，包含三個不同的資訊頁面
const char *overview[] = {
    // 第一頁：ESP32 基本介紹
    "   ESP32 is a single 2.4 GHz Wi-Fi-and-Bluetooth\n"
    "combo chip designed with the TSMC ultra-low-po\n"
    "wer 40 nm technology. It is designed to achieve \n"
    "the best power and RF performance, showing rob\n"
    "ustness versatility and reliability in a wide variet\n"
    "y of applications and power scenarios.\n"
    // ESP32 是單一的 2.4 GHz Wi-Fi 和藍牙組合晶片，使用台積電超低功耗 40 奈米技術設計
    // 旨在實現最佳的功耗和射頻性能，在各種應用和功耗場景中展現堅固性、多功能性和可靠性
    ,
    // 第二頁：ESP32 硬體規格
    "➸ Xtensa® dual-core 32-bit LX6 microprocessor\n"
    "➸ 448 KB ROM & External 16MBytes falsh\n"
    "➸ 520 KB SRAM & External 16MBytes PSRAM\n"
    "➸ 16 KB SRAM in RTC\n"
    "➸ Multi-connections in Classic BT and BLE\n"
    "➸ 802.11 n (2.4 GHz), up to 150 Mbps\n"
    // ➸ Xtensa® 雙核心 32 位元 LX6 微處理器
    // ➸ 448 KB 唯讀記憶體與外部 16MB 快閃記憶體
    // ➸ 520 KB 靜態隨機存取記憶體與外部 16MB 偽靜態隨機存取記憶體
    // ➸ 實時時鐘中的 16 KB 靜態隨機存取記憶體
    /* 記憶體架構詳細說明：
     * ROM (唯讀記憶體)：存儲開機程式和核心韌體，確保系統穩定啟動
     * Flash (快閃記憶體)：存儲使用者程式碼、字型檔案、圖像資料等，為EPD提供豐富的顯示內容
     * SRAM (靜態記憶體)：高速暫存區，處理即時運算和EPD影像緩衝區，確保流暢的顯示更新
     * PSRAM (偽靜態記憶體)：大容量緩存區，可存儲多張EPD圖片和複雜圖形資料
     * RTC SRAM：低功耗記憶體，在深度睡眠時保持關鍵資料，實現EPD的節能顯示功能
     */
    // ➸ 支援傳統藍牙和低功耗藍牙的多重連線
    // ➸ 802.11 n (2.4 GHz) 無線網路，最高速度 150 Mbps
    ,
    // 第三頁：電子紙顯示器特性
    "➸ 16 color grayscale\n"
    "➸ Use with 4.7\" EPDs\n"
    "➸ High-quality font rendering\n"
    "➸ ~630ms for full frame draw\n"
    // ➸ 16 色灰階顯示
    // ➸ 適用於 4.7 吋電子紙顯示器
    // ➸ 高品質字型渲染
    // ➸ 全畫面繪製約需 630 毫秒
};

/**
 * 顯示資訊函數，根據當前狀態顯示不同內容
 */
void displayInfo(void)
{
    // 重設游標位置到左上角附近
    cursor_x = 20;
    cursor_y = 60;
    // 使用模運算確保狀態值在 0-3 之間循環
    state %= 4;
    switch (state)
    {
    case 0:
        // 狀態 0：清除指定區域並顯示第一個概覽內容
        epd_clear_area(area1);
        write_string((GFXfont *)&FiraSans, (char *)overview[0], &cursor_x, &cursor_y, NULL);
        break;
    case 1:
        // 狀態 1：清除指定區域並顯示第二個概覽內容
        epd_clear_area(area1);
        write_string((GFXfont *)&FiraSans, (char *)overview[1], &cursor_x, &cursor_y, NULL);
        break;
    case 2:
        // 狀態 2：清除指定區域並顯示第三個概覽內容
        epd_clear_area(area1);
        write_string((GFXfont *)&FiraSans, (char *)overview[2], &cursor_x, &cursor_y, NULL);
        break;
    case 3:
        // 狀態 3：進入深度睡眠模式
        delay(1000);           // 延遲 1 秒讓使用者看到切換
        epd_clear_area(area1); // 清除顯示區域
        // 顯示 "DeepSleep" 訊息告知使用者即將進入睡眠模式
        write_string((GFXfont *)&FiraSans, "DeepSleep", &cursor_x, &cursor_y, NULL);
        // 完全關閉電子紙顯示器所有電源
        epd_poweroff_all();
#if defined(CONFIG_IDF_TARGET_ESP32)
        // Set to wake up by GPIO39
        // 設定 GPIO39 作為喚醒源，當此腳位接收到低電位信號時喚醒 ESP32
        esp_sleep_enable_ext1_wakeup(GPIO_SEL_39, ESP_EXT1_WAKEUP_ANY_LOW);
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
        // 針對 ESP32-S3 晶片，設定 GPIO21 作為喚醒源
        esp_sleep_enable_ext1_wakeup(GPIO_SEL_21, ESP_EXT1_WAKEUP_ANY_LOW);
#endif
        // 進入深度睡眠模式，此時 ESP32 消耗最少電力，只能透過設定的喚醒源喚醒
        esp_deep_sleep_start();
        break;
    case 4:
        // 狀態 4：空的處理分支，可能預留給未來功能擴展
        break;
    default:
        // 預設情況：處理未預期的狀態值
        break;
    }
    // 關閉電子紙顯示器電源以節省電力
    epd_poweroff();
}

/**
 * 按鈕按下時的回調函數
 * @param b Button2 物件的參考，代表被按下的按鈕
 */
void buttonPressed(Button2 &b)
{
    // 更新顯示內容，根據當前狀態顯示對應的資訊
    displayInfo();
    // 狀態計數器遞增，用於循環切換不同的顯示內容
    state++;
}

/**
 * 系統初始化函數，在程式啟動時執行一次
 */
void setup()
{
    // 初始化串列通訊，設定傳輸速率為 115200 bps，用於除錯訊息輸出
    Serial.begin(115200);

    // 初始化電子紙顯示器硬體
    epd_init();

    // 動態分配記憶體給影像緩衝區，大小為電子紙寬度 × 高度 ÷ 2（每個像素佔 4 位元）
    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer)
    {
        // 記憶體分配失敗時輸出錯誤訊息並無限迴圈停止程式
        Serial.println("alloc memory failed !!!");
        while (1)
            ;
    }
    // 將影像緩衝區填滿 0xFF（白色），清空顯示內容
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    /* 🎯 記憶體極致使用技巧實作示例：
     *
     * 當前程式改進建議：
     *
     * 1. PSRAM 雙緩衝優化：
     *    uint8_t *frontBuffer = (uint8_t *)ps_malloc(EPD_WIDTH * EPD_HEIGHT / 2);
     *    uint8_t *backBuffer = (uint8_t *)ps_malloc(EPD_WIDTH * EPD_HEIGHT / 2);
     *    // 前景緩衝區顯示，背景緩衝區準備下一幀
     *
     * 2. SRAM 快取池建立：
     *    uint8_t fontCache[FONT_CACHE_SIZE];  // 字型快取
     *    uint8_t tileCache[TILE_CACHE_SIZE];  // 圖塊快取
     *
     * 3. Flash 資源預載入：
     *    const uint8_t images[][] PROGMEM = {...};  // 圖片存於 Flash
     *    const GFXfont fonts[] PROGMEM = {...};     // 字型存於 Flash
     *
     * 4. 差異更新演算法：
     *    bool needsUpdate[EPD_HEIGHT/TILE_SIZE][EPD_WIDTH/TILE_SIZE];
     *    // 只更新變更的圖塊，節省 SRAM 和處理時間
     *
     * 5. 記憶體使用監控：
     *    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
     *    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
     */

    // 設定按鈕 1 的按下事件處理函數
    btn1.setPressedHandler(buttonPressed);
#if defined(CONFIG_IDF_TARGET_ESP32)
    // 針對 ESP32 晶片，額外設定按鈕 2 和按鈕 3 的事件處理函數
    btn2.setPressedHandler(buttonPressed);
    btn3.setPressedHandler(buttonPressed);
#endif

    // 開啟電子紙顯示器電源
    epd_poweron();
    // 清除電子紙顯示器上的所有內容
    epd_clear();
    // 在指定位置顯示第一個概覽文字內容
    write_string((GFXfont *)&FiraSans, (char *)overview[0], &cursor_x, &cursor_y, framebuffer);

    // Draw Box - 繪製使用者介面按鈕框
    //  繪製 "Prev" 按鈕的矩形邊框（座標: x=600, y=450, 寬=120, 高=60, 黑色邊框）
    epd_draw_rect(600, 450, 120, 60, 0, framebuffer);
    cursor_x = 615; // 設定文字起始 x 座標
    cursor_y = 490; // 設定文字起始 y 座標
    // 在矩形內顯示 "Prev" 文字
    writeln((GFXfont *)&FiraSans, "Prev", &cursor_x, &cursor_y, framebuffer);

    // 繪製 "Next" 按鈕的矩形邊框
    epd_draw_rect(740, 450, 120, 60, 0, framebuffer);
    cursor_x = 755; // 設定文字起始 x 座標
    cursor_y = 490; // 設定文字起始 y 座標
    // 在矩形內顯示 "Next" 文字
    writeln((GFXfont *)&FiraSans, "Next", &cursor_x, &cursor_y, framebuffer);

    // 定義 LilyGo 標誌的顯示區域
    Rect_t area = {
        .x = 160,               // 起始 x 座標
        .y = 420,               // 起始 y 座標
        .width = lilygo_width,  // 標誌寬度
        .height = lilygo_height // 標誌高度
    };
    // 將 LilyGo 標誌圖像複製到影像緩衝區的指定區域
    epd_copy_to_framebuffer(area, (uint8_t *)lilygo_data, framebuffer);

    // 繪製主要內容區域的邊框（距離邊緣 10 像素，高度為螢幕一半加 80 像素）
    epd_draw_rect(10, 20, EPD_WIDTH - 20, EPD_HEIGHT / 2 + 80, 0, framebuffer);
    // 將影像緩衝區的內容顯示到電子紙螢幕上
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    // 關閉電子紙顯示器電源以節省電力
    epd_poweroff();
}

/**
 * 主迴圈函數，持續執行處理按鈕事件
 */
void loop()
{
    // 檢查並處理按鈕 1 的狀態變化（按下、釋放等事件）
    btn1.loop();

#if defined(CONFIG_IDF_TARGET_ESP32)
    // 針對 ESP32 晶片，額外處理按鈕 2 和按鈕 3 的狀態變化
    btn2.loop();
    btn3.loop();
#endif

    // 短暫延遲 2 毫秒，避免過度頻繁檢查按鈕狀態，降低 CPU 使用率
    delay(2);
}

/* 🚀 記憶體極致優化實作範例函數
 * 以下是如何將 Flash、SRAM、PSRAM 發揮到極致的完整實作
 */

/*
// === Flash 記憶體極致使用 ===
const uint8_t largeImage[] PROGMEM = {
    // 大型圖片資料存儲在 Flash 中，節省 RAM
};

const char* menuText[] PROGMEM = {
    "Menu Item 1", "Menu Item 2", "Menu Item 3"
    // 靜態文字存於 Flash，動態載入到 RAM
};

// === SRAM 快取系統 ===
typedef struct {
    uint8_t data[TILE_SIZE * TILE_SIZE / 2];  // 圖塊資料
    bool dirty;                                // 是否需要更新
    uint32_t lastAccess;                       // 最後存取時間
} TileCache;

TileCache sramCache[CACHE_SIZE];  // SRAM 中的快速快取

// === PSRAM 大容量緩衝 ===
uint8_t* psramImageBuffer;     // 大型圖片緩衝區
uint8_t* psramBackBuffer;      // 背景繪製緩衝區
uint8_t* psramHistoryBuffer;   // 頁面歷史緩衝區

void initAdvancedMemorySystem() {
    // 1. PSRAM 大容量分配
    psramImageBuffer = (uint8_t*)ps_malloc(EPD_WIDTH * EPD_HEIGHT / 2 * 4);  // 4頁緩存
    psramBackBuffer = (uint8_t*)ps_malloc(EPD_WIDTH * EPD_HEIGHT / 2);
    psramHistoryBuffer = (uint8_t*)ps_malloc(EPD_WIDTH * EPD_HEIGHT / 2 * 2);

    // 2. SRAM 快取初始化
    memset(sramCache, 0, sizeof(sramCache));

    // 3. Flash 資源預載入檢查
    if (psramImageBuffer && psramBackBuffer) {
        Serial.println("✅ 記憶體系統初始化成功");
        Serial.printf("📊 PSRAM 使用: %d KB\n",
                     (EPD_WIDTH * EPD_HEIGHT / 2 * 7) / 1024);
    }
}

void optimizedImageRender(const uint8_t* flashImage, int imageSize) {
    // 1. 從 Flash 載入到 PSRAM 背景緩衝區
    memcpy_P(psramBackBuffer, flashImage, imageSize);

    // 2. 在 PSRAM 中進行圖像處理
    // （縮放、旋轉、濾鏡等耗記憶體的操作）

    // 3. 差異比較，只更新變更部分
    for (int i = 0; i < imageSize; i++) {
        if (framebuffer[i] != psramBackBuffer[i]) {
            framebuffer[i] = psramBackBuffer[i];  // 複製到 SRAM 顯示緩衝區
        }
    }
}

void smartMemoryUsage() {
    // 記憶體使用分層策略：
    // Layer 1 (最快): SRAM - 當前顯示緩衝區、常用快取
    // Layer 2 (大容量): PSRAM - 圖片預載、背景處理
    // Layer 3 (永久): Flash - 資源庫、程式碼、常數資料

    static uint32_t lastMemoryCheck = 0;
    if (millis() - lastMemoryCheck > 5000) {  // 每5秒檢查一次
        lastMemoryCheck = millis();

        Serial.println("=== 記憶體使用狀況 ===");
        Serial.printf("🔹 SRAM 剩餘: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("🔹 PSRAM 剩餘: %d bytes\n", ESP.getFreePsram());
        Serial.printf("🔹 Flash 程式大小: %d bytes\n", ESP.getSketchSize());
        Serial.printf("🔹 Flash 剩餘: %d bytes\n", ESP.getFreeSketchSpace());

        // 自動記憶體清理
        if (ESP.getFreePsram() < 1024 * 100) {  // 少於100KB時清理
            // 清理最舊的快取
            Serial.println("🧹 執行記憶體清理...");
        }
    }
}
*/
/*
程式碼解析重點
這個程式是一個基於 ESP32 的電子紙顯示器控制系統，主要功能包括：

🔋 電源管理
使用深度睡眠模式最大化電池壽命
透過 GPIO 喚醒機制實現低功耗操作
🖥️ 顯示控制
動態分配影像緩衝區管理記憶體
支援灰階圖像顯示和文字渲染
繪製使用者介面元素（按鈕、邊框等）
🎮 使用者互動
多按鈕支援（ESP32 支援 3 個按鈕，ESP32-S3 支援 1 個）
狀態機制循環顯示不同內容
直觀的 "Prev"/"Next" 按鈕介面
⚠️ 注意事項
記憶體分配失敗會導致程式停止運行
不同 ESP32 型號使用不同的喚醒 GPIO
電子紙更新後需要關閉電源以節省電力
*/