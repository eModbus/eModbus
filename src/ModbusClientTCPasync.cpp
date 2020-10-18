// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusClientTCPasync.h"

ModbusClientTCPasync::ModbusClientTCPasync(IPAddress address, uint16_t port, uint16_t queueLimit) :
  ModbusClient(),
  txQueue(),
  rxQueue(),
  MTA_client(),
  MTA_timeout(DEFAULTTIMEOUT),
  MTA_idleTimeout(DEFAULTIDLETIME),
  MTA_qLimit(queueLimit),
  MTA_lastActivity(0),
  MTA_state(DISCONNECTED),
  MTA_target()
    {
      // attach all handlers on async tcp events
      MTA_client.onConnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onConnected(); }, this);
      MTA_client.onDisconnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onDisconnected(); }, this);
      // MTA_client.onError([](void* i, AsyncClient* c, int8_t error) { (static_cast<ModbusClientTCPasync*>(i))->onError(error); }, this);
      // MTA_client.onTimeout([](void* i, AsyncClient* c, uint32_t time) { (static_cast<ModbusClientTCPasync*>(i))->onTimeout(time); }, this);
      // MTA_client.onAck([](void* i, AsyncClient* c, size_t len, uint32_t time) { (static_cast<ModbusClientTCPasync*>(i))->onAck(len, time); }, this);
      MTA_client.onData([](void* i, AsyncClient* c, void* data, size_t len) { (static_cast<ModbusClientTCPasync*>(i))->onPacket(static_cast<uint8_t*>(data), len); }, this);
      MTA_client.onPoll([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onPoll(); }, this);

      // disable nagle algorithm ref Modbus spec
      MTA_client.setNoDelay(true);

      // fill target
      MTA_target.host = address;
      MTA_target.port = port;
    }

// Destructor: clean up queue, task etc.
ModbusClientTCPasync::~ModbusClientTCPasync() {
  // Clean up queue
  {
    // Safely lock access
    lock_guard<mutex> lockGuard(qLock);
    lock_guard<mutex> lockguard(sLock);
    // Delete all elements from queues
    while (!txQueue.empty()) {
      TCPRequest *r = txQueue.front();
      delete r;
      txQueue.pop_front();
    }
    while (!rxQueue.empty()) {
      TCPRequest *r = rxQueue.front();
      delete r;
      rxQueue.pop_front();
    }
  }
  // force close client
  MTA_client.close(true);
}

// optionally manually connect to modbus server. Otherwise connection will be made upon first request
void ModbusClientTCPasync::connect() {
  Serial.print("connecting\n");
  lock_guard<mutex> lockGuard(sLock);
  // only connect if disconnected
  if (MTA_state == DISCONNECTED) {
    MTA_state = CONNECTING;
    MTA_lastActivity = millis();
    MTA_client.connect(MTA_target.host, MTA_target.port);
  }
}

// manually disconnect from modbus server. Connection will also auto close after idle time
void ModbusClientTCPasync::disconnect(bool force) {
  Serial.print("disconnecting\n");
  lock_guard<mutex> lockGuard(sLock);
  // only disconnect if connected
  if (MTA_state == CONNECTED) {
    MTA_state = DISCONNECTING;
    MTA_client.close(force);
  }
}

// Set timeout value
void ModbusClientTCPasync::setTimeout(uint32_t timeout) {
  MTA_timeout = timeout;
}

// Set idle timeout value (time before connection auto closes after being idle)
void ModbusClientTCPasync::setIdleTimeout(uint32_t timeout) {
  MTA_idleTimeout = timeout;
}

// makeHead: helper function to set up a MSB TCP header
bool ModbusClientTCPasync::makeHead(uint8_t *data, uint16_t dataLen, uint16_t TID, uint16_t PID, uint16_t LEN) {
  uint16_t headlong = 6;
  uint16_t offs = 0;
  uint16_t ptr = 0;

  if (dataLen < headlong) return false;   // Will not fit
  if (data == nullptr) return false;      // No data allocated?

  offs = addValue(data + ptr, headlong, TID);
  headlong -= offs;
  ptr += offs;
  offs = addValue(data + ptr, headlong, PID);
  headlong -= offs;
  ptr += offs;
  offs = addValue(data + ptr, headlong, LEN);
  headlong -= offs;
  // headlong should be 0 here!
  if (headlong) return false;
  return true;
}

// addToQueue: send freshly created request to queue
bool ModbusClientTCPasync::addToQueue(TCPRequest *request) {
  // Did we get one?
  if (request) {
    Serial.print("request added to queue\n");
    // Inject actual transactionID into tcpHead
    request->tcpHead.transactionID = messageCount++;
    lock_guard<mutex> lockGuard(qLock);
    if (txQueue.size() + rxQueue.size() < MTA_qLimit) {
      // add sending time to request for timeout purposes
      request->target.timeout = millis();
      // if we're already connected, try to send and push to rxQueue
      // or push to txQueue and (re)connect
      if (MTA_state == CONNECTED && send(request)) {
        rxQueue.push_back(request);
      } else {
        txQueue.push_back(request);
        if (MTA_state == DISCONNECTED) {
          connect();
        }
      }
      return true;
    }
  }
  return false;
}

// Move complete message data including tcpHead into a std::vector
vector<uint8_t> ModbusClientTCPasync::vectorize(uint16_t transactionID, TCPRequest *request, Error err) {
  vector<uint8_t> rv;       /// Returned std::vector

  // Was the message generated?
  if (err != SUCCESS) {
    // No. Return the Error code only - vector size is 1
    rv.reserve(1);
    rv.push_back(err);
  // If it was successful - did we get a message?
  } else if (request) {
    // Yes, obviously. 
    // Resize the vector to take tcpHead (6 bytes) + message proper
    rv.reserve(request->len() + 6);
    rv.resize(request->len() + 6);

    // Do a fast (non-C++-...) copy
    uint8_t *cp = rv.data();
    // Copy in TCP header
    makeHead(cp, 6, transactionID, request->tcpHead.protocolID, request->tcpHead.len);
    // Copy in message contents
    memcpy(cp + 6, request->data(), request->len());
  }
  // Bring back the vector
  return rv;
}

void ModbusClientTCPasync::onConnected() {
  Serial.print("connected\n");
  lock_guard<mutex> lockGuard(sLock);
  MTA_state = CONNECTED;
  MTA_lastActivity = millis();
  // from now on onPoll will be called every 500 msec
}

void ModbusClientTCPasync::onDisconnected() {
  Serial.print("disconnected\n");
  lock_guard<mutex> slockGuard(sLock);
  MTA_state = DISCONNECTED;

  // empty queue on disconnect, calling errorcode on every waiting request
  lock_guard<mutex> qlockGuard(qLock);
  while (!txQueue.empty()) {
    TCPRequest* r = txQueue.front();
    if (onError) {
      onError(IP_CONNECTION_FAILED, r->getToken());
    }
    delete r;
    txQueue.pop_front();
  }
  while (!rxQueue.empty()) {
    TCPRequest* r = rxQueue.front();
    if (onError) {
      onError(IP_CONNECTION_FAILED, r->getToken());
    }
    delete r;
    rxQueue.pop_front();
  }
}

/*
void ModbusClientTCPasync::onError(int8_t error) {
  // onDisconnect will alse be called, so nothing to do here
}

void onTimeout(uint32_t time) {
  // timeOut is handled by onPoll or onDisconnect
}

void onAck(size_t len, uint32_t time) {
  // assuming we don't need this
}
*/
void ModbusClientTCPasync::onPacket(uint8_t* data, size_t length) {
  Serial.printf("packet received - len %d\n", length);
  // reset idle timeout
  MTA_lastActivity = millis();

  TCPRequest* request = nullptr;
  TCPResponse* response = nullptr;
  uint16_t transactionID = 0;
  uint16_t messageLength = 0;
  size_t processedBytes = 0;

  // 1. Check for valid modbus message

  // MBAP header is 6 bytes, we can't do anything with less
  // total message should be longer then byte 5 (remaining length) + MBAP length
  // remaining length should be less then 254
  if (length > 6 &&
      data[2] == 0 &&
      data[3] == 0 &&
      length >= data[5] + 6 &&
      data[5] < 254) {
    transactionID = data[0] << 8 | data[1];
    messageLength = data[4] << 8 | data[5];
    response = new TCPResponse(messageLength);
    response->add(messageLength, &data[6]);
    processedBytes += 6 + messageLength;
    Serial.printf("packet validated - len %d\n", processedBytes);
  } else {
    // invalid packet, abort function
    Serial.print("packet invalid\n");
    return;
  }

  // 2. if we got a valid response, match with a request

  if (response) {
    lock_guard<mutex> lockGuard(qLock);
    std::list<TCPRequest*>::iterator i = rxQueue.begin();
    Serial.print("looking for request\n");
    while (i != rxQueue.end()) {
      Serial.printf("rxID: %d - qID: %d\n", transactionID, (*i)->tcpHead.transactionID);
      if ((*i)->tcpHead.transactionID == transactionID) {
        // found it, handle it and stop iterating
        request = *i;
        i = rxQueue.erase(i);
        Serial.print("matched request\n");
        break;
      } else {
        ++i;
      }
    }
  } else {
    // TCP packet did not yield valid modbus response, abort function
    return;
  }

  // 3. we have a valid request and a valid response, call appropriate callback

  if (request) {
    // compare request with response
    Error error = SUCCESS;
    if (request->getFunctionCode() != response->getFunctionCode()) {
      error = FC_MISMATCH;
    } else if (request->getServerID() != response->getServerID()) {
      error = SERVER_ID_MISMATCH;
    } else {
      error = response->getError();
    }
    if (error == SUCCESS) {
      if (onData) {
        onData(response->getServerID(), response->getFunctionCode(), response->data(), response->len(), request->getToken());
      }
    } else {
      if (onError) {
        onError(response->getError(), request->getToken());
      }
    }
    delete request;
  }
  delete response;
}

void ModbusClientTCPasync::onPoll() {
  lock_guard<mutex> lockGuard(qLock);

  Serial.printf("Queue sizes: tx: %d rx: %d\n", txQueue.size(), rxQueue.size());

  // try to send whatever is waiting
  handleSendingQueue();

  // next check if timeout has struck for oldest request
  if (!rxQueue.empty()) {
    TCPRequest* request = rxQueue.front();
    if (millis() - request->target.timeout > MTA_timeout) {
      Serial.print("request timeouts\n");
      // oldest element timeouts, call onError and clean up
      if (onError) {
        // Handle timeout error
        onError(TIMEOUT, request->getToken());
      }
      delete request;
      rxQueue.pop_front();
    }
  }

  // if nothing happened during idle timeout, gracefully close connection
  if (millis() - MTA_lastActivity > MTA_idleTimeout) {
    disconnect();
  }
}

void ModbusClientTCPasync::handleSendingQueue() {
  // ATTENTION: This method does not have a lock guard.
  // Calling sites must assure shared resources are protected
  // by mutex.

  // try to send everything we have waiting
  std::list<TCPRequest*>::iterator i = txQueue.begin();
  while (i != txQueue.end()) {
    Serial.print("we are trying to send");
    // get the actual element
    TCPRequest* r = *i;
    if (send(r)) {
      // after sending, update timeout value, add to other queue and remove from this queue
      r->target.timeout = millis();
      rxQueue.push_back(r);      // push request to other queue
      i = txQueue.erase(i);  // remove from toSend queue and point i to next request
      Serial.print("-ok\n");
    } else {
      // sending didn't succeed, try next request
      Serial.print("-nok\n");
      ++i;
    }
  }
}

bool ModbusClientTCPasync::send(TCPRequest* request) {
  // check if TCP client is able to send
  if (MTA_client.space() > request->len() + 6) {
    // tcpHead is not yet in MSB order:
    uint8_t head[6];
    if (makeHead(head, 6, request->tcpHead.transactionID, request->tcpHead.protocolID, request->tcpHead.len)) {
      // Write TCP header first
      MTA_client.add(reinterpret_cast<char*>(head), 6);
      // Request comes next
      MTA_client.add(reinterpret_cast<const char*>(request->data()), request->len());
      // done
      MTA_client.send();
      // reset idle timeout
      MTA_lastActivity = millis();
      return true;
    }
  }
  return false;
}

TCPResponse* ModbusClientTCPasync::errorResponse(Error e, TCPRequest *request) {
  TCPResponse *errResponse = new TCPResponse(3);
  
  errResponse->add(request->getServerID());
  errResponse->add(static_cast<uint8_t>(request->getFunctionCode() | 0x80));
  errResponse->add(static_cast<uint8_t>(e));
  errResponse->tcpHead.transactionID = request->tcpHead.transactionID;
  errResponse->tcpHead.protocolID = request->tcpHead.protocolID;
  errResponse->tcpHead.len = 3;

  return errResponse;
}
