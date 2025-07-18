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

#include "ExositeHTTP.h"

ExositeHTTP::ExositeHTTP(Client* client, const char* connector) {
  _client = client;
  setDomain(connector);
}

ExositeHTTP::ExositeHTTP(Client* client, String& connector)
  : ExositeHTTP(client, connector.c_str()) {}

ExositeHTTP::ExositeHTTP(Client* client, const char* connector, const char* clientToken) {
  _client = client;
  setDomain(connector);
  setToken(clientToken);
}

ExositeHTTP::ExositeHTTP(Client* client, String& connector, String& clientToken)
  : ExositeHTTP(client, connector.c_str(), clientToken.c_str()) {}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ExositeHTTP::setDomain(const char* domain) {
  strncpy(_connector, domain, sizeof(_connector) - 1);
  _connector[sizeof(_connector) - 1] = '\0';
}

void ExositeHTTP::setToken(const char* token) {
  strncpy(_clientToken, token, sizeof(_clientToken) - 1);
  _clientToken[sizeof(_clientToken) - 1] = '\0';
}

void ExositeHTTP::setToken(const String& token) {
  return setToken(token.c_str());
}

void ExositeHTTP::setTimeout(const unsigned long rxTimeoutMs) {
  _rxTimeout = rxTimeoutMs;
}

void ExositeHTTP::flushClient() {
  unsigned long start = millis();

  while ((millis() - start) < _flushTimeout) {
    while (_client->available()) {
      _client->read();
      start = millis(); // Reset timeout as more data is available
    }
    delay(_flushDelay); // Small wait for more data to potentially arrive
  }
}

bool ExositeHTTP::isConnected() {
  if (!_client->connected()) {
    LOG_DEBUG(G("Opening client connection..."));
    _client->stop();
    return _client->connect(_connector, _port);
  }

  return true;
}

bool ExositeHTTP::timeExpired(unsigned long start, unsigned long duration) {
  return (millis() - start) >= duration;
}

bool ExositeHTTP::readHttpResponse(char* buffer, size_t bufferSize, unsigned long timeoutMs) {
  unsigned long startTime = millis();

  const size_t maxSize = bufferSize - 1;

  char c;
  size_t pos = 0;
  bool dataReceived = false;
  bool fullyParsed = false;

  const unsigned int waitMs = 10;
  const unsigned int maxWaitCycles = 10;
  unsigned int waitCycles = 0;

  while (true) {
    // Timeout check
    if (timeExpired(startTime, timeoutMs)) {
      LOG_ERROR(G("Timed out processing HTTP response"));
      flushClient(); // Flush any remaining data
      break;
    }
    // Read data as it becomes available
    else if (_client->available()) {
      // Size check
      if (pos < maxSize) {
        dataReceived = true;
        c = _client->read();
        buffer[pos++] = c;
      }
      else {
        LOG_ERROR(G("Request response is larger than internal buffer allocation (≥"), bufferSize, G(" B)"));
        flushClient(); // Flush any remaining data
        break;
      }
    }
    else if (dataReceived && waitCycles++ < maxWaitCycles) {
      delay(waitMs); // Small pause in case of a delay in the response data
    }
    else if (dataReceived) {
      fullyParsed = true;
      break;
    }
  }

  buffer[pos] = '\0'; // Always null terminate

  return fullyParsed;
}

void ExositeHTTP::sendGetRequest(const char* path, const char* resource, const char* clientAuth, const char* pollHeaders) {
  _client->print(G("GET "));
  _client->print(path);
  if (resource) {
    _client->print("?");
    _client->print(resource);
  }
  _client->println(G(" HTTP/1.1"));

  _client->print(G("Host: "));
  _client->println(_connector);

  _client->print(G("User-Agent: ExositeHTTP-Cpp/"));
  _client->print(ACTIVATOR_VERSION);
  _client->print(G(" Arduino/"));
  _client->println(ARDUINO);

  _client->println(G("Accept: application/x-www-form-urlencoded; charset=utf-8"));

  if (clientAuth) {
    // Add authorization header
    _client->print(G("Authorization: token "));
    _client->println(clientAuth);
  }

  if (pollHeaders) {
    _client->println(pollHeaders);  // e.g. "If-Modified-Since: 0\r\nRequest-Timeout: 5000"
  }

  _client->println();  // End of headers
}

void ExositeHTTP::sendPostRequest(const char* path, const char* key, const char* value, const char* clientAuth) {
  _client->print(G("POST "));
  _client->print(path);
  _client->println(G(" HTTP/1.1"));

  _client->print(G("Host: "));
  _client->println(_connector);

  _client->print(G("User-Agent: ExositeHTTP-Cpp/"));
  _client->print(ACTIVATOR_VERSION);
  _client->print(G(" Arduino/"));
  _client->println(ARDUINO);

  _client->println(G("Accept: application/x-www-form-urlencoded; charset=utf-8"));
  _client->println(G("Content-Type: application/x-www-form-urlencoded; charset=utf-8"));

  // Compute content length
  size_t contentLength = (key ? strlen(key) : 0) + strlen("=") + (value ? strlen(value) : 0);
  _client->print(G("Content-Length: "));
  _client->println(contentLength);

  if (clientAuth) {
    // Add authorization header
    _client->print(G("Authorization: token "));
    _client->println(clientAuth);
  }
  _client->println();  // End of headers

  // Write body (as key=value)
  _client->print(key);
  _client->print("=");
  _client->println(value);
}

void ExositeHTTP::buildPollHeaders(char* buffer, size_t bufferSize, unsigned long lastModified, unsigned long pollTimeoutMs) {
  snprintf(buffer, bufferSize, "If-Modified-Since: %lu\r\nRequest-Timeout: %lu", lastModified, pollTimeoutMs);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

ApiResponse ExositeHTTP::provision(const char* identity, char* responseBuffer, size_t bufferSize) {
  ApiResponse res;
  res.statusCode = 0;
  res.success = false;

  responseBuffer[0] = '\0'; // Ensure the provided response buffer is cleared for use

  if (!isConnected()) {
    LOG_ERROR(G("Failed to connect to server"));
    return res;
  }

  if (!identity || !responseBuffer || bufferSize < 41) {
    LOG_ERROR(G("Invalid arguments for provisioning"));
    return res;
  }

  sendPostRequest("/provision/activate", "id", identity, nullptr);

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    LOG_ERROR(G("Failed to fully parse HTTP response"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    LOG_ERROR(G("Could not parse HTTP status code"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  res.statusCode = statusCode;

  // Handle by HTTP status code
  if (statusCode == 200) {
    const char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      LOG_ERROR(G("Malformed HTTP response"));
      LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
      return res;
    }
    else {
      body += 4; // Skip past double CRLF ("\r\n\r\n")
      res.success = urlDecode(body, responseBuffer, bufferSize);
      return res;
    }
  }
  else if (statusCode == 409) {
    LOG_ERROR(G("Identity is already provisioned (409 Conflict)"));
    return res;
  }
  else {
    LOG_ERROR(G("Unexpected HTTP status: "), statusCode);
    return res;
  }
}

ApiResponse ExositeHTTP::provision(const String& identity, String& responseString) {
  ApiResponse res;
  res.statusCode = 0;
  res.success = false;

  responseString = ""; // Ensure the provided response String is cleared for use

  if (!isConnected()) {
    LOG_ERROR(G("Failed to connect to server"));
    return res;
  }

  if (identity.length() == 0) {
    LOG_ERROR(G("Cannot provision provided identity: "), identity);
    return res;
  }

  sendPostRequest("/provision/activate", "id", identity.c_str(), nullptr);

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    LOG_ERROR(G("Failed to fully parse HTTP response"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    LOG_ERROR(G("Could not parse HTTP status code"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  res.statusCode = statusCode;

  // Handle by HTTP status code
  if (statusCode == 200) {
    const char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      LOG_ERROR(G("Malformed HTTP response"));
      LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
      return res;
    }
    else {
      body += 4; // Skip past double CRLF ("\r\n\r\n")
      res.success = urlDecode(String(body), responseString);
      return res;
    }
  }
  else if (statusCode == 409) {
    LOG_ERROR(G("Identity is already provisioned (409 Conflict)"));
    return res;
  }
  else {
    LOG_ERROR(G("Unexpected HTTP status: "), statusCode);
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }
}

ApiResponse ExositeHTTP::write(const char* resource, const char* writeChars) {
  ApiResponse res;
  res.statusCode = 0;
  res.success = false;

  if (!isConnected()) {
    LOG_ERROR(G("Failed to connect to server"));
    return res;
  }

  if (!resource || !writeChars) {
    LOG_ERROR(G("Missing value for resource and/or writeChars"));
    return res;
  }

  // Use the shared buffer to hold encoded request payload
  if (urlEncode(writeChars, _dataBuffer, sizeof(_dataBuffer))) {
    sendPostRequest("/onep:v1/stack/alias", resource, _dataBuffer, _clientToken);

    // [Re]use the shared buffer to receive the HTTP response
    if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
      LOG_ERROR(G("Failed to fully parse HTTP response"));
      LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
      return res;
    }

    // Extract HTTP status code
    int statusCode = 0;
    if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
      LOG_ERROR(G("Could not parse HTTP status code"));
      LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
      return res;
    }

    res.statusCode = statusCode;

    // Handle by HTTP status code
    if (statusCode == 204) {
      res.success = true;
      return res;
    }
    else {
      LOG_ERROR(G("Unexpected HTTP status: "), statusCode);
      return res;
    }
  }
  else {
    return res; // Failed to encode provided writeChars
  }
}

ApiResponse ExositeHTTP::write(const String& resource, const String& writeString) {
  return write(resource.c_str(), writeString.c_str());
}

ApiResponse ExositeHTTP::read(const char* resource, char* responseBuffer, size_t bufferSize) {
  ApiResponse res;
  res.statusCode = 0;
  res.success = false;

  responseBuffer[0] = '\0'; // Ensure the provided response buffer is cleared for use

  if (!isConnected()) {
    LOG_ERROR(G("Failed to connect to server"));
    return res;
  }

  sendGetRequest("/onep:v1/stack/alias", resource, _clientToken, nullptr);

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    LOG_ERROR(G("Failed to fully parse HTTP response"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    LOG_ERROR(G("Could not parse HTTP status code"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  res.statusCode = statusCode;

  // Handle by HTTP status code
  if (statusCode == 200) {
    char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      LOG_ERROR(G("Malformed HTTP response"));
      LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
      return res;
    }
    else {
      body += 4; // Skip past double CRLF ("\r\n\r\n")

      // Confirm the response body matches the expected structure
      const char* delimiter = strchr(body, '=');
      if (!delimiter || *(delimiter + 1) == '\0') {
        LOG_ERROR(G("Malformed response body (not 'resource=value')"));
        LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
        return res;
      }

      const char* value = delimiter + 1; // Skip past the delimiter, to just the value

      res.success = urlDecode(value, responseBuffer, bufferSize);
      return res;
    }
  }
  else if (statusCode == 204) {
    res.success = true;
    return res;
  }

  LOG_ERROR(G("Unexpected HTTP status: "), statusCode);
  return res;
}

ApiResponse ExositeHTTP::read(const String& resource, String& responseString) {
  ApiResponse res;
  res.statusCode = 0;
  res.success = false;

  responseString = ""; // Ensure the provided response String is cleared for use

  if (!isConnected()) {
    LOG_ERROR(G("Failed to connect to server"));
    return res;
  }

  sendGetRequest("/onep:v1/stack/alias", resource.c_str(), _clientToken, nullptr);

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    LOG_ERROR(G("Failed to fully parse HTTP response"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    LOG_ERROR(G("Could not parse HTTP status code"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  res.statusCode = statusCode;

  // Handle by HTTP status code
  if (statusCode == 200) {
    char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      LOG_ERROR(G("Malformed HTTP response"));
      LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
      return res;
    }
    else {
      body += 4; // Skip past double CRLF ("\r\n\r\n")

      // Confirm the response body matches the expected structure
      const char* delimiter = strchr(body, '=');
      if (!delimiter || *(delimiter + 1) == '\0') {
        LOG_ERROR(G("Malformed response body (not 'resource=value')"));
        LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
        return res;
      }

      const char* value = delimiter + 1; // Skip past the delimiter, to just the value
      res.success = urlDecode(value, responseString);
      return res;
    }
  }
  else if (statusCode == 204) {
    res.success = true;
    return res;
  }
  else {
    LOG_ERROR(G("Unexpected HTTP status: "), statusCode);
    return res;
  }
}

ApiResponse ExositeHTTP::longPoll(const char* resource, char* responseBuffer, size_t bufferSize, unsigned long lastModified, unsigned long pollTimeout) {
  ApiResponse res;
  res.statusCode = 0;
  res.success = false;

  responseBuffer[0] = '\0'; // Ensure the provided response buffer is cleared for use

  buildPollHeaders(_pollHeaders, sizeof(_pollHeaders), lastModified, pollTimeout);

  if (!isConnected()) {
    LOG_ERROR(G("Failed to connect to server"));
    return res;
  }

  sendGetRequest("/onep:v1/stack/alias", resource, _clientToken, _pollHeaders);

  // For longPoll() only, adjust the receive timeout to ensure complete processing of the request
  unsigned long effectiveTimeout = _rxTimeout + pollTimeout;

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), effectiveTimeout)) {
    LOG_ERROR(G("Failed to fully parse HTTP response"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    LOG_ERROR(G("Could not parse HTTP status code"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  res.statusCode = statusCode;

  // Handle by HTTP status code
  if (statusCode == 304) {
    res.success = true;
    return res;
  }
  else if (statusCode == 200) {
    char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      LOG_ERROR(G("Malformed response body"));
      return res;
    }
    else {
      body += 4; // Skip past double CRLF ("\r\n\r\n")

      // Confirm the response body matches the expected structure
      const char* delimiter = strchr(body, '=');
      if (!delimiter || *(delimiter + 1) == '\0') {
        LOG_ERROR(G("Malformed response body (not 'resource=value')"));
        return res;
      }

      const char* value = delimiter + 1; // Skip past the delimiter, to just the value

      res.success = urlDecode(value, responseBuffer, bufferSize);
      return res;
    }
  }
  else {
    LOG_ERROR(G("Unexpected HTTP status: "), statusCode);
    return res;
  }
}

ApiResponse ExositeHTTP::longPoll(const String& resource, String& responseString, unsigned long lastModified, unsigned long pollTimeout) {
  ApiResponse res;
  res.statusCode = 0;
  res.success = false;

  responseString = ""; // Ensure the provided response String is cleared for use

  buildPollHeaders(_pollHeaders, sizeof(_pollHeaders), lastModified, pollTimeout);

  if (!isConnected()) {
    LOG_ERROR(G("Failed to connect to server"));
    return res;
  }

  sendGetRequest("/onep:v1/stack/alias", resource.c_str(), _clientToken, _pollHeaders);

  // For longPoll() only, adjust the receive timeout to ensure complete processing of the request
  unsigned long effectiveTimeout = _rxTimeout + pollTimeout;

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), effectiveTimeout)) {
    LOG_ERROR(G("Failed to fully parse HTTP response"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    LOG_ERROR(G("Could not parse HTTP status code"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  res.statusCode = statusCode;

  // Handle by HTTP status code
  if (statusCode == 304) {
    res.success = true;
    return res;
  }
  else if (statusCode == 200) {
    char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      LOG_ERROR(G("Malformed HTTP response"));
      return res;
    }
    else {
      body += 4; // Skip past double CRLF ("\r\n\r\n")

      // Confirm the response body matches the expected structure
      const char* delimiter = strchr(body, '=');
      if (!delimiter || *(delimiter + 1) == '\0') {
        LOG_ERROR(G("Malformed response body (not 'resource=value')"));
        return res;
      }

      const char* value = delimiter + 1; // Skip past the delimiter, to just the value

      res.success = urlDecode(value, responseString);
      return res;
    }
  }
  else {
    LOG_ERROR(G("Unexpected HTTP status: "), statusCode);
    return res;
  }
}

ApiResponse ExositeHTTP::timestamp(unsigned long* serverTime) {
  ApiResponse res;
  res.statusCode = 0;
  res.success = false;

  if (!isConnected()) {
    LOG_ERROR(G("Failed to connect to server"));
    return res;
  }

  sendGetRequest("/timestamp", nullptr, nullptr, nullptr);

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    LOG_ERROR(G("Failed to fully parse HTTP response"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    LOG_ERROR(G("Could not parse HTTP status code"));
    LOG_DEBUG(G("Raw response:\n"), _dataBuffer);
    return res;
  }

  res.statusCode = statusCode;

  if (statusCode == 200) {
    char* bodyPos = strstr(_dataBuffer, "\r\n\r\n");
    if (bodyPos) {
      bodyPos = bodyPos + 4;
      *serverTime = strtoul(bodyPos, nullptr, 10);
      res.success = true;
    }
  }
  else {
    LOG_ERROR(G("Unexpected HTTP status: "), statusCode);
    return res;
  }

  return res;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ExositeHTTP::urlEncode(const char* src, char* dest, size_t destSize) {
  static const char hex[] = "0123456789ABCDEF";
  const size_t maxSize = destSize - 1;

  size_t pos = 0;
  bool fullyEncoded = true;

  while (*src) {
    if (pos < maxSize) { // Size check
      char c = *src++;
      if (('a' <= c && c <= 'z') || 
          ('A' <= c && c <= 'Z') || 
          ('0' <= c && c <= '9') || 
          c == '-' || c == '_' || c == '.' || c == '~') {
        dest[pos++] = c; // Unreserved characters are not encoded
      }
      else if (c == ' ') {
        dest[pos++] = '+'; // Space is replaced by a plus sign
      }
      else {
        // Ensure room for %HH
        if (pos + 3 >= maxSize) {
          LOG_ERROR(G("Encoded request body larger than internal buffer (≥"), destSize, G(" B)"));
          fullyEncoded = false;
          break;
        }
        else {
          dest[pos++] = '%';
          dest[pos++] = hex[(c >> 4) & 0x0F];
          dest[pos++] = hex[c & 0x0F];
        }
      }
    }
    else {
      LOG_ERROR(G("Encoded request body larger than internal buffer (≥"), destSize, G(" B)"));
      fullyEncoded = false;
      break;
    }
  }

  dest[pos] = '\0'; // Null-terminate the encoded value

  if (!fullyEncoded) {
    LOG_DEBUG(G("Encoded: "), dest);
    LOG_DEBUG(G("Remainder: "), src);
  }

  return fullyEncoded;
}

bool ExositeHTTP::urlDecode(const char* src, char* dest, size_t destSize) {
  const size_t maxSize = destSize - 1;

  size_t pos = 0;
  bool fullyDecoded = true;

  while (*src) {
    if (pos < maxSize) { // Size check
      if (*src == '%') {
        if (src[1] && src[2]) {
          char high = src[1];
          char low = src[2];

          if (high >= '0' && high <= '9') high = high - '0';
          else if (high >= 'A' && high <= 'F') high = high - 'A' + 10;
          else if (high >= 'a' && high <= 'f') high = high - 'a' + 10;
          else {
            LOG_ERROR(G("Invalid hex in response body"));
            fullyDecoded = false;
            break;
          }

          if (low >= '0' && low <= '9') low = low - '0';
          else if (low >= 'A' && low <= 'F') low = low - 'A' + 10;
          else if (low >= 'a' && low <= 'f') low = low - 'a' + 10;
          else {
            LOG_ERROR(G("Invalid hex in response body"));
            fullyDecoded = false;
            break;
          }

          dest[pos++] = (high << 4) + low;
          src += 3;
        }
        else {
          LOG_ERROR(G("Incomplete escape sequence in response body"));
          fullyDecoded = false;
          break;
        }
      }
      else if (*src == '+') {
        dest[pos++] = ' '; // Plus sign is replaced by a space
        src++;
      }
      else {
        dest[pos++] = *src++; // Directly copy unreserved character
      }
    }
    else {
      LOG_ERROR(G("Decoded response body larger than provided buffer (≥"), destSize, G(" B)"));
      fullyDecoded = false;
      break;
    }
  }

  dest[pos] = '\0'; // Null-terminate the decoded value

  if (!fullyDecoded) {
    LOG_DEBUG(G("Decoded: "), dest);
    LOG_DEBUG(G("Remainder: "), src);
  }

  return fullyDecoded;
}

bool ExositeHTTP::urlDecode(const String& input, String& responseString) {
  bool fullyDecoded = true;

  for (size_t i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c == '%') {
      if (i + 2 < input.length()) {
        char high = input.charAt(i + 1);
        char low = input.charAt(i + 2);
        i += 2;

        if (high >= '0' && high <= '9') high -= '0';
        else if (high >= 'A' && high <= 'F') high = high - 'A' + 10;
        else if (high >= 'a' && high <= 'f') high = high - 'a' + 10;
        else {
          LOG_ERROR(G("Invalid hex in response body"));
          fullyDecoded = false;
          break;
        }

        if (low >= '0' && low <= '9') low -= '0';
        else if (low >= 'A' && low <= 'F') low = low - 'A' + 10;
        else if (low >= 'a' && low <= 'f') low = low - 'a' + 10;
        else {
          LOG_ERROR(G("Invalid hex in response body"));
          fullyDecoded = false;
          break;
        }

        responseString += char((high << 4) + low);
      }
      else {
        LOG_ERROR(G("Incomplete escape sequence in response body"));
        fullyDecoded = false;
        break;
      }
    }
    else if (c == '+') {
      responseString += ' '; // Plus sign is replaced by a space
    }
    else {
      responseString += c; // Directly copy unreserved character
    }
  }

  if (!fullyDecoded) {
    LOG_DEBUG(G("Decoded: "), input);
    LOG_DEBUG(G("Remainder: "), responseString);
  }

  return fullyDecoded;
}
