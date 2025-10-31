using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.IO.Ports;
using System.Threading;

namespace ArduinoImageSender
{
    /// <summary>
    /// Arduino UNO GC9A01A 圖片傳送程式
    /// .NET Framework 4.6.0 相容版本
    /// 
    /// 編譯指令:
    /// csc /target:exe /reference:System.Drawing.dll /reference:System.dll Program.cs
    /// 
    /// 或在Visual Studio中建立Console應用程式專案並貼上此程式碼
    /// </summary>
    class Program
    {
        // GC9A01A 顯示器規格
        private const int DISPLAY_WIDTH = 240;
        private const int DISPLAY_HEIGHT = 240;
        private const int BAUD_RATE = 115200;

        static void Main(string[] args)
        {
            Console.OutputEncoding = System.Text.Encoding.UTF8;

            Console.WriteLine("==========================================");
            Console.WriteLine("Arduino UNO GC9A01A 圖片傳送器");
            Console.WriteLine("支援 .NET Framework 4.6.0");
            Console.WriteLine("==========================================\n");

            try
            {
                // 顯示可用串列埠
                ShowAvailablePorts();

                // 取得使用者輸入
                string portName = GetUserInput("請輸入串列埠 (例如: COM3): ");
                string imagePath = GetUserInput("請輸入圖片路徑: ").Trim('"');

                // 檢查檔案
                if (!File.Exists(imagePath))
                {
                    Console.WriteLine("錯誤: 找不到圖片檔案!");
                    Console.ReadKey();
                    return;
                }

                // 傳送圖片
                SendImage(portName, imagePath);
            }
            catch (Exception ex)
            {
                Console.WriteLine("錯誤: " + ex.Message);
            }

            Console.WriteLine("\n按任意鍵退出...");
            Console.ReadKey();
        }

        static void ShowAvailablePorts()
        {
            string[] ports = SerialPort.GetPortNames();
            Console.WriteLine("可用串列埠:");

            if (ports.Length == 0)
            {
                Console.WriteLine("  (沒有找到串列埠)");
            }
            else
            {
                for (int i = 0; i < ports.Length; i++)
                {
                    Console.WriteLine("  " + ports[i]);
                }
            }
            Console.WriteLine();
        }

        static string GetUserInput(string prompt)
        {
            Console.Write(prompt);
            return Console.ReadLine();
        }

        static void SendImage(string portName, string imagePath)
        {
            SerialPort port = null;

            try
            {
                Console.WriteLine("\n連接到 " + portName + "...");

                port = new SerialPort(portName, BAUD_RATE);
                port.Timeout = 5000;
                port.Open();

                Console.WriteLine("連接成功! 等待Arduino準備...");
                Thread.Sleep(3000);

                Console.WriteLine("載入圖片: " + Path.GetFileName(imagePath));

                using (Bitmap original = new Bitmap(imagePath))
                {
                    Console.WriteLine(string.Format("原始尺寸: {0}x{1}", original.Width, original.Height));

                    using (Bitmap resized = ResizeImage(original))
                    {
                        Console.WriteLine(string.Format("調整尺寸: {0}x{1}", DISPLAY_WIDTH, DISPLAY_HEIGHT));
                        Console.WriteLine("\n開始傳送...");

                        SendPixels(port, resized);
                    }
                }

                Console.WriteLine("\n傳送完成!");
            }
            catch (Exception ex)
            {
                Console.WriteLine("錯誤: " + ex.Message);
            }
            finally
            {
                if (port != null && port.IsOpen)
                {
                    port.Close();
                }
                if (port != null)
                {
                    port.Dispose();
                }
            }
        }

        static Bitmap ResizeImage(Bitmap original)
        {
            Bitmap result = new Bitmap(DISPLAY_WIDTH, DISPLAY_HEIGHT);

            using (Graphics g = Graphics.FromImage(result))
            {
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.SmoothingMode = SmoothingMode.HighQuality;
                g.DrawImage(original, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
            }

            return result;
        }

        static void SendPixels(SerialPort port, Bitmap bitmap)
        {
            int total = DISPLAY_WIDTH * DISPLAY_HEIGHT;
            int sent = 0;
            int lastPercent = -1;

            for (int y = 0; y < DISPLAY_HEIGHT; y++)
            {
                for (int x = 0; x < DISPLAY_WIDTH; x++)
                {
                    Color pixel = bitmap.GetPixel(x, y);
                    ushort rgb565 = ToRGB565(pixel);

                    // 傳送2個bytes (Big Endian)
                    port.Write(new byte[] {
                        (byte)(rgb565 >> 8),
                        (byte)(rgb565 & 0xFF)
                    }, 0, 2);

                    sent++;

                    // 顯示進度
                    int percent = (sent * 100) / total;
                    if (percent != lastPercent)
                    {
                        Console.Write(string.Format("\r進度: {0}% ({1}/{2})", percent, sent, total));
                        lastPercent = percent;
                    }

                    // 每100個像素延遲1ms避免Arduino緩衝區溢出
                    if (sent % 100 == 0)
                    {
                        Thread.Sleep(1);
                    }
                }
            }
        }

        static ushort ToRGB565(Color color)
        {
            // RGB888 轉 RGB565
            byte r = (byte)(color.R >> 3);  // 8位 -> 5位
            byte g = (byte)(color.G >> 2);  // 8位 -> 6位  
            byte b = (byte)(color.B >> 3);  // 8位 -> 5位

            return (ushort)((r << 11) | (g << 5) | b);
        }
    }
}