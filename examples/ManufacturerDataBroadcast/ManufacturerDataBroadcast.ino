/**
 * Manufacturer Data Broadcast Example
 * 
 * This example demonstrates how to use the manufacturer data broadcasting feature
 * to send 80 bytes of custom manufacturer data via BLE scan response.
 * 
 * The data is split into 4 segments (20 bytes each) and broadcasted every 500ms.
 * A complete cycle takes 2.5 seconds (including one period of original content).
 * 
 * Features:
 * - Broadcast 80 bytes of manufacturer data (ID: 0xFFFF)
 * - Automatic segmentation (4 segments of 20 bytes each)
 * - 500ms update interval
 * - Can be triggered by button, serial command, or mobile app
 * - Preserves original scan response (TxPower + Device Name) for discoverability
 */

#include "configuration.h"
#include "platform/nrf52/NRF52Bluetooth.h"

// External references to global variables and functions
extern class NRF52Bluetooth *nrf52Bluetooth;
extern bool buttonPressed(); // If your board has a button

// Test data: 1-80 (can be replaced with actual manufacturer data)
static uint8_t testData[80] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
    51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 62, 63, 64, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80
};

// Example 1: Manual trigger via serial command
void handleSerialCommand()
{
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd == "start") {
            Serial.println("Starting manufacturer data broadcast...");
            if (nrf52Bluetooth) {
                nrf52Bluetooth->startManufacturerDataBroadcasting();
                Serial.println("Manufacturer data broadcast started");
            } else {
                Serial.println("ERROR: nrf52Bluetooth not initialized!");
            }
        }
        else if (cmd == "stop") {
            LOG_INFO("Stopping manufacturer data broadcast...");
            nrf52Bluetooth->stopManufacturerDataBroadcasting();
            Serial.println("Manufacturer data broadcast stopped");
        }
        else if (cmd == "status") {
            bool isBroadcasting = nrf52Bluetooth->isManufacturerDataBroadcasting();
            Serial.print("Broadcasting: ");
            Serial.println(isBroadcasting ? "YES" : "NO");
        }
        else if (cmd == "update") {
            // Update with test data
            nrf52Bluetooth->setManufacturerData(testData, sizeof(testData));
            Serial.println("Manufacturer data updated");
        }
        else if (cmd == "help") {
            Serial.println("Available commands:");
            Serial.println("  start  - Start manufacturer data broadcast");
            Serial.println("  stop   - Stop manufacturer data broadcast");
            Serial.println("  status - Check broadcast status");
            Serial.println("  update - Update manufacturer data");
            Serial.println("  help   - Show this help");
        }
        else {
            Serial.println("Unknown command. Type 'help' for available commands.");
        }
    }
}

// Example 2: Trigger via button press
#if HAS_BUTTON
void handleButtonPress()
{
    static unsigned long lastButtonPress = 0;
    static bool isBroadcasting = false;
    
    // Check if button is pressed (debounce)
    if (buttonPressed() && millis() - lastButtonPress > 1000) {
        lastButtonPress = millis();
        
        isBroadcasting = !isBroadcasting;
        
        if (isBroadcasting) {
            LOG_INFO("Button pressed: Starting manufacturer data broadcast");
            nrf52Bluetooth->startManufacturerDataBroadcasting();
        } else {
            LOG_INFO("Button pressed: Stopping manufacturer data broadcast");
            nrf52Bluetooth->stopManufacturerDataBroadcasting();
        }
    }
}
#endif

// Example 3: Trigger via mobile app (through Bluetooth)
// This would typically be integrated into the PhoneAPI/FromRadio handler
void handleMobileCommand(const uint8_t* data, size_t length)
{
    // Example: Check for custom command from mobile app
    if (length > 0 && data[0] == 0xFF) {
        // Custom manufacturer data command
        if (length >= 2) {
            switch (data[1]) {
                case 0x01: // Start broadcast
                    nrf52Bluetooth->startManufacturerDataBroadcasting();
                    LOG_INFO("Mobile: Start manufacturer data broadcast");
                    break;
                case 0x02: // Stop broadcast
                    nrf52Bluetooth->stopManufacturerDataBroadcasting();
                    LOG_INFO("Mobile: Stop manufacturer data broadcast");
                    break;
                case 0x03: // Update data
                    if (length >= 3 && length <= 83) {
                        uint8_t newData[80];
                        memcpy(newData, &data[2], length - 2);
                        nrf52Bluetooth->setManufacturerData(newData, length - 2);
                        LOG_INFO("Mobile: Updated %d bytes of manufacturer data", length - 2);
                    }
                    break;
            }
        }
    }
}

// Example 4: Load manufacturer data from flash (for production use)
void loadManufacturerDataFromFlash()
{
    // This is where you would load data from flash memory
    // Example code structure:
    
    /*
    uint8_t flashData[80];
    size_t bytesRead = readFromFlash(MANUFACTURER_DATA_FLASH_ADDR, flashData, sizeof(flashData));
    
    if (bytesRead > 0) {
        nrf52Bluetooth->setManufacturerData(flashData, bytesRead);
        LOG_INFO("Loaded %d bytes of manufacturer data from flash", bytesRead);
    } else {
        LOG_WARN("Failed to load manufacturer data from flash, using default");
    }
    */
}

// Example 5: Dynamic data generation (e.g., sensor data)
void updateDynamicManufacturerData()
{
    // Example: Generate dynamic data based on sensors/status
    uint8_t dynamicData[80] = {0};
    
    // Example structure:
    // [Device ID (4 bytes)] [Timestamp (4 bytes)] [Status (1 byte)] 
    // [Sensor data (variable)] [Reserved (remaining bytes)]
    
    // Fill with device info
    uint32_t nodeId = myNodeInfo.my_node_num;
    memcpy(&dynamicData[0], &nodeId, 4);
    
    uint32_t timestamp = millis();
    memcpy(&dynamicData[4], &timestamp, 4);
    
    dynamicData[8] = 0x01; // Status byte
    
    // Add sensor data (example)
    uint8_t batteryLevel = 85;
    dynamicData[9] = batteryLevel;
    
    // Update the broadcast data
    nrf52Bluetooth->setManufacturerData(dynamicData, 10); // Update with 10 bytes
    
    LOG_DEBUG("Updated dynamic manufacturer data");
}

void setup()
{
    // Initialize serial for debugging
    Serial.begin(115200);
    
    // Wait for serial to be ready
    delay(1000);
    
    Serial.println("========================================");
    Serial.println("Manufacturer Data Broadcast Example");
    Serial.println("========================================");
    Serial.println();
    Serial.println("This example demonstrates BLE manufacturer data broadcasting.");
    Serial.println("The 80-byte data is split into 4 segments and broadcasted every 500ms.");
    Serial.println();
    Serial.println("Trigger methods:");
    Serial.println("  1. Serial commands: 'start', 'stop', 'status', 'update', 'help'");
    Serial.println("  2. Button press (if available)");
    Serial.println("  3. Mobile app commands (custom protocol)");
    Serial.println();
    Serial.println("Data format (each segment):");
    Serial.println("  [Company ID (2 bytes)] [Sequence (1 byte)] [Total Segments (1 byte)]");
    Serial.println("  [Current Segment (1 byte)] [Data (up to 20 bytes)]");
    Serial.println();
    
    // Initialize manufacturer data with test data
    if (nrf52Bluetooth) {
        nrf52Bluetooth->setManufacturerData(testData, sizeof(testData));
        Serial.println("Manufacturer data initialized with test data (1-80)");
    }
    
    // Option: Load from flash (uncomment for production use)
    // loadManufacturerDataFromFlash();
    
    Serial.println();
    Serial.println("Type 'help' for available serial commands.");
    Serial.println();
}

void loop()
{
    // Handle serial commands
    handleSerialCommand();
    
    // Handle button press (if available)
    #if HAS_BUTTON
    handleButtonPress();
    #endif
    
    // Option: Update dynamic data periodically
    // static unsigned long lastDynamicUpdate = 0;
    // if (millis() - lastDynamicUpdate > 60000) { // Every 60 seconds
    //     updateDynamicManufacturerData();
    //     lastDynamicUpdate = millis();
    // }
    
    // Small delay to prevent CPU overload
    delay(10);
}

/**
 * Receiver-Side Notes:
 * 
 * To receive and reconstruct the manufacturer data on the receiving side:
 * 
 * 1. Continuously scan for BLE devices for at least 2.5 seconds (5 periods)
 * 2. Filter devices by Manufacturer ID (0xFFFF)
 * 3. Collect all segments (0, 1, 2, 3) with the same sequence number
 * 4. Reconstruct the 80-byte data by concatenating segments in order
 * 5. Validate the sequence number to ensure you have the latest data
 * 
 * Pseudo-code for receiver:
 * 
 * ```
 * Map<sequence, Array<segments>> collectedData;
 * 
 * onAdvertisementReceived(advertisement) {
 *     if (advertisement.hasManufacturerData(0xFFFF)) {
 *         packet = advertisement.getManufacturerData();
 *         sequence = packet[2];
 *         segment = packet[4];
 *         totalSegments = packet[3];
 *         
 *         collectedData[sequence][segment] = packet.data;
 *         
 *         if (collectedData[sequence].size() == totalSegments) {
 *             // All segments collected
 *             fullData = reconstruct(collectedData[sequence]);
 *             processManufacturerData(fullData);
 *         }
 *     }
 * }
 * ```
 */