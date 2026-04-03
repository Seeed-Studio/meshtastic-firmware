#pragma once
#ifndef _BMM_350_SENSOR_H_
#define _BMM_350_SENSOR_H_

#include "MotionSensor.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<DFRobot_BMM350.h>)

#include "Fusion/Fusion.h"
#include <DFRobot_BMM350.h>

// The I2C address of the Accelerometer (if found) from main.cpp
extern ScanI2C::DeviceAddress accelerometer_found;

// Singleton wrapper
class BMM350Singleton : public DFRobot_BMM350_I2C
{
  private:
    static BMM350Singleton *pinstance;

  protected:
    BMM350Singleton(TwoWire *tw, uint8_t addr) : DFRobot_BMM350_I2C(tw, addr) {}
    ~BMM350Singleton();

  public:
    // Create a singleton instance (not thread safe)
    static BMM350Singleton *GetInstance(ScanI2C::FoundDevice device);

    // Singletons should not be cloneable.
    BMM350Singleton(BMM350Singleton &other) = delete;

    // Singletons should not be assignable.
    void operator=(const BMM350Singleton &) = delete;

    // Initialise the motion sensor singleton for normal operation
    bool init(ScanI2C::FoundDevice device);
};

class BMM350Sensor : public MotionSensor
{
  private:
    BMM350Singleton *sensor = nullptr;
    bool showingScreen = false;

  public:
    explicit BMM350Sensor(ScanI2C::FoundDevice foundDevice);

    // Initialise the motion sensor
    virtual bool init() override;

    // Called each time our sensor gets a chance to run
    virtual int32_t runOnce() override;
};

#endif

#endif