// =================================================================================================
// ModbusClient: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================

#include "ModbusServerTCPasync.h"
#define LOCAL_LOG_LEVEL LOG_LEVEL_VERBOSE
// #undef LOCAL_LOG_LEVEL
#include "Logging.h"

ModbusServerTCPasync::mb_client::mb_client(ModbusServerTCPasync* s, AsyncClient* c) :
  server(s),
  client(c),
  lastActiveTime(millis()),
  currentRequest(),
  requestLength(0),
  currentResponse(nullptr),
  error(SUCCESS),
  outbox(),
  state(RCV1) {
    client->onData([](void* i, AsyncClient* c, void* data, size_t len) { (static_cast<mb_client*>(i))->onData(static_cast<uint8_t*>(data), len); }, this);
    client->onPoll([](void* i, AsyncClient* c) { (static_cast<mb_client*>(i))->onPoll(); }, this);
    client->onDisconnect([](void* i, AsyncClient* c) { (static_cast<mb_client*>(i))->onDisconnect(); }, this);
    client->setNoDelay(true);
    // request.reserve(256 + 6);  // reserve maximum size to avoid resizing on-the-fly
    // response.reserve(256 + 6);  // reserve maximum size to avoid resizing on-the-fly
}

ModbusServerTCPasync::mb_client::~mb_client() {
  delete client;  // will also close connection, if any
}

void ModbusServerTCPasync::mb_client::onData(uint8_t* data, size_t len) {
  lastActiveTime = millis();
  LOG_D("data len %d", len);

  size_t i = 0;
  while (i < len) {
    switch (state) {
    //  1. get minimal 8 bytes to move on
    case RCV1:
      currentRequest.push_back(data[i++]);
      if (currentRequest.size() == 8) state = VAL1;
      break;
    // 2. preliminary validation: protocol bytes and message length
    case VAL1:
      LOG_D("validation stage 1");
      error = SUCCESS;
      state = RCV2;
      currentResponse = new ResponseType();
      requestLength = ((data[4] << 8) | data[5]) + 6;
      LOG_D("expected length: %d", requestLength);
      // check modbus protocol
      if (currentRequest[2] != 0 || currentRequest[3] != 0) {
        error = TCP_HEAD_MISMATCH;
        LOG_D("invalid protocol");
      }
      // check max request size
      if (requestLength > 264) {  // 256 + ID(1) + FC(1) + MBAP(6) = 264
        error = PACKET_LENGTH_ERROR;
        LOG_D("max length error");
      }
      if (error != SUCCESS) {
        generateResponse(error, nullptr);
        addResponseToOutbox();
        state = RCV1;
        i = len;  // break out of while loop to discard data
      }
      break;
    // get data untill request is complete
    case RCV2:
      while (currentRequest.size() < requestLength) {
        currentRequest.push_back(data[i++]);
      }
      if (currentRequest.size() == requestLength) {
        LOG_D("request complete (len:%d)", currentRequest.size());
        state = VAL2;
      } 
      break;
    // request is complete, final validation
    case VAL2:
      LOG_D("validation stage 2");
      MBSworker callback = server->getWorker(currentRequest[6], currentRequest[7]);
      // check server id
      if (!server->isServerFor(currentRequest[6])) {
        LOG_D("invalid server id");
        generateResponse(INVALID_SERVER, nullptr);
      } else if (!callback) {  // check function code
        LOG_D("invalid fc");
        generateResponse(ILLEGAL_FUNCTION, nullptr);
      } else if (callback) {
        LOG_D("passing request to user API");
        // all is well
        ResponseType r = callback(currentRequest[6], currentRequest[7], currentRequest.size() - 8, currentRequest.data() + 8);
        // Process Response
        // One of the predefined types?
        if (r[0] == 0xFF && (r[1] == 0xF0 || r[1] == 0xF1 || r[1] == 0xF2 || r[1] == 0xF3)) {
          // Yes. Check it
          switch (r[1]) {
            case 0xF0: // NIL
              LOG_D("user response NIL");
              currentResponse->clear();
              break;
            case 0xF1: // ECHO
              LOG_D("user response ECHO");
              (*currentResponse) = currentRequest;
              break;
            case 0xF2: // ERROR
              LOG_D("user response ERROR");
              generateResponse(static_cast<Modbus::Error>(r[2]), nullptr);
              break;
            case 0xF3: // DATA
              LOG_D("user response DATA");
              generateResponse(SUCCESS, &r);
              break;
            default:   // Will not get here!
              break;
          }
        } else {
          // No. User provided data in free format
          LOG_D("user response [free form]");
          currentResponse->at(4) = (r.size() >> 8) & 0xFF;
          currentResponse->at(5) = r.size() & 0xFF;
          for (auto& byte : r) {
            currentResponse->push_back(byte);
          }
        }
      } else {
        LOG_E("VAL2 error");
      }
      addResponseToOutbox();
      state = RCV1;
    }  // end data parsing FSM
  }  // end while (len > 0)

  // try to send the response
  handleOutbox();
}

void ModbusServerTCPasync::mb_client::onPoll() {
  handleOutbox();
  if (millis() - lastActiveTime > server->idle_timeout) {
    LOG_D("client idle, closing");
    client->close();
  }
}

void ModbusServerTCPasync::mb_client::onDisconnect() {
  LOG_D("client disconnected");
  server->onClientDisconnect(this);
}

void ModbusServerTCPasync::mb_client::generateResponse(Modbus::Error e,  ResponseType* data) {
  LOG_D("generating response");
  currentResponse->push_back(currentRequest[0]);  // transaction id
  currentResponse->push_back(currentRequest[1]);  // transaction id
  currentResponse->push_back(0);                  // protocol byte
  currentResponse->push_back(0);                  // protocol byte
  if (e != SUCCESS) {
    currentResponse->push_back(0);                  // remaining length
    currentResponse->push_back(3);                  // remaining length
    currentResponse->push_back(currentRequest[6]);         // device id
    currentResponse->push_back(currentRequest[7] | 0x80);  // FC with high bit set to 1
    currentResponse->push_back(e);                  // errorcode
  } else {
    currentResponse->push_back((data->size() >> 8) & 0xFF);
    currentResponse->push_back(data->size() & 0xFF);
    currentResponse->push_back(currentRequest[6]);
    currentResponse->push_back(currentRequest[7]);
    for (auto byte = data->begin() + 2; byte < data->end(); ++byte) {
      currentResponse->push_back(*byte);
    }
  }
}

void ModbusServerTCPasync::mb_client::addResponseToOutbox() {
  LOG_D("adding response to outbox");
  if (currentResponse->size() > 0) { 
    std::lock_guard<std::mutex> lock(m);
    outbox.push(currentResponse);  // outbox owns the pointer now
    currentResponse = nullptr;
    currentRequest.clear();
  }
}

void ModbusServerTCPasync::mb_client::handleOutbox() {
  LOG_D("processing outbox");
  std::lock_guard<std::mutex> lock(m);
  while (!outbox.empty()) {
    ResponseType* r = outbox.front();
    if (r->size() <= client->space()) {
      LOG_D("sending (%d)", r->size());
      client->add(reinterpret_cast<const char*>(r->data()), r->size());
      client->send();
      delete r;
      outbox.pop();
    } else {
      return;
    }
  }
}

ModbusServerTCPasync::ModbusServerTCPasync() :
  server(nullptr),
  clients(),
  maxNoClients(5),
  idle_timeout(60000) {

}


ModbusServerTCPasync::~ModbusServerTCPasync() {
  stop();
  delete server;
}


uint16_t ModbusServerTCPasync::activeClients() {
  std::lock_guard<std::mutex> cntLock(m);
  return clients.size();
}


bool ModbusServerTCPasync::start(uint16_t port, uint8_t maxClients, uint32_t timeout, int coreID) {
  // don't restart if already running
  if (server) return false;
  
  maxNoClients = maxClients;
  idle_timeout = timeout;
  server = new AsyncServer(port);
  if (server) {
    server->setNoDelay(true);
    server->onClient([](void* i, AsyncClient* c) { (static_cast<ModbusServerTCPasync*>(i))->onClientConnect(c); }, this);
    server->begin();
    LOG_D("modbus server started");
    return true;
  }
  return false;
}

bool ModbusServerTCPasync::stop() {
  // stop server to prevent new clients connecting
  server->end();

  // now close existing clients
  std::lock_guard<std::mutex> cntLock(m);
  while (!clients.empty()) {
    // prevent onDisconnect handler to be called, resulting in deadlock
    clients.front()->client->onDisconnect(nullptr, nullptr);
    delete clients.front();
    clients.pop_front();
  }
  LOG_D("gmodbus server stopped");
  return true;
}

void ModbusServerTCPasync::onClientConnect(AsyncClient* client) {
  LOG_D("New client");
  std::lock_guard<std::mutex> cntLock(m);
  if (clients.size() < maxNoClients) {
    clients.emplace_back(new mb_client(this, client));
    LOG_D("nr clients: %d", clients.size());
  } else {
    client->close(true);
    delete client;
  }
}

void ModbusServerTCPasync::onClientDisconnect(mb_client* client) {
  LOG_D("Cleanup client");
  std::lock_guard<std::mutex> cntLock(m);
  // delete mb_client from list
  clients.remove_if([client](mb_client* i) { return i->client == client->client; });
  // delete client itself
  delete client;
  LOG_D("nr clients: %d", clients.size());
}
