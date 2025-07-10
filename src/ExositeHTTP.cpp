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

bool ExositeHTTP::readHttpResponse(char* buffer, size_t maxLen, unsigned long timeoutMs) {
  LOG_DEBUG(G("Parsing HTTP response..."));

  char c;
  size_t pos = 0;
  bool dataReceived = false;

  unsigned long startTime = millis();

  const unsigned int waitMs = 10;
  const unsigned int maxWaitCycles = 10;
  unsigned int waitCycles = 0;

  while (pos < (maxLen - 1)) {
    // Check against specified timeout
    if (timeExpired(startTime, timeoutMs)) {
      Serial.println(G("[Error] Timed out processing HTTP response"));

      // Flush remaining data
      while (_client->available()) c = _client->read();
      return false;
    }
    // Read data, once (and while) available
    else if (_client->available()) {
      dataReceived = true;
      c = _client->read();
      buffer[pos++] = c; // Assigns c into buffer[pos], then increments pos
    }
    else if (dataReceived && waitCycles++ < maxWaitCycles) {
      delay(waitMs); // Small pause to wait for more data
    }
    else if (dataReceived) {
      buffer[pos] = '\0';
      break;
    }
  }

  buffer[pos++] = '\0'; // For asolute certainty that the buffer is safely terminated

  // Validate content size
  if (pos >= maxLen) {
    Serial.print(G("[Error] Response content too large for internal buffer (>= "));
    Serial.print(maxLen);
    Serial.println(G(" bytes)"));

    // Flush remaining data
    while (_client->available()) c = _client->read();

    return false;
  }

  return dataReceived;
}

void ExositeHTTP::sendGetRequest(const char* path, const char* resource, const char* clientAuth, const char* pollHeaders) {
  LOG_DEBUG(G("Sending GET"));
  LOG_DEBUG(path);

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
  LOG_DEBUG(G("Sending POST"));

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

bool ExositeHTTP::provision(const char* identity, char* responseBuffer, size_t bufferSize) {
  responseBuffer[0] = '\0'; // Ensure the provided response buffer is cleared for use

  if (!isConnected()) {
    Serial.println(G("[Error] Could not connect to server"));
    return false;
  }

  if (!identity || !responseBuffer || bufferSize < 41) {
    Serial.println(G("[Error] Invalid arguments for provisioning"));
    return false;
  }

  _client->flush();
  sendPostRequest("/provision/activate", "id", identity, nullptr);

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    Serial.println(G("[Error] Failed to read HTTP response"));
    return false;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    Serial.println(G("[Error] Could not parse HTTP status code"));
    LOG_DEBUG(_dataBuffer);
    return false;
  }

  if (statusCode == 200) {
    const char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      Serial.println(G("[Error] Malformed HTTP response"));
      return false;
    }

    body += 4; // Skip past double CRLF ("\r\n\r\n")

    urlDecode(body, responseBuffer);

    return true;
  }

  if (statusCode == 409) {
    Serial.println(G("[Error] Identity is already provisioned (409 Conflict)"));
    return false;
  }

  Serial.print(G("[Error] Unexpected HTTP status: "));
  Serial.println(statusCode);
  return false;
}

bool ExositeHTTP::provision(const String& identity, String& responseString) {
  responseString = ""; // Ensure the provided response String is cleared for use

  if (!isConnected()) {
    Serial.println(G("[Error] Could not connect to server"));
    return false;
  }

  if (identity.length() == 0) {
    Serial.println(G("[Error] Empty identity string"));
    return false;
  }

  _client->flush();
  sendPostRequest("/provision/activate", "id", identity.c_str(), nullptr);

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    Serial.println(G("[Error] Failed to read HTTP response"));
    return false;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    Serial.println(G("[Error] Could not parse HTTP status code"));
    LOG_DEBUG(_dataBuffer);
    return false;
  }

  if (statusCode == 200) {
    const char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      Serial.println(G("[Error] Malformed HTTP response"));
      return false;
    }

    body += 4; // Skip past double CRLF ("\r\n\r\n")

    responseString = String(body);

    if (responseString.length() < 40) {
      Serial.println(G("[Error] Response body too short"));
      return false;
    }

    return true;
  }

  Serial.print(G("[Error] Unexpected HTTP status: "));
  Serial.println(statusCode);
  return false;
}

bool ExositeHTTP::write(const char* resource, const char* writeChars) {
  if (!isConnected()) {
    Serial.println(G("[Error] Could not connect to server"));
    return false;
  }

  if (!resource || !writeChars) {
    Serial.println(G("[Error] Missing value for resource and/or writeString"));
    return false;
  }

  // Use the shared buffer to hold encoded request payload
  urlEncode(writeChars, _dataBuffer, sizeof(_dataBuffer));

  _client->flush();
  sendPostRequest("/onep:v1/stack/alias", resource, _dataBuffer, _clientToken);

  // [Re]use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    Serial.println(G("[Error] Failed to read HTTP response"));
    return false;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    Serial.println(G("[Error] Could not parse HTTP status code"));
    LOG_DEBUG(_dataBuffer);
    return false;
  }

  if (statusCode == 204) {
    return true;
  }

  Serial.print(G("[Error] Unexpected HTTP status: "));
  Serial.println(statusCode);
  return false;
}

bool ExositeHTTP::write(const String& resource, const String& writeString) {
  return write(resource.c_str(), writeString.c_str());
}

bool ExositeHTTP::read(const char* resource, char* responseBuffer, size_t bufferSize) {
  responseBuffer[0] = '\0'; // Ensure the provided response buffer is cleared for use

  if (!isConnected()) {
    Serial.println(G("[Error] Could not connect to server"));
    return false;
  }

  _client->flush();
  sendGetRequest("/onep:v1/stack/alias", resource, _clientToken, nullptr);

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    Serial.println(G("[Error] Failed to read HTTP response"));
    return false;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    Serial.println(G("[Error] Could not parse HTTP status code"));
    LOG_DEBUG(_dataBuffer);
    return false;
  }

  if (statusCode == 200) {
    char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      Serial.println(G("[Error] Malformed HTTP response"));
      return false;
    }

    body += 4; // Skip past double CRLF ("\r\n\r\n")

    // Confirm the response body matches the expected structure
    const char* delimiter = strchr(body, '=');
    if (!delimiter || *(delimiter + 1) == '\0') {
      Serial.println(G("[Error] Malformed response body (not 'resource=value')"));
      return false;
    }

    const char* value = delimiter + 1; // Skip past the delimiter, to just the value
    if (strlen(value) >= (bufferSize - 1)) {
      Serial.print(G("[Error] Response body too large for provided buffer: "));
      Serial.println(strlen(value));
      return false;
    }

    urlDecode(value, responseBuffer);

    return true;
  }
  else if (statusCode == 204) {
    return true;
  }

  Serial.print(G("[Error] Unexpected HTTP status: "));
  Serial.println(statusCode);
  return false;
}

bool ExositeHTTP::read(const String& resource, String& responseString) {
  responseString = ""; // Ensure the provided response String is cleared for use

  if (!isConnected()) {
    Serial.println(G("[Error] Could not connect to server"));
    return false;
  }

  _client->flush();
  sendGetRequest("/onep:v1/stack/alias", resource.c_str(), _clientToken, nullptr);

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    Serial.println(G("[Error] Failed to read HTTP response"));
    return false;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    Serial.println(G("[Error] Could not parse HTTP status code"));
    LOG_DEBUG(_dataBuffer);
    return false;
  }

  if (statusCode == 200) {
    char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      Serial.println(G("[Error] Malformed HTTP response"));
      return false;
    }

    body += 4; // Skip past double CRLF ("\r\n\r\n")

    // Confirm the response body matches the expected structure
    const char* delimiter = strchr(body, '=');
    if (!delimiter || *(delimiter + 1) == '\0') {
      Serial.println(G("[Error] Malformed response body (not 'resource=value')"));
      return false;
    }

    const char* value = delimiter + 1; // Skip past the delimiter, to just the value
    urlDecode(value, responseString);

    return true;
  }
  else if (statusCode == 204) {
    return true;
  }

  Serial.print(G("[Error] Unexpected HTTP status: "));
  Serial.println(statusCode);
  return false;
}

bool ExositeHTTP::longPoll(const char* resource, char* responseBuffer, size_t bufferSize, unsigned long lastModified, unsigned long pollTimeout) {
  responseBuffer[0] = '\0'; // Ensure the provided response buffer is cleared for use
  buildPollHeaders(_pollHeaders, sizeof(_pollHeaders), lastModified, pollTimeout);

  if (!isConnected()) {
    Serial.println(G("[Error] Could not connect to server"));
    return false;
  }

  _client->flush();
  sendGetRequest("/onep:v1/stack/alias", resource, _clientToken, _pollHeaders);

  // For longPoll() only, adjust the receive timeout to ensure complete processing of the request
  unsigned long effectiveTimeout = _rxTimeout + pollTimeout;

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), effectiveTimeout)) {
    Serial.println(G("[Error] Failed to read HTTP response"));
    return false;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    Serial.println(G("[Error] Could not parse HTTP status code"));
    LOG_DEBUG(_dataBuffer);
    return false;
  }

  if (statusCode == 304) {
    return true;
  }
  else if (statusCode == 200) {
    char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      Serial.println(G("[Error] Malformed HTTP response"));
      return false;
    }

    body += 4; // Skip past double CRLF ("\r\n\r\n")

    // Confirm the response body matches the expected structure
    const char* delimiter = strchr(body, '=');
    if (!delimiter || *(delimiter + 1) == '\0') {
      Serial.println(G("[Error] Malformed response body (not 'resource=value')"));
      return false;
    }

    const char* value = delimiter + 1; // Skip past the delimiter, to just the value
    if (strlen(value) >= (bufferSize - 1)) {
      Serial.print(G("[Error] Response body too large for provided buffer: "));
      Serial.println(strlen(value));
      return false;
    }

    urlDecode(value, responseBuffer);

    return true;
  }

  Serial.print(G("[Error] Unexpected HTTP status: "));
  Serial.println(statusCode);
  return false;
}

bool ExositeHTTP::longPoll(const String& resource, String& responseString, unsigned long lastModified, unsigned long pollTimeout) {
  responseString = ""; // Ensure the provided response String is cleared for use
  buildPollHeaders(_pollHeaders, sizeof(_pollHeaders), lastModified, pollTimeout);

  if (!isConnected()) {
    Serial.println(G("[Error] Could not connect to server"));
    return false;
  }

  _client->flush();
  sendGetRequest("/onep:v1/stack/alias", resource.c_str(), _clientToken, _pollHeaders);

  // For longPoll() only, adjust the receive timeout to ensure complete processing of the request
  unsigned long effectiveTimeout = _rxTimeout + pollTimeout;

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), effectiveTimeout)) {
    Serial.println(G("[Error] Failed to read HTTP response"));
    return false;
  }

  // Extract HTTP status code
  int statusCode = 0;
  if (sscanf(_dataBuffer, "HTTP/1.1 %d", &statusCode) != 1) {
    Serial.println(G("[Error] Could not parse HTTP status code"));
    LOG_DEBUG(_dataBuffer);
    return false;
  }

  if (statusCode == 304) {
    return true;
  }
  else if (statusCode == 200) {
    char* body = strstr(_dataBuffer, "\r\n\r\n"); // Assume body starts after double CRLF
    if (!body) {
      Serial.println(G("[Error] Malformed HTTP response"));
      return false;
    }

    body += 4; // Skip past double CRLF ("\r\n\r\n")

    // Confirm the response body matches the expected structure
    const char* delimiter = strchr(body, '=');
    if (!delimiter || *(delimiter + 1) == '\0') {
      Serial.println(G("[Error] Malformed response body (not 'resource=value')"));
      return false;
    }

    const char* value = delimiter + 1; // Skip past the delimiter, to just the value
    urlDecode(value, responseString);

    return true;
  }

  Serial.print(G("[Error] Unexpected HTTP status: "));
  Serial.println(statusCode);
  LOG_DEBUG(_dataBuffer);
  return false;
}

unsigned long ExositeHTTP::timestamp() {
  unsigned long timestamp = 0;

  if (!isConnected()) {
    Serial.println(G("[Error] Failed to open connection"));
    return timestamp; // 0
  }

  _client->flush();
  sendGetRequest("/timestamp", nullptr, nullptr, nullptr);

  // Use the shared buffer to receive the HTTP response
  if (!readHttpResponse(_dataBuffer, sizeof(_dataBuffer), _rxTimeout)) {
    return timestamp; // 0
  }

  if (strstr(_dataBuffer, "HTTP/1.1 200 OK")) {
    char* bodyPos = strstr(_dataBuffer, "\r\n\r\n");
    if (bodyPos) {
      bodyPos = bodyPos + 4;
      timestamp = strtoul(bodyPos, nullptr, 10);
    }
  }
  else {
    Serial.println(G("[Error] Unexpected HTTP response"));
  }

  return timestamp;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ExositeHTTP::urlEncode(const char* src, char* dest, size_t destSize) {
  static const char hex[] = "0123456789ABCDEF";
  size_t pos = 0;

  while (*src && pos < destSize - 1) {
    char c = *src++;
    if (('a' <= c && c <= 'z') || 
        ('A' <= c && c <= 'Z') || 
        ('0' <= c && c <= '9') || 
        c == '-' || c == '_' || c == '.' || c == '~') {
      dest[pos++] = c; // Unreserved characters are not encoded
    }
    else if (c == ' ') {
      dest[pos++] = '+';
    }
    else {
      if (pos + 3 >= destSize) break; // Ensure room for %HH
      dest[pos++] = '%';
      dest[pos++] = hex[(c >> 4) & 0x0F];
      dest[pos++] = hex[c & 0x0F];
    }
  }

  dest[pos] = '\0'; // Null-terminate the encoded string
}

void ExositeHTTP::urlEncode(const String& src, String& dest) {
  dest = "";
  char hex[] = "0123456789ABCDEF";

  for (size_t i = 0; i < src.length(); i++) {
    char c = src.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      dest += c; // Unreserved characters are not encoded
    }
    else if (c == ' ') {
      dest += '+';
    }
    else {
      dest += '%';
      dest += hex[(c >> 4) & 0x0F];
      dest += hex[c & 0x0F];
    }
  }
}

void ExositeHTTP::urlDecode(const char* src, char* dest) {
  while (*src) {
    if (*src == '%') {
      // Convert the next two characters into a byte and append to dest
      char high = src[1];
      char low = src[2];

      if (high >= '0' && high <= '9') high = high - '0';
      else if (high >= 'A' && high <= 'F') high = high - 'A' + 10;
      else if (high >= 'a' && high <= 'f') high = high - 'a' + 10;

      if (low >= '0' && low <= '9') low = low - '0';
      else if (low >= 'A' && low <= 'F') low = low - 'A' + 10;
      else if (low >= 'a' && low <= 'f') low = low - 'a' + 10;

      *dest = (high << 4) + low;
      src += 3;
    }
    else if (*src == '+') {
      *dest = ' '; // Plus sign is replaced by a space
      src++;
    }
    else {
      *dest = *src; // Directly copy unreserved characters
      src++;
    }
    dest++; // Move the pointer forward
  }

  *dest = '\0'; // Null-terminate the decoded string
}

void ExositeHTTP::urlDecode(const String& input, String& responseString) {
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

        if (low >= '0' && low <= '9') low -= '0';
        else if (low >= 'A' && low <= 'F') low = low - 'A' + 10;
        else if (low >= 'a' && low <= 'f') low = low - 'a' + 10;

        responseString += char((high << 4) + low);
      }
    }
    else if (c == '+') {
      responseString += ' '; // Plus sign is replaced by a space
    }
    else {
      responseString += c; // Directly copy unreserved characters
    }
  }
}
