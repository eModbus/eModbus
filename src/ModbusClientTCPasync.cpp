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
  MT_lastActivity(0),
  MT_defaultInterval(TARGETHOSTINTERVAL),
  MTA_qLimit(queueLimit),
  MTA_state(DISCONNECTED)
    {
      // attach all handlers on async tcp events
      MTA_client.onConnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onConnected(); }, this);
      MTA_client.onDisconnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onDisconnected(); }, this);
      MTA_client.onError([](void* i, AsyncClient* c, int8_t error) { (static_cast<ModbusClientTCPasync*>(i))->onACError(c, error); }, this);
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
  MT_lastActivity(0),
  MT_defaultInterval(TARGETHOSTINTERVAL),
  MTA_qLimit(queueLimit),
  MTA_state(DISCONNECTED)
    {
      MTA_client.onConnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onConnected(); }, this);
      MTA_client.onDisconnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onDisconnected(); }, this);
      MTA_client.onError([](void* i, AsyncClient* c, int8_t error) { (static_cast<ModbusClientTCPasync*>(i))->onACError(c, error); }, this);
      MTA_client.onData([](void* i, AsyncClient* c, void* data, size_t len) { (static_cast<ModbusClientTCPasync*>(i))->onPacket(static_cast<uint8_t*>(data), len); }, this);
      MTA_client.onPoll([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onPoll(); }, this);
    }

ModbusClientTCPasync::~ModbusClientTCPasync() {
    LOCK_GUARD(lock, aoLock);
    while (!requests.empty()) {
      requests.pop();
    }
  // client will disconnect on destruction, avoid calling onDisconnect
  MTA_client.onDisconnect(nullptr);
}

void ModbusClientTCPasync::connect() {
  LOCK_GUARD(lock, aoLock);
  // Manual action, only allow when disconnected
  if (MTA_state == DISCONNECTED) {
    connectUnlocked();
  } else {
    LOG_W("Could not connect: client not disconnected");
  }
}

void ModbusClientTCPasync::connectUnlocked() {
  MT_lastTarget = requests.front()->target;
  LOG_D("Target connecting (%d.%d.%d.%d:%d).\n", MT_lastTarget.host[0], MT_lastTarget.host[1], MT_lastTarget.host[2], MT_lastTarget.host[3], MT_lastTarget.port);
  MTA_state = CONNECTING;
  MTA_client.connect(MT_lastTarget.host, MT_lastTarget.port);
}

void ModbusClientTCPasync::disconnect(bool force) {
  LOCK_GUARD(lock, aoLock);
  disconnectUnlocked(force);
}

void ModbusClientTCPasync::disconnectUnlocked(bool force) {
  MTA_state = DISCONNECTING;
  LOG_D("disconnecting\n");
  MTA_client.close(force);
}

bool ModbusClientTCPasync::setTarget(IPAddress host, uint16_t port, uint32_t timeout, uint32_t interval) {
  MT_target.host = host;
  MT_target.port = port;
  MT_target.timeout = timeout ? timeout : MT_defaultTimeout;
  MT_target.interval = interval ? interval : MT_defaultInterval;
  LOG_D("Target set: %d.%d.%d.%d:%d\n", host[0], host[1], host[2], host[3], port);
  if (MT_target.host == MT_lastTarget.host && MT_target.port == MT_lastTarget.port) return false;
  return true;
}

void ModbusClientTCPasync::setTimeout(uint32_t timeout, uint32_t interval) {
  MT_target.timeout = timeout;
  MT_target.interval = interval;
}

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

// Base syncRequest follows the same pattern as async but returning a ModbusMessage
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

bool ModbusClientTCPasync::addToQueue(int32_t token, ModbusMessage request, TargetHost target, bool syncReq) {
  LOG_D("Queue size: %d\n", (uint32_t)requests.size());
  HEXDUMP_D("Enqueue", request.data(), request.size());
  RequestEntry *re = nullptr;
  bool success = false;
  // Did we get one?
  if (request) {
    LOCK_GUARD(lock, aoLock);
    if (requests.size() < MTA_qLimit) {
      re = new RequestEntry(token, request, target, syncReq);
      if (!re) {
        LOG_E("Could not create request entry");
        return false;  //TODO: proper error returning in case allocation fails
      }
      // inject transactionID
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
      MT_target = re->target;
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
  LOCK_GUARD(lock, aoLock);
  LOG_D("connected\n");
  MTA_state = CONNECTED;
  MTA_client.setNoDelay(true);
  MT_lastActivity = millis() - MT_target.interval - 1;  // send first request immediately after connecting
  handleSendingQueue();
  // from now on onPoll will be called every 500 msec
}

void ModbusClientTCPasync::onDisconnected() {
  // 1. Check if we're in the process of changing the target.
  //    In this case, there is (at least) one request in the queue
  //    and onDisconnected is called from mutex-protected code.
  if (MTA_state == CHANGE_TARGET) {
    connectUnlocked();
    return;
  }

  RequestEntry* request = nullptr;
  Error error = IP_CONNECTION_FAILED;
  bool doRespond = false;

  {  // start lock scope
  LOCK_GUARD(lock, aoLock);
  LOG_D("disconnected\n");

  // 2. Set state and return error to user
  
  if (MTA_state == BUSY) {  // do not test for empty queue: if busy, there should be at least one request pending
    request = requests.front();
    doRespond = true;
    requests.pop();
  }

  // 3. Connect again using next request
  if (!requests.empty()) {
    connectUnlocked();
  }

  }  // end lock scope
  MTA_state = DISCONNECTED;
  if (doRespond) {
    respond(error, request, nullptr);
  }
}

void ModbusClientTCPasync::onACError(AsyncClient* c, int8_t error) {
  // AsyncTCP calls both onACError and OnDisconnect on error.
  // Use this method only for logging
  LOG_W("TCP error: %s\n", c->errorToString(error));
}

void ModbusClientTCPasync::onPacket(uint8_t* data, size_t length) {
  // We assume one full Modbus packet is received in one TCP packet

  RequestEntry* request = nullptr;
  ModbusMessage* response = nullptr;
  Error error = SUCCESS;
  bool doRespond = false;

  {  // start lock scope
  LOCK_GUARD(lock1, aoLock);

  LOG_D("packet received (len:%d)\n", length);
  HEXDUMP_V("Response packet", data, length);
  uint16_t transactionID = 0;
  uint16_t protocolID = 0;
  uint16_t messageLength = 0;
  bool isOkay = false;

  // 1. Check if we're actually expecting a packet
  if (requests.empty()) {
    LOG_W("No request waiting for response");
    return;
  } else {
    request = requests.front();
  }

  // 2. Check if packet is valid modbus message

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

  // 3. Match response with request

  if (!isOkay || request->head.transactionID != transactionID) {
    // invalid packet, abort function
    // TODO: do we need to return this error to the user?
    LOG_W("Packet invalid or does not match request\n");
    delete response;
    return;
  }

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
  doRespond = true;

  // 4. cleanup and reset state
  //    request and response are deleted in respond()

  MTA_state = CONNECTED;
  requests.pop();

  // 5. check if we have to send the next request
  MT_lastActivity = millis();
  handleSendingQueue();
  }  // end lock scope

  if (doRespond) {
    respond(response->getError(), request, response);
  }
}

void ModbusClientTCPasync::onPoll() {
  bool doRespond = false;
  RequestEntry* request = nullptr;
  Error error = SUCCESS;
  {  // start lock scope
  LOCK_GUARD(lock, aoLock);

  // try to send whatever is waiting
  handleSendingQueue();

  // when waiting for a response, check if timeout has struck
  if (MTA_state == BUSY) {  // do not test for empty queue: if busy, there should be at least one request pending
    request = requests.front();
    if (millis() - request->sentTime > MT_target.timeout) {
      LOG_D("request timeouts (now:%lu-sent:%u)\n", millis(), request->sentTime);
      error = TIMEOUT;
      doRespond = true;
      MTA_state = CONNECTED;
      requests.pop();
    }
  }
  }  // end lock scope
  if (doRespond) {
    respond(error, request, nullptr);
  }
}

void ModbusClientTCPasync::handleSendingQueue() {
  // ATTENTION: This method does not have a lock guard.
  // Calling sites must assure shared resources are protected
  // by mutex.

  // 1. If we're not busy, check if we have to switch target
  if (MTA_state == CONNECTED && !requests.empty()) {
    RequestEntry* re = requests.front();
    if (MT_lastTarget != re->target) {
      LOG_V("Switching target\n");
      MTA_state = CHANGE_TARGET;
      MTA_client.close();
      return;
    }

  // 2. Mind interval when sending next request
  if (millis() - MT_lastActivity < MT_target.interval) {
    return;
  }

  // 3. We have a request waiting and are connected, try to send (a complete packet)
    if (MTA_client.space() > ((uint32_t)re->msg.size() + 6)) {
      // Write TCP header first
      MTA_client.add(reinterpret_cast<const char *>((const uint8_t *)(re->head)), 6, ASYNC_WRITE_FLAG_COPY | ASYNC_WRITE_FLAG_MORE);
      // Request comes next
      MTA_client.add(reinterpret_cast<const char*>(re->msg.data()), re->msg.size(), ASYNC_WRITE_FLAG_COPY);
      // done
      MTA_client.send();
      re->sentTime = millis();
      HEXDUMP_V("Request packet", re->msg.data(), re->msg.size());
      MTA_state = BUSY;
    }
  }
}

void ModbusClientTCPasync::respond(Error error, RequestEntry* request, ModbusMessage* response) {
  LOG_V("Forwarding response to API\n");
  // response is not always set at calling sites whereas request is
  if (!response) {
    response = new ModbusMessage();
    if (!response) {
      LOG_E("Could not create response\n");
      delete request;  // requests is always passed
      return;
    }
    response->setError(request->msg.getServerID(), request->msg.getFunctionCode(), error);
  }

  if (request->isSyncRequest) {
    LOCK_GUARD(sL ,syncRespM);
    syncResponse[request->token] = *response;
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
  delete request;
  delete response;
}
