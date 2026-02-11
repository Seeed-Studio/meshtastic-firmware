/*
 * Seeed Tracker T2000 - nRF52840 + SX1262 + L76K GPS
 *
 * Hardware Features:
 *  - MCU: nRF52840 (ARM Cortex-M4F, BLE 5.0, NFC)
 *  - LoRa: Semtech SX1262 with SKY13453 RF switch
 *  - GNSS: Quectel L76K (GPS/BeiDou/GLONASS)
 *  - IMU: ST LSM6DSOWTR (6-axis accelerometer/gyroscope)
 *  - Hall Sensor: TI DRV5032
 *  - Flash: 2MB QSPI (P25Q16SH)
 *  - Power: Dual 18500 Li-ion batteries, solar input (0.5W)
 *  - Charging: CN3165 USB charger
 *  - DC-DC: TI TPS628438 (3.0V output)
 *
 * Created: 2026-01-14
 * License: GPL-3.0
 */

#ifndef _VARIANT_SEEED_TRACKER_T2000_H_
#define _VARIANT_SEEED_TRACKER_T2000_H_

#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  DEBUG: Temporarily disable I2C to test startup
//  Uncomment the next line to skip I2C scan and verify boot works
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// #define MESHTASTIC_EXCLUDE_I2C 1

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Clock Configuration
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define VARIANT_MCK (64000000ul) // Master clock frequency
#define USE_LFXO                 // 32.768kHz crystal for LFCLK

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Pin Capacity Definitions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define PINS_COUNT (48u)
#define NUM_DIGITAL_PINS (48u)
#define NUM_ANALOG_INPUTS (8u)
#define NUM_ANALOG_OUTPUTS (0u)

// Use native nrf52 USB power detection
#define NRF_APM

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  LED Configuration (RGB LED)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// RGB LED on P0.19, P1.02, P1.03
#define PIN_LED1 (0 + 19)  // P0.19 - Red
#define PIN_LED2 (32 + 2)  // P1.02 - Green
#define PIN_LED3 (32 + 3)  // P1.03 - Blue

#define LED_BUILTIN PIN_LED1
#define LED_RED PIN_LED1
#define LED_GREEN PIN_LED2
#define LED_BLUE PIN_LED3

#define LED_STATE_ON 1     // State when LED is lit

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Button Configuration
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define BUTTON_PIN (0 + 11)        // P0.11 - Mode Key
#define BUTTON_PIN_TAMPER (0 + 8)  // P0.08 - Tamper Key
#define BUTTON_ACTIVE_LOW true
#define BUTTON_ACTIVE_PULLUP true

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Power Control
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define PIN_PWR_ON (0 + 12)        // P0.12 - PMOS power switch control
#define PIN_SENSOR_PWR (0 + 15)    // P0.15 - Sensor power control (MUST stay HIGH always)

// Wait for peripherals to stabilize after power is applied
// LSM6DSOWTR IMU needs time after Sensor_PWR is enabled before I2C is ready
#define PERIPHERAL_WARMUP_MS 1000

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  I2C Configuration (Sensors - LSM6DSOWTR)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// DEBUG: Set to 0 to disable I2C scan and test boot
// Change back to 1 when I2C hardware issue is resolved
#define HAS_WIRE 0
#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA (0 + 6)   // P0.06
#define PIN_WIRE_SCL (0 + 5)   // P0.05
#define I2C_NO_RESCAN

static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

// IMU Sensor Interrupts
#define PIN_IMU_INT1 (0 + 13)  // P0.13 - IMU Interrupt 1
#define PIN_IMU_INT2 (0 + 14)  // P0.14 - IMU Interrupt 2

// IMU sensor (LSM6DSOWTR) - 6-axis accelerometer/gyroscope
#define HAS_LSM6DS3
#define LSM6DS3_WAKE_THRESH 20

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  SPI Configuration (SX1262 LoRa)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define SPI_INTERFACES_COUNT 1

#define PIN_SPI_MISO (0 + 3)   // P0.03
#define PIN_SPI_MOSI (0 + 28)  // P0.28
#define PIN_SPI_SCK (0 + 30)   // P0.30

static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t SCK = PIN_SPI_SCK;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  LoRa Module (SX1262) Configuration
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define USE_SX1262

#define SX126X_CS (32 + 14)              // P1.14 - Chip Select
#define SX126X_DIO1 (0 + 7)              // P0.07 - Interrupt/DIO1
#define SX126X_BUSY (32 + 10)            // P1.10 - Busy Status
#define SX126X_RESET (32 + 7)            // P1.07 - Reset

// TCXO voltage (adjust based on your module)
#define SX126X_DIO3_TCXO_VOLTAGE 1.8

// RF Switch control (SKY13453)
// DIO2 controls the RF switch
#define SX126X_DIO2_AS_RF_SWITCH

// If your board uses separate RX/TX enable pins, uncomment and configure:
// #define SX126X_RXEN (xx)
// #define SX126X_TXEN (xx)

#define LORA_CS SX126X_CS
#define LORA_DIO1 SX126X_DIO1
#define LORA_BUSY SX126X_BUSY
#define LORA_RESET SX126X_RESET
#define LORA_SCK PIN_SPI_SCK
#define LORA_MISO PIN_SPI_MISO
#define LORA_MOSI PIN_SPI_MOSI

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  GNSS Module (L76K) Configuration
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define HAS_GPS 1
#define GPS_L76K

// UART pins
#define GPS_TX_PIN (0 + 27)    // P0.27 - nRF TX -> GPS RX
#define GPS_RX_PIN (0 + 26)    // P0.26 - nRF RX <- GPS TX
#define PIN_SERIAL1_TX GPS_TX_PIN
#define PIN_SERIAL1_RX GPS_RX_PIN

// GPS control pins
#define GPS_EN (32 + 5)        // P1.05 - GPS Enable
#define PIN_GPS_EN GPS_EN
#define GPS_EN_ACTIVE 1        // Active HIGH to enable GPS

#define GPS_RESET (32 + 6)     // P1.06 - GPS Reset
#define PIN_GPS_RESET GPS_RESET
#define GPS_RESET_MODE 0       // Active LOW to reset GPS

#define GPS_WAKEUP (32 + 9)    // P1.09 - GPS Wakeup
#define PIN_GPS_WAKEUP GPS_WAKEUP

// GPS configuration
#define GPS_BAUDRATE 9600
#define GPS_THREAD_INTERVAL 50

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  WiFi Module (ESP8684H2X) - Reserved
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// WiFi module is optional/reserved position
#define PIN_WIFI_EN (32 + 4)   // P1.04 - WiFi Enable
#define PIN_WIFI_RST (32 + 0)  // P1.00 - WiFi Reset
#define PIN_SERIAL2_TX (0 + 16) // P0.16 - WiFi TX
#define PIN_SERIAL2_RX (0 + 17) // P0.17 - WiFi RX

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Battery & Power Management
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define BATTERY_PIN (0 + 31)   // P0.31 - Battery ADC input
#define BAT_READ (0 + 2)       // P0.02 - Battery ADC control

#define ADC_MULTIPLIER 2.0     // Voltage divider ratio (adjust based on actual circuit)
#define BATTERY_SENSE_RESOLUTION_BITS 12
#define ADC_RESOLUTION 14

#undef AREF_VOLTAGE
#define AREF_VOLTAGE 3.0
#define VBAT_AR_INTERNAL AR_INTERNAL_3_0

// Open Circuit Voltage table for Li-ion battery (mV)
// 100%, 90%, 80%, 70%, 60%, 50%, 40%, 30%, 20%, 10%, 0%
#define OCV_ARRAY 4200, 4050, 3970, 3890, 3820, 3760, 3710, 3680, 3620, 3500, 3000

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  NFC Configuration
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define PIN_NFC1 (0 + 9)   // P0.09
#define PIN_NFC2 (0 + 10)  // P0.10

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  QSPI Flash (P25Q16SH - 2MB)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define PIN_QSPI_SCK (0 + 19)  // P0.19
#define PIN_QSPI_CS (32 + 13)  // P1.13
#define PIN_QSPI_IO0 (0 + 20)  // P0.20 - Data 0
#define PIN_QSPI_IO1 (0 + 21)  // P0.21 - Data 1
#define PIN_QSPI_IO2 (0 + 22)  // P0.22 - Data 2
#define PIN_QSPI_IO3 (0 + 23)  // P0.23 - Data 3

#define EXTERNAL_FLASH_DEVICES P25Q16H
#define EXTERNAL_FLASH_USE_QSPI

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  USB Configuration
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Native USB on nRF52840
// VBUS detection on P0.29
#define PIN_USB_VBUS_DETECT (0 + 29)

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Display Configuration
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// No display on this board
#define HAS_SCREEN 0

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Analog Pin Definitions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define PIN_A0 (0 + 2)   // P0.02/AIN0 - Battery ADC control
#define PIN_A1 (0 + 3)   // P0.03/AIN1
#define PIN_A2 (0 + 28)  // P0.28/AIN4
#define PIN_A3 (0 + 29)  // P0.29/AIN5
#define PIN_A4 (0 + 30)  // P0.30/AIN6
#define PIN_A5 (0 + 31)  // P0.31/AIN7 - Battery voltage

#define PIN_VBAT PIN_A5

#ifdef __cplusplus
}
#endif

#endif // _VARIANT_SEEED_TRACKER_T2000_H_
