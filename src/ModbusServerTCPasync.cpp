// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to eModbus
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
  message(nullptr),
  error(SUCCESS),
  outbox() {
    client->onData([](void* i, AsyncClient* c, void* data, size_t len) { (static_cast<mb_client*>(i))->onData(static_cast<uint8_t*>(data), len); }, this);
    client->onPoll([](void* i, AsyncClient* c) { (static_cast<mb_client*>(i))->onPoll(); }, this);
    client->onDisconnect([](void* i, AsyncClient* c) { (static_cast<mb_client*>(i))->onDisconnect(); }, this);
    client->setNoDelay(true);
}

ModbusServerTCPasync::mb_client::~mb_client() {
  delete client;  // will also close connection, if any
}

void ModbusServerTCPasync::mb_client::onData(uint8_t* data, size_t len) {
  lastActiveTime = millis();
  LOG_D("data len %d", len);

  Error error = SUCCESS;
  size_t i = 0;
  while (i < len) {
    // 0. start
    if (!message) {
      message = new ModbusMessage(8);
      error = SUCCESS;
    }

    //  1. get minimal 8 bytes to move on
    if (message->size() < 8) {
      message->push_back(data[i++]);
      continue;
    }
    
    // 2. preliminary validation: protocol bytes and message length
    if ((*message)[2] != 0 || (*message)[3] != 0) {
        error = TCP_HEAD_MISMATCH;
        LOG_D("invalid protocol");
    }
    size_t messageLength = (((*message)[4] << 8) | (*message)[5]) + 6;
    if (messageLength > 264) {  // 256 + ID(1) + FC(1) + MBAP(6) = 264
      error = PACKET_LENGTH_ERROR;
      LOG_D("max length error");
    }
    if (error != SUCCESS) {
      ModbusMessage* response = new ModbusMessage(
        message->getServerID(),
        message->getFunctionCode()
      );
      addResponseToOutbox(response);  // outbox has pointer ownership now
      // reset to starting values and process remaining data
      delete message;
      message = nullptr;
      continue;
    }

    // 3. receive untill request is complete
    while (message->size() < messageLength && i < len) {
      message->push_back(data[i++]);
    }
    if (message->size() == messageLength) {
      LOG_D("request complete (len:%d)", message->size());
    } else {
      LOG_D("request incomplete (len:%d), waiting for next TCP packet", message->size());
      continue;
    }

    // 4. request complete, process
    ModbusMessage request(messageLength - 6);  // create request without MBAP, with server ID
    request.add(message->data() + 6, message->size() - 6);
    ModbusMessage userData;
    if (server->isServerFor(request.getServerID())) {
      MBSworker callback = server->getWorker(request.getServerID(), request.getFunctionCode());
      if (callback) {
        // request is well formed and is being served by user API
        userData = callback(request);
        // Process Response
        // One of the predefined types?
        if (userData[0] == 0xFF && (userData[1] == 0xF0 || userData[1] == 0xF1)) {
          // Yes. Check it
          switch (userData[1]) {
          case 0xF0: // NIL
            userData.clear();
            LOG_D("NIL response\n");
            break;
          case 0xF1: // ECHO
            userData = request;
            LOG_D("ECHO response\n");
            break;
          default:   // Will not get here!
            break;
          }
        } else {
          // No. User provided data response
          LOG_D("Data response\n");
        }
        error = SUCCESS;
      } else {  // no worker found
        error = ILLEGAL_FUNCTION;
      }
    } else {  // mismatch server ID
      error = INVALID_SERVER;
    }
    if (error == SUCCESS) {
      message->resize(4);
      message->add(static_cast<uint16_t>(userData.size()));
      message->append(userData);
    } else {
      userData.setError(request.getServerID(), request.getFunctionCode(), error);
    }
    if (message->size() > 3) {
      addResponseToOutbox(message);
      message = nullptr;
    }
  }  // end while loop iterating incoming data
}

void ModbusServerTCPasync::mb_client::onPoll() {
  handleOutbox();
  if (server->idle_timeout > 0 && 
      millis() - lastActiveTime > server->idle_timeout) {
    LOG_D("client idle, closing");
    client->close();
  }
}

void ModbusServerTCPasync::mb_client::onDisconnect() {
  LOG_D("client disconnected");
  server->onClientDisconnect(this);
}

void ModbusServerTCPasync::mb_client::addResponseToOutbox(ModbusMessage* response) {
  if (response->size() > 0) { 
    std::lock_guard<std::mutex> lock(m);
    outbox.push(response);
  }
}

void ModbusServerTCPasync::mb_client::handleOutbox() {
  std::lock_guard<std::mutex> lock(m);
  while (!outbox.empty()) {
    ModbusMessage* m = outbox.front();
    if (m->size() <= client->space()) {
      LOG_D("sending (%d)", m->size());
      client->add(reinterpret_cast<const char*>(m->data()), m->size());
      client->send();
      delete m;
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
    // setup will be done in 'start'
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
  LOG_E("could not start server");
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
  LOG_D("modbus server stopped");
  return true;
}

void ModbusServerTCPasync::onClientConnect(AsyncClient* client) {
  LOG_D("new client");
  std::lock_guard<std::mutex> cntLock(m);
  if (clients.size() < maxNoClients) {
    clients.emplace_back(new mb_client(this, client));
    LOG_D("nr clients: %d", clients.size());
  } else {
    LOG_D("max number of clients reached, closing new");
    client->close(true);
    delete client;
  }
}

void ModbusServerTCPasync::onClientDisconnect(mb_client* client) {
  std::lock_guard<std::mutex> cntLock(m);
  // delete mb_client from list
  clients.remove_if([client](mb_client* i) { return i->client == client->client; });
  // delete client itself
  delete client;
  LOG_D("nr clients: %d", clients.size());
}
