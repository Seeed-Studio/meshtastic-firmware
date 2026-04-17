/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/
#include <bluefruit.h>

#define MAX_PRPH_CONNECTION   2
ble_gap_addr_t addr;
uint8_t connection_count = 0;

// BLE Service
BLEDfu  bledfu;  // OTA DFU service

BLEUart bleuart; // uart over ble

char ble_name_buffer[20] = {0};

const uint16_t MANUFACTURER_COMPANY_ID = 0xFFFF;
const uint8_t FACTORY_DATA_TOTAL_LEN = 80;
const uint8_t FACTORY_CHUNK_LEN = 9;
const uint8_t FACTORY_CHUNK_COUNT = (FACTORY_DATA_TOTAL_LEN + FACTORY_CHUNK_LEN - 1) / FACTORY_CHUNK_LEN;
const uint32_t ADV_UPDATE_INTERVAL_MS = 2000UL;
const uint8_t ADV_SHORT_NAME_LEN = 5;

uint8_t factory_data[FACTORY_DATA_TOTAL_LEN] = {
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
  0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
  0x57, 0x58, 0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C,
  0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x80, 0x81,
  0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91
};

uint32_t last_adv_update_ms = 0;
int8_t next_adv_chunk_index = 0;

void buildBaseAdvertising(void);
void addNameToAdvertising(void);
void setBaseScanResponse(void);
void setFactoryChunkScanResponse(uint8_t chunk_index);
void restartAdvertisingPayload(void);
void updateAdvertisingPayload(void);
void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb
  
  Serial.println("Bluefruit52 BLEUART Example");
  Serial.println("---------------------------\n");

  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behaviour, but provided
  // here in case you want to control this LED manually via PIN 19

  // Initialize Bluefruit with max concurrent connections as Peripheral = 2, Central = 0
  Bluefruit.begin(MAX_PRPH_CONNECTION, 0);
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values

  addr = Bluefruit.getAddr();

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();

  hexToString(addr.addr, 6, ble_name_buffer);


  // Set up and start advertising
  startAdv();


  // /* Start Central Scanning
  //  * - Enable auto scan if disconnected
  //  * - Filter out packet with a min rssi
  //  * - Interval = 100 ms, window = 50 ms
  //  * - Use active scan (used to retrieve the optional scan response adv packet)
  //  * - Start(0) = will scan forever since no timeout is given
  //  */
  // Bluefruit.Scanner.setRxCallback(scan_callback);
  // Bluefruit.Scanner.restartOnDisconnect(true);
  // Bluefruit.Scanner.filterRssi(-80);
  // //Bluefruit.Scanner.filterUuid(BLEUART_UUID_SERVICE); // only invoke callback if detect bleuart service
  // Bluefruit.Scanner.setInterval(160, 80);       // in units of 0.625 ms
  // Bluefruit.Scanner.useActiveScan(true);        // Request scan response data
  // Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds

  Serial.println("Please use Adafruit's Bluefruit LE app to connect in UART mode");
  Serial.println("Once connected, enter character(s) that you wish to send");
}

void startAdv(void)
{
  Bluefruit.setName(ble_name_buffer);
  buildBaseAdvertising();
  setFactoryChunkScanResponse(0);
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  

  last_adv_update_ms = millis();
  next_adv_chunk_index = 1;

  Serial.print("Advertising manufacturer chunk 1/");
  Serial.println(FACTORY_CHUNK_COUNT);

}

void buildBaseAdvertising(void)
{
  Bluefruit.Advertising.clearData();

  // Keep the original fields in the primary advertising packet.
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  addNameToAdvertising();
}

void addNameToAdvertising(void)
{
  uint8_t short_name_len = strlen(ble_name_buffer);

  if (short_name_len > ADV_SHORT_NAME_LEN)
  {
    short_name_len = ADV_SHORT_NAME_LEN;
  }

  Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, ble_name_buffer, short_name_len);
}

void setBaseScanResponse(void)
{
  Bluefruit.ScanResponse.clearData();
  Bluefruit.ScanResponse.addName();
}

void setFactoryChunkScanResponse(uint8_t chunk_index)
{
  uint8_t manufacturer_data[6 + FACTORY_CHUNK_LEN] = { 0 };
  uint8_t payload_len = FACTORY_CHUNK_LEN;
  uint8_t offset = chunk_index * FACTORY_CHUNK_LEN;

  if (offset >= FACTORY_DATA_TOTAL_LEN)
  {
    return;
  }

  if (offset + payload_len > FACTORY_DATA_TOTAL_LEN)
  {
    payload_len = FACTORY_DATA_TOTAL_LEN - offset;
  }

  manufacturer_data[0] = (uint8_t) (MANUFACTURER_COMPANY_ID & 0xFF);
  manufacturer_data[1] = (uint8_t) (e >> 8);
  manufacturer_data[2] = 0x01;                   // Data format version
  manufacturer_data[3] = chunk_index;            // Current chunk index, zero-based
  manufacturer_data[4] = FACTORY_CHUNK_COUNT;    // Total chunk count
  manufacturer_data[5] = payload_len;            // Valid payload length in this chunk
  memcpy(manufacturer_data + 6, factory_data + offset, payload_len);

  Bluefruit.ScanResponse.clearData();
  Bluefruit.ScanResponse.addName();
  Bluefruit.ScanResponse.addManufacturerData(manufacturer_data, payload_len + 6);
}

void restartAdvertisingPayload(void)
{
  if (connection_count >= MAX_PRPH_CONNECTION)
  {
    return;
  }

  Bluefruit.Advertising.start(0);
}

void updateAdvertisingPayload(void)
{
  uint32_t now = millis();

  if (now - last_adv_update_ms < ADV_UPDATE_INTERVAL_MS)
  {
    return;
  }

  last_adv_update_ms = now;

  if (next_adv_chunk_index < FACTORY_CHUNK_COUNT)
  {
    setFactoryChunkScanResponse(next_adv_chunk_index);
    Serial.print("Advertising manufacturer chunk ");
    Serial.print(next_adv_chunk_index + 1);
    Serial.print("/");
    Serial.println(FACTORY_CHUNK_COUNT);
    next_adv_chunk_index++;
  }
  else
  {
    setBaseScanResponse();
    Serial.println("Advertising base payload without manufacturer data");
    next_adv_chunk_index = 0;
  }

  restartAdvertisingPayload();
}


// print a string to Serial Uart and all connected BLE Uart
void printAll(uint8_t* buf, int count)
{
  Serial.write(buf, count);

  // Send to all connected centrals
  for (uint8_t conn_hdl=0; conn_hdl < MAX_PRPH_CONNECTION; conn_hdl++)
  {
    bleuart.write(conn_hdl, buf, count);
  }
}

void loop()
{
  uint8_t buf[64];
  int count;

  updateAdvertisingPayload();

  // Forward data from HW Serial to BLEUART
  while (Serial.available())
  {
    // Delay to wait for enough input
    delay(2);
    count = Serial.readBytes(buf, sizeof(buf));

    printAll(buf, count);
  }

  // Serial.print("MAC: ");
  // for (int i = 5; i >= 0; i--)
  // {
  //   Serial.print(addr.addr[i], HEX);
  //   if (i > 0) Serial.print(":");
  // }
  // Forward from BLEUART to HW Serial
  while ( bleuart.available() )
  {
    count = bleuart.read(buf, sizeof(buf));

    printAll(buf, count);
  }
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);

  connection_count++;
  Serial.print("Connection count: ");
  Serial.println(connection_count);
  
  // Keep advertising if not reaching max
  if (connection_count < MAX_PRPH_CONNECTION)
  {
    Serial.println("Keep advertising");
    Bluefruit.Advertising.start(0);
  }
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);

  connection_count--;
}

void scan_callback(ble_gap_evt_adv_report_t* report)
{
  PRINT_LOCATION();
  uint8_t len = 0;
  uint8_t buffer[32];
  memset(buffer, 0, sizeof(buffer));
  
  /* Display the timestamp and device address */
  if (report->type.scan_response)
  {
    Serial.printf("[SR%10d] Packet received from ", millis());
  }
  else
  {
    Serial.printf("[ADV%9d] Packet received from ", millis());
  }
  // MAC is in little endian --> print in reverse order
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial.print("\n");

  /* Raw buffer contents */
  Serial.printf("%14s %d bytes\n", "PAYLOAD", report->data.len);
  if (report->data.len)
  {
    Serial.printf("%15s", " ");
    Serial.printBuffer(report->data.p_data, report->data.len, '-');
    Serial.println();
  }

  /* RSSI value */
  Serial.printf("%14s %d dBm\n", "RSSI", report->rssi);

  /* Adv Type */
  Serial.printf("%14s ", "ADV TYPE");
  if ( report->type.connectable ) 
  {
    Serial.print("Connectable ");
  }else
  {
    Serial.print("Non-connectable ");
  }
  
  if ( report->type.directed )
  {
    Serial.println("directed");
  }else
  {
    Serial.println("undirected");
  }

  /* Shortened Local Name */
  if(Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, buffer, sizeof(buffer)))
  {
    Serial.printf("%14s %s\n", "SHORT NAME", buffer);
    memset(buffer, 0, sizeof(buffer));
  }

  /* Complete Local Name */
  if(Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof(buffer)))
  {
    Serial.printf("%14s %s\n", "COMPLETE NAME", buffer);
    memset(buffer, 0, sizeof(buffer));
  }

  /* TX Power Level */
  if (Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_TX_POWER_LEVEL, buffer, sizeof(buffer)))
  {
    Serial.printf("%14s %i\n", "TX PWR LEVEL", buffer[0]);
    memset(buffer, 0, sizeof(buffer));
  }

  /* Check for UUID16 Complete List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid16List(buffer, len);
  }

  /* Check for UUID16 More Available List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid16List(buffer, len);
  }

  /* Check for UUID128 Complete List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid128List(buffer, len);
  }

  /* Check for UUID128 More Available List */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE, buffer, sizeof(buffer));
  if ( len )
  {
    printUuid128List(buffer, len);
  }  

  /* Check for BLE UART UUID */
  if ( Bluefruit.Scanner.checkReportForUuid(report, BLEUART_UUID_SERVICE) )
  {
    Serial.printf("%14s %s\n", "BLE UART", "UUID Found!");
  }

  /* Check for DIS UUID */
  if ( Bluefruit.Scanner.checkReportForUuid(report, UUID16_SVC_DEVICE_INFORMATION) )
  {
    Serial.printf("%14s %s\n", "DIS", "UUID Found!");
  }

  /* Check for Manufacturer Specific Data */
  len = Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, buffer, sizeof(buffer));
  if (len)
  {
    Serial.printf("%14s ", "MAN SPEC DATA");
    Serial.printBuffer(buffer, len, '-');
    Serial.println();
    memset(buffer, 0, sizeof(buffer));
  }  

  Serial.println();

  // For SoftDevice v6: after a report is received, scanning is paused.
  // Call resume() to continue scanning.
  Bluefruit.Scanner.resume();
}


void printUuid16List(uint8_t* buffer, uint8_t len)
{
  Serial.printf("%14s %s", "16-Bit UUID");
  for(int i=0; i<len; i+=2)
  {
    uint16_t uuid16;
    memcpy(&uuid16, buffer+i, 2);
    Serial.printf("%04X ", uuid16);
  }
  Serial.println();
}

void printUuid128List(uint8_t* buffer, uint8_t len)
{
  (void) len;
  Serial.printf("%14s %s", "128-Bit UUID");

  // Print in reversed order.
  for(int i=0; i<16; i++)
  {
    const char* fm = (i==4 || i==6 || i==8 || i==10) ? "-%02X" : "%02X";
    Serial.printf(fm, buffer[15-i]);
  }

  Serial.println();  
}

void hexToString(uint8_t *data, int len, char *out)
{
  for (int i = 0; i < len; i++)
  {
    sprintf(out + i * 2, "%02X", data[len - 1 - i]);
  }

  out[len * 2] = '\0';
}