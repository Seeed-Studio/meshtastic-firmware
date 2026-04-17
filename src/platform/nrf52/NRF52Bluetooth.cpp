#include "NRF52Bluetooth.h"
#include "BLEDfuSecure.h"
#include "BluetoothCommon.h"
#include "PowerFSM.h"
#include "configuration.h"
#include "main.h"
#include "mesh/PhoneAPI.h"
#include "mesh/mesh-pb-constants.h"
#include <bluefruit.h>
#include <utility/bonding.h>
#include <ble_gap.h>
#include "concurrency/OSThread.h"
#include <string.h>
static BLEService meshBleService = BLEService(BLEUuid(MESH_SERVICE_UUID_16));
static BLECharacteristic fromNum = BLECharacteristic(BLEUuid(FROMNUM_UUID_16));
static BLECharacteristic fromRadio = BLECharacteristic(BLEUuid(FROMRADIO_UUID_16));
static BLECharacteristic toRadio = BLECharacteristic(BLEUuid(TORADIO_UUID_16));
static BLECharacteristic logRadio = BLECharacteristic(BLEUuid(LOGRADIO_UUID_16));

static BLEDis bledis; // DIS (Device Information Service) helper class instance
static BLEBas blebas; // BAS (Battery Service) helper class instance
#ifndef BLE_DFU_SECURE
static BLEDfu bledfu; // DFU software update helper service
#else
static BLEDfuSecure bledfusecure;                                             // DFU software update helper service
#endif

// This scratch buffer is used for various bluetooth reads/writes - but it is safe because only one bt operation can be in
// process at once
// static uint8_t trBytes[_max(_max(_max(_max(ToRadio_size, RadioConfig_size), User_size), MyNodeInfo_size), FromRadio_size)];
static uint8_t fromRadioBytes[meshtastic_FromRadio_size];
static uint8_t toRadioBytes[meshtastic_ToRadio_size];

// Last ToRadio value received from the phone
static uint8_t lastToRadio[MAX_TO_FROM_RADIO_SIZE];

static uint16_t connectionHandle;
static bool passkeyShowing;
static constexpr uint8_t MAX_PRPH_CONNECTIONS = 2;

static void configureAdvertisingParameters()
{
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 668); // in unit of 0.625 ms
    Bluefruit.Advertising.setFastTimeout(30);   // number of seconds in fast mode
}

static void logManufacturerChunkHex(const uint8_t *data, uint8_t length)
{
    if (data == nullptr || length == 0) {
        LOG_DEBUG("Manufacturer chunk raw: <empty>");
        return;
    }

    char hexBuf[(32 * 3) + 1] = {0};
    size_t offset = 0;

    for (uint8_t i = 0; i < length && offset + 4 < sizeof(hexBuf); ++i) {
        offset += snprintf(hexBuf + offset, sizeof(hexBuf) - offset, "%02X ", data[i]);
    }

    if (offset > 0) {
        hexBuf[offset - 1] = '\0';
    }

    LOG_INFO("Manufacturer chunk raw (%u bytes): %s", length, hexBuf);
}

static void addShortNameToAdvertising()
{
    const char *deviceName = getDeviceName();
    size_t fullNameLen = strlen(deviceName);
    if (fullNameLen > 5) {
        fullNameLen = 5;
    }

    Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, deviceName, fullNameLen);
}

static bool buildBaseAdvertising()
{
    Bluefruit.Advertising.clearData();
    bool ok = true;
    ok &= Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    ok &= Bluefruit.Advertising.addTxPower();
    ok &= Bluefruit.Advertising.addService(meshBleService);
    addShortNameToAdvertising();
    return ok;
}

static void setBaseScanResponse()
{
    Bluefruit.ScanResponse.clearData();
    Bluefruit.ScanResponse.addName();
}

static bool setManufacturerChunkScanResponse(const uint8_t *manufacturerData, uint8_t manufacturerLen)
{
    Bluefruit.ScanResponse.clearData();
    
    Bluefruit.ScanResponse.addName();

    logManufacturerChunkHex(manufacturerData, manufacturerLen);

    if (!Bluefruit.ScanResponse.addManufacturerData(manufacturerData, manufacturerLen)) {
        return false;
    }


    return true;
}

static bool restartAdvertisingIfPossible(const char *reason)
{
    configureAdvertisingParameters();

    uint8_t connectionCount = Bluefruit.connected();
    uint8_t advLen = Bluefruit.Advertising.count();
    uint8_t scanRspLen = Bluefruit.ScanResponse.count();
    if (connectionCount >= MAX_PRPH_CONNECTIONS) {
        LOG_DEBUG("Skip advertising restart after %s: %u/%u peripheral links in use", reason, connectionCount,
                  MAX_PRPH_CONNECTIONS);
        return true;
    }

    if (Bluefruit.Advertising.isRunning()) {
        if (connectionCount > 0) {
            LOG_INFO("Advertising already active after %s (%u/%u links used), skip stop/start reconfigure", reason,
                     connectionCount, MAX_PRPH_CONNECTIONS);
            return true;
        }
    }

    LOG_INFO("Restart advertising after %s (adv=%u bytes, scanRsp=%u bytes, running=%s)", reason, advLen, scanRspLen,
             Bluefruit.Advertising.isRunning() ? "true" : "false");

    bool started = Bluefruit.Advertising.start(0);
    if (started) {
        LOG_DEBUG("Advertising active after %s (%u/%u links used)", reason, connectionCount, MAX_PRPH_CONNECTIONS);
    } else {
        LOG_WARN("Failed to start advertising after %s (adv=%u bytes, scanRsp=%u bytes)", reason, advLen,
                 scanRspLen);
    }

    return started;
}

class BluetoothPhoneAPI : public PhoneAPI
{
    /**
     * Subclasses can use this as a hook to provide custom notifications for their transport (i.e. bluetooth notifies)
     */
    virtual void onNowHasData(uint32_t fromRadioNum) override
    {
        PhoneAPI::onNowHasData(fromRadioNum);

        LOG_INFO("BLE notify fromNum");
        fromNum.notify32(fromRadioNum);
    }

    /// Check the current underlying physical link to see if the client is currently connected
    virtual bool checkIsConnected() override { return Bluefruit.connected(connectionHandle); }

  public:
    BluetoothPhoneAPI() { api_type = TYPE_BLE; }
};

static BluetoothPhoneAPI *bluetoothPhoneAPI;

void onConnect(uint16_t conn_handle)
{
    // Get the reference to current connection
    BLEConnection *connection = Bluefruit.Connection(conn_handle);
    connectionHandle = conn_handle;
    char central_name[32] = {0};
    connection->getPeerName(central_name, sizeof(central_name));
    LOG_INFO("BLE Connected to %s", central_name);

    // Notify UI (or any other interested firmware components)
    meshtastic::BluetoothStatus newStatus(meshtastic::BluetoothStatus::ConnectionState::CONNECTED);
    bluetoothStatus->updateStatus(&newStatus);
    
    uint8_t connectionCount = Bluefruit.connected();
    if (connectionCount < MAX_PRPH_CONNECTIONS) {
        LOG_INFO("BLE keeping advertising active for additional connections (%u/%u)", connectionCount,
                 MAX_PRPH_CONNECTIONS);
        restartAdvertisingIfPossible("BLE connection");
    } else {
        LOG_INFO("BLE advertising paused: reached max peripheral connections (%u/%u)", connectionCount,
                 MAX_PRPH_CONNECTIONS);
    }
}
/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void onDisconnect(uint16_t conn_handle, uint8_t reason)
{
    LOG_INFO("BLE Disconnected, reason = 0x%x", reason);
    if (bluetoothPhoneAPI) {
        bluetoothPhoneAPI->close();
    }

    // Clear the last ToRadio packet buffer to avoid rejecting first packet from new connection
    memset(lastToRadio, 0, sizeof(lastToRadio));

    // Notify UI (or any other interested firmware components)
    meshtastic::BluetoothStatus newStatus(meshtastic::BluetoothStatus::ConnectionState::DISCONNECTED);
    bluetoothStatus->updateStatus(&newStatus);

#if HAS_SCREEN
    // If a pairing prompt is active, make sure we dismiss it on disconnect/cancel/failure paths.
    if (passkeyShowing) {
        passkeyShowing = false;
        if (screen) {
            screen->endAlert();
        }
    }
#endif
}
void onCccd(uint16_t conn_hdl, BLECharacteristic *chr, uint16_t cccd_value)
{
    // Display the raw request packet
    LOG_INFO("CCCD Updated: %u", cccd_value);
    // Check the characteristic this CCCD update is associated with in case
    // this handler is used for multiple CCCD records.

    // According to the GATT spec: cccd value = 0x0001 means notifications are enabled
    // and cccd value = 0x0002 means indications are enabled

    if (chr->uuid == fromNum.uuid || chr->uuid == logRadio.uuid) {
        auto result = cccd_value == 2 ? chr->indicateEnabled(conn_hdl) : chr->notifyEnabled(conn_hdl);
        if (result) {
            LOG_INFO("Notify/Indicate enabled");
        } else {
            LOG_INFO("Notify/Indicate disabled");
        }
    }
}
void startAdv(void)
{
    if (!buildBaseAdvertising()) {
        LOG_WARN("Failed to build default BLE advertising payload");
    }
    setBaseScanResponse();

    /* Start Advertising
     * - Enable multi-connection mode (BLE 4.2+ supports up to 8 simultaneous connections)
     * - Continue advertising while connected (restartOnDisconnect is set to true)
     * - Interval:  fast mode = 20 ms, slow mode = 417,5 ms
     * - Timeout for fast mode is 30 seconds
     * - Start(timeout) with timeout = 0 will advertise forever
     *
     * For recommended advertising interval
     * https://developer.apple.com/library/content/qa/qa1931/_index.html
     */
    if (restartAdvertisingIfPossible("BLE startup")) {
        LOG_INFO("Advertising started (multi-connection mode enabled)");
    }
}
// Just ack that the caller is allowed to read
static void authorizeRead(uint16_t conn_hdl)
{
    ble_gatts_rw_authorize_reply_params_t reply = {.type = BLE_GATTS_AUTHORIZE_TYPE_READ};
    reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
    sd_ble_gatts_rw_authorize_reply(conn_hdl, &reply);
}
/**
 * client is starting read, pull the bytes from our API class
 */
void onFromRadioAuthorize(uint16_t conn_hdl, BLECharacteristic *chr, ble_gatts_evt_read_t *request)
{
    if (request->offset == 0) {
        // If the read is long, we will get multiple authorize invocations - we only populate data on the first
        size_t numBytes = bluetoothPhoneAPI->getFromRadio(fromRadioBytes);
        // Someone is going to read our value as soon as this callback returns.  So fill it with the next message in the queue
        // or make empty if the queue is empty
        fromRadio.write(fromRadioBytes, numBytes);
    } else {
        // LOG_INFO("Ignore successor read");
    }
    authorizeRead(conn_hdl);
}

void onToRadioWrite(uint16_t conn_hdl, BLECharacteristic *chr, uint8_t *data, uint16_t len)
{
    LOG_INFO("toRadioWriteCb data %p, len %u", data, len);
    if (memcmp(lastToRadio, data, len) != 0) {
        LOG_DEBUG("New ToRadio packet");
        memcpy(lastToRadio, data, len);
        bluetoothPhoneAPI->handleToRadio(data, len);
    } else {
        LOG_DEBUG("Drop dup ToRadio packet we just saw");
    }
}

void setupMeshService(void)
{
    bluetoothPhoneAPI = new BluetoothPhoneAPI();
    meshBleService.begin();
    // Note: You must call .begin() on the BLEService before calling .begin() on
    // any characteristic(s) within that service definition.. Calling .begin() on
    // a BLECharacteristic will cause it to be added to the last BLEService that
    // was 'begin()'ed!
    auto secMode =
        config.bluetooth.mode == meshtastic_Config_BluetoothConfig_PairingMode_NO_PIN ? SECMODE_OPEN : SECMODE_ENC_NO_MITM;
    fromNum.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);
    fromNum.setPermission(secMode, SECMODE_NO_ACCESS); // FIXME, secure this!!!
    fromNum.setFixedLen(
        0); // Variable len (either 0 or 4)  FIXME consider changing protocol so it is fixed 4 byte len, where 0 means empty
    fromNum.setMaxLen(4);
    fromNum.setCccdWriteCallback(onCccd); // Optionally capture CCCD updates
    // We don't yet need to hook the fromNum auth callback
    // fromNum.setReadAuthorizeCallback(fromNumAuthorizeCb);
    fromNum.write32(0); // Provide default fromNum of 0
    fromNum.begin();

    fromRadio.setProperties(CHR_PROPS_READ);
    fromRadio.setPermission(secMode, SECMODE_NO_ACCESS);
    fromRadio.setMaxLen(sizeof(fromRadioBytes));
    fromRadio.setReadAuthorizeCallback(
        onFromRadioAuthorize,
        false); // We don't call this callback via the adafruit queue, because we can safely run in the BLE context
    fromRadio.setBuffer(fromRadioBytes, sizeof(fromRadioBytes)); // we preallocate our fromradio buffer so we won't waste space
    // for two copies
    fromRadio.begin();

    toRadio.setProperties(CHR_PROPS_WRITE);
    toRadio.setPermission(secMode, secMode); // FIXME secure this!
    toRadio.setFixedLen(0);
    toRadio.setMaxLen(512);
    toRadio.setBuffer(toRadioBytes, sizeof(toRadioBytes));
    // We don't call this callback via the adafruit queue, because we can safely run in the BLE context
    toRadio.setWriteCallback(onToRadioWrite, false);
    toRadio.begin();

    logRadio.setProperties(CHR_PROPS_INDICATE | CHR_PROPS_NOTIFY | CHR_PROPS_READ);
    logRadio.setPermission(secMode, SECMODE_NO_ACCESS);
    logRadio.setMaxLen(512);
    logRadio.setCccdWriteCallback(onCccd);
    logRadio.write32(0);
    logRadio.begin();
}
static uint32_t configuredPasskey;
void NRF52Bluetooth::shutdown()
{
    // Shutdown bluetooth for minimum power draw
    LOG_INFO("Disable NRF52 bluetooth");
    Bluefruit.Security.setPairPasskeyCallback(NRF52Bluetooth::onUnwantedPairing); // Actively refuse (during factory reset)
    disconnect();
    Bluefruit.Advertising.stop();
}
void NRF52Bluetooth::startDisabled()
{
    // Setup Bluetooth
    nrf52Bluetooth->setup();
    // Shutdown bluetooth for minimum power draw
    Bluefruit.Advertising.stop();
    Bluefruit.setTxPower(-40); // Minimum power
    LOG_INFO("Disable NRF52 Bluetooth. (Workaround: tx power min, advertise stopped)");
}
bool NRF52Bluetooth::isConnected()
{
    return Bluefruit.connected(connectionHandle);
}
int NRF52Bluetooth::getRssi()
{
    return 0; // FIXME figure out where to source this
}

// Valid BLE TX power levels as per nRF52840 Product Specification are: "-20 to +8 dBm TX power, configurable in 4 dB steps".
// See https://docs.nordicsemi.com/bundle/ps_nrf52840/page/keyfeatures_html5.html
#define VALID_BLE_TX_POWER(x)                                                                                                    \
    ((x) == -20 || (x) == -16 || (x) == -12 || (x) == -8 || (x) == -4 || (x) == 0 || (x) == 4 || (x) == 8)

void NRF52Bluetooth::setup()
{
    // Initialise the Bluefruit module
    LOG_INFO("Init the Bluefruit nRF52 module");
    Bluefruit.autoConnLed(false);
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
    
    Bluefruit.begin(MAX_PRPH_CONNECTIONS, 0);
    // Clear existing data.
    Bluefruit.Advertising.stop();
    Bluefruit.Advertising.clearData();
    Bluefruit.ScanResponse.clearData();
#if defined(NRF52_BLE_TX_POWER) && VALID_BLE_TX_POWER(NRF52_BLE_TX_POWER)
    Bluefruit.setTxPower(NRF52_BLE_TX_POWER);
#endif
    if (config.bluetooth.mode != meshtastic_Config_BluetoothConfig_PairingMode_NO_PIN) {
        configuredPasskey = config.bluetooth.mode == meshtastic_Config_BluetoothConfig_PairingMode_FIXED_PIN
                                ? config.bluetooth.fixed_pin
                                : random(100000, 999999);
        auto pinString = std::to_string(configuredPasskey);
        LOG_INFO("Bluetooth pin set to '%i'", configuredPasskey);
        Bluefruit.Security.setPIN(pinString.c_str());
        Bluefruit.Security.setIOCaps(true, false, false);
        Bluefruit.Security.setPairPasskeyCallback(NRF52Bluetooth::onPairingPasskey);
        Bluefruit.Security.setPairCompleteCallback(NRF52Bluetooth::onPairingCompleted);
        Bluefruit.Security.setSecuredCallback(NRF52Bluetooth::onConnectionSecured);
        meshBleService.setPermission(SECMODE_ENC_WITH_MITM, SECMODE_ENC_WITH_MITM);
    } else {
        Bluefruit.Security.setIOCaps(false, false, false);
        meshBleService.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    }
    // Set the advertised device name (keep it short!)
    Bluefruit.setName(getDeviceName());
    // Set the connect/disconnect callback handlers
    Bluefruit.Periph.setConnectCallback(onConnect);
    Bluefruit.Periph.setDisconnectCallback(onDisconnect);

    // Do not change Slave Latency to value other than 0 !!!
    // There is probably a bug in SoftDevice + certain Apple iOS versions being
    // brain damaged causing connectivity problems.

    // On one side it seems SoftDevice is using SlaveLatency value even
    // if connection parameter negotation failed and phone sees it as connectivity errors.

    // On the other hand Apple can randomly refuse any parameter negotiation and shutdown connection
    // even if you meet Apple Developer Guidelines for BLE devices. Because f* you, that's why.

    // While this API call sets preferred connection parameters (PPCP) - many phones ignore it (yeah) and it seems SoftDevice
    // will try to renegotiate connection parameters based on those values after phone connection.
    // So those are relatively safe values so Apple braindead firmware won't get angry and at least we may try
    // to negotiate some longer connection interval to save battery.

    // See https://github.com/meshtastic/firmware/pull/8858 for measurements.  We are dealing with microamp savings anyway so not
    // worth dying on a hill here.

    Bluefruit.Periph.setConnSlaveLatency(0);
    // 1.25 ms units - so min, max is 15, 100 ms range.
    Bluefruit.Periph.setConnInterval(12, 80);

#ifndef BLE_DFU_SECURE
    bledfu.setPermission(SECMODE_ENC_WITH_MITM, SECMODE_ENC_WITH_MITM);
    bledfu.begin(); // Install the DFU helper
#else
    bledfusecure.setPermission(SECMODE_ENC_WITH_MITM, SECMODE_ENC_WITH_MITM); // add by WayenWeng
    bledfusecure.begin();                                                     // Install the DFU helper
#endif
    // Configure and Start the Device Information Service
    LOG_INFO("Init the Device Information Service");
    bledis.setModel(optstr(HW_VERSION));
    bledis.setFirmwareRev(optstr(APP_VERSION));
    bledis.begin();
    // Start the BLE Battery Service and set it to 100%
    LOG_INFO("Init the Battery Service");
    blebas.begin();
    blebas.write(0); // Unknown battery level for now
    // Setup the Heart Rate Monitor service using
    // BLEService and BLECharacteristic classes
    LOG_INFO("Init the Mesh bluetooth service");
    setupMeshService();
    // Setup the advertising packet(s)
    LOG_INFO("Set up the advertising payload(s)");
    startAdv();
    LOG_INFO("Advertise");
}
void NRF52Bluetooth::resumeAdvertising()
{
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 668); // in unit of 0.625 ms
    Bluefruit.Advertising.setFastTimeout(30);   // number of seconds in fast mode
    Bluefruit.Advertising.start(0);
}
/// Given a level between 0-100, update the BLE attribute
void updateBatteryLevel(uint8_t level)
{
    blebas.write(level);
}
void NRF52Bluetooth::clearBonds()
{
    LOG_INFO("Clear bluetooth bonds!");
    bond_print_list(BLE_GAP_ROLE_PERIPH);
    bond_print_list(BLE_GAP_ROLE_CENTRAL);
    Bluefruit.Periph.clearBonds();
    Bluefruit.Central.clearBonds();
}
void NRF52Bluetooth::onConnectionSecured(uint16_t conn_handle)
{
    LOG_INFO("BLE connection secured");
}
bool NRF52Bluetooth::onPairingPasskey(uint16_t conn_handle, uint8_t const passkey[6], bool match_request)
{
    char passkey1[4] = {passkey[0], passkey[1], passkey[2], '\0'};
    char passkey2[4] = {passkey[3], passkey[4], passkey[5], '\0'};
    LOG_INFO("BLE pair process started with passkey %s %s", passkey1, passkey2);
    powerFSM.trigger(EVENT_BLUETOOTH_PAIR);

    // Get passkey as string
    // Note: possible leading zeros
    std::string textkey;
    for (uint8_t i = 0; i < 6; i++)
        textkey += (char)passkey[i];

    // Notify UI (or other components) of pairing event and passkey
    meshtastic::BluetoothStatus newStatus(textkey);
    bluetoothStatus->updateStatus(&newStatus);

#if HAS_SCREEN &&                                                                                                                \
    !defined(MESHTASTIC_EXCLUDE_SCREEN) // Todo: migrate this display code back into Screen class, and observe bluetoothStatus
    if (screen) {
        screen->startAlert([](OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) -> void {
            char btPIN[16] = "888888";
            snprintf(btPIN, sizeof(btPIN), "%06u", configuredPasskey);
            int x_offset = display->width() / 2;
            int y_offset = display->height() <= 80 ? 0 : 12;
            display->setTextAlignment(TEXT_ALIGN_CENTER);
            display->setFont(FONT_MEDIUM);
            display->drawString(x_offset + x, y_offset + y, "Bluetooth");

            display->setFont(FONT_SMALL);
            y_offset = display->height() == 64 ? y_offset + FONT_HEIGHT_MEDIUM - 4 : y_offset + FONT_HEIGHT_MEDIUM + 5;
            display->drawString(x_offset + x, y_offset + y, "Enter this code");

            display->setFont(FONT_LARGE);
            String displayPin(btPIN);
            String pin = displayPin.substring(0, 3) + " " + displayPin.substring(3, 6);
            y_offset = display->height() == 64 ? y_offset + FONT_HEIGHT_SMALL - 5 : y_offset + FONT_HEIGHT_SMALL + 5;
            display->drawString(x_offset + x, y_offset + y, pin);

            display->setFont(FONT_SMALL);
            String deviceName = "Name: ";
            deviceName.concat(getDeviceName());
            y_offset = display->height() == 64 ? y_offset + FONT_HEIGHT_LARGE - 6 : y_offset + FONT_HEIGHT_LARGE + 5;
            display->drawString(x_offset + x, y_offset + y, deviceName);
        });
    }
#endif
    passkeyShowing = true;

    if (match_request) {
        uint32_t start_time = millis();
        while (millis() < start_time + 30000) {
            if (!Bluefruit.connected(conn_handle))
                break;
        }
    }
    LOG_INFO("BLE passkey pair: match_request=%i", match_request);
    return true;
}

// Actively refuse new BLE pairings
// After clearing bonds (at factory reset), clients seem initially able to attempt to re-pair, even with advertising disabled.
// On NRF52Bluetooth::shutdown, we change the pairing callback to this method, to aggressively refuse any connection attempts.
bool NRF52Bluetooth::onUnwantedPairing(uint16_t conn_handle, uint8_t const passkey[6], bool match_request)
{
    NRF52Bluetooth::disconnect();
    return false;
}

// Disconnect any BLE connections
void NRF52Bluetooth::disconnect()
{
    uint8_t connection_num = Bluefruit.connected();
    if (connection_num) {
        // Close all connections. We're only expecting one.
        for (uint8_t i = 0; i < connection_num; i++)
            Bluefruit.disconnect(i);

        // Wait for disconnection
        while (Bluefruit.connected())
            yield();

        LOG_INFO("Ended BLE connection");
    }
}

void NRF52Bluetooth::onPairingCompleted(uint16_t conn_handle, uint8_t auth_status)
{
    if (auth_status == BLE_GAP_SEC_STATUS_SUCCESS) {
        LOG_INFO("BLE pair success");
        meshtastic::BluetoothStatus newConnectedStatus(meshtastic::BluetoothStatus::ConnectionState::CONNECTED);
        bluetoothStatus->updateStatus(&newConnectedStatus);
    } else {
        LOG_INFO("BLE pair failed");
        // Notify UI (or any other interested firmware components)
        meshtastic::BluetoothStatus newDisconnectedStatus(meshtastic::BluetoothStatus::ConnectionState::DISCONNECTED);
        bluetoothStatus->updateStatus(&newDisconnectedStatus);
    }

    // Todo: migrate this display code back into Screen class, and observe bluetoothStatus
    passkeyShowing = false;
    if (screen) {
        screen->endAlert();
    }
}

void NRF52Bluetooth::sendLog(const uint8_t *logMessage, size_t length)
{
    if (!isConnected() || length > 512)
        return;
    if (logRadio.indicateEnabled())
        logRadio.indicate(logMessage, (uint16_t)length);
    else
        logRadio.notify(logMessage, (uint16_t)length);
}

/**
 * Static wrapper callback for BLE scan results
 * This function is called by the Bluefruit library when a device is found
 */
void NRF52Bluetooth::scanCallbackWrapper(void* void_report)
{
    ble_gap_evt_adv_report_t* report = static_cast<ble_gap_evt_adv_report_t*>(void_report);
    
    if (!nrf52Bluetooth) {
        LOG_DEBUG("scanCallbackWrapper: nrf52Bluetooth is NULL");
        return;
    }
    
    char addr_str[18];
    uint8_t* addr = report->peer_addr.addr;
    snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    
    LOG_DEBUG("scanCallbackWrapper called for %s, user callback is %s", addr_str, nrf52Bluetooth->scanCallback ? "SET" : "NOT SET");
    
    if (!nrf52Bluetooth->scanCallback) {
        // No callback set, just log the device
        LOG_INFO("BLE Device found: %s, RSSI: %d dBm", addr_str, report->rssi);
    } else {
        // Call user callback
        LOG_DEBUG("Calling user callback for %s", addr_str);
        nrf52Bluetooth->scanCallback(void_report);
    }

    // Bluefruit scanner stops after delivering a report to the callback,
    // so resume scanning explicitly to continue receiving more devices.
    if (Bluefruit.Scanner.isRunning()) {
        Bluefruit.Scanner.resume();
    }
}

void NRF52Bluetooth::setScanCallback(ScanCallback callback)
{
    scanCallback = callback;
    LOG_INFO("Scan callback set: %s", callback ? "yes" : "no");
}

void NRF52Bluetooth::startScanning(uint16_t duration)
{
    LOG_INFO("Starting BLE scan%s, callback is %s", duration > 0 ? "" : " (continuous)", scanCallback ? "SET" : "NOT SET");
    
    // Create a lambda wrapper that matches the Bluefruit API signature
    auto ble_callback = [](ble_gap_evt_adv_report_t* report) {
        NRF52Bluetooth::scanCallbackWrapper(static_cast<void*>(report));
    };
    
    // Set scan callback
    Bluefruit.Scanner.setRxCallback(ble_callback);
    
    // Set scan parameters (using Bluefruit API)
    Bluefruit.Scanner.setInterval(160, 160); // Interval and window in 0.625ms units (100ms interval, 100ms window for maximum coverage)
    Bluefruit.Scanner.useActiveScan(true);   // Active scan so scan response payloads are requested and delivered too
    
    // Start scanning
    if (duration > 0) {
        // Scan for specified duration (in seconds)
        Bluefruit.Scanner.start(duration);
    } else {
        // Continuous scan
        Bluefruit.Scanner.start(0);
    }
}

void NRF52Bluetooth::stopScanning()
{
    LOG_INFO("Stopping BLE scan");
    Bluefruit.Scanner.stop();
}

bool NRF52Bluetooth::isScanning()
{
    return Bluefruit.Scanner.isRunning();
}

// ============================================================================
// Manufacturer Data Broadcaster Implementation
// ============================================================================

/**
 * Manufacturer Data Broadcaster class
 * Handles broadcasting 80 bytes of manufacturer data in 4 segments
 * When enabled, cycles through segments and rewrites the active BLE scan response payload
 * When disabled, restores the default advertising/scan response payloads
 */
class ManufacturerDataBroadcaster : public concurrency::OSThread
{
  private:
    uint8_t manufacturerData[MANUFACTURER_DATA_MAX_SIZE];
    uint8_t dataLength;
    uint8_t sequenceNumber;
    uint8_t currentSegment;
    uint32_t lastUpdateTime;
    bool enabled;
    NRF52Bluetooth* bluetooth;

  public:
    ManufacturerDataBroadcaster(NRF52Bluetooth* bt) 
        : OSThread("ManufacturerDataBroadcaster"), bluetooth(bt)
    {
        // Initialize with test data (1-80)
        for (uint8_t i = 0; i < MANUFACTURER_DATA_MAX_SIZE; i++) {
            manufacturerData[i] = i + 1;
        }
        dataLength = MANUFACTURER_DATA_MAX_SIZE;
        sequenceNumber = 0;
        currentSegment = 0;
        lastUpdateTime = 0;
        enabled = false;
    }

    void setData(const uint8_t* data, size_t length)
    {
        if (length > MANUFACTURER_DATA_MAX_SIZE) {
            LOG_WARN("Manufacturer data too large, truncating to %d bytes", MANUFACTURER_DATA_MAX_SIZE);
            length = MANUFACTURER_DATA_MAX_SIZE;
        }
        
        memcpy(manufacturerData, data, length);
        dataLength = length;
        sequenceNumber++;
        LOG_INFO("Manufacturer data updated to %d bytes, sequence %d", dataLength, sequenceNumber);
        
        if (enabled) {
            // Reset to first segment if we're currently broadcasting
            currentSegment = 0;
            updateAdvertisingPayload();
        }
    }

    void update() // Public method to update broadcast manually
    {
        if (enabled) {
            uint32_t currentTime = millis();
            
            // Check if it's time to update
            if (currentTime - lastUpdateTime >= MANUFACTURER_BROADCAST_INTERVAL_MS) {
                lastUpdateTime = currentTime;
                LOG_INFO("Manufacturer broadcast tick: segment=%u/%u", currentSegment + 1, MANUFACTURER_DATA_SEGMENTS);
                updateAdvertisingPayload();
            }
        }
    }

    void start()
    {
        if (!enabled) {
            enabled = true;
            currentSegment = 0;
            lastUpdateTime = millis();
            LOG_INFO("Manufacturer data broadcasting started");
            
            // Update immediately
            updateAdvertisingPayload();
        }
    }

    void stop()
    {
        if (enabled) {
            enabled = false;
            lastUpdateTime = 0;
            LOG_INFO("Manufacturer data broadcasting stopped");
            
            // Restore original advertising payload
            restoreDefaultAdvertisingPayload();
        }
    }

    bool isEnabled() const { return enabled; }

  protected:
    virtual int32_t runOnce() override
    {
        if (!enabled) {
            return INT32_MAX; // Don't run if not enabled
        }
        return MANUFACTURER_BROADCAST_INTERVAL_MS; // Run every 500ms
    }

  private:
    void updateAdvertisingPayload()
    {
        std::vector<uint8_t> packet = prepareManufacturerDataPacket();
        uint8_t connectionCount = Bluefruit.connected();
        bool advertisingRunning = Bluefruit.Advertising.isRunning();

        if (!setManufacturerChunkScanResponse(packet.data(), static_cast<uint8_t>(packet.size()))) {
            LOG_ERROR("Failed to update BLE scan response before manufacturer update");
            return;
        }

        if (!advertisingRunning) {
            if (!restartAdvertisingIfPossible("manufacturer data update")) {
                LOG_WARN("Advertising payload updated but could not restart advertising");
            }
        } else if (connectionCount > 0) {
            LOG_INFO("Manufacturer scan response updated while connected (%u/%u), keeping current advertising session",
                     connectionCount, MAX_PRPH_CONNECTIONS);
        } else {
            if (!restartAdvertisingIfPossible("manufacturer data update")) {
                LOG_WARN("Advertising payload updated but could not restart advertising");
            }
        }

        LOG_INFO("Advertising manufacturer chunk %d/%d, payload=%d bytes",
             currentSegment + 1, MANUFACTURER_DATA_SEGMENTS, packet.size());

        currentSegment++;
        if (currentSegment >= MANUFACTURER_DATA_SEGMENTS) {
            currentSegment = 0;
        }
    }

    void restoreDefaultAdvertisingPayload()
    {
        setBaseScanResponse();

        if (!restartAdvertisingIfPossible("manufacturer data stop")) {
            LOG_WARN("Default BLE payload restored but advertising restart failed");
        }

        LOG_DEBUG("Restored default BLE payload");
    }

    std::vector<uint8_t> prepareManufacturerDataPacket()
    {
        uint16_t offset = currentSegment * MANUFACTURER_DATA_SEGMENT_SIZE;
        uint16_t remainingBytes = dataLength - offset;
        uint8_t dataBytes = (remainingBytes > MANUFACTURER_DATA_SEGMENT_SIZE) 
                           ? MANUFACTURER_DATA_SEGMENT_SIZE 
                           : remainingBytes;
        
        std::vector<uint8_t> packet;
        packet.reserve(4 + dataBytes);
        
        packet.push_back(MANUFACTURER_ID & 0xFF);
        packet.push_back((MANUFACTURER_ID >> 8) & 0xFF);

        // High nibble = current segment index, low nibble = total segment count.
        packet.push_back(static_cast<uint8_t>(((currentSegment & 0x0F) << 4) | (MANUFACTURER_DATA_SEGMENTS & 0x0F)));
        packet.push_back(dataBytes);

        for (uint8_t i = 0; i < dataBytes; i++) {
            packet.push_back(manufacturerData[offset + i]);
        }
        
        return packet;
    }
};

// Static instance
static ManufacturerDataBroadcaster* sManufacturerDataBroadcaster = nullptr;

// ============================================================================
// NRF52Bluetooth Manufacturer Data Methods
// ============================================================================

void NRF52Bluetooth::setManufacturerData(const uint8_t* data, size_t length)
{
    if (!mfgDataBroadcaster) {
        mfgDataBroadcaster = new ManufacturerDataBroadcaster(this);
    }

    mfgDataBroadcaster->setData(data, length);
}

void NRF52Bluetooth::startManufacturerDataBroadcasting()
{
    if (!mfgDataBroadcaster) {
        mfgDataBroadcaster = new ManufacturerDataBroadcaster(this);
    }
    
    mfgDataBroadcaster->start();
}

void NRF52Bluetooth::stopManufacturerDataBroadcasting()
{
    if (mfgDataBroadcaster) {
        mfgDataBroadcaster->stop();
    }
}

bool NRF52Bluetooth::isManufacturerDataBroadcasting()
{
    return (mfgDataBroadcaster && mfgDataBroadcaster->isEnabled());
}

void NRF52Bluetooth::updateManufacturerDataBroadcasting()
{
    if (mfgDataBroadcaster) {
        mfgDataBroadcaster->update();
    }
}
