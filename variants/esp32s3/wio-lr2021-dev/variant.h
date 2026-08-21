#pragma once

// Replace each -1 with the GPIO or PCA9535 port from the board schematic.
#define WIO_LR2021_PIN_NC (-1)
#define WIO_LR2021_PCA_PIN_NC 0xff

// Shared I2C bus: SH1106, PCA9535, and ES8311.
#define I2C_SDA             47
#define I2C_SCL             48

#if I2C_SDA >= 0 && I2C_SCL >= 0
#define HAS_SCREEN 1
#define USE_SH1106
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#endif

// LR2021 radio.
#define USE_LR2021
#define LORA_SCK            4
#define LORA_MISO           5
#define LORA_MOSI           6
#define LORA_CS             21
#define LORA_DIO1           45

#define LR2021_SPI_NSS_PIN  LORA_CS
#define LR2021_IRQ_PIN      LORA_DIO1
#define LR2021_NRESET_PIN   WIO_LR2021_PIN_NC
#define LR2021_BUSY_PIN     7
#define LR2021_IRQ_DIO_NUM  8

// BOOT is the primary BaseUI button. The second button is a direct ESP32-S3 GPIO.
#define BUTTON_PIN 0
#define BUTTON_NEED_PULLUP

#define WIO_LR2021_ALT_BUTTON_PIN 46
#if WIO_LR2021_ALT_BUTTON_PIN >= 0
#define ALT_BUTTON_PIN WIO_LR2021_ALT_BUTTON_PIN
#define ALT_BUTTON_ACTIVE_LOW       false
#define ALT_BUTTON_ACTIVE_PULLUP    true
#endif

// PCA9535 (A0/A1/A2 low by default). Ports use the range 0..15.
#define WIO_LR2021_PCA9535_ADDR     0x21
#define WIO_LR2021_PCA9535_INT      3
#define PCA9535_OLED_ENABLE_PIN     3
#define PCA9535_OLED_ENABLE_LEVEL   HIGH
#define PCA9535_AUDIO_ENABLE_PIN    2

// ES8311 codec and I2S speaker.
#define ES8311_I2C_ADDR      0x18
#define DAC_I2S_MCLK         41
#define DAC_I2S_BCK          9
#define DAC_I2S_WS           42
#define DAC_I2S_DOUT         1
#define DAC_I2S_DIN          2

#if DAC_I2S_MCLK >= 0 && DAC_I2S_BCK >= 0 && DAC_I2S_WS >= 0 && DAC_I2S_DOUT >= 0
#define HAS_I2S
void wioLr2021SetAudioAmp(bool enabled);
#define AUDIO_AMP_ENABLE(on) wioLr2021SetAudioAmp(on)
#endif

// Status LED. Set LED_STATE_ON to LOW for an active-low LED.
#define WIO_LR2021_LED_PIN WIO_LR2021_PIN_NC
#if WIO_LR2021_LED_PIN >= 0
#define LED_PIN WIO_LR2021_LED_PIN
#define LED_STATE_ON HIGH
#endif

// Optional battery ADC. Set the divider ratio before enabling this pin.
#define WIO_LR2021_BATTERY_PIN WIO_LR2021_PIN_NC
#if WIO_LR2021_BATTERY_PIN >= 0
#define BATTERY_PIN WIO_LR2021_BATTERY_PIN
#define ADC_MULTIPLIER 2.0
#define BATTERY_SENSE_RESOLUTION_BITS 12
#endif

#define WIO_LR2021_DEV 1
