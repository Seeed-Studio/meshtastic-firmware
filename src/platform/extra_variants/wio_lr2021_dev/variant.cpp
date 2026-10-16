#include "configuration.h"

#ifdef WIO_LR2021_DEV

#include <Wire.h>

#include "Observer.h"
#include "sleep.h"

#include PCA95X5_INC
extern PCA95X5_CLS io;

#ifdef HAS_I2S
#include "AudioBoard.h"
#endif

namespace
{
#ifdef HAS_I2S
DriverPins audioPins;
AudioBoard audioBoard(AudioDriverES8311, audioPins);
#endif

// earlyInitVariant() runs before consoleInit(): a LOG_* call there crashes the device
// (see extra_variants/README.md). PCA9535 results are stashed here and reported from
// lateInitVariant() instead.
bool pca9535Ok = false;
bool oledPowerOk = false;
bool oledResetOk = false;
bool audioAmpOk = false;
bool loraResetOk = false;
bool batAdcOk = false;
bool ledOk = false;
} // namespace

#if defined(HAS_I2S) && PCA9535_AUDIO_ENABLE_PIN < 16
void wioLr2021SetAudioAmp(bool enabled)
{
    io.digitalWrite(PCA9535_AUDIO_ENABLE_PIN, enabled);
}
#elif defined(HAS_I2S)
void wioLr2021SetAudioAmp(bool)
{
}
#endif

// PCA9535 outputs stay latched through ESP32 deep sleep; drop these rails before sleeping.
struct DeepSleepPowerOff {
    int onDeepSleep(void *)
    {
        io.digitalWrite(PCA9535_OLED_ENABLE_PIN, false);
        io.digitalWrite(PCA9535_OLED_RESET_PIN, false);
        io.digitalWrite(PCA9535_AUDIO_ENABLE_PIN, false);
        io.digitalWrite(PCA9535_BAT_ENABLE_PIN, false);
#ifdef PCA_LED_POWER
        io.digitalWrite(PCA_LED_POWER, LED_STATE_OFF); // expander outputs latch through deep sleep
#endif
        return 0;
    }
    CallbackObserver<DeepSleepPowerOff, void *> observer{this, &DeepSleepPowerOff::onDeepSleep};
} static deepSleepPowerOff;

void lateInitVariant()
{
    deepSleepPowerOff.observer.observe(&notifyDeepSleep);

    if (!pca9535Ok) {
        LOG_WARN("Wio LR2021: failed to init PCA9535");
    }
#ifdef PCA_LED_POWER
    if (!ledOk) {
        LOG_WARN("Wio LR2021: failed to init status LED");
    }
#endif

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
        LOG_WARN("Wio LR2021: failed to configure PCA9535 BatAdc enable");
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

    pca9535Ok = io.begin(Wire, WIO_LR2021_PCA9535_ADDR);

#if PCA9535_OLED_ENABLE_PIN < 16
    const bool oledEnabled = PCA9535_OLED_ENABLE_LEVEL == HIGH;
    io.digitalWrite(PCA9535_OLED_ENABLE_PIN, oledEnabled);
    oledPowerOk = io.pinMode(PCA9535_OLED_ENABLE_PIN, OUTPUT);
    // Hold the panel in reset while its rail stabilises, then release (resets are active low).
    io.digitalWrite(PCA9535_OLED_RESET_PIN, false);
    oledResetOk = io.pinMode(PCA9535_OLED_RESET_PIN, OUTPUT);
    delay(20);
    oledResetOk = io.digitalWrite(PCA9535_OLED_RESET_PIN, true) && oledResetOk;
#endif

#if PCA9535_AUDIO_ENABLE_PIN < 16
    io.digitalWrite(PCA9535_AUDIO_ENABLE_PIN, true);
    audioAmpOk = io.pinMode(PCA9535_AUDIO_ENABLE_PIN, OUTPUT);
#endif

#if PCA9535_LORA_RESET_PIN < 16
    io.digitalWrite(PCA9535_LORA_RESET_PIN, true);
    loraResetOk = io.pinMode(PCA9535_LORA_RESET_PIN, OUTPUT);
#endif

#if PCA9535_BAT_ENABLE_PIN < 16
    io.digitalWrite(PCA9535_BAT_ENABLE_PIN, true);
    batAdcOk = io.pinMode(PCA9535_BAT_ENABLE_PIN, OUTPUT);
#endif

#ifdef PCA_LED_POWER
    io.digitalWrite(PCA_LED_POWER, LED_STATE_OFF);
    ledOk = io.pinMode(PCA_LED_POWER, OUTPUT);
#endif
}

#endif
