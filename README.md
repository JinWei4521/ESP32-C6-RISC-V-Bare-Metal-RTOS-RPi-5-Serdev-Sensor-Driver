# ESP32-C6 RISC-V Bare-Metal RTOS & RPi 5 Serdev Sensor Driver

本專案實作了一套跨越微控制器 (MCU) 與微處理器 (MPU) 的完整嵌入式通訊與作業系統架構。

在微控制器端，基於 **RISC-V 架構 (ESP32-C6)** 從零打造了一個具備搶佔式排程 (Preemptive Scheduling) 的裸機作業系統核心 (Bare-metal RTOS)；在微處理器端，為 **Raspberry Pi 5** 開發了基於 Linux 核心 **Serdev 子系統**的自定義驅動程式，透過 UART 實現雙向非同步的感測器數據收集與硬體控制。

## 🌟 核心技術與特色 (Key Features)

### 1. RISC-V 裸機作業系統 (OS Kernel Development)
* **底層架構控制**：深入 RISC-V 特權模式，自定義 `trap_handler.S` 處理中斷與異常，並利用 `mscratch` 暫存器實現零成本的 Trap Frame 指標切換。
* **搶佔式排程器 (Preemptive Scheduler)**：基於 `SYSTIMER` 硬體計時器實作系統心跳 (Tick)，支援任務的狀態轉換 (Ready, Running, Suspended, Blocked) 與精準的 Context Switch。
* **同步機制 (Synchronization)**：實作 `os_mutex` 互斥鎖，並具備基礎的阻塞與喚醒機制，安全保護 I2C 匯流排與 UART 傳輸資源，避免 Race Condition。
* **硬體中斷管理 (Interrupt Handling)**：實作 UART RXFIFO 中斷服務常式 (ISR)，直接在硬體層級攔截 Linux 端發送的控制指令，達成極低的延遲響應。

### 2. 裸機硬體驅動 (Bare-Metal Drivers)
* **暫存器級別 GPIO 控制**：不依賴高階 HAL 庫，直接操作記憶體映射暫存器 (Memory-Mapped Registers) 驅動硬體。
* **軟體 I2C (Software I2C)**：結合 RISC-V 週期計數器實現微秒級精準延遲，自主實作 I2C 通訊協定，成功驅動 AHT20 (溫濕度) 與 BMP280 (大氣壓力) 感測器。

### 3. Linux 核心驅動程式 (Linux Kernel Module)
* **Serdev 框架整合**：捨棄傳統 User-space 的 TTY 讀寫，將 UART 感測器註冊為 Linux 核心的 Character Device (`/dev/tty_sensor`)，由核心直接接管底層資料流。
* **核心併發保護**：使用 `spinlock` 保護接收緩衝區 (Ring Buffer)，並透過 `wait_queue_head_t` 處理 User-space 讀取時的休眠與喚醒，實現高效的非阻塞/阻塞 I/O。
* **IOCTL 硬體控制**：定義專屬的 Magic Number (`SENSOR_IOC_ENABLE`, `SENSOR_IOC_DISABLE`)，允許 User-space 程式直接向 ESP32-C6 派發任務啟停指令。
* **Device Tree Overlay (DTO)**：編寫 `.dts` 檔案並動態載入 `.dtbo`，實現驅動程式與硬體節點 (`mycompany,esp32-sensor`) 的優雅匹配。

## 📂 專案目錄結構

```text
esp32c6-rpi-sensor-system/
├── firmware/                   # ESP32-C6 裸機 OS 核心與感測器驅動        
│   └── main/
│       ├── main.c              # 系統初始化與 Task 創建
│       ├── task.c / task.h     # Context switch, 排程器與 Mutex 實作
│       ├── timer.c / timer.h   # RISC-V SYSTIMER 中斷設定
│       ├── trap_handler.S      # RISC-V 組合語言 Trap Entry
│       ├── uart.c / uart.h     # UART 中斷與底層傳輸
│       ├── soft_i2c.c / .h     # 暫存器級別軟體 I2C
│       ├── aht20.c / aht20.h   # AHT20 感測器驅動
│       └── bmp280.c / bmp280.h # BMP280 感測器驅動 (含補償算法)
├── linux_driver/               # Raspberry Pi 5 Serdev 核心驅動程式
│   ├── Makefile                
│   ├── c6_driver.c             # Linux Kernel Module 原始碼
│   ├── c6_driver.h             # IOCTL 定義檔
│   ├── test_sensor.c           # User-space 多執行緒互動測試程式
│   └── dts/
│       └── c6_sensor.dts       # Device Tree 描述檔
└── README.md
