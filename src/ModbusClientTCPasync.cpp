// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
//               MIT license - see license.md for details
// =================================================================================================
#include "ModbusClientTCPasync.h"
#define LOCAL_LOG_LEVEL LOG_LEVEL_VERBOSE
// #undef LOCAL_LOG_LEVEL
#include "Logging.h"

ModbusClientTCPasync::ModbusClientTCPasync(uint16_t queueLimit) :
  ModbusClient(),
  requests(),
  MTA_client(),
  MT_lastTarget(IPAddress(0, 0, 0, 0), 0, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MT_target(IPAddress(0, 0, 0, 0), 0, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MT_defaultTimeout(DEFAULTTIMEOUT),
  MT_defaultInterval(TARGETHOSTINTERVAL),
  MTA_qLimit(queueLimit),
  MTA_state(DISCONNECTED)
    {
      // attach all handlers on async tcp events
      MTA_client.onConnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onConnected(); }, this);
      MTA_client.onDisconnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onDisconnected(); }, this);
      MTA_client.onError([](void* i, AsyncClient* c, int8_t error) { (static_cast<ModbusClientTCPasync*>(i))->onACError(c, error); }, this);
      // MTA_client.onTimeout([](void* i, AsyncClient* c, uint32_t time) { (static_cast<ModbusClientTCPasync*>(i))->onTimeout(time); }, this);
      // MTA_client.onAck([](void* i, AsyncClient* c, size_t len, uint32_t time) { (static_cast<ModbusClientTCPasync*>(i))->onAck(len, time); }, this);
      MTA_client.onData([](void* i, AsyncClient* c, void* data, size_t len) { (static_cast<ModbusClientTCPasync*>(i))->onPacket(static_cast<uint8_t*>(data), len); }, this);
      MTA_client.onPoll([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onPoll(); }, this);
    }

ModbusClientTCPasync::ModbusClientTCPasync(IPAddress host, uint16_t port, uint16_t queueLimit) :
  ModbusClient(),
  requests(),
  MTA_client(),
  MT_lastTarget(IPAddress(0, 0, 0, 0), 0, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MT_target(host, port, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MT_defaultTimeout(DEFAULTTIMEOUT),
  MT_defaultInterval(TARGETHOSTINTERVAL),
  MTA_qLimit(queueLimit),
  MTA_state(DISCONNECTED)
    {
      // attach all handlers on async tcp events
      MTA_client.onConnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onConnected(); }, this);
      MTA_client.onDisconnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onDisconnected(); }, this);
      MTA_client.onError([](void* i, AsyncClient* c, int8_t error) { (static_cast<ModbusClientTCPasync*>(i))->onACError(c, error); }, this);
      // MTA_client.onTimeout([](void* i, AsyncClient* c, uint32_t time) { (static_cast<ModbusClientTCPasync*>(i))->onTimeout(time); }, this);
      // MTA_client.onAck([](void* i, AsyncClient* c, size_t len, uint32_t time) { (static_cast<ModbusClientTCPasync*>(i))->onAck(len, time); }, this);
      MTA_client.onData([](void* i, AsyncClient* c, void* data, size_t len) { (static_cast<ModbusClientTCPasync*>(i))->onPacket(static_cast<uint8_t*>(data), len); }, this);
      MTA_client.onPoll([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onPoll(); }, this);
    }

// Destructor: clean up queue, task etc.
ModbusClientTCPasync::~ModbusClientTCPasync() {
  // Clean up queue
  {
    // Safely lock access
    LOCK_GUARD(lock, aoLock);
    // Delete all elements from queue
    while (!requests.empty()) {
      requests.pop();
    }
  }
  // force close client
  MTA_client.close(true);
}

// optionally manually connect to modbus server. Otherwise connection will be made upon first request
void ModbusClientTCPasync::connect() {
  LOCK_GUARD(lock, aoLock);
  // only connect if disconnected
  if (MTA_state == DISCONNECTED) {
    LOG_D("Target connecting (%d.%d.%d.%d:%d).\n", MT_target.host[0], MT_target.host[1], MT_target.host[2], MT_target.host[3], MT_target.port);
    MTA_state = CONNECTING;
    MTA_client.connect(MT_target.host, MT_target.port);
  }
}

// manually disconnect from modbus server.
void ModbusClientTCPasync::disconnect(bool force) {
  {
  LOCK_GUARD(lock, aoLock);
  MTA_state = DISCONNECTING;
  LOG_D("disconnecting\n");
  }
  MTA_client.close(force);
}

// Switch target host (if necessary)
// Return true, if host/port is different from last host/port used
bool ModbusClientTCPasync::setTarget(IPAddress host, uint16_t port, uint32_t timeout, uint32_t interval) {
  MT_target.host = host;
  MT_target.port = port;
  MT_target.timeout = timeout ? timeout : MT_defaultTimeout;
  MT_target.interval = interval ? interval : MT_defaultInterval;
  LOG_D("Target set: %d.%d.%d.%d:%d\n", host[0], host[1], host[2], host[3], port);
  if (MT_target.host == MT_lastTarget.host && MT_target.port == MT_lastTarget.port) return false;
  return true;
}

// Set timeout value
void ModbusClientTCPasync::setTimeout(uint32_t timeout, uint32_t interval) {
  MT_target.timeout = timeout;
  MT_target.interval = interval;
}

// Base addRequest for preformatted ModbusMessage and last set target
Error ModbusClientTCPasync::addRequestM(ModbusMessage msg, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Add it to the queue, if valid
  if (msg) {
    // Queue add successful?
    if (!addToQueue(token, msg, MT_target)) {
      // No. Return error after deleting the allocated request.
      rc = REQUEST_QUEUE_FULL;
    }
  }

  LOG_D("Add TCP request result: %02X\n", rc);
  return rc;
}

// Base syncRequest follows the same pattern
ModbusMessage ModbusClientTCPasync::syncRequestM(ModbusMessage msg, uint32_t token) {
  ModbusMessage response;

  if (msg) {
    // Queue add successful?
    if (!addToQueue(token, msg, MT_target, true)) {
      // No. Return error after deleting the allocated request.
      response.setError(msg.getServerID(), msg.getFunctionCode(), REQUEST_QUEUE_FULL);
    } else {
      // Request is queued - wait for the result.
      response = waitSync(msg.getServerID(), msg.getFunctionCode(), token);
    }
  } else {
    response.setError(msg.getServerID(), msg.getFunctionCode(), EMPTY_MESSAGE);
  }
  return response;
}

// addToQueue: send freshly created request to queue
bool ModbusClientTCPasync::addToQueue(int32_t token, ModbusMessage request, TargetHost target, bool syncReq) {
  // Did we get one?
  LOG_D("Queue size: %d\n", (uint32_t)requests.size());
  HEXDUMP_D("Enqueue", request.data(), request.size());
  bool success = false;
  if (request) {
    LOCK_GUARD(lock, aoLock);
    if (requests.size() < MTA_qLimit) {
      RequestEntry *re = new RequestEntry(token, request, target, syncReq);
      if (!re) {
        LOG_E("Could not create request entry");
        return false;  //TODO: proper error returning in case allocation fails
      }
      // inject proper transactionID
      re->head.transactionID = messageCount++;
      re->head.len = request.size();
      requests.push(re);
      success = true;
    } else {
      LOG_E("queue is full\n");
    }
  }
  if (success) {
    if (MTA_state == DISCONNECTED) {
      connect();
    } else if (MTA_state == CONNECTED) {
      LOCK_GUARD(lock, aoLock);
      handleSendingQueue();
    }
    return true;
  }
  return false;
}

void ModbusClientTCPasync::onConnected() {
  LOG_D("connected\n");
  LOCK_GUARD(lock, aoLock);
  MTA_state = CONNECTED;
  MTA_client.setNoDelay(true);
  // from now on onPoll will be called every 500 msec
  handleSendingQueue();
}

void ModbusClientTCPasync::onDisconnected() {
  LOG_D("disconnected\n");
  {
  LOCK_GUARD(lock, aoLock);
  MTA_state = DISCONNECTED;

  // calling errorcode on request
  if (!requests.empty()) {
    RequestEntry* r = requests.front();
    if (onError) {
      onError(IP_CONNECTION_FAILED, r->token);
    }
    delete r;
    requests.pop();
    }
  }  // unscope lock_guard to enable reconnecting

  // try again with next request
  if (!requests.empty()) connect();
}

void ModbusClientTCPasync::onACError(AsyncClient* c, int8_t error) {
  // onDisconnect will alse be called, so nothing to do here
  LOG_W("TCP error: %s\n", c->errorToString(error));
}

void ModbusClientTCPasync::onPacket(uint8_t* data, size_t length) {
  // We assume one full Modbus packet is received in one TCP packet
  LOG_D("packet received (len:%d)\n", length);
  HEXDUMP_V("Response packet", data, length);

  LOCK_GUARD(lock1, aoLock);
  RequestEntry* request = requests.front();
  ModbusMessage* response = nullptr;
  uint16_t transactionID = 0;
  uint16_t protocolID = 0;
  uint16_t messageLength = 0;
  bool isOkay = false;

  if (!request) {
    LOG_V("No request waiting for response");
    return;
  }

  // 1. Check for valid modbus message

  // MBAP header is 6 bytes, we can't do anything with less
  // total message should fit MBAP plus remaining bytes (in data[4], data[5])
  if (length > 6) {
    transactionID = (data[0] << 8) | data[1];
    protocolID = (data[2] << 8) | data[3];
    messageLength = (data[4] << 8) | data[5];
    if (protocolID == 0 &&
      length >= (uint32_t)messageLength + 6 &&
      messageLength < 256) {
      response = new ModbusMessage(messageLength);
      if (!response) {
        LOG_E("Could not create response packet");
        return;
      }
      response->add(&data[6], messageLength);
      isOkay = true;
    }
  }

  // 2. Match response with request
  if (!isOkay || request->head.transactionID != transactionID) {
    // invalid packet, abort function
    LOG_W("Packet invalid or does not match request\n");
    delete response;
    return;
  }

  // 3. we have a valid request and a valid response, call appropriate callback
  // In depth comparison request with response
  Error error = SUCCESS;
  if (request->msg.getFunctionCode() != (response->getFunctionCode() & 0x7F)) {
    error = FC_MISMATCH;
  } else if (request->msg.getServerID() != response->getServerID()) {
    error = SERVER_ID_MISMATCH;
  } else {
    error = response->getError();
  }

  if (error != SUCCESS) {
    LOCK_GUARD(errorCntLock, countAccessM);
    errorCount++;
  }

  if (request->isSyncRequest) {
    {
      LOCK_GUARD(sL ,syncRespM);
      syncResponse[request->token] = *response;
    }
  } else if (onResponse) {
    onResponse(*response, request->token);
  } else {
    if (error == SUCCESS) {
      if (onData) {
        onData(*response, request->token);
      }
    } else {
      if (onError) {
        onError(response->getError(), request->token);
      }
    }
  }
  MTA_state = CONNECTED;
  delete request;
  requests.pop();
  delete response;

  // check if we have to send the next request
  handleSendingQueue();
}

void ModbusClientTCPasync::onPoll() {
  LOCK_GUARD(lock, aoLock);

  // try to send whatever is waiting
  handleSendingQueue();

  // next check if timeout has struck for oldest request
  if (MTA_state == BUSY && !requests.empty()) {
    RequestEntry* request = requests.front();
    if (millis() - request->sentTime > MT_target.timeout) {
      LOG_D("request timeouts (now:%lu-sent:%u)\n", millis(), request->sentTime);
      // oldest element timeouts, call onError and clean up
      if (onError) {
        // Handle timeout error
        onError(TIMEOUT, request->token);
      }
      delete request;
      requests.pop();
    }
  }
}

void ModbusClientTCPasync::handleSendingQueue() {
  // ATTENTION: This method does not have a lock guard.
  // Calling sites must assure shared resources are protected
  // by mutex.

  if (MTA_state == CONNECTED && !requests.empty()) {
    RequestEntry* re = requests.front();
    if (MTA_client.space() > ((uint32_t)re->msg.size() + 6)) {
      // Write TCP header first
      MTA_client.add(reinterpret_cast<const char *>((const uint8_t *)(re->head)), 6, ASYNC_WRITE_FLAG_COPY | ASYNC_WRITE_FLAG_MORE);
      // Request comes next
      MTA_client.add(reinterpret_cast<const char*>(re->msg.data()), re->msg.size(), ASYNC_WRITE_FLAG_COPY);
      // done
      MTA_client.send();
      re->sentTime = millis();
      LOG_D("request sent (msgid:%d)\n", re->head.transactionID);
      MTA_state = BUSY;
    }
  }
}
