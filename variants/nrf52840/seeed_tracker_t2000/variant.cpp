/*
 * Seeed Tracker T2000 - Pin Mapping
 *
 * This file defines the pin mapping array that maps logical digital pins
 * to physical GPIO ports/pins on the Nordic nRF52840 microcontroller.
 *
 * Pin mapping based on board_diss.md specification:
 *  - LoRa (SX1262): SPI interface with RF switch control
 *  - GNSS (L76K): UART interface
 *  - IMU (LSM6DSOWTR): I2C interface
 *  - RGB LED: 3 GPIO pins
 *  - Buttons: Mode key, Tamper key
 *  - QSPI Flash: 2MB external storage
 *
 * Created: 2026-01-14
 * License: GPL-3.0
 */

#include "variant.h"
#include "nrf.h"
#include "wiring_constants.h"
#include "wiring_digital.h"
#include <nrf_gpio.h>
#include <Arduino.h>

extern "C" {

/**
 * @brief Digital pin to GPIO port/pin mapping table
 *
 * nRF52840 pin numbering:
 * - P0.xx pins: 0 + xx (0-31)
 * - P1.xx pins: 32 + xx (32-47)
 */
const uint32_t g_ADigitalPinMap[] = {
    // P0.00 - P0.31
    0,   // D0  - P0.00 (XL1 - 32.768kHz crystal)
    1,   // D1  - P0.01 (XL2 - 32.768kHz crystal)
    2,   // D2  - P0.02 - BAT_ADC_CTRL
    3,   // D3  - P0.03 - SPI_MISO (LoRa)
    4,   // D4  - P0.04
    5,   // D5  - P0.05 - I2C_SCL (Sensors)
    6,   // D6  - P0.06 - I2C_SDA (Sensors)
    7,   // D7  - P0.07 - LORA_DIO1/IRQ
    8,   // D8  - P0.08 - TAMPER_KEY
    9,   // D9  - P0.09 - NFC1
    10,  // D10 - P0.10 - NFC2
    11,  // D11 - P0.11 - MODE_KEY (User Button)
    12,  // D12 - P0.12 - PWR_ON (PMOS control)
    13,  // D13 - P0.13 - IMU_INT1
    14,  // D14 - P0.14 - IMU_INT2
    15,  // D15 - P0.15 - SENSOR_PWR
    16,  // D16 - P0.16 - WIFI_TX (Reserved)
    17,  // D17 - P0.17 - WIFI_RX (Reserved)
    18,  // D18 - P0.18
    19,  // D19 - P0.19 - LED_RED / QSPI_SCK (shared)
    20,  // D20 - P0.20 - QSPI_IO0
    21,  // D21 - P0.21 - QSPI_IO1
    22,  // D22 - P0.22 - QSPI_IO2
    23,  // D23 - P0.23 - QSPI_IO3
    24,  // D24 - P0.24
    25,  // D25 - P0.25
    26,  // D26 - P0.26 - GPS_RX (UART1 RX)
    27,  // D27 - P0.27 - GPS_TX (UART1 TX)
    28,  // D28 - P0.28 - SPI_MOSI (LoRa)
    29,  // D29 - P0.29 - USB_VBUS_DET
    30,  // D30 - P0.30 - SPI_SCK (LoRa)
    31,  // D31 - P0.31 - BAT_ADC (Battery voltage)

    // P1.00 - P1.15
    32,  // D32 - P1.00 - WIFI_RST (Reserved)
    33,  // D33 - P1.01
    34,  // D34 - P1.02 - LED_GREEN
    35,  // D35 - P1.03 - LED_BLUE
    36,  // D36 - P1.04 - WIFI_EN (Reserved)
    37,  // D37 - P1.05 - GPS_EN
    38,  // D38 - P1.06 - GPS_RST
    39,  // D39 - P1.07 - LORA_RST
    40,  // D40 - P1.08
    41,  // D41 - P1.09 - GPS_WAKEUP
    42,  // D42 - P1.10 - LORA_BUSY
    43,  // D43 - P1.11
    44,  // D44 - P1.12
    45,  // D45 - P1.13 - QSPI_CS
    46,  // D46 - P1.14 - LORA_CS
    47,  // D47 - P1.15
};

/**
 * @brief I2C bus recovery - toggle SCL to release stuck SDA
 *
 * If a slave device is holding SDA low (e.g., due to interrupted transfer),
 * toggling SCL up to 9 times should cause it to release the bus.
 * This uses direct nRF GPIO access to avoid Arduino Wire conflicts.
 *
 * For LSM6DSOWTR: The device may hold SDA low after power-on. This function
 * must be called AFTER sensor power is enabled and stabilized.
 */
static void i2cBusRecovery(void)
{
    // Use raw GPIO pin numbers (P0.05 = SCL, P0.06 = SDA)
    const uint32_t pinSCL = 5;  // P0.05
    const uint32_t pinSDA = 6;  // P0.06

    // Configure SCL as output (open-drain style with pull-up)
    nrf_gpio_cfg(pinSCL,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_PULLUP,
                 NRF_GPIO_PIN_S0D1,  // Standard 0, Disconnect 1 (open-drain)
                 NRF_GPIO_PIN_NOSENSE);

    // Configure SDA as input with pull-up to check its state
    nrf_gpio_cfg_input(pinSDA, NRF_GPIO_PIN_PULLUP);

    // Start with SCL high
    nrf_gpio_pin_set(pinSCL);

    // Simple delay using NOP loop (approximately 5us per iteration at 64MHz)
    volatile int i;

    // Wait a bit for pull-ups to stabilize
    for (i = 0; i < 500; i++) { __NOP(); }

    // Toggle SCL up to 18 times (two full recovery attempts) to release a stuck slave
    for (int clkCnt = 0; clkCnt < 18; clkCnt++) {
        // SCL low
        nrf_gpio_pin_clear(pinSCL);
        for (i = 0; i < 200; i++) { __NOP(); }  // ~10us delay

        // SCL high
        nrf_gpio_pin_set(pinSCL);
        for (i = 0; i < 200; i++) { __NOP(); }  // ~10us delay

        // Check if SDA is released (high)
        if (nrf_gpio_pin_read(pinSDA)) {
            // SDA is free, generate STOP and exit
            break;
        }
    }

    // Generate STOP condition: SDA low -> SCL high -> SDA high
    nrf_gpio_cfg(pinSDA,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_PULLUP,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    nrf_gpio_pin_clear(pinSDA);  // SDA low
    for (i = 0; i < 200; i++) { __NOP(); }
    nrf_gpio_pin_set(pinSCL);    // SCL high
    for (i = 0; i < 200; i++) { __NOP(); }
    nrf_gpio_pin_set(pinSDA);    // SDA high (STOP)
    for (i = 0; i < 200; i++) { __NOP(); }

    // Leave pins as inputs with pull-ups for Wire library to reconfigure
    nrf_gpio_cfg_input(pinSCL, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(pinSDA, NRF_GPIO_PIN_PULLUP);

    // Additional delay after bus recovery
    for (i = 0; i < 1000; i++) { __NOP(); }
}

/**
 * @brief Initialize variant-specific hardware
 *
 * Called during system startup to configure board-specific
 * GPIO states and peripherals.
 */
void initVariant()
{
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  Power Management - FIRST! Enable power before anything else
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Enable main power (PMOS switch)
    pinMode(PIN_PWR_ON, OUTPUT);
    digitalWrite(PIN_PWR_ON, HIGH);

    // Sensor power control - MUST stay HIGH always per hardware design
    // P0.15 powers the IMU (LSM6DSOWTR) and other I2C sensors
    // WARNING: Do NOT disable this pin - required for proper board operation
    pinMode(PIN_SENSOR_PWR, OUTPUT);
    digitalWrite(PIN_SENSOR_PWR, HIGH);

    // Wait for sensor power to stabilize before I2C bus recovery
    // LSM6DSOWTR needs time after power-on before it releases the I2C bus
    // Increase delay if I2C scan still hangs
    delay(200);

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  I2C Bus Recovery - recover stuck I2C bus before Wire.begin()
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // The LSM6DSOWTR or other I2C devices may hold SDA low after power-on
    // or incomplete transactions. Perform bus recovery before Wire init.
    i2cBusRecovery();

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  SPI Chip Selects - Deselect before any other SPI init
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Deselect QSPI flash FIRST (shares P0.19 with LED_RED)
    pinMode(PIN_QSPI_CS, OUTPUT);
    digitalWrite(PIN_QSPI_CS, HIGH);

    // Deselect LoRa SPI
    pinMode(SX126X_CS, OUTPUT);
    digitalWrite(SX126X_CS, HIGH);

    // LoRa reset - not in reset
    pinMode(SX126X_RESET, OUTPUT);
    digitalWrite(SX126X_RESET, HIGH);

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  Battery ADC
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Battery ADC control - initially OFF
    pinMode(BAT_READ, OUTPUT);
    digitalWrite(BAT_READ, LOW);

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  LED Configuration (P0.19 is shared with QSPI_SCK)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Note: PIN_LED1 (P0.19) is shared with QSPI_SCK
    // Only use as LED when QSPI is not active
    pinMode(PIN_LED1, OUTPUT);
    digitalWrite(PIN_LED1, LOW);  // Red OFF

    pinMode(PIN_LED2, OUTPUT);
    digitalWrite(PIN_LED2, LOW);  // Green OFF

    pinMode(PIN_LED3, OUTPUT);
    digitalWrite(PIN_LED3, LOW);  // Blue OFF

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  GPS Module
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // GPS reset - not in reset (HIGH = normal operation)
    pinMode(GPS_RESET, OUTPUT);
    digitalWrite(GPS_RESET, HIGH);

    // GPS wakeup - LOW initially
    pinMode(GPS_WAKEUP, OUTPUT);
    digitalWrite(GPS_WAKEUP, LOW);

    // GPS enable - ON (HIGH = enabled per GPS_EN_ACTIVE)
    // Meshtastic GPS driver will control this later
    pinMode(GPS_EN, OUTPUT);
    digitalWrite(GPS_EN, HIGH);
}

} // extern "C"
