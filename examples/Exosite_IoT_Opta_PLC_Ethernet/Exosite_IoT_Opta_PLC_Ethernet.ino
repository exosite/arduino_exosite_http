#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <Arduino_ConnectionHandler.h>
#include <Arduino_JSON.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <NTPClient.h>

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#include "opta_info.h"

String macAddress;
// char macAddress[18] = {0}; // 17 + 1 (for null terminator)

OptaBoardInfo* info;
OptaBoardInfo* boardInfo();

// Array to hold the pin numbers for Opta's Input Pins
const int OPTA_DIGITAL_INS[] = {A0, A1, A2, A3};
const int NUM_DIGITAL_INS = 4;

const int OPTA_ANALOG_INS[] = {A4};
const int NUM_ANALOG_INS = 1;

// Array to hold the pin numbers for Opta's User LEDs
const int OPTA_LEDS[] = {LED_D0, LED_D1, LED_D2, LED_D3};
const int NUM_LEDS = 4;

// Array to hold the pin numbers for Opta's Relay Outputs
const int OPTA_RELAYS[] = {D0, D1, D2, D3};
const int NUM_RELAYS = 4;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#include "QSPIFBlockDevice.h"
#include "MBRBlockDevice.h"

using namespace mbed;

// Constants
#define BLOCK_DEVICE_SIZE (1024 * 8)     // 8 KB partition
#define PARTITION_TYPE 0x0B              // FAT32 (just a label)
#define TOKEN_MAX_LEN  64                // Safety buffer (40 chars + margin)
#define TOKEN_OFFSET    0                // Offset in flash to store the token

// Pin configuration for QSPI on Opta
QSPIFBlockDevice qspi(QSPI_SO0, QSPI_SO1, QSPI_SO2, QSPI_SO3, QSPI_SCK, QSPI_CS, QSPIF_POLARITY_MODE_1, 40000000);
MBRBlockDevice flashPartition(&qspi, 1);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

EthernetConnectionHandler conMan;
EthernetClient tcpClient;
EthernetUDP NTPUdp;

NTPClient timeClient(NTPUdp);
BearSSLClient sslClient(tcpClient);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#include <ExositeHTTP.h>
#include <ExositeTrustAnchors.h>
#include "cloud_config.h"

const String configResource = "config_io";
const String dataResource = "data_in";
const String controlResource = "data_out";

String deviceToken;

String responseString;

JSONVar writeJson;
String writeString;

ApiResponse res;

ExositeHTTP exosite(&sslClient, CONNECTOR_DOMAIN);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// Number of milliseconds to delay between loop iterations
const int LOOP_DELAY = 10000;

/*================================================================================================
 * setup
 *
 * Arduino setup function
 *==============================================================================================*/
void setup() {
  Serial.begin(115200);

  // Wait for Serial Monitor
  unsigned long startMillis = millis();
  while (!Serial && (millis() - startMillis < 2500)) {
    delay(250);
  }

  Serial.println(F("setup | Start"));

  // If Hardware Security Module (HSM) not present, halt execution
  if (!ECCX08.begin()) {
    Serial.println(F("setup | No ECCX08 present - troubleshoot and reboot!"));
    while (1) {};
  }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  //                                 Local Init
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  while (!initFlash()) {
    Serial.println(F("setup | Flash init failed - troubleshoot and reboot!"));
    delay(60000);
  }

  // Initialize pins, network callbacks, and global macAddress
  //   and retrieve and print Opta device info
  initOpta();

  // Initialize TCP/IP client SSL configuration (note: updates trusted CA Certs)
  initSsl();

  // Manage Opta connection to local network
  checkNetwork();

  Serial.println();

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  //                             Cloud Provisioning
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if (!loadToken(deviceToken)) {
    Serial.println(F("setup | No auth token found - provisioning..."));

    res = exosite.provision(macAddress, deviceToken);

    if (res.success) {
      Serial.print(F("setup | Provisioning successful: "));
      Serial.println(deviceToken);

      // Store the token in the persistent QSPI Flash
      storeToken(deviceToken);

      // Update the existing client with the newly retrieved auth
      exosite.setToken(deviceToken);
    }
    else {
      Serial.print(F("setup | Provisioning failed ("));
      Serial.print(res.statusCode);
      Serial.println(F(")"));
    }
  }
  else {
    Serial.print(F("setup | Stored token found: "));
    Serial.println(deviceToken);

    // Update the existing client with the defined auth
    exosite.setToken(deviceToken);
  }

  // If client auth has not been attained, halt execution
  while (deviceToken.isEmpty()) {
    Serial.println(F("setup | No auth token found - troubleshoot and reboot!"));
    delay(60000);
  }

  Serial.println(F("setup | Reporting Channel Configuration..."));

  // Note: Channel Config is parsed and [re]serialized for whitespace removal
  res = exosite.write(configResource, JSON.stringify(JSON.parse(CHANNEL_CONFIG)));

  if (!res.success) {
    Serial.print(F("setup | Failed to report Channel Config ("));
    Serial.print(res.statusCode);
    Serial.println(F(")"));
  }

  delay(1000);
}

/*================================================================================================
 * loop
 *
 * Arduino loop function
 *==============================================================================================*/
void loop() {
  Serial.println();
  Serial.println(F("loop | Start"));

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  //                               Data Publishing
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  if (buildWriteJson(writeJson)) {
    writeString = JSON.stringify(writeJson);

    Serial.print(F("loop | Writing data to the cloud: "));
    Serial.println(writeString);

    res = exosite.write(dataResource, writeString);

    if (!res.success) {
      Serial.print(F("loop | Failed to write data to the cloud ("));
      Serial.print(res.statusCode);
      Serial.println(F(")"));
    }
  }
  else {
    Serial.println(F("loop | Failed to build JSON payload"));
  }

  Serial.print(F("loop | Delaying: ~"));
  Serial.print(LOOP_DELAY);
  Serial.println(F("ms"));

  writeJson = JSONVar();
  delay(LOOP_DELAY);
  checkNetwork();

  Serial.println(F("loop | End"));
}

/*================================================================================================
 *                                            EXOSITE
 *==============================================================================================*/

// Returns true if successful, otherwise false
bool buildWriteJson(JSONVar& output) {
  int* digitalInputStates = readDigitalInputs();
  float* analogInputStates = readAnalogInputs();

  output["001"] = digitalInputStates[0];
  output["002"] = digitalInputStates[1];
  output["003"] = digitalInputStates[2];
  output["004"] = digitalInputStates[3];
  output["005"] = analogInputStates[0];

  // Confirm that all keys are present
  return output.hasOwnProperty("001") &&
         output.hasOwnProperty("002") &&
         output.hasOwnProperty("003") &&
         output.hasOwnProperty("004") &&
         output.hasOwnProperty("005");
}

void handleResponse(const String& resource, String& response) {
  Serial.println();
  if (resource == "config_io") {
    Serial.print(F("handleResponse | Received Channel Configuration (config_io): "));
    Serial.println(response);
    handleConfigIo(response);
  }
  else if (resource == "data_out") {
    Serial.print(F("handleResponse | Received Control Command (data_out): "));
    Serial.println(response);
    handleDataOut(response);
  }
  else {
    Serial.print(F("handleResponse | Received unexpected: "));
    Serial.print(resource);
    Serial.print(F("="));
    Serial.println(response);
  }
}

void handleConfigIo(String& value) {
  // Report the value back to the cloud to acknowledge receipt
  res = exosite.write(configResource, value);

  if (!res.success) {
    Serial.print(F("handleConfigIo | Failed to acknowledge config_io resource value ("));
    Serial.print(res.statusCode);
    Serial.println(F(")"));
  }
  // - - - - - - - - - - - - - - - - - - - - - -
  // <custom handling>
  // - - - - - - - - - - - - - - - - - - - - - -
}

void handleDataOut(String& value) {
  // Report the value back to the cloud to acknowledge receipt
  res = exosite.write(controlResource, value);

  if (!res.success) {
    Serial.print(F("handleDataOut | Failed to acknowledge data_out resource value ("));
    Serial.print(res.statusCode);
    Serial.println(F(")"));
  }
  // - - - - - - - - - - - - - - - - - - - - - -
  // <custom handling>
  // - - - - - - - - - - - - - - - - - - - - - -
}

/*================================================================================================
 *                                             OPTA
 *==============================================================================================*/

int* readDigitalInputs() {
  static int digitalInputs[NUM_DIGITAL_INS];
  for (int i = 0; i < NUM_DIGITAL_INS; i++) {
    digitalInputs[i] = digitalRead(OPTA_DIGITAL_INS[i]);
  }

  return digitalInputs;
}

float* readAnalogInputs() {
  // 65535 is the max value with 16 bits resolution set by analogReadResolution(16)
  // 4095 is the max value with 12 bits resolution set by analogReadResolution(12)
  analogReadResolution(12);

  static float analogInputs[NUM_ANALOG_INS];
  for (int i = 0; i < NUM_ANALOG_INS; i++) {
    float rawVoltage = analogRead(OPTA_ANALOG_INS[i]);
    analogInputs[i] = rawVoltage * (3.0 / 4095.0) / 0.3; // Scale the voltage reading
  }

  return analogInputs;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void initOpta() {
  // Begin NTP sync process
  timeClient.begin();

  // Initialize Opta input & output pins
  initPins();

  // Initialize Opta network event callbacks
  initNetworkCallbacks();

  // Retrieve Opta board info
  info = boardInfo();

  // Populate the macAddress String
  getEthernetMacAddress(info, macAddress);

  // Print Opta Device information
  Serial.println();
  Serial.println(F("initOpta | Opta Device Information:"));
  printOptaSecureInfo(info);
}

void initPins() {
  for (int i = 0; i < NUM_DIGITAL_INS; i++) {
    pinMode(OPTA_DIGITAL_INS[i], INPUT);
  }

  for (int i = 0; i < NUM_ANALOG_INS; i++) {
    pinMode(OPTA_ANALOG_INS[i], INPUT);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void printOptaSecureInfo(OptaBoardInfo* info) {
  if (info->magic == 0xB5) {
    Serial.println("          - Secure information version: " + String(info->version));
    Serial.println("          - Ethernet functionality: " + String(info->_board_functionalities.ethernet == 1 ? "Yes" : "No"));
    Serial.println("          - Wi-Fi module functionality: " + String(info->_board_functionalities.wifi == 1 ? "Yes" : "No"));
    Serial.println("          - RS-485 functionality: " + String(info->_board_functionalities.rs485 == 1 ? "Yes" : "No"));
    Serial.println("          - QSPI memory size: " + String(info->external_flash_size) + " MB");
    Serial.println("          - Secure board revision: " + String(info->revision >> 8) + "." + String(info->revision & 0xFF));
    Serial.println("          - Secure VID: 0x" + String(info->vid, HEX));
    Serial.println("          - Secure PID: 0x" + String(info->pid, HEX));
    Serial.print(F("          - Ethernet MAC address: "));
    Serial.println(macAddress);
    if (info->_board_functionalities.wifi == 1) {
      Serial.print(F("          - Wi-Fi MAC address: "));
      Serial.println(macAddress);
    }
  }
  else {
    Serial.println(F("          - No secure information available!"));
  }
}

void getEthernetMacAddress(OptaBoardInfo* info, String& macAddress) {
  char buffer[18];
  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
          info->mac_address[0], info->mac_address[1], info->mac_address[2],
          info->mac_address[3], info->mac_address[4], info->mac_address[5]);
  macAddress = String(buffer);
}

void getWifiMacAddress(OptaBoardInfo* info, String& macAddress) {
  char buffer[18];
  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
          info->mac_address_2[0], info->mac_address_2[1], info->mac_address_2[2],
          info->mac_address_2[3], info->mac_address_2[4], info->mac_address_2[5]);
  macAddress = String(buffer);
}

/*================================================================================================
 *                                             NTP
 *==============================================================================================*/

void setNtpTime() {
  timeClient.forceUpdate();
  const auto epoch = timeClient.getEpochTime();
  set_time(epoch);
}

unsigned long getTime() {
  const auto now = time(NULL);
  return now;
}

/*================================================================================================
 *                                             SSL
 *==============================================================================================*/

void initSsl() {
  // Configure TLS to use HSM and the key/cert pair
  ArduinoBearSSL.onGetTime(getTime);

  // Override built-in Trust Anchors (if any) with those provided by the ExositeHTTP Library
  sslClient.setTrustAnchors(EXOSITE_TAs, EXOSITE_TAs_NUM);
}

/*================================================================================================
 *                                           NETWORK
 *==============================================================================================*/

void initNetworkCallbacks() {
  // Set the callbacks for connectivity management
  conMan.addCallback(NetworkConnectionEvent::CONNECTED, onNetworkConnect);
  conMan.addCallback(NetworkConnectionEvent::DISCONNECTED, onNetworkDisconnect);
  conMan.addCallback(NetworkConnectionEvent::ERROR, onNetworkError);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void checkNetwork() {
  // Serial.println(F("network | Checking local network connection..."));
  auto conStatus = conMan.check();
  while (conStatus != NetworkConnectionState::CONNECTED) {
    Serial.println();
    Serial.println(F("network | Not connected to the network... "));
    Serial.println(F("network |   Delaying (2s), then rechecking"));
    delay(2000);
    conStatus = conMan.check();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void onNetworkConnect() {
  Serial.println();
  Serial.println(F("onNetworkConnect | >>>> CONNECTED to network"));
  printEthernetStatus();

  setNtpTime();
}

void onNetworkDisconnect() {
  Serial.println();
  Serial.println(F("network | >>>> DISCONNECTED from network"));
}

void onNetworkError() {
  Serial.println();
  Serial.println(F("network | >>>> ERROR"));
}

void printEthernetStatus() {
  // Print the board's IP address
  Serial.println();
  Serial.print(F("ethernet | Local GW: "));
  Serial.println(Ethernet.gatewayIP());
  Serial.print(F("ethernet | Local IP: "));
  Serial.println(Ethernet.localIP());
}

/*================================================================================================
 *                                        FLASH STORAGE
 *==============================================================================================*/

bool initFlash() {
  // Try to initialize the existing partition
  if (flashPartition.init() != 0 || flashPartition.size() != BLOCK_DEVICE_SIZE) {
    flashPartition.deinit(); // Deinitialize the partition to safely reconfigure

    // Allocate a FAT 32 partition
    if (MBRBlockDevice::partition(&qspi, 1, PARTITION_TYPE, 0, BLOCK_DEVICE_SIZE) != 0) {
      Serial.println(F("initFlash | Failed to allocate a partition!"));
      return false;
    }

    return flashPartition.init() == 0;
  }

  return true;
}

bool loadToken(String& outToken) {
  char buffer[TOKEN_MAX_LEN] = {0};

  if (!loadToken(buffer, TOKEN_MAX_LEN)) {
    outToken = "";  // Clear the output on failure
    return false;
  }

  outToken = String(buffer);
  return true;
}

bool loadToken(char* buffer, size_t maxLen) {
  if (flashPartition.init() != 0) return false;

  flashPartition.read(buffer, TOKEN_OFFSET, maxLen);
  flashPartition.deinit();

  return buffer[0] != '\0' && isPrintable(buffer[0]);
}

void storeToken(const String& token) {
  storeToken(token.c_str());
}

void storeToken(const char* token) {
  if (flashPartition.init() != 0) return;

  flashPartition.erase(TOKEN_OFFSET, flashPartition.get_erase_size());
  flashPartition.program(token, TOKEN_OFFSET, strlen(token) + 1);

  flashPartition.deinit();
}
