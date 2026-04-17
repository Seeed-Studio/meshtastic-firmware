# Manufacturer Data Broadcast Integration in main.cpp

## Overview

The manufacturer data broadcast feature has been successfully integrated into `src/main.cpp` for NRF52 platforms. This document explains how the integration works and how to use it.

## Integration Summary

### 1. Automatic Toggle

The system automatically toggles manufacturer data broadcast on and off every 10 seconds:

- **Toggle Interval**: 10 seconds (configurable)
- **Cycle Pattern**: 
  - Broadcast manufacturer data (80 bytes in 4 segments) for 10 seconds
  - Restore original scan response for 10 seconds
  - Repeat automatically
- **Purpose**: Easy testing and verification of both states

**Important:** When toggling states:
- **Start**: Changes scan response to manufacturer data and **restarts advertising** to make changes visible
- **Stop**: Restores original scan response (TxPower + Device Name) and **restarts advertising** to make changes visible

This simple toggle pattern ensures:
- Device remains discoverable (alternates between manufacturer data and original scan response)
- Easy to verify both states during testing
- Similar to the scanning toggle pattern used elsewhere in the code
- **Device stays visible** - advertising is never stopped, only the scan response content changes

### 2. Key Components

#### Global Variables (in main.cpp)

```cpp
static uint8_t manufacturerTestData[80] = {0};  // Storage for 80-byte data
static uint32_t lastMfgDataToggle = 0;          // Last toggle timestamp
```

#### Key Functions

1. **initManufacturerData()** - Initialize with test data (1-80)
2. **handleManufacturerDataBroadcast()** - Toggle broadcast on/off every 10 seconds

### 3. Integration Points

#### In setup()

```cpp
#ifdef ARCH_NRF52
    nrf52Setup();
    
    // Initialize manufacturer data broadcast functionality
    if (nrf52Bluetooth) {
        initManufacturerData();
        LOG_INFO("Manufacturer data broadcast feature initialized");
        LOG_INFO("Auto toggle: %d seconds between ON and OFF", 10);
    }
#endif
```

#### In loop()

```cpp
#ifdef ARCH_NRF52
    // Handle manufacturer data broadcast automatic toggle
    handleManufacturerDataBroadcast();
#endif
```

## Usage

### Automatic Mode (Only Mode)

The system automatically toggles manufacturer data broadcast on and off:

1. **Start**: System begins with original scan response
2. **After 10s**: Switches to manufacturer data broadcast (80 bytes split into 4 segments)
3. **After another 10s**: Returns to original scan response
4. **Repeat**: Continues alternating every 10 seconds

This is completely automatic - no manual intervention required.

### Example Code Pattern

The implementation follows the same pattern as the scanning toggle:

```cpp
// Scanning toggle (existing code)
static uint32_t lastscanReset;
if (!Throttle::isWithinTimespanMs(lastscanReset, 10 * 1000L)) {
    lastscanReset = millis();
    nrf52Bluetooth->isScanning() ? nrf52Bluetooth->stopScanning()
                                : nrf52Bluetooth->startScanning(0);
}

// Manufacturer data broadcast toggle (new code)
void handleManufacturerDataBroadcast()
{
    if (!nrf52Bluetooth) {
        return;
    }
    
    static uint32_t lastToggle = 0;
    if (!Throttle::isWithinTimespanMs(lastToggle, 10 * 1000L)) {
        lastToggle = millis();
        
        if (nrf52Bluetooth->isManufacturerDataBroadcasting()) {
            nrf52Bluetooth->stopManufacturerDataBroadcasting();
            LOG_INFO("Stopped manufacturer data broadcast, restored original scan response");
        } else {
            nrf52Bluetooth->startManufacturerDataBroadcasting();
            LOG_INFO("Started manufacturer data broadcast");
        }
    }
}
```

### Changing the Toggle Interval

To change the toggle interval from 10 seconds to a different value:

```cpp
// In handleManufacturerDataBroadcast()
// Change 10 * 1000L to your desired interval in milliseconds
if (!Throttle::isWithinTimespanMs(lastToggle, 30 * 1000L)) {  // 30 seconds
    lastToggle = millis();
    // ... toggle logic
}
```

Also update the log message in setup():

```cpp
LOG_INFO("Auto toggle: %d seconds between ON and OFF", 30);  // Update this
```

### Programmatic Control

If you need to control broadcasting programmatically (e.g., from another module), you can access the NRF52Bluetooth object directly:

```cpp
// Start broadcasting
if (nrf52Bluetooth) {
    nrf52Bluetooth->startManufacturerDataBroadcasting();
}

// Stop broadcasting
if (nrf52Bluetooth) {
    nrf52Bluetooth->stopManufacturerDataBroadcasting();
}

// Check status
if (nrf52Bluetooth && nrf52Bluetooth->isManufacturerDataBroadcasting()) {
    LOG_INFO("Currently broadcasting manufacturer data");
}

// Update data
uint8_t myCustomData[80] = { /* your 80 bytes */ };
if (nrf52Bluetooth) {
    nrf52Bluetooth->setManufacturerData(myCustomData, sizeof(myCustomData));
}
```

## Customization

### Change Toggle Interval

Modify the toggle interval in `handleManufacturerDataBroadcast()`:

```cpp
// Change from 10 seconds to your desired interval
if (!Throttle::isWithinTimespanMs(lastToggle, 30 * 1000L)) {  // 30 seconds
    lastToggle = millis();
    // ... toggle logic
}
```

Also update the log message in `setup()`:

```cpp
LOG_INFO("Auto toggle: %d seconds between ON and OFF", 30);  // Update this
```

### Initial Data

Modify `initManufacturerData()` to use your default data:

```cpp
void initManufacturerData()
{
    // Your custom initialization
    for (uint8_t i = 0; i < 80; i++) {
        manufacturerTestData[i] = /* your value */;
    }
    
    if (nrf52Bluetooth) {
        nrf52Bluetooth->setManufacturerData(manufacturerTestData, sizeof(manufacturerTestData));
        LOG_INFO("Manufacturer data initialized with custom data");
    }
}
```

### Dynamic Updates

You can update data programmatically at any time:

```cpp
// Update manufacturer data with new values
if (nrf52Bluetooth) {
    uint8_t newData[80] = { /* your data */ };
    nrf52Bluetooth->setManufacturerData(newData, sizeof(newData));
    LOG_INFO("Manufacturer data updated");
}
```

## Broadcast Details

### Data Format

Each manufacturer data segment contains:

```
[Length:1][Type:1][CompanyID:2][Sequence:1][TotalSeg:1][CurSeg:1][Data:20]
```

- **Company ID**: 0xFFFF
- **Total Segments**: 4
- **Data per Segment**: 20 bytes
- **Total Data**: 80 bytes

### Broadcast Cycle

The scan response cycles through 5 periods:

1. **Period 1 (0-500ms)**: Original content (TxPower + Device Name)
2. **Period 2 (500-1000ms)**: Manufacturer data segment 0
3. **Period 3 (1000-1500ms)**: Manufacturer data segment 1
4. **Period 4 (1500-2000ms)**: Manufacturer data segment 2
5. **Period 5 (2000-2500ms)**: Manufacturer data segment 3

Then repeats. This ensures:
- Device remains discoverable (Period 1 shows original scan response)
- All 4 segments are broadcast (Periods 2-5)
- Complete data cycle takes 2.5 seconds

## Testing

### Mobile App Test

1. Install nRF Connect (Android/iOS)
2. Start scanning
3. Find your device
4. Inspect advertising data
5. Observe the following:
   - Every 10 seconds, the scan response alternates between:
     - Original content (TxPower + Device Name)
     - Manufacturer data with ID 0xFFFF
   - When manufacturer data is present, you'll see 4 segments rotating every 500ms
   - Total data: 80 bytes split into 4 segments of 20 bytes each

### Data Reception Test

See `docs/MANUFACTURER_DATA_BROADCAST.md` for detailed receiver implementation.

## Troubleshooting

### Broadcasting Not Working

1. **Check platform**: Only works on NRF52 (ARCH_NRF52)
2. **Verify Bluetooth initialization**: Check logs for "Manufacturer data broadcast feature initialized"
3. **Review logs**: Look for "Started manufacturer data broadcast" and "Stopped manufacturer data broadcast" messages

### Device Not Discoverable

- The feature preserves discoverability by alternating between manufacturer data and original scan response
- Each 10-second period shows either manufacturer data OR original content
- If device is not discoverable at all, check if original BLE is working correctly

### Data Not Alternating

1. **Check logs**: Verify toggle messages appear every 10 seconds
2. **Test with nRF Connect**: Observe scan response changes every 10 seconds
3. **Verify platform**: Ensure you're running on NRF52 hardware

## Performance Impact

- **Memory**: ~100 bytes RAM for data storage
- **CPU**: Negligible (500ms updates)
- **Power**: Minimal (scan response updates)
- **Network**: No impact on mesh networking

## Integration with Other Features

The manufacturer data broadcast works seamlessly with:
- Bluetooth mesh networking
- Device discovery
- Serial console
- Power management
- Sleep modes

It does NOT interfere with:
- Radio communication
- GPS functionality
- Sensor readings
- WiFi (if available)
- Serial communication

## Advanced Usage

### Event-Based Updates

You can update manufacturer data based on events or conditions:

```cpp
// Example: Update data when a new node joins the mesh
void onNodeJoined(uint32_t nodeId)
{
    if (nrf52Bluetooth) {
        uint8_t eventData[80] = {0};
        // Prepare event-specific data
        memcpy(&eventData[0], &nodeId, 4);  // Node ID
        
        nrf52Bluetooth->setManufacturerData(eventData, 4);
        LOG_INFO("Updated manufacturer data for node join event");
    }
}
```

### Time-Based Data

```cpp
// Update data based on time of day
void updateTimeBasedData()
{
    if (!nrf52Bluetooth) return;
    
    uint8_t timeData[80] = {0};
    
    // Encode current time
    uint32_t currentTime = getValidTime(RTCQualityNet);
    memcpy(&timeData[0], &currentTime, 4);
    
    // Add time-based data
    struct tm* timeinfo = localtime((time_t*)&currentTime);
    timeData[4] = timeinfo->tm_hour;
    timeData[5] = timeinfo->tm_min;
    timeData[6] = timeinfo->tm_sec;
    
    nrf52Bluetooth->setManufacturerData(timeData, 7);
}
```

### Sensor-Based Updates

```cpp
// Update data with sensor readings
void updateSensorData()
{
    if (!nrf52Bluetooth) return;
    
    uint8_t sensorData[80] = {0};
    
    // Add sensor readings
    if (powerStatus) {
        sensorData[0] = powerStatus->getBatteryPercent();
    }
    
    if (gpsStatus) {
        float temp = gpsStatus->getTemperature();
        memcpy(&sensorData[1], &temp, 4);
    }
    
    nrf52Bluetooth->setManufacturerData(sensorData, 5);
}
```

## Summary

The manufacturer data broadcast feature is now fully integrated into main.cpp with:

✅ **Automatic toggle management** - No manual intervention required
✅ **Simple toggle pattern** - Easy testing and verification
✅ **Programmatic API** - Full control from code
✅ **Backward compatible** - Doesn't break existing functionality
✅ **Discoverable device** - Preserves device discoverability
✅ **Configurable timing** - Easy to adjust intervals
✅ **Dynamic data updates** - Support for real-time data changes

For more details on the underlying implementation, see:
- `src/platform/nrf52/NRF52Bluetooth.h` - API definitions
- `src/platform/nrf52/NRF52Bluetooth.cpp` - Implementation
- `docs/MANUFACTURER_DATA_BROADCAST.md` - Full documentation