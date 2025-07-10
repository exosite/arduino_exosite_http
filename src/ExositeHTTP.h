//************************************************************************************************
// BSD 3-Clause License
//
// Copyright (c) 2025, Exosite
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//************************************************************************************************

#pragma once

#define ACTIVATOR_VERSION "1.0.0"

#include <Arduino.h>
#include <Client.h>

//================================================================================================
//                                      Optional Overrides
//================================================================================================

// Debug logging control (uncomment to enable)
// #define EXO_DEBUG_LOGGING

// String literals are stored in flash (PROGMEM) rather than RAM (uncomment to disable)
// #define NO_FLASH_NET_STRINGS

// Size of internal response parsing buffer (uncomment to override)
// #define EXO_DATA_BUFFER_SIZE 2048

//================================================================================================

#ifdef EXO_DEBUG_LOGGING
  #define LOG_DEBUG(...) do { \
    if (Serial) { \
      Serial.print("[Debug] "); \
      Serial.print("(line:"); \
      Serial.print(__LINE__); \
      Serial.print(") "); \
      Serial.println(__VA_ARGS__); \
    } \
  } while (0)
#else
  #define LOG_DEBUG(...) do {} while (0)
#endif

#ifndef EXO_DATA_BUFFER_SIZE
  #define EXO_DATA_BUFFER_SIZE 1024
#endif

#if EXO_DATA_BUFFER_SIZE <= 256
  #warning "EXO_DATA_BUFFER_SIZE is likely too small. Minimum: >= 256. Recommended: >= 1024."
#elif EXO_DATA_BUFFER_SIZE >= 2048
  #warning "EXO_DATA_BUFFER_SIZE is fairly large. Ensure your target hardware has sufficient RAM."
#endif

// Macro for storing string literals in flash (PROGMEM) rather than RAM
#if !(defined(ESP8266) || defined(NO_FLASH_NET_STRINGS)) // Disabled for ESP, or manually
  #define G(x) F(x)
#else
  #define G(x) x
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

class ExositeHTTP {
  public:
    /**
     * @brief Construct an ExositeHTTP client instance
     *
     * @param client     Pointer to a secured network client (e.g. BearSSLClient)
     * @param connector  Domain of the target IoT Connector
     */
    ExositeHTTP(Client* client, const char* connector);

    /**
     * @brief Construct an ExositeHTTP client instance
     *
     * @param client     Pointer to a secured network client (e.g. BearSSLClient)
     * @param connector  Domain of the target IoT Connector
     */
    ExositeHTTP(Client* client, String& connector);

    /**
     * @brief Construct an ExositeHTTP client instance
     *
     * @param client       Pointer to a secured network client (e.g. BearSSLClient)
     * @param connector    Domain of the target IoT Connector
     * @param clientToken  Client token to enable authenticated requests
     */
    ExositeHTTP(Client* client, const char* connector, const char* clientToken);

    /**
     * @brief Construct an ExositeHTTP client instance
     *
     * @param client       Pointer to a secured network client (e.g. BearSSLClient)
     * @param connector    Domain of the target IoT Connector
     * @param clientToken  Client token to enable authenticated requests
     */
    ExositeHTTP(Client* client, String& connector, String& clientToken);

    /**
     * @brief Set/update the Client authentication token
     *
     * @param token  Client authentication token (40 characters)
     */
    void setToken(const char* token);

    /**
     * @brief Set/update the Client authentication token
     *
     * @param token  Client authentication token (40 characters)
     */
    void setToken(const String& token);

    /**
     * @brief Set/update the max timeout (ms) of all cloud request repsonses
     *
     * Note:
     *
     * - Default value of `rxTimeout` is `10000` (ms)
     *
     * - To ensure complete processing, this is automatically extended for `longPoll()` requests
     *
     * @param rxTimeoutMs  Request response timeout (ms)
     */
    void setTimeout(const unsigned long rxTimeoutMs);

    /**
     * @brief Provision the device identity and receive a server-generated authentication token
     *
     * @param identity        Unique identity (e.g. MAC address) of the device
     * @param responseBuffer  Buffer in which to store the returned token (>= 41 bytes)
     * @param bufferSize      Size of the provided `responseBuffer`
     *
     * @return `true` if successful and a token was received (HTTP 200), `false` otherwise
     */
    bool provision(const char* identity, char* responseBuffer, size_t bufferSize);

    /**
     * @brief Provision the device identity and receive a server-generated authentication token
     *
     * @param identity        Unique identity (e.g. MAC address) of the device
     * @param responseString  String in which to store the returned token
     *
     * @return `true` if successful and a token was received (HTTP 200), `false` otherwise
     */
    bool provision(const String& identity, String& responseString);

    /**
     * @brief Write the provided value to the specified resource
     *
     * @param resource    Target resource (e.g. `data_in`)
     * @param writeChars  Value to be written (e.g. `{"temp":23.5,"hum":40.1}`)
     *
     * @return `true` if successful (HTTP 204), `false` otherwise
     */
    bool write(const char* resource, const char* writeChars);

    /**
     * @brief Write the provided value to the specified resource
     *
     * @param resource     Target resource (e.g. `data_in`)
     * @param writeString  Value to be written (e.g. `{"temp":23.5,"hum":40.1}`)
     *
     * @return `true` if successful (HTTP 204), `false` otherwise
     */
    bool write(const String& resource, const String& writeString);

    /**
     * @brief Read the latest value of the specified resource
     *
     * Note:
     *
     * - Response includes only `{value}` from the raw `{resource}={value}` request response
     *
     * @param resource        Resource to read (e.g. `data_out`)
     * @param responseBuffer  Buffer in which to store the decoded response value (if any)
     * @param bufferSize      Size of the provided `responseBuffer`
     *
     * @return `true` if successful (HTTP 200 or 204), `false` otherwise
     */
    bool read(const char* resource, char* responseBuffer, size_t bufferSize);

    /**
     * @brief Read the latest value of the specified resource
     *
     * Note:
     *
     * - Response includes only `{value}` from the raw `{resource}={value}` request response
     *
     * @param resource        Resource to read (e.g. `data_out`)
     * @param responseString  String in which to store the decoded response value (if any)
     *
     * @return `true` if successful (HTTP 200 or 204), `false` otherwise
     */
    bool read(const String& resource, String& responseString);

    /**
     * @brief Check/wait for a new value on the specified resource
     *
     * Note:
     *
     * - Response includes only `{value}` from the raw `{resource}={value}` request response
     *
     * @param resource        Resource to monitor (e.g. `data_out`)
     * @param responseBuffer  Buffer in which to store the decoded response value (if any)
     * @param bufferSize      Size of the provided `responseBuffer`
     * @param lastModified    (Optional) Epoch timestamp (seconds) of the last known update; (default: `0`)
     * @param pollTimeout     (Optional) Polling timeout in milliseconds (default: `5000`)
     *
     * @return `true` if new data or pollTimeout reached (HTTP 200 or 304), `false` otherwise
     */
    bool longPoll(const char* resource, char* responseBuffer, size_t bufferSize, unsigned long lastModified = 0, unsigned long pollTimeout = 5000);

    /**
     * @brief Check/wait for a new value on the specified resource
     *
     * Note:
     *
     * - Response includes only `{value}` from the raw `{resource}={value}` request response
     *
     * @param resource        Resource to monitor (e.g. `data_out`)
     * @param responseString  String in which to store the decoded response value (if any)
     * @param lastModified    (Optional) Epoch timestamp (seconds) of the last known update; (default: `0`)
     * @param pollTimeout     (Optional) Polling timeout in milliseconds (default: `5000`)
     *
     * @return `true` if new data or pollTimeout reached (HTTP 200 or 304), `false` otherwise
     */
    bool longPoll(const String& resource, String& responseString, unsigned long lastModified = 0, unsigned long pollTimeout = 5000);

    /**
     * @brief Retrieve the current time from the server
     *
     * Note:
     *
     * - This can be an unauthenticated request (e.g. to confirm general server connectivity)
     *
     * @return Current time from the server (epoch seconds), or `0` if the request fails
     */
    unsigned long timestamp();

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  private:
    Client* _client;

    static const int _port = 443;

    char _connector[128];
    char _clientToken[41];

    unsigned long _rxTimeout = 10000; // Request response timeout (ms) (see: `setTimeout()`)

    char _dataBuffer[EXO_DATA_BUFFER_SIZE]; // Internal buffer for cloud request/response handling

    char _pollHeaders[64]; // Internal buffer for building Long Poll headers

    void setDomain(const char* domain);

    bool isConnected();

    bool timeExpired(unsigned long start, unsigned long duration);

    bool readHttpResponse(char* destBuffer, size_t bufferSize, unsigned long timeoutMs);

    void sendGetRequest(const char* path, const char* resource = nullptr, const char* authToken = nullptr, const char* extraHeaders = nullptr);
    void sendPostRequest(const char* path, const char* key, const char* value, const char* authToken = nullptr);

    void buildPollHeaders(char* buffer, size_t bufferSize, unsigned long lastModified, unsigned long pollTimeoutMs);

    void urlEncode(const char* src, char* dest, size_t destSize);
    void urlEncode(const String& src, String& dest);

    void urlDecode(const char* src, char* dest);
    void urlDecode(const String& input, String& responseString);
};
