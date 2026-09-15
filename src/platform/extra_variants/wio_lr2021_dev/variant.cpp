#include "configuration.h"

#ifdef WIO_LR2021_DEV

#include <Wire.h>

#ifdef HAS_I2S
#include "AudioBoard.h"
#endif

namespace
{
constexpr uint8_t PCA_OUTPUT_PORT_0 = 0x02;
constexpr uint8_t PCA_CONFIG_PORT_0 = 0x06;

bool readPca9535Register(uint8_t reg, uint8_t &value)
{
    Wire.beginTransmission(WIO_LR2021_PCA9535_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom((uint8_t)WIO_LR2021_PCA9535_ADDR, (uint8_t)1) != 1) {
        return false;
    }

    value = Wire.read();
    return true;
}

bool writePca9535Register(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(WIO_LR2021_PCA9535_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool configurePca9535Pin(uint8_t pin, bool input)
{
    if (pin >= 16) {
        return false;
    }

    const uint8_t port = pin / 8;
    const uint8_t mask = 1U << (pin % 8);
    uint8_t config = 0xff;
    if (!readPca9535Register(PCA_CONFIG_PORT_0 + port, config)) {
        return false;
    }

    config = input ? config | mask : config & ~mask;
    return writePca9535Register(PCA_CONFIG_PORT_0 + port, config);
}

bool writePca9535Pin(uint8_t pin, bool high)
{
    if (pin >= 16) {
        return false;
    }

    const uint8_t port = pin / 8;
    const uint8_t mask = 1U << (pin % 8);
    uint8_t output = 0;
    if (!readPca9535Register(PCA_OUTPUT_PORT_0 + port, output)) {
        return false;
    }

    output = high ? output | mask : output & ~mask;
    return writePca9535Register(PCA_OUTPUT_PORT_0 + port, output);
}

bool configurePca9535Output(uint8_t pin, bool initialHigh)
{
    if (!writePca9535Pin(pin, initialHigh)) {
        return false;
    }

    return configurePca9535Pin(pin, false);
}

#ifdef HAS_I2S
DriverPins audioPins;
AudioBoard audioBoard(AudioDriverES8311, audioPins);
#endif

// earlyInitVariant() runs before consoleInit(): a LOG_* call there crashes the device
// (see extra_variants/README.md). PCA9535 results are stashed here and reported from
// lateInitVariant() instead.
bool oledPowerOk = false;
bool oledResetOk = false;
bool audioAmpOk = false;
bool loraResetOk = false;
bool batAdcOk = false;
} // namespace

#if defined(HAS_I2S) && PCA9535_AUDIO_ENABLE_PIN < 16
void wioLr2021SetAudioAmp(bool enabled)
{
    writePca9535Pin(PCA9535_AUDIO_ENABLE_PIN, enabled);
}
#elif defined(HAS_I2S)
void wioLr2021SetAudioAmp(bool)
{
}
#endif

void lateInitVariant()
{
#if PCA9535_OLED_ENABLE_PIN < 16
    if (!oledPowerOk) {
        LOG_WARN("Wio LR2021: failed to enable OLED power");
    }
    if (!oledResetOk) {
        LOG_WARN("Wio LR2021: failed to enable OLED reset");
    }
#endif
#if PCA9535_AUDIO_ENABLE_PIN < 16
    if (!audioAmpOk) {
        LOG_WARN("Wio LR2021: failed to configure PCA9535 audio amp enable");
    }
#endif
#if PCA9535_LORA_RESET_PIN < 16
    if (!loraResetOk) {
        LOG_WARN("Wio LR2021: failed to configure PCA9535 LoRa reset");
    }
#endif
#if PCA9535_BAT_ENABLE_PIN < 16
    if (!batAdcOk) {
        LOG_WARN("Wio LR2021: failed to configure PCA9535 BatAdc enbale");
    }
#endif

#if defined(HAS_I2S)
    audioPins.addI2C(PinFunction::CODEC, Wire);
    audioPins.addI2S(PinFunction::CODEC, DAC_I2S_MCLK, DAC_I2S_BCK, DAC_I2S_WS, DAC_I2S_DOUT, DAC_I2S_DIN);

    CodecConfig config;
    config.input_device = ADC_INPUT_NONE;
    config.output_device = DAC_OUTPUT_ALL;
    config.i2s.bits = BIT_LENGTH_16BITS;
    config.i2s.rate = RATE_22K;

    if (audioBoard.begin(config)) {
        audioBoard.setVolume(70); // ES8311 DAC volume, 0~100
        LOG_INFO("Wio LR2021: ES8311 initialized at 22.05 kHz");
    } else {
        LOG_WARN("Wio LR2021: failed to initialize ES8311");
    }
#endif
}

void earlyInitVariant()
{
    Wire.begin(I2C_SDA, I2C_SCL);

#if PCA9535_OLED_ENABLE_PIN < 16
    const bool oledEnabled = PCA9535_OLED_ENABLE_LEVEL == HIGH;
    oledPowerOk = configurePca9535Output(PCA9535_OLED_ENABLE_PIN, oledEnabled);
    // Hold the panel in reset while its rail stabilises, then release (resets are active low).
    configurePca9535Output(PCA9535_OLED_RESET_PIN, false);
    delay(20);
    oledResetOk = configurePca9535Output(PCA9535_OLED_RESET_PIN, true);
#endif

#if PCA9535_AUDIO_ENABLE_PIN < 16
    audioAmpOk = configurePca9535Output(PCA9535_AUDIO_ENABLE_PIN, true);
#endif

#if PCA9535_LORA_RESET_PIN < 16
    loraResetOk = configurePca9535Output(PCA9535_LORA_RESET_PIN, true);
#endif

#if PCA9535_BAT_ENABLE_PIN <16
    batAdcOk =  configurePca9535Output(PCA9535_BAT_ENABLE_PIN, true);
#endif
}

#endif
