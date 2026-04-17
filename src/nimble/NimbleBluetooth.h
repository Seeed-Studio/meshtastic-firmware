#pragma once
#include "BluetoothCommon.h"

#include <functional>

// Forward declaration
class NimBLEAdvertisedDevice;

class NimbleBluetooth : BluetoothApi
{
  public:
    void setup();
    void shutdown();
    void deinit();
    void clearBonds();
    bool isActive();
    bool isConnected();
    int getRssi();
    void sendLog(const uint8_t *logMessage, size_t length);
#if defined(NIMBLE_TWO)
    void startAdvertising();
#endif
    bool isDeInit = false;

    // Bluetooth scanning methods
    typedef std::function<void(NimBLEAdvertisedDevice*)> ScanCallback;
    void startScanning(uint32_t duration = 0);
    void stopScanning();
    bool isScanning();
    void setScanCallback(ScanCallback callback);

  private:
    void setupService();
#if !defined(NIMBLE_TWO)
    void startAdvertising();
#endif

    // Scanning state
    ScanCallback scanCallback = nullptr;
};

void setBluetoothEnable(bool enable);
void clearNVS();
