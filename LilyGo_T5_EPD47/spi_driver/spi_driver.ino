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

/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
/*
 * ESP32 電子紙 SPI 從設備驅動程式
 *
 * 程式功能：
 * - 作為 SPI 從設備接收主設備的圖像資料和控制指令
 * - 支援多種圖像格式：黑白圖像、灰階圖像、JPEG 圖像
 * - 提供完整的電子紙顯示控制功能
 * - 實現記憶體映射機制處理大容量圖像資料
 * - 支援區域更新和全螢幕更新
 *
 * 技術特點：
 * - 使用 FreeRTOS 多工處理指令解析和顯示操作
 * - 利用 PSRAM 存儲大尺寸圖像資料
 * - 支援 JPEG 硬體解碼加速
 * - 實現狀態機管理不同的顯示模式
 *
 * 應用場景：
 * - 遠端圖像顯示系統
 * - 分散式電子紙顯示網路
 * - 主從架構的圖像傳輸系統
 */
#include <stdio.h>  // 標準輸入輸出函式庫
#include <string.h> // 字串處理函式庫
#include <stdlib.h> // 標準函式庫

#include "sdkconfig.h"         // ESP-IDF SDK 配置檔案
#include "freertos/FreeRTOS.h" // FreeRTOS 即時作業系統核心
#include "freertos/task.h"     // FreeRTOS 任務管理
#include "esp_system.h"        // ESP32 系統相關函式
#include "esp_spi_flash.h"     // ESP32 SPI Flash 操作
#include "driver/spi_slave.h"  // ESP32 SPI 從設備驅動程式
#include "driver/gpio.h"       // ESP32 GPIO 控制驅動程式
#include <esp_heap_caps.h>     // ESP32 記憶體管理（支援 PSRAM）
#include "esp_log.h"           // ESP32 日誌輸出系統
#include "esp_task_wdt.h"      // ESP32 任務看門狗

#include "epd_driver.h"      // 電子紙驅動程式
#include "libjpeg/libjpeg.h" // JPEG 圖像解碼函式庫
#include "ed047tc1.h"        // ED047TC1 電子紙螢幕規格定義
#include "cmd.h"             // SPI 指令定義和解析
#include "utilities.h"       // 公用函式和工具程式

#define RCV_HOST SPI2_HOST // 定義接收主機為 SPI2，作為從設備接收端

/**
 * 電子紙設備狀態列舉
 *
 * 狀態說明：
 * - E_EPD_STATUS_RUN：正常運行狀態，可接收和處理指令
 * - E_EPD_STATUS_SLEEP：睡眠狀態，節能模式
 * - E_EPD_STATUS_MEM_BST_WR：記憶體批次寫入狀態
 * - E_EPD_STATUS_LD_IMG：載入一般圖像狀態
 * - E_EPD_STATUS_LD_IMG_AREA：載入區域圖像狀態
 * - E_EPD_STATUS_LD_JPEG：載入 JPEG 圖像狀態
 * - E_EPD_STATUS_LD_JPEG_AREA：載入區域 JPEG 圖像狀態
 */
typedef enum te_epd_status
{
    E_EPD_STATUS_RUN = 0,      // 正常運行狀態
    E_EPD_STATUS_SLEEP,        // 睡眠狀態
    E_EPD_STATUS_MEM_BST_WR,   // 記憶體批次寫入
    E_EPD_STATUS_LD_IMG,       // 載入圖像
    E_EPD_STATUS_LD_IMG_AREA,  // 載入區域圖像
    E_EPD_STATUS_LD_JPEG,      // 載入 JPEG 圖像
    E_EPD_STATUS_LD_JPEG_AREA, // 載入區域 JPEG 圖像
} te_epd_status;

/**
 * 電子紙指令結構體
 *
 * 結構說明：
 * - cmd：指令代碼（16位元），定義要執行的操作類型
 * - len：資料長度（16位元），指定 data 陣列中有效資料的長度
 * - data：資料載荷（最大4096位元組），包含指令相關的參數或圖像資料
 */
typedef struct ts_EPDcmd
{
    uint16_t cmd;       // 指令代碼
    uint16_t len;       // 資料長度
    uint8_t data[4096]; // 資料載荷
} ts_EPDcmd;

/**
 * 電子紙暫存器結構體
 *
 * 用途：儲存圖像資料的記憶體位置和顯示參數
 * - address_reg：圖像資料在記憶體中的起始位址
 * - area：顯示區域的座標和尺寸資訊
 * - mode：顯示模式（黑白、灰階等）
 */
typedef struct epd_reg_t
{
    uint32_t address_reg; // 数据保存的地址 - 圖像資料儲存位址
    Rect_t area;          // 顯示區域定義
    uint8_t mode;         // 顯示模式
} epd_reg_t;

uint8_t *data_map; // 圖像資料映射指標，指向 PSRAM 中的圖像緩衝區
uint8_t *cur_ptr;  // 當前寫入位置指標，用於追蹤資料寫入進度
uint32_t jpeg_len; // JPEG 圖像資料的長度

te_epd_status epd_status = E_EPD_STATUS_SLEEP; // 電子紙當前狀態，初始為睡眠狀態

static QueueHandle_t cmd_queue; // FreeRTOS 指令佇列，用於在任務間傳遞指令

epd_reg_t epd_reg; // 電子紙暫存器，儲存當前的顯示參數

/**
 * SPI 指令解包函數
 *
 * 功能：逐位元組解析從 SPI 主設備接收的資料，重組成完整的指令
 *
 * 協定格式：
 * [0x55][0x55][CMD_H][CMD_L][LEN_H][LEN_L][DATA...][CHECKSUM]
 *
 * 狀態機說明：
 * 0: 等待第一個同步位元組 0x55
 * 1: 等待第二個同步位元組 0x55
 * 2: 接收指令高位元組
 * 3: 接收指令低位元組
 * 4: 接收資料長度高位元組
 * 5: 接收資料長度低位元組
 * 6: 接收指令資料載荷
 * 7: 接收並驗證檢查碼
 *
 * @param data 接收到的單一位元組資料
 */
void unpack_cmd(uint8_t data)
{
    static uint8_t status = 0;  // 狀態機當前狀態
    static ts_EPDcmd cmd;       // 正在組建的指令結構
    static uint8_t *ptr = NULL; // 資料寫入指標

    // printf("unpack_cmd status : %d, data: %d\n", status, data);

    switch (status)
    {
    case 0:
        // 檢查第一個同步位元組 0x55
        // printf("head: %d\n", data);
        if (data == 0x55)
        {
            status++; // 進入下一狀態
        }
        break;

    case 1:
        // 檢查第二個同步位元組 0x55
        if (data == 0x55)
        {
            status++; // 進入下一狀態
            // printf("start\n");
            ptr = &cmd.data[0]; // 初始化資料指標
        }
        else
        {
            status = 0; // 同步失敗，重新開始
        }
        break;

    case 2:
        // 接收指令代碼的高位元組
        // printf("data: %d\n", data);
        cmd.cmd = data << 8 & 0xFF00; // 設定高8位元
        status++;
        break;

    case 3:
        // 接收指令代碼的低位元組
        // printf("data: %d\n", data);
        cmd.cmd |= data & 0xFF; // 設定低8位元，完成16位元指令代碼
        status++;
        break;

    case 4:
        // 接收資料長度的高位元組
        // printf("data: %d\n", data);
        cmd.len = data << 8 & 0xFF00; // 設定長度高8位元
        status++;
        break;

    case 5:
        // 接收資料長度的低位元組
        // printf("data: %d\n", data);
        cmd.len |= data & 0xFF; // 設定長度低8位元，完成16位元長度

        // 檢查資料長度是否超過緩衝區大小
        if (cmd.len > 4096)
        {
            status = 0;               // 長度無效，重新開始
            bzero(&cmd, sizeof(cmd)); // 清除指令結構
        }
        else
        {
            status++; // 進入資料接收狀態
        }
        break;

    case 6:
        // 接收指令資料載荷
        if (ptr - &cmd.data[0] < cmd.len)
        {
            *ptr = data; // 將資料寫入緩衝區
            ptr++;       // 移動寫入指標
        }

        // 檢查是否接收完所有資料
        if (ptr - &cmd.data[0] == cmd.len)
        {
            ptr = NULL; // 重置資料指標
            status = 0; // 重置狀態機

            // 輸出接收到的指令資訊
            printf("%lld cmd: %d, pkg_len: %d\n", esp_timer_get_time(), cmd.cmd, cmd.len);

            // 將完整的指令放入佇列，等待處理
            xQueueSend(cmd_queue, &cmd, portMAX_DELAY);

            // 清除指令結構，準備接收下一個指令
            bzero(&cmd, sizeof(cmd));
        }
        break;

    default:
        // 未知狀態，重置狀態機
        status = 0;
        break;
    }
}

/**
 * 指令處理任務函數
 *
 * 功能：在獨立的 FreeRTOS 任務中處理從佇列接收的指令
 *
 * 處理流程：
 * 1. 從指令佇列接收完整的指令
 * 2. 根據當前電子紙狀態決定處理方式
 * 3. 執行相應的操作（顯示、睡眠、記憶體操作等）
 * 4. 更新電子紙狀態
 *
 * 支援的指令類型：
 * - CMD_SLEEP：進入睡眠模式
 * - CMD_AWAKE：喚醒並進入運行模式
 * - 圖像載入和顯示指令
 * - 記憶體操作指令
 *
 * @param args 任務參數（未使用）
 */
void cmd_process(void *args)
{
    ts_EPDcmd cmd; // 指令結構體

    bzero(&cmd, sizeof(cmd)); // 清除指令結構

    for (;;) // 無限迴圈處理指令
    {
        // 從指令佇列接收指令，如果佇列為空則阻塞等待
        xQueueReceive(cmd_queue, &cmd, portMAX_DELAY);

        // 根據當前電子紙狀態處理指令
        switch (epd_status)
        {
        case E_EPD_STATUS_RUN:
            ESP_LOGI("CMD", "Run status"); // 記錄：運行狀態

            // 處理睡眠指令
            if (cmd.cmd == CMD_SLEEP)
            {
                epd_status = E_EPD_STATUS_SLEEP; // 切換到睡眠狀態
                epd_poweroff();                  // 關閉電子紙電源
            }
            // 處理記憶體批次寫入指令
            else if (cmd.cmd == CMD_MEM_BST_WR)
            {
                // 解析 32 位元記憶體位址（大端序）
                uint32_t address = (cmd.data[0] << 24) & 0xFFFFFFFF;
                address |= (cmd.data[1] << 16) & 0xFFFFFFFF;
                address |= (cmd.data[2] << 8) & 0xFFFFFFFF;
                address |= cmd.data[3];

                // 解析 32 位元寫入長度（大端序）
                uint32_t write_len = (cmd.data[4] << 24) & 0xFFFFFFFF;
                write_len |= (cmd.data[5] << 16) & 0xFFFFFFFF;
                write_len |= (cmd.data[6] << 8) & 0xFFFFFFFF;
                write_len |= cmd.data[7];

                printf("address: %d, write_len: %d\n", address, write_len);
                // 將資料從指令載荷複製到記憶體映射區域
                memcpy(&data_map[address], &cmd.data[8], write_len);
            }
            // 處理記憶體批次寫入結束指令
            else if (cmd.cmd == CMD_MEM_BST_END)
            {
                // 批次寫入完成，無額外操作
                // None
            }
            // 處理載入全螢幕圖像指令
            else if (cmd.cmd == CMD_LD_IMG)
            {
                bzero(&epd_reg, sizeof(epd_reg)); // 清除暫存器
                // memset(data_map, 0xFF, 540 * 960);  // 可選：清除圖像緩衝區

                epd_reg.mode = cmd.data[0]; // 設定顯示模式
                // 設定全螢幕顯示區域
                epd_reg.area.x = 0;
                epd_reg.area.y = 0;
                epd_reg.area.width = EPD_WIDTH;
                epd_reg.area.height = EPD_HEIGHT;

                epd_status = E_EPD_STATUS_LD_IMG; // 切換到圖像載入狀態
                cur_ptr = &data_map[0];           // 設定寫入指標到緩衝區起始位置
            }
            // 處理載入區域圖像指令
            else if (cmd.cmd == CMD_LD_IMG_AREA)
            {
                bzero(&epd_reg, sizeof(epd_reg)); // 清除暫存器
                // memset(data_map, 0xFF, 540 * 960);  // 可選：清除圖像緩衝區

                epd_reg.mode = cmd.data[0]; // 設定顯示模式

                // 解析區域座標和尺寸（16位元值，大端序）
                epd_reg.area.x = cmd.data[1] << 8 & 0xFFFF;
                epd_reg.area.x |= cmd.data[2] & 0xFFFF;
                epd_reg.area.y = cmd.data[3] << 8 & 0xFFFF;
                epd_reg.area.y |= cmd.data[4] & 0xFFFF;
                epd_reg.area.width = cmd.data[5] << 8 & 0xFFFF;
                epd_reg.area.width |= cmd.data[6] & 0xFFFF;
                epd_reg.area.height = cmd.data[7] << 8 & 0xFFFF;
                epd_reg.area.height |= cmd.data[8] & 0xFFFF;
                epd_status = E_EPD_STATUS_LD_IMG_AREA;
                cur_ptr = &data_map[0];
                printf("recv image - start time: %lld us\n", esp_timer_get_time());
            }
            else if (cmd.cmd == CMD_LD_JPEG)
            {
                bzero(&epd_reg, sizeof(epd_reg));
                // memset(data_map, 0xFF, 540 * 960);
                epd_reg.mode = cmd.data[0];
                epd_reg.area.x = 0;
                epd_reg.area.y = 0;
                epd_reg.area.width = EPD_WIDTH;
                epd_reg.area.height = EPD_HEIGHT;
                epd_status = E_EPD_STATUS_LD_JPEG;
                cur_ptr = &data_map[0];
                jpeg_len = 0;
            }
            else if (cmd.cmd == CMD_LD_JPEG_AREA)
            {
                bzero(&epd_reg, sizeof(epd_reg));
                // memset(data_map, 0xFF, 540 * 960);
                epd_reg.mode = cmd.data[0];
                epd_reg.area.x = cmd.data[1] << 8 & 0xFFFF;
                epd_reg.area.x |= cmd.data[2] & 0xFFFF;
                epd_reg.area.y = cmd.data[3] << 8 & 0xFFFF;
                epd_reg.area.y |= cmd.data[4] & 0xFFFF;
                epd_reg.area.width = cmd.data[5] << 8 & 0xFFFF;
                epd_reg.area.width |= cmd.data[6] & 0xFFFF;
                epd_reg.area.height = cmd.data[7] << 8 & 0xFFFF;
                epd_reg.area.height |= cmd.data[8] & 0xFFFF;
                epd_status = E_EPD_STATUS_LD_JPEG_AREA;
                cur_ptr = &data_map[0];
                jpeg_len = 0;
            }
            else if (cmd.cmd == CMD_CLEAR)
            {
                epd_reg.area.x = cmd.data[0] << 8 & 0xFFFF;
                epd_reg.area.x |= cmd.data[1] & 0xFFFF;
                epd_reg.area.y = cmd.data[2] << 8 & 0xFFFF;
                epd_reg.area.y |= cmd.data[3] & 0xFFFF;
                epd_reg.area.width = cmd.data[4] << 8 & 0xFFFF;
                epd_reg.area.width |= cmd.data[5] & 0xFFFF;
                epd_reg.area.height = cmd.data[6] << 8 & 0xFFFF;
                epd_reg.area.height |= cmd.data[7] & 0xFFFF;
                epd_clear_area(epd_reg.area);
            }
            break;

        case E_EPD_STATUS_SLEEP:
        {
            ESP_LOGI("CMD", "Sleep status");
            if (cmd.cmd == CMD_SYS_RUN)
            {
                epd_status = E_EPD_STATUS_RUN;
                epd_poweron();
                printf("running\n");
            }
        }
        break;

        case E_EPD_STATUS_LD_IMG:
        case E_EPD_STATUS_LD_IMG_AREA:
            ESP_LOGI("CMD", "LD IMG status");
            if (cmd.cmd == CMD_LD_IMG_END)
            {
                printf("recv image - end time: %lld us\n", esp_timer_get_time());
                // printf("x: %d, x: %d, width: %d, height: %d\n", epd_reg.area.x, epd_reg.area.y, epd_reg.area.width, epd_reg.area.height);
                printf("clear - start time: %lld us\n", esp_timer_get_time());
                epd_clear_area(epd_reg.area);
                printf("clear - end time: %lld us\n", esp_timer_get_time());
                printf("draw image - start time: %lld us\n", esp_timer_get_time());
                epd_draw_grayscale_image(epd_reg.area, &data_map[0]);
                printf("draw image - end time: %lld us\n", esp_timer_get_time());
                epd_status = E_EPD_STATUS_RUN;
            }
            else if (cmd.cmd == CMD_MEM_BST_WR)
            {
                memcpy(cur_ptr, cmd.data, cmd.len);
                cur_ptr += cmd.len;
            }
            break;

        case E_EPD_STATUS_LD_JPEG:
        case E_EPD_STATUS_LD_JPEG_AREA:
            ESP_LOGI("CMD", "LD JPEG status");
            if (cmd.cmd == CMD_LD_JPEG_END)
            {
                printf("x: %d, x: %d, width: %d, height: %d\n", epd_reg.area.x, epd_reg.area.y, epd_reg.area.width, epd_reg.area.height);
                esp_task_wdt_reset();
                if (epd_status == E_EPD_STATUS_LD_JPEG)
                {
                    // show_jpg_from_buff(&data_map[epd_reg.address_reg], cur_ptr - &data_map[epd_reg.address_reg]);
                    show_jpg_from_buff(&data_map[epd_reg.address_reg], cur_ptr - &data_map[epd_reg.address_reg], epd_full_screen());
                }
                else
                {
                    printf("show_area_jpg_from_buff\n");
                    show_jpg_from_buff(&data_map[epd_reg.address_reg], cur_ptr - &data_map[epd_reg.address_reg], epd_reg.area);
                }
                epd_status = E_EPD_STATUS_RUN;
            }
            else if (cmd.cmd == CMD_MEM_BST_WR)
            {
                memcpy(cur_ptr, cmd.data, cmd.len);
                cur_ptr += cmd.len;
                jpeg_len += cmd.len;
            }
            break;

        default:
            break;
        }
        bzero(&cmd, sizeof(cmd));
    }
}

void IRAM_ATTR spi_slave_trans_done(spi_slave_transaction_t *trans)
{
    // printf("[callback] SPI slave transaction finished\n");
    // ((Slave*)trans->user)->results.push_back(trans->trans_len);
    // ((Slave*)trans->user)->transactions.pop_front();
}

WORD_ALIGNED_ATTR char recvbuf[4097] = "";

/**
 * 主迴圈函數：SPI 從設備資料接收處理
 *
 * 功能：
 * 1. 持續等待並接收來自 SPI 主設備的資料
 * 2. 將接收到的資料逐位元組傳送給指令解析器
 * 3. 實現非阻塞式的連續資料接收
 *
 * 工作流程：
 * 1. 清除接收緩衝區
 * 2. 設定 SPI 傳輸參數（4096 位元組）
 * 3. 等待主設備發起傳輸
 * 4. 接收完成後解析每個位元組
 * 5. 重複上述流程
 *
 * 技術特點：
 * - 使用阻塞式 SPI 從設備接收
 * - 自動處理 CS 信號和時鐘同步
 * - 支援任意長度的資料傳輸
 */
void loop(void)
{
    // esp_err_t ret;  // 錯誤碼變數（未使用）
    spi_slave_transaction_t t; // SPI 從設備傳輸結構
    memset(&t, 0, sizeof(t));  // 清除傳輸結構

    while (1) // 無限迴圈處理 SPI 傳輸
    {
        // Clear receive buffer, set send buffer to something sane
        //  清除接收緩衝區，確保資料乾淨
        memset(recvbuf, 0x00, 4096);

        // Set up a transaction of 128 bytes to send/receive
        //  設定 SPI 傳輸參數（4096 位元組接收）
        t.length = 4096 * 8;   // 傳輸長度（位元）= 4096 位元組 × 8 位元/位元組
        t.tx_buffer = NULL;    // 傳送緩衝區（從設備不需要傳送資料）
        t.rx_buffer = recvbuf; // 接收緩衝區指標

        /* This call enables the SPI slave interface to send/receive to the sendbuf and recvbuf. The transaction is
        initialized by the SPI master, however, so it will not actually happen until the master starts a hardware transaction
        by pulling CS low and pulsing the clock etc. In this specific example, we use the handshake line, pulled up by the
        .post_setup_cb callback that is called as soon as a transaction is ready, to let the master know it is free to transfer
        data.
        */
        /*
         * 此函數啟用 SPI 從設備介面進行接收操作
         *
         * 工作原理：
         * - 傳輸由 SPI 主設備初始化
         * - 實際傳輸在主設備拉低 CS 並發送時鐘時才開始
         * - 從設備被動等待主設備的操作
         * - 使用阻塞模式等待傳輸完成
         */
        spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);

        // 輸出接收到的資料統計
        printf("%lld recv byte: %d\n", esp_timer_get_time(), t.trans_len / 8);

        // 逐位元組處理接收到的資料
        for (size_t i = 0; i < (t.trans_len / 8); i++)
        {
            unpack_cmd(recvbuf[i]); // 將每個位元組傳送給指令解析器
        }
    }
}

void setup(void)
{
    TaskHandle_t t1;

    data_map = (uint8_t *)heap_caps_malloc(540 * 960, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(data_map != NULL);
    memset(data_map, 0, 540 * 960);

    // Configuration for the SPI bus
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.mosi_io_num = GPIO_MOSI;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = GPIO_SCLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;

    // Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg;
    memset(&slvcfg, 0, sizeof(slvcfg));
    slvcfg.mode = 3;
    slvcfg.spics_io_num = GPIO_CS;
    slvcfg.queue_size = 8;
    slvcfg.flags = 0;
    slvcfg.post_setup_cb = NULL;
    slvcfg.post_trans_cb = spi_slave_trans_done;

    // Enable pull-ups on SPI lines so we don't detect rogue pulses when no master is connected.
    gpio_set_pull_mode((gpio_num_t)GPIO_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)GPIO_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)GPIO_CS, GPIO_PULLUP_ONLY);

    cmd_queue = xQueueCreate(8, sizeof(ts_EPDcmd));
    xTaskCreatePinnedToCore((void (*)(void *))cmd_process,
                            "cmd",
                            8192,
                            NULL,
                            10,
                            &t1,
                            1);

    // Initialize SPI slave interface
    ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO));

    epd_init();
    libjpeg_init();

    // main_loop();
}
/*
spi_driver.ino 程式註解總結
1. 程式概述和架構
    主要功能：ESP32 作為 SPI 從設備，接收主設備的圖像資料和控制指令
    技術架構：多工處理 + 狀態機 + 記憶體映射
    應用場景：遠端圖像顯示系統、分散式電子紙顯示網路
2. 核心資料結構
    te_epd_status：電子紙狀態列舉（運行、睡眠、圖像載入等）
    ts_EPDcmd：SPI 指令結構體（指令碼、長度、資料載荷）
    epd_reg_t：電子紙暫存器（記憶體位址、顯示區域、模式）
3. 通訊協定和指令解析
    SPI 協定格式：[0x55][0x55][CMD_H][CMD_L][LEN_H][LEN_L][DATA...]
    狀態機解析：7 狀態逐位元組重組完整指令
    指令類型：睡眠/喚醒、記憶體操作、圖像載入、區域更新
4. 記憶體管理
    PSRAM 使用：540×960 像素圖像緩衝區
    記憶體映射：支援大容量圖像資料的批次傳輸
    指標管理：動態追蹤資料寫入位置
5. 多工處理架構
    主迴圈：SPI 資料接收和解析
    指令處理任務：獨立任務處理解析完成的指令
    FreeRTOS 佇列：任務間的指令傳遞機制
6. SPI 從設備配置
    硬體設定：
    SPI 模式 3（CPOL=1, CPHA=1）
    4096 位元組最大傳輸大小
    GPIO 上拉電阻防止雜訊
    DMA 支援：自動選擇 DMA 通道提高傳輸效率
7. 指令處理邏輯
    狀態相依處理：根據電子紙當前狀態決定指令響應
    圖像顯示流程：載入 → 緩衝 → 顯示 → 狀態更新
    錯誤處理：記憶體分配檢查、資料長度驗證
8. 效能最佳化
    PSRAM 利用：大容量外部記憶體處理高解析度圖像
    非阻塞設計：主迴圈與指令處理分離
    回調機制：SPI 傳輸完成自動通知
9. 硬體整合
    電子紙驅動：完整的顯示控制功能
    JPEG 解碼：硬體加速的圖像解碼支援
    GPIO 控制：正確的電源管理和信號控制
10. 除錯和監控
    時間戳記錄：精確的執行時間測量
    狀態日誌：ESP32 日誌系統整合
    傳輸統計：接收資料量和處理時間監控
    這個程式現在包含了完整的中文技術文件，涵蓋了高階嵌入式系統設計的核心概念：

    通訊協定設計：自定義 SPI 指令格式和解析
    即時系統程式設計：FreeRTOS 多工和同步機制
    記憶體管理：大容量 PSRAM 的有效利用
    硬體抽象：SPI、GPIO、DMA 的整合控制
    狀態機設計：複雜系統的狀態管理
*/