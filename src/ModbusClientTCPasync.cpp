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
  queue(),
  currentRequest(nullptr),
  MTA_client(),
  MTA_lastTarget(IPAddress(0, 0, 0, 0), 0, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MTA_target(IPAddress(0, 0, 0, 0), 0, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MTA_defaultTimeout(DEFAULTTIMEOUT),
  MTA_defaultInterval(TARGETHOSTINTERVAL),
  MTA_qLimit(queueLimit),
  MTA_lastActivity(0),
  MTA_state(DISCONNECTED),
  timer()
    {
      // attach all handlers on async tcp events
      MTA_client.onConnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onConnected(); }, this);
      MTA_client.onDisconnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onDisconnected(); }, this);
      MTA_client.onError([](void* i, AsyncClient* c, int8_t error) { (static_cast<ModbusClientTCPasync*>(i))->onACError(c, error); }, this);
      MTA_client.onData([](void* i, AsyncClient* c, void* data, size_t len) { (static_cast<ModbusClientTCPasync*>(i))->onPacket(static_cast<uint8_t*>(data), len); }, this);
      //MTA_client.onPoll([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onPoll(); }, this);

      // disable nagle algorithm ref Modbus spec
      MTA_client.setNoDelay(true);
    }

ModbusClientTCPasync::ModbusClientTCPasync(IPAddress host, uint16_t port, uint16_t queueLimit) :
  ModbusClient(),
  queue(),
  currentRequest(nullptr),
  MTA_client(),
  MTA_lastTarget(IPAddress(0, 0, 0, 0), 0, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MTA_target(host, port, DEFAULTTIMEOUT, TARGETHOSTINTERVAL),
  MTA_defaultTimeout(DEFAULTTIMEOUT),
  MTA_defaultInterval(TARGETHOSTINTERVAL),
  MTA_qLimit(queueLimit),
  MTA_lastActivity(0),
  MTA_state(DISCONNECTED),
  timer()
    {
      // attach all handlers on async tcp events
      MTA_client.onConnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onConnected(); }, this);
      MTA_client.onDisconnect([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onDisconnected(); }, this);
      MTA_client.onError([](void* i, AsyncClient* c, int8_t error) { (static_cast<ModbusClientTCPasync*>(i))->onACError(c, error); }, this);
      MTA_client.onData([](void* i, AsyncClient* c, void* data, size_t len) { (static_cast<ModbusClientTCPasync*>(i))->onPacket(static_cast<uint8_t*>(data), len); }, this);
      //MTA_client.onPoll([](void* i, AsyncClient* c) { (static_cast<ModbusClientTCPasync*>(i))->onPoll(); }, this);

      // disable nagle algorithm ref Modbus spec
      MTA_client.setNoDelay(true);
    }

// Destructor: clean up queue, task etc.
ModbusClientTCPasync::~ModbusClientTCPasync() {
  // Clean up all requests
  while (!queue.empty()) {
    delete queue.front();
    queue.pop();
  }
  if (currentRequest) delete currentRequest;
  // force close client
  MTA_client.close(true);
}

// optionally manually connect to modbus server. Otherwise connection will be made upon first request
void ModbusClientTCPasync::connect() {
  if (MTA_state != DISCONNECTED) {
    LOG_W("Client not disconnected\n");
    return;
  }

  LOG_D("connecting\n");
  connectUnlocked();
}

// manually disconnect from modbus server.
void ModbusClientTCPasync::disconnect(bool force) {
  if (MTA_state != CONNECTED && !force) {
    LOG_W("Client not connected\n");
    return;
  }

  LOG_D("disconnecting\n");
  MTA_state = DISCONNECTING;
  MTA_client.close(force);
}

// Set default timeout value (and interval)
void ModbusClientTCPasync::setTimeout(uint32_t timeout, uint32_t interval) {
  MTA_defaultTimeout = timeout;
  MTA_defaultInterval = interval;
}

// Switch target host (if necessary)
bool ModbusClientTCPasync::setTarget(IPAddress host, uint16_t port, uint32_t timeout, uint32_t interval) {
  MTA_target.host = host;
  MTA_target.port = port;
  MTA_target.timeout = timeout ? timeout : MTA_defaultTimeout;
  MTA_target.interval = interval ? interval : MTA_defaultInterval;
  LOG_D("Target set: %d.%d.%d.%d:%d\n", host[0], host[1], host[2], host[3], port);
  if (MTA_target.host == MTA_lastTarget.host && MTA_target.port == MTA_lastTarget.port) return false;
  return true;
}

// Remove all pending request from queue
void ModbusClientTCPasync::clearQueue()
{
  LOCK_GUARD(lock1, qLock);
  // Delete all elements from queues
  while (!queue.empty()) {
    delete queue.front();
    queue.pop_front();
  }
}

// Base addRequest for preformatted ModbusMessage and last set target
Error ModbusClientTCPasync::addRequestM(ModbusMessage msg, uint32_t token) {
  Error rc = SUCCESS;        // Return value

  // Add it to the queue, if valid
  if (msg) {
    // Queue add successful?
    if (!addToQueue(token, msg)) {
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
    if (!addToQueue(token, msg, true)) {
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
bool ModbusClientTCPasync::addToQueue(int32_t token, ModbusMessage request, bool syncReq) {
  // Did we get one?
  if (request) {
    if (queue.size() < MTA_qLimit) {
      HEXDUMP_V("Enqueue", request.data(), request.size());
      RequestEntry *re = new RequestEntry(token, request, MTA_target, syncReq);
      if (!re) return false;  //TODO: proper error returning in case allocation fails
      // inject proper transactionID
      re->head.transactionID = messageCount++;
      re->head.len = request.size();
      queue.push(re);
      if (MTA_state == DISCONNECTED) {
        connectUnlocked();
      } else {
        // TODO(bertmelis): check if we can send immediately
      }
      return true;
    }
    LOG_E("queue is full\n");
  }
  return false;
}

void ModbusClientTCPasync::connectUnlocked() {
  // get current request if not set by other methods
  if (getNextRequest()) {
    MTA_lastTarget = currentRequest->target;
  }

  TargetHost t(IPAddress(0, 0, 0, 0), 0, DEFAULTTIMEOUT, TARGETHOSTINTERVAL);
  if (MTA_lastTarget == t) {
    LOG_E("No target to connect to\n");
  }

  MTA_state = CONNECTING;
  LOG_D("Connecting to : %d.%d.%d.%d:%d\n", MTA_lastTarget.host[0], MTA_lastTarget.host[1], MTA_lastTarget.host[2], MTA_lastTarget.host[3], MTA_lastTarget.port);
  if (!MTA_client.connect(MTA_lastTarget.host, MTA_lastTarget.port)) {
    MTA_state = RETRY_CONNECT;
  }
  timer.attach_ms(POLL_FREQ, onPollStatic, (void*)this);
}

void ModbusClientTCPasync::onConnected() {
  LOG_D("connected\n");
  MTA_state = CONNECTED;
  MTA_lastActivity = millis() - MTA_lastTarget.interval - 1;  // send first request immediately after connecting
  handleCurrentRequest();
}

void ModbusClientTCPasync::onDisconnected() {
  LOG_D("disconnected\n");

  timer.detach();

  // Are we changing to to targethost?
  if (MTA_state == CHANGE_TARGET) {
    MTA_lastTarget = currentRequest->target;
    connectUnlocked();
    return;
  }

  // host disconnected while busy
  if (currentRequest) {
    respond(IP_CONNECTION_FAILED, nullptr);
  }

  MTA_state = DISCONNECTED;

  // Still work to do?
  if (getNextRequest()) {
    MTA_lastTarget = currentRequest->target;
    connectUnlocked();
  }
}


void ModbusClientTCPasync::onACError(AsyncClient* c, int8_t error) {
  // onDisconnect will alse be called, so nothing to do here
  LOG_W("TCP error: %s\n", c->errorToString(error));
}

void ModbusClientTCPasync::onPacket(uint8_t* data, size_t length) {
  // We assume one full Modbus packet is received in one TCP packet
  LOG_D("packet received (len:%u)\n", length);
  // reset idle timeout
  MTA_lastActivity = millis();
  ModbusMessage* response = nullptr;
  Error error = SUCCESS;
  LOG_D("packet received (len:%d)\n", length);
  HEXDUMP_V("Response packet", data, length);
  uint16_t transactionID = 0;
  uint16_t protocolID = 0;
  uint16_t messageLength = 0;
  bool isOkay = false;

  // 1. Check if we're actually expecting a packet
  if (!currentRequest) {
    LOG_W("No request waiting for response\n");
    return;
  }
  if (length) {
    LOG_D("parsing (len:%u)\n", length + 1);
  }
  while (length > 0) {
    RequestEntry* request = nullptr;
    ModbusMessage* response = nullptr;
    uint16_t transactionID = 0;
    bool isOkay = false;

    // 1. Check for valid modbus message

    // MBAP header is 6 bytes, we can't do anything with less
    // total message should fit MBAP plus remaining bytes (in data[4], data[5])
    if (length > 6) {
      transactionID = (data[0] << 8) | data[1];
      uint16_t protocolID = (data[2] << 8) | data[3];
      uint16_t messageLength = (data[4] << 8) | data[5];
      if (protocolID == 0 &&
        length >= (uint32_t)messageLength + 6 &&
        messageLength < 256) {
        response = new ModbusMessage(messageLength);
        response->add(&data[6], messageLength);
        LOG_D("packet validated (len:%d)\n", messageLength);

        // on next iteration: adjust remaining length and pointer to data
        length -= 6 + messageLength;
        data += 6 + messageLength;
        isOkay = true;
      }
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
        LOG_E("Could not create response packet\n");
        return;
      }
      response->add(&data[6], messageLength);
      isOkay = true;
    }
  }

  // 3. Match response with request
  if (!isOkay || currentRequest->head.transactionID != transactionID) {
    // invalid packet, abort function
    // TODO: do we need to return this error to the user?
    LOG_W("Packet invalid or does not match request\n");
    delete response;
    return;
  }
  if (currentRequest->msg.getFunctionCode() != (response->getFunctionCode() & 0x7F)) {
    error = FC_MISMATCH;
  } else if (currentRequest->msg.getServerID() != response->getServerID()) {
    error = SERVER_ID_MISMATCH;
  } else {
    error = response->getError();
  }
  if (error != SUCCESS) {
    LOCK_GUARD(errorCntLock, countAccessM);
    errorCount++;
  }

  // 4. Respond to API and cleanup
  respond(response->getError(), response);
  MTA_state = CONNECTED;

  // 5. check if we have to send the next request
  if (getNextRequest()) {
    handleCurrentRequest();
  }
}

void ModbusClientTCPasync::onPoll() {
  if (MTA_state == RETRY_CONNECT) {
    connectUnlocked();
    return;
  }

  // check if timeout for connecting or responding has struck
  if (currentRequest && millis() - currentRequest->sentTime > MTA_target.timeout) {
    if (MTA_state == CONNECTING) {
      LOG_D("connect timeouts (now:%lu-sent:%u)\n", millis(), currentRequest->sentTime);
      timer.detach();  // stop calling onPoll and wait for onDisconnect to be called
      // ESPAsyncTCP/AsyncTCP doesn't allow aborting a not yet made connection.
      // This statement therefore doesn't do anything and we have to wait for the hardcoded 30s timeout.
      MTA_client.abort();
      //onDisconnected();  // Don't manually call this. eModbus will create a new connection and AsyncTCP will loose the old reference.
                           // High risk of memleak + reaching connection limit
      return;
    } else if (MTA_state == BUSY) {
      LOG_D("request timeouts (now:%lu-sent:%u)\n", millis(), currentRequest->sentTime);
      MTA_state = CONNECTED;
      respond(TIMEOUT, nullptr);
    }
  }

  // still work to be done?
  if (getNextRequest()) {
    handleCurrentRequest();
  }
}

void ModbusClientTCPasync::onPollStatic(void* mta) {
  static_cast<ModbusClientTCPasync*>(mta)->onPoll();
}

bool ModbusClientTCPasync::getNextRequest() {
  // ATTENTION: This method does not have a lock guard.
  // Calling sites must assure shared resources are protected
  // by mutex.
  if (!currentRequest && !queue.empty()) {
    LOG_D("Getting next request from queue\n");
    currentRequest = queue.front();
    currentRequest->sentTime = millis();
    queue.pop();
    return true;
  }
  return false;
}

void ModbusClientTCPasync::handleCurrentRequest() {
  // ATTENTION: This method does not have a lock guard.
  // Calling sites must assure shared resources are protected
  // by mutex.

  // AsyncTCP requirement
  if (!MTA_client.connected()) {
    LOG_E("Client not connected\n");
    return;
  }

  // Check if we're not busy and have something to do
  if (MTA_state == CONNECTED && currentRequest) {

    // 1. Do we need to change the target host?
    if (MTA_lastTarget != currentRequest->target) {
      LOG_V("Changing target\n");
      MTA_state = CHANGE_TARGET;
      MTA_client.close();
      return;
    }

    // 2. Mind interval when sending next request
    if (millis() - MTA_lastActivity < MTA_target.interval) {
      LOG_D("Waiting interval before sending\n");
      return;
    }

    // 3. Try to send (a complete packet)
    if (MTA_client.space() > ((uint32_t)currentRequest->msg.size() + 6)) {
      // Write TCP header first
      MTA_client.add(reinterpret_cast<const char *>((const uint8_t *)(currentRequest->head)), 6, ASYNC_WRITE_FLAG_COPY | ASYNC_WRITE_FLAG_MORE);
      // Request comes next
      MTA_client.add(reinterpret_cast<const char*>(currentRequest->msg.data()), currentRequest->msg.size(), ASYNC_WRITE_FLAG_COPY);
      // done
      MTA_client.send();
      LOG_D("Sending request\n");
      HEXDUMP_V("Request packet", currentRequest->msg.data(), currentRequest->msg.size());
      MTA_state = BUSY;
    }
  }
}

void ModbusClientTCPasync::respond(Error error, ModbusMessage* response) {
  LOG_V("Forwarding response to API\n");
  // response is not always set at calling sites
  if (!response) {
    response = new ModbusMessage();
    if (!response) {
      LOG_E("Could not create response\n");
      delete currentRequest;
      currentRequest = nullptr;
      return;
    }
    response->setError(currentRequest->msg.getServerID(), currentRequest->msg.getFunctionCode(), error);
  }

  if (currentRequest->isSyncRequest) {
    LOCK_GUARD(sL ,syncRespM);
    syncResponse[currentRequest->token] = *response;
  } else if (onResponse) {
    onResponse(*response, currentRequest->token);
  } else {
    if (error == SUCCESS) {
      if (onData) {
        onData(*response, currentRequest->token);
      }
    } else {
      if (onError) {
        onError(response->getError(), currentRequest->token);
      }
    }
  }
  delete currentRequest;
  currentRequest = nullptr;
  delete response;
}
