# Seeed Tracker T2000

基于 nRF52840 的多功能物联网追踪器开发板，集成 LoRa (SX1262)、GNSS (L76K)、6轴 IMU 传感器，支持太阳能充电。

## 硬件规格

| 组件 | 型号 | 说明 |
|------|------|------|
| **MCU** | nRF52840 | ARM Cortex-M4F, 64MHz, BLE 5.0, NFC |
| **LoRa** | SX1262 + SKY13453 | 868/915MHz, 带 RF 开关 |
| **GNSS** | Quectel L76K | GPS/BeiDou/GLONASS |
| **IMU** | ST LSM6DSOWTR | 6轴加速度计/陀螺仪 |
| **霍尔传感器** | TI DRV5032 | 磁性检测 |
| **Flash** | P25Q16SH | 2MB QSPI |
| **电池** | 2x INR18500NP | 3.7V 锂电池并联 |
| **充电** | CN3165 | USB 充电管理 |
| **太阳能** | 0.5W | 太阳能板输入 |
| **DC-DC** | TPS628438 | 3.0V 输出 |

## 引脚映射

### LoRa 模块 (SX1262) - SPI

| 功能 | nRF 引脚 | 说明 |
|------|----------|------|
| CS | P1.14 | 片选 |
| MISO | P0.03 | SPI 数据输入 |
| MOSI | P0.28 | SPI 数据输出 |
| SCK | P0.30 | SPI 时钟 |
| RST | P1.07 | 复位 |
| BUSY | P1.10 | 忙状态 |
| DIO1 | P0.07 | 中断 |

### GNSS 模块 (L76K) - UART

| 功能 | nRF 引脚 | 说明 |
|------|----------|------|
| RX | P0.26 | 串口接收 |
| TX | P0.27 | 串口发送 |
| EN | P1.05 | 模块使能 |
| RST | P1.06 | 模块复位 |
| Wakeup | P1.09 | 唤醒引脚 |

### 传感器 (IMU) - I2C

| 功能 | nRF 引脚 |
|------|----------|
| SCL | P0.05 |
| SDA | P0.06 |
| INT1 | P0.13 |
| INT2 | P0.14 |
| PWR | P0.15 |

### LED & 按键

| 组件 | nRF 引脚 |
|------|----------|
| LED Red | P0.19 |
| LED Green | P1.02 |
| LED Blue | P1.03 |
| Mode Key | P0.11 |
| Tamper Key | P0.08 |

### 电源管理

| 功能 | nRF 引脚 |
|------|----------|
| PWR ON | P0.12 |
| BAT ADC Control | P0.02 |
| BAT ADC Input | P0.31 |

## 编译

```bash
# 进入 firmware 目录
cd meshtastic-firmware

# 编译固件
pio run -e seeed_tracker_t2000

# 编译并烧录
pio run -e seeed_tracker_t2000 -t upload
```

## 烧录方法

### 方法 1: USB DFU (推荐)

1. 连接 USB
2. 双击复位按钮进入 bootloader 模式
3. 运行 `pio run -e seeed_tracker_t2000 -t upload`

### 方法 2: J-Link

1. 连接 J-Link 调试器
2. 运行 `pio run -e seeed_tracker_t2000 -t upload --upload-port jlink`

## 注意事项

1. **天线连接**: 使用前请确保连接正确频段的 LoRa 和 GNSS 天线
2. **电源开关**: P0.12 控制 PMOS 电源开关
3. **WiFi 模块**: 预留位置，可能未贴片
4. **USB**: nRF52840 原生 USB 支持，P0.29 检测 VBUS

## 文件结构

```
variants/nrf52840/seeed_tracker_t2000/
├── variant.h        # 引脚定义
├── variant.cpp      # 引脚映射和初始化
├── platformio.ini   # 构建配置
└── README.md        # 本文档

boards/
└── seeed_tracker_t2000.json  # PlatformIO board 定义
```

## 许可证

GPL-3.0
