# Manufacturer Data Broadcast Feature

## Overview

This feature enables broadcasting 80 bytes of custom manufacturer data via BLE scan response. The data is automatically split into 4 segments and broadcasted in a rotating pattern.

## Key Features

- **Data Size**: 80 bytes of manufacturer data
- **Manufacturer ID**: 0xFFFF (configurable)
- **Segmentation**: 4 segments, 20 bytes each
- **Update Interval**: 500ms per segment
- **Complete Cycle**: 2.5 seconds (including one period of original scan response)
- **Discoverability**: Preserves original scan response (TxPower + Device Name) for device discoverability

## Architecture

### Broadcast Cycle

The scanner response cycles through 5 periods:

1. **Period 1 (0-500ms)**: Original content (TxPower + Device Name)
2. **Period 2 (500-1000ms)**: Manufacturer data segment 0
3. **Period 3 (1000-1500ms)**: Manufacturer data segment 1
4. **Period 4 (1500-2000ms)**: Manufacturer data segment 2
5. **Period 5 (2000-2500ms)**: Manufacturer data segment 3

Then the cycle repeats.

### Data Format

Each manufacturer data segment follows this format:

```
[Length: 1 byte][Type: 1 byte][Company ID: 2 bytes][Sequence: 1 byte][Total Segments: 1 byte][Current Segment: 1 byte][Data: up to 20 bytes]
```

Where:
- **Length**: Total bytes following (type + data)
- **Type**: 0xFF (Manufacturer Specific Data)
- **Company ID**: 0xFFFF (configurable)
- **Sequence**: Data sequence number (increments when data changes)
- **Total Segments**: 4 (fixed)
- **Current Segment**: 0-3 (segment index)
- **Data**: 20 bytes of actual manufacturer data

### Example Data Packet

For segment 0 of test data (1-80):
```
0x17 0xFF 0xFF 0xFF 0x00 0x04 0x00 0x01 0x02 0x03 ... 0x14
|    |    |    |    |    |    |    |    |    |    |
23   0xFF 0xFFFF  0   4    0   1    2    3    ...  20
```

## API Reference

### Public Methods

```cpp
class NRF52Bluetooth {
public:
    // Set manufacturer data (80 bytes max)
    void setManufacturerData(const uint8_t* data, size_t length);
    
    // Start broadcasting manufacturer data
    void startManufacturerDataBroadcasting();
    
    // Stop broadcasting manufacturer data
    void stopManufacturerDataBroadcasting();
    
    // Check if manufacturer data is currently being broadcasted
    bool isManufacturerDataBroadcasting();
    
    // Update manufacturer data broadcast manually (called automatically)
    void updateManufacturerDataBroadcasting();
};
```

## Usage Examples

### Basic Usage

```cpp
#include "platform/nrf52/NRF52Bluetooth.h"

extern NRF52Bluetooth* nrf52Bluetooth;

// Set manufacturer data
uint8_t myData[80] = { /* your 80 bytes */ };
nrf52Bluetooth->setManufacturerData(myData, sizeof(myData));

// Start broadcasting
nrf52Bluetooth->startManufacturerDataBroadcasting();

// Check status
if (nrf52Bluetooth->isManufacturerDataBroadcasting()) {
    Serial.println("Broadcasting manufacturer data");
}

// Stop broadcasting
nrf52Bluetooth->stopManufacturerDataBroadcasting();
```

### Serial Command Trigger

```cpp
void handleSerialCommands() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd == "start") {
            nrf52Bluetooth->startManufacturerDataBroadcasting();
            Serial.println("Started broadcasting");
        }
        else if (cmd == "stop") {
            nrf52Bluetooth->stopManufacturerDataBroadcasting();
            Serial.println("Stopped broadcasting");
        }
    }
}
```

### Load Data from Flash (Production)

```cpp
#include "FS.h"
#include "LittleFS.h"

void loadManufacturerDataFromFlash() {
    uint8_t flashData[80];
    File file = LittleFS.open("/manufacturer_data.bin", "r");
    
    if (file) {
        size_t bytesRead = file.read(flashData, sizeof(flashData));
        file.close();
        
        if (bytesRead > 0) {
            nrf52Bluetooth->setManufacturerData(flashData, bytesRead);
            LOG_INFO("Loaded %d bytes from flash", bytesRead);
        }
    }
}
```

### Dynamic Data Update

```cpp
void updateDynamicData() {
    uint8_t dynamicData[80];
    
    // Fill with dynamic data (e.g., sensor readings, status, etc.)
    uint32_t nodeId = myNodeInfo.my_node_num;
    memcpy(&dynamicData[0], &nodeId, 4);
    
    uint32_t timestamp = millis();
    memcpy(&dynamicData[4], &timestamp, 4);
    
    // Add sensor data...
    dynamicData[8] = batteryLevel;
    dynamicData[9] = temperature;
    
    // Update broadcast data
    nrf52Bluetooth->setManufacturerData(dynamicData, 10);
}
```

## Receiving and Reconstructing Data

On the receiving side (e.g., mobile app, scanner), follow these steps:

### 1. Scan for Devices

Scan continuously for at least 2.5 seconds to capture all segments.

### 2. Filter by Manufacturer ID

```python
# Example in Python (bleak library)
from bleak import BleakScanner

def detection_callback(device, advertisement_data):
    if 0xFFFF in advertisement_data.manufacturer_data:
        process_manufacturer_data(device, advertisement_data.manufacturer_data[0xFFFF])

scanner = BleakScanner(detection_callback=detection_callback)
scanner.start()
```

### 3. Collect and Reconstruct Segments

```python
from collections import defaultdict

class DataCollector:
    def __init__(self):
        self.collected_data = defaultdict(dict)  # {sequence: {segment: data}}
        self.current_sequence = None
    
    def process_packet(self, raw_data):
        # Parse packet
        if len(raw_data) < 5:
            return None
        
        company_id = (raw_data[1] << 8) | raw_data[0]
        sequence = raw_data[2]
        total_segments = raw_data[3]
        current_segment = raw_data[4]
        data = raw_data[5:]
        
        # Store segment
        self.collected_data[sequence][current_segment] = data
        
        # Check if we have all segments
        if len(self.collected_data[sequence]) == total_segments:
            return self.reconstruct_data(sequence)
        
        return None
    
    def reconstruct_data(self, sequence):
        segments = self.collected_data[sequence]
        full_data = bytearray()
        
        # Concatenate segments in order
        for segment_num in sorted(segments.keys()):
            full_data.extend(segments[segment_num])
        
        # Clean up old sequences
        old_sequences = [seq for seq in self.collected_data if seq != sequence]
        for old_seq in old_sequences:
            del self.collected_data[old_seq]
        
        return bytes(full_data)

collector = DataCollector()
```

### 4. Process Complete Data

```python
def process_complete_data(data):
    if len(data) == 80:
        print(f"Received complete manufacturer data: {data.hex()}")
        # Process the data according to your protocol
        process_device_info(data)
    else:
        print(f"Warning: Expected 80 bytes, got {len(data)}")
```

## Configuration Constants

Edit `src/platform/nrf52/NRF52Bluetooth.h` to customize:

```cpp
#define MANUFACTURER_DATA_MAX_SIZE 80        // Maximum data size
#define MANUFACTURER_DATA_SEGMENTS 4          // Number of segments
#define MANUFACTURER_DATA_SEGMENT_SIZE 20     // Bytes per segment
#define MANUFACTURER_ID 0xFFFF               // Manufacturer ID
#define MANUFACTURER_BROADCAST_INTERVAL_MS 500 // Update interval
#define MANUFACTURER_BROADCAST_CYCLE_PERIODS 5 // Cycle length
```

## Implementation Details

### Storage

- Manufacturer data is stored in RAM
- For production, load from flash or EEPROM on startup
- Sequence number increments when data changes

### Thread Safety

- The broadcaster runs as a FreeRTOS thread
- Thread-safe data updates
- Atomic operations for critical sections

### Power Management

- Broadcasting continues even in sleep mode
- Can be stopped to save power when not needed
- Automatic restoration of original scan response on stop

## Troubleshooting

### Data Not Received

1. **Ensure scanning duration**: Scan for at least 2.5 seconds
2. **Check manufacturer ID**: Verify both sides use 0xFFFF
3. **Verify segment collection**: Ensure all 4 segments are collected
4. **Validate sequence number**: Check if segments belong to the same sequence

### Device Not Discoverable

- The feature preserves original scan response in Period 1
- Device should remain discoverable during broadcasting
- If discoverability is affected, stop broadcasting and check original BLE settings

### Performance Issues

- Update interval is 500ms, which is conservative
- Can be reduced (e.g., 250ms) if needed
- Monitor CPU usage and power consumption

## Testing

### Basic Test

```cpp
// Test with known data
uint8_t testData[80];
for (int i = 0; i < 80; i++) {
    testData[i] = i + 1;  // 1, 2, 3, ..., 80
}

nrf52Bluetooth->setManufacturerData(testData, 80);
nrf52Bluetooth->startManufacturerDataBroadcasting();
```

### Verify with nRF Connect

1. Install nRF Connect app (Android/iOS)
2. Start scanning
3. Find your device and inspect advertising data
4. Look for manufacturer data with ID 0xFFFF
5. Verify segment rotation every 500ms

### Serial Monitor

```
Type 'help' for available serial commands.

> status
Broadcasting: YES

> update
Manufacturer data updated

> stop
Manufacturer data broadcast stopped
```

## Integration with Main Firmware

### Initialization

```cpp
void setup() {
    // Initialize BLE
    nrf52Bluetooth = new NRF52Bluetooth();
    nrf52Bluetooth->setup();
    
    // Load manufacturer data from flash
    loadManufacturerDataFromFlash();
    
    // Optionally start broadcasting
    // nrf52Bluetooth->startManufacturerDataBroadcasting();
}
```

### Loop Update

```cpp
void loop() {
    // Handle serial commands
    handleSerialCommands();
    
    // Handle button presses
    handleButtonPress();
    
    // Process mobile app commands
    handleMobileCommands();
    
    // Update manufacturer data broadcast
    nrf52Bluetooth->updateManufacturerDataBroadcasting();
    
    delay(10);
}
```

## Performance Characteristics

- **Power Consumption**: Minimal (BLE scan response)
- **CPU Usage**: Negligible (500ms updates)
- **Memory**: ~100 bytes RAM for data storage
- **Bandwidth**: Effective throughput: 32 bytes/s (80 bytes / 2.5s)

## Future Enhancements

Potential improvements:

1. **Data Compression**: Compress data to reduce segments
2. **Encryption**: Encrypt manufacturer data for security
3. **Dynamic Adjustment**: Adjust interval based on network conditions
4. **Multiple Data Types**: Support different data formats
5. **Ack/Retransmission**: Add reliability mechanisms

## References

- [BLE Advertising Data Format](https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/)
- [nRF52 BLE Documentation](https://infocenter.nordicsemi.com/)
- [Meshtastic Firmware](https://github.com/meshtastic/firmware)

## Support

For issues or questions:
1. Check the troubleshooting section
2. Review example code
3. Consult Meshtastic community forums
4. Submit an issue on GitHub