#include "BMM350Sensor.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<DFRobot_BMM350.h>)
#if !defined(MESHTASTIC_EXCLUDE_SCREEN)

// screen is defined in main.cpp
extern graphics::Screen *screen;
#endif

// Flag when an interrupt has been detected
volatile static bool BMM350_IRQ = false;

BMM350Sensor::BMM350Sensor(ScanI2C::FoundDevice foundDevice) : MotionSensor::MotionSensor(foundDevice) {}

bool BMM350Sensor::init()
{
    // Initialise the sensor
    sensor = BMM350Singleton::GetInstance(device);
    return sensor->init(device);
}

int32_t BMM350Sensor::runOnce()
{
#if !defined(MESHTASTIC_EXCLUDE_SCREEN) && HAS_SCREEN
    float heading = sensor->getCompassDegree();

    switch (config.display.compass_orientation) {
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_0_INVERTED:
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_0:
        break;
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_90:
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_90_INVERTED:
        heading += 90;
        break;
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_180:
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_180_INVERTED:
        heading += 180;
        break;
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_270:
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_270_INVERTED:
        heading += 270;
        break;
    }
    if (screen)
        screen->setHeading(heading);
#endif
    return MOTION_SENSOR_CHECK_INTERVAL_MS;
}

// ----------------------------------------------------------------------
// BMM350Singleton
// ----------------------------------------------------------------------

// Get a singleton wrapper for an Sparkfun BMM_350_I2C
BMM350Singleton *BMM350Singleton::GetInstance(ScanI2C::FoundDevice device)
{
#if defined(WIRE_INTERFACES_COUNT) && (WIRE_INTERFACES_COUNT > 1)
    TwoWire &bus = (device.address.port == ScanI2C::I2CPort::WIRE1 ? Wire1 : Wire);
#else
    TwoWire &bus = Wire; // fallback if only one I2C interface
#endif
    if (pinstance == nullptr) {
        pinstance = new BMM350Singleton(&bus, device.address.address);
    }
    return pinstance;
}

BMM350Singleton::~BMM350Singleton() {}

BMM350Singleton *BMM350Singleton::pinstance{nullptr};

// Initialise the BMM350 Sensor
// https://github.com/DFRobot/DFRobot_BMM350
bool BMM350Singleton::init(ScanI2C::FoundDevice device)
{
    // startup
    LOG_DEBUG("BMM350 begin on addr 0x%02X (port=%d)", device.address.address, device.address.port);
    uint8_t status = begin();
    if (status != 0) {
        LOG_DEBUG("BMM350 init error %u", status);
        return false;
    }

    // SW reset to make sure the device starts in a known state
    setOperationMode(eBmm350NormalMode);
    setPresetMode(BMM350_PRESETMODE_HIGHACCURACY,BMM350_DATA_RATE_25HZ);
    setMeasurementXYZ();
    return true;
}

#endif