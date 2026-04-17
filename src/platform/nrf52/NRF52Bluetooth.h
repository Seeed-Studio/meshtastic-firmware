#pragma once

#include "BluetoothCommon.h"
#include <Arduino.h>
#include <functional>

// Manufacturer Data Constants
#define MANUFACTURER_DATA_MAX_SIZE 80
#define MANUFACTURER_DATA_SEGMENT_SIZE 7    // Follow ble_adv_scan.ino: 9 bytes payload per scan response chunk
#define MANUFACTURER_DATA_SEGMENTS ((MANUFACTURER_DATA_MAX_SIZE + MANUFACTURER_DATA_SEGMENT_SIZE - 1) / MANUFACTURER_DATA_SEGMENT_SIZE)
#define MANUFACTURER_ID 0xFFFF // Use a visible non-reserved company ID for debugging in scanner apps.
#define MANUFACTURER_BROADCAST_INTERVAL_MS 2000  // 2 seconds per segment for easy mobile observation
#define MANUFACTURER_BROADCAST_CYCLE_PERIODS 20

class ManufacturerDataBroadcaster;

class NRF52Bluetooth : BluetoothApi
{
  public:
    void setup();
    void shutdown();
    void startDisabled();
    void resumeAdvertising();
    void clearBonds();
    bool isConnected();
    int getRssi();
    void sendLog(const uint8_t *logMessage, size_t length);

    // Bluetooth scanning methods
    typedef std::function<void(void*)> ScanCallback;
    void startScanning(uint16_t duration = 0);
    void stopScanning();
    bool isScanning();
    void setScanCallback(ScanCallback callback);

    // Manufacturer Data Broadcasting methods
    void setManufacturerData(const uint8_t* data, size_t length);
    void startManufacturerDataBroadcasting();
    void stopManufacturerDataBroadcasting();
    bool isManufacturerDataBroadcasting();
    void updateManufacturerDataBroadcasting(); // Called periodically by timer

  private:
    static void onConnectionSecured(uint16_t conn_handle);
    static bool onPairingPasskey(uint16_t conn_handle, uint8_t const passkey[6], bool match_request);
    static void onPairingCompleted(uint16_t conn_handle, uint8_t auth_status);

    static bool onUnwantedPairing(uint16_t conn_handle, uint8_t const passkey[6], bool match_request);
    static void disconnect();
    
    // Scan callback
    ScanCallback scanCallback = nullptr;
    static void scanCallbackWrapper(void* report);

    // Manufacturer Data Broadcasting
    ManufacturerDataBroadcaster* mfgDataBroadcaster = nullptr;
};
