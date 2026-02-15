#include "Drivers/OPCUA/OPCUADriver.h"
#include "Database/Repositories/ProtocolRepository.h"
#include "Database/RepositoryFactory.h"
#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include <iostream>
#include <thread>

namespace PulseOne {
namespace Drivers {

using namespace PulseOne::Structs;
using namespace PulseOne::Enums;

OPCUADriver::OPCUADriver() : client_(nullptr), is_running_(false) {
  status_ = DriverStatus::UNINITIALIZED;
  // statistics_ is initialized in base class
}

OPCUADriver::~OPCUADriver() { Disconnect(); }

bool OPCUADriver::Initialize(const DriverConfig &config) {
  config_ = config; // Base class member
  endpoint_url_ = config.endpoint;

  // Parse properties if needed
  // Parse properties if needed
  if (config.properties.find("timeout") != config.properties.end()) {
    timeout_ms_ = std::stoul(config.properties.at("timeout"));
  }
  if (config.properties.find("username") != config.properties.end()) {
    username_ = config.properties.at("username");
  }
  if (config.properties.find("password") != config.properties.end()) {
    password_ = config.properties.at("password");
  }

  LogManager::getInstance().Info("OPC-UA Driver initialized. Target: " +
                                 endpoint_url_);
  UpdateStatus(DriverStatus::STOPPED);
  return true;
}

bool OPCUADriver::WriteValue(const PulseOne::Structs::DataPoint &point,
                             const PulseOne::Structs::DataValue &value) {
  if (!IsConnected())
    return false;
  std::lock_guard<std::mutex> lock(client_mutex_);

  std::string nodeIdStr = point.address_string;
  if (nodeIdStr.empty())
    nodeIdStr = point.name;

  UA_NodeId nodeId;
  UA_NodeId_parse(&nodeId, UA_STRING((char *)nodeIdStr.c_str()));

  UA_Variant *val = UA_Variant_new();

  if (std::holds_alternative<double>(value)) {
    UA_Double d = std::get<double>(value);
    UA_Variant_setScalarCopy(val, &d, &UA_TYPES[UA_TYPES_DOUBLE]);
  } else if (std::holds_alternative<int>(value)) {
    UA_Int32 i = std::get<int>(value);
    UA_Variant_setScalarCopy(val, &i, &UA_TYPES[UA_TYPES_INT32]);
  } else if (std::holds_alternative<bool>(value)) {
    UA_Boolean b = std::get<bool>(value);
    UA_Variant_setScalarCopy(val, &b, &UA_TYPES[UA_TYPES_BOOLEAN]);
  } else if (std::holds_alternative<std::string>(value)) {
    std::string s = std::get<std::string>(value);
    UA_String ua_s = UA_STRING((char *)s.c_str());
    UA_Variant_setScalarCopy(val, &ua_s, &UA_TYPES[UA_TYPES_STRING]);
  }

  UA_StatusCode retval = UA_Client_writeValueAttribute(client_, nodeId, val);

  UA_Variant_delete(val);
  UA_NodeId_clear(&nodeId);

  if (retval == UA_STATUSCODE_GOOD) {
    UpdateWriteStats(true);
    return true;
  } else {
    UpdateWriteStats(false);
    LogManager::getInstance().Error("Write failed: " +
                                    std::string(UA_StatusCode_name(retval)));
    return false;
  }
}

void OPCUADriver::UpdateStatus(PulseOne::Structs::DriverStatus status) {
  status_ = status;
}

// =============================================================================
// 🔎 Node Browsing (Discovery)
// =============================================================================
std::vector<PulseOne::Structs::DataPoint> OPCUADriver::DiscoverPoints() {
  std::vector<PulseOne::Structs::DataPoint> points;

  if (!IsConnected()) {
    LogManager::getInstance().Error(
        "[OPC-UA] Cannot discover points: Not connected");
    return points;
  }

  std::lock_guard<std::mutex> lock(client_mutex_);
  LogManager::getInstance().Info(
      "[OPC-UA] Starting Node Discovery (Root=Objects)");

  // Browse Objects Folder
  BrowseRecursive(UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), points, 0);

  LogManager::getInstance().Info("[OPC-UA] Discovery Completed. Found " +
                                 std::to_string(points.size()) + " variables.");
  return points;
}

void OPCUADriver::BrowseRecursive(
    UA_NodeId nodeId, std::vector<PulseOne::Structs::DataPoint> &points,
    int depth) {
  if (depth > MAX_BROWSE_DEPTH) {
    // LogManager::getInstance().Warn("[OPC-UA] Max browse depth reached,
    // stopping recursion on this branch.");
    return;
  }

  // Browse request
  UA_BrowseRequest bReq;
  UA_BrowseRequest_init(&bReq);
  bReq.requestedMaxReferencesPerNode = 0;
  bReq.nodesToBrowse = UA_BrowseDescription_new();
  bReq.nodesToBrowseSize = 1;
  bReq.nodesToBrowse[0].nodeId = nodeId;
  bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

  UA_BrowseResponse bResp = UA_Client_Service_browse(client_, bReq);

  if (bResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
    for (size_t i = 0; i < bResp.resultsSize; ++i) {
      for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
        UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);

        // 1. If it's a Variable, add to points if readable
        if (ref->nodeClass == UA_NODECLASS_VARIABLE) {
          PulseOne::Structs::DataPoint p;
          UA_NodeId targetNode = ref->nodeId.nodeId;

          std::string nodeIdStr;
          if (targetNode.identifierType == UA_NODEIDTYPE_STRING) {
            nodeIdStr = "ns=" + std::to_string(targetNode.namespaceIndex) +
                        ";s=" +
                        std::string((char *)targetNode.identifier.string.data,
                                    targetNode.identifier.string.length);
          } else if (targetNode.identifierType == UA_NODEIDTYPE_NUMERIC) {
            nodeIdStr = "ns=" + std::to_string(targetNode.namespaceIndex) +
                        ";i=" + std::to_string(targetNode.identifier.numeric);
          }

          if (!nodeIdStr.empty()) {
            p.name = std::string((char *)ref->displayName.text.data,
                                 ref->displayName.text.length);
            p.address_string = nodeIdStr;
            p.description = "Discovered via OPC UA Browsing";

            if (targetNode.namespaceIndex != 0) {
              points.push_back(p);
            }
          }
        }

        // 2. If it's an Object (Folder), Recurse with depth limit
        if (ref->nodeClass == UA_NODECLASS_OBJECT) {
          if (ref->nodeId.nodeId.namespaceIndex != 0) {
            BrowseRecursive(ref->nodeId.nodeId, points, depth + 1);
          }
        }
      }
    }
  }

  UA_BrowseRequest_clear(&bReq);
  UA_BrowseResponse_deleteMembers(&bResp);
}
bool OPCUADriver::Connect() {
  std::lock_guard<std::mutex> lock(client_mutex_);

  if (client_) {
    UA_Client_delete(client_);
    client_ = nullptr;
  }

  client_ = UA_Client_new();
  UA_ClientConfig *config = UA_Client_getConfig(client_);
  UA_ClientConfig_setDefault(config);
  config->timeout = timeout_ms_;

  // Log connecting
  LogManager::getInstance().Info("Connecting to OPC UA Server: " +
                                 endpoint_url_);

  UA_StatusCode retval;
  // Authenticate
  if (!username_.empty()) {
    LogManager::getInstance().Info("Authenticating with User: " + username_);
    retval = UA_Client_connectUsername(client_, endpoint_url_.c_str(),
                                       username_.c_str(), password_.c_str());
  } else {
    retval = UA_Client_connect(client_, endpoint_url_.c_str());
  }

  if (retval != UA_STATUSCODE_GOOD) {
    SetError("Connection failed: " + std::string(UA_StatusCode_name(retval)),
             PulseOne::Structs::ErrorCode::CONNECTION_FAILED);
    UA_Client_delete(client_);
    client_ = nullptr;
    return false;
  }

  LogManager::getInstance().Info("Connected to OPC UA Server!");

  // Create Subscription Channel
  CreateSubscription();

  // Start background thread for async callbacks
  stop_thread_ = false;
  background_thread_ = std::thread(&OPCUADriver::BackgroundLoop, this);

  UpdateStatus(DriverStatus::RUNNING);
  statistics_.successful_connections++;
  return true;
}

bool OPCUADriver::Disconnect() {
  stop_thread_ = true;
  if (background_thread_.joinable()) {
    background_thread_.join();
  }

  std::lock_guard<std::mutex> lock(client_mutex_);
  if (client_) {
    // Remove Subscription is handled by server on disconnect,
    // but cleaner to be explicit if we wanted to reuse connection.
    // For now just disconnect.
    UA_Client_disconnect(client_);
    UA_Client_delete(client_);
    client_ = nullptr;
  }
  UpdateStatus(DriverStatus::STOPPED);
  return true;
}

bool OPCUADriver::IsConnected() const {
  return (status_ == DriverStatus::RUNNING);
}

// ... Rest of methods using statistics_ instead of driver_statistics_ ...

bool OPCUADriver::Start() {
  is_running_ = true;
  return Connect();
}

bool OPCUADriver::Stop() {
  is_running_ = false;
  return Disconnect();
}

PulseOne::Structs::DriverStatus OPCUADriver::GetStatus() const {
  return status_;
}

PulseOne::Structs::ErrorInfo OPCUADriver::GetLastError() const {
  return last_error_;
}

std::string OPCUADriver::GetProtocolType() const { return "OPC_UA"; }

// =============================================================================
// 🚀 High-Performance Read: Cache Lookup
// =============================================================================
bool OPCUADriver::ReadValues(const std::vector<DataPoint> &points,
                             std::vector<TimestampedValue> &values) {
  LogManager::getInstance().Info("[DEBUG] ReadValues: Start");
  if (!IsConnected())
    return false;

  // 1. Identify new items to monitor
  {
    std::lock_guard<std::mutex> pending_lock(pending_mutex_);
    bool new_items_found = false;

    for (const auto &point : points) {
      std::string nodeIdStr = point.address_string;
      if (nodeIdStr.empty())
        nodeIdStr = point.name;

      // Check if already monitored or pending
      if (monitored_nodes_.find(nodeIdStr) == monitored_nodes_.end()) {
        // Not monitored yet. Add to pending.
        // NOTE: We could check if it's already in pending list to avoid
        // duplicates, but SyncMonitoredItems deals with unique set anyway or we
        // optimize here.
        bool already_pending = false;
        for (const auto &p : pending_monitored_items_) {
          if (p == nodeIdStr) {
            already_pending = true;
            break;
          }
        }
        if (!already_pending) {
          pending_monitored_items_.push_back(nodeIdStr);
          new_items_found = true;
        }
      }
    }
  } // Unlock pending_mutex_

  // 2. Read from Cache (Non-blocking)
  std::lock_guard<std::mutex> cache_lock(cache_mutex_);
  LogManager::getInstance().Info("[DEBUG] ReadValues: Locked Cache");
  auto now = std::chrono::system_clock::now();

  for (const auto &point : points) {
    std::string nodeIdStr = point.address_string;
    if (nodeIdStr.empty())
      nodeIdStr = point.name;

    auto it = value_cache_.find(nodeIdStr);
    if (it != value_cache_.end()) {
      values.push_back(it->second);
      UpdateReadStats(true);
    } else {
      // Not in cache yet (waiting for subscription to fire)
      TimestampedValue tv;
      tv.point_id = std::stoi(point.id);
      tv.timestamp = now;
      tv.quality = PulseOne::Enums::DataQuality::BAD; // Or INITIALIZING
      values.push_back(tv);
      // Don't count as failure, just waiting for data
    }
  }
  LogManager::getInstance().Info("[DEBUG] ReadValues: Done");
  return true;
}

// =============================================================================
// 🔄 Background Thread: OPC UA Callback Loop
// =============================================================================
void OPCUADriver::BackgroundLoop() {
  LogManager::getInstance().Info("[OPC-UA] Background thread started");
  while (!stop_thread_) {
    // 1. Process Network Callbacks
    {
      std::lock_guard<std::mutex> lock(client_mutex_);
      if (client_) {
        UA_Client_run_iterate(client_, 0); // Non-blocking check
      }
    }

    // 2. Sync Pending Monitored Items
    SyncMonitoredItems();

    // 3. Sleep
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  LogManager::getInstance().Info("[OPC-UA] Background thread stopped");
}

void OPCUADriver::CreateSubscription() {
  if (!client_)
    return;

  UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();
  request.requestedPublishingInterval = 500.0; // 500ms
  request.requestedLifetimeCount = 100;
  request.requestedMaxKeepAliveCount = 10;
  request.maxNotificationsPerPublish = 0;
  request.publishingEnabled = true;
  request.priority = 0;

  UA_CreateSubscriptionResponse response =
      UA_Client_Subscriptions_create(client_, request, this, NULL, NULL);

  if (response.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
    subscription_id_ = response.subscriptionId;
    LogManager::getInstance().Info("[OPC-UA] Subscription created. ID: " +
                                   std::to_string(subscription_id_));
  } else {
    LogManager::getInstance().Error("[OPC-UA] Failed to create subscription");
  }
}

void OPCUADriver::SyncMonitoredItems() {
  std::vector<std::string> new_items;

  // Quickly grab pending items
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    if (pending_monitored_items_.empty())
      return;
    new_items = pending_monitored_items_;
    pending_monitored_items_.clear();
  }

  if (subscription_id_ == 0)
    return;

  std::lock_guard<std::mutex> client_lock(client_mutex_);
  if (!client_)
    return;

  for (const auto &nodeIdStr : new_items) {
    if (monitored_nodes_.count(nodeIdStr))
      continue; // Already done

    UA_NodeId nodeId = UA_NODEID_NULL;
    UA_NodeId_parse(&nodeId, UA_STRING((char *)nodeIdStr.c_str()));

    UA_MonitoredItemCreateRequest monRequest =
        UA_MonitoredItemCreateRequest_default(nodeId);
    monRequest.itemToMonitor.attributeId = UA_ATTRIBUTEID_VALUE;
    monRequest.monitoringMode = UA_MONITORINGMODE_REPORTING;

    // We pass 'this' as subContext to access instance in static callback?
    // Actually UA_Client_Subscriptions_create takes context.
    // Here we pass nodeIdStr as context for this specific item so we know which
    // one updated Note: we must allocate string memory that persists. For
    // simplicity, let's look up NodeID in cache or map key if possible. Better:
    // Pass a heap-allocated string pointer.
    std::string *contextStr = new std::string(nodeIdStr);

    UA_MonitoredItemCreateResult monResult =
        UA_Client_MonitoredItems_createDataChange(
            client_, subscription_id_, UA_TIMESTAMPSTORETURN_BOTH, monRequest,
            contextStr, DataChangeNotificationCallback, NULL);

    if (monResult.statusCode == UA_STATUSCODE_GOOD) {
      monitored_nodes_.insert(nodeIdStr);
      // LogManager::getInstance().Info("[OPC-UA] Monitored Item created: " +
      // nodeIdStr);
    } else {
      LogManager::getInstance().Error("[OPC-UA] Failed to monitor: " +
                                      nodeIdStr);
      delete contextStr; // Cleanup on failure
      UA_NodeId_clear(&nodeId);
    }
  }
}

// Static Callback
void OPCUADriver::DataChangeNotificationCallback(
    UA_Client *client, UA_UInt32 subId, void *subContext, UA_UInt32 monId,
    void *monContext, UA_DataValue *value) {
  // Get instance from somewhere?
  // We didn't pass instance to createSubscription.
  // BUT! We can get the instance if we used a global map or singleton.
  // Ideally, we should have passed 'this' to CreateSubscription or set user
  // context. open62541 allows getting client context.

  // Workaround: We need access to 'value_cache_'.
  // Since this is a plugin, we might need a way to link back.
  // For now, let's assume we can get the driver instance via a static pointer
  // if we were a singleton, but we are an instance.

  // Wait, monContext IS the string pointer we passed!
  std::string *nodeIdStr = (std::string *)monContext;
  if (!nodeIdStr || !value)
    return;

  // We also need access to the driver instance to lock the mutex and update
  // cache. This is tricky in C callbacks without a "User Data" passed to
  // Validating. Ah, 'subContext' is passed! We passed NULL in
  // CreateSubscription. We should fix CreateSubscription to pass 'this'.

  // IMPORTANT: We need to fix CreateSubscription first.
  // Assuming subContext is 'OPCUADriver*'.
  OPCUADriver *driver = (OPCUADriver *)subContext;
  if (!driver)
    return; // Can't update if we don't know the driver

  PulseOne::Structs::TimestampedValue tv;
  tv.timestamp = std::chrono::system_clock::now();
  tv.quality = PulseOne::Enums::DataQuality::GOOD;

  if (UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_DOUBLE])) {
    tv.value = *(UA_Double *)value->value.data;
  } else if (UA_Variant_hasScalarType(&value->value,
                                      &UA_TYPES[UA_TYPES_FLOAT])) {
    tv.value = (double)*(UA_Float *)value->value.data;
  } else if (UA_Variant_hasScalarType(&value->value,
                                      &UA_TYPES[UA_TYPES_INT32])) {
    tv.value = (double)*(UA_Int32 *)value->value.data;
  } else if (UA_Variant_hasScalarType(&value->value,
                                      &UA_TYPES[UA_TYPES_BOOLEAN])) {
    tv.value = *(UA_Boolean *)value->value.data;
  } else {
    // Unsupported type or empty
    return;
  }

  // Update Cache
  std::lock_guard<std::mutex> lock(driver->cache_mutex_);
  driver->value_cache_[*nodeIdStr] = tv;
}

bool dummy_prevent_compile_error_helper =
    false; // Placeholder to ensure valid C++ syntax block ending before
           // SetError implementation

void OPCUADriver::SetError(const std::string &message,
                           PulseOne::Structs::ErrorCode code) {
  last_error_.message = message;
  last_error_.code = code;
  last_error_.occurred_at = std::chrono::system_clock::now();
  status_ = DriverStatus::DRIVER_ERROR;
  LogManager::getInstance().Error(message);
}

// Plugin registration logic moved outside namespace to avoid mangling and stay
// consistent.
} // namespace Drivers
} // namespace PulseOne

// =============================================================================
// 🔥 Plugin Entry Point
// =============================================================================
#ifndef TEST_BUILD
extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
void RegisterPlugin() {
  // 1. DB에 프로토콜 정보 자동 등록 (없을 경우)
  try {
    auto &repo_factory = PulseOne::Database::RepositoryFactory::getInstance();
    auto protocol_repo = repo_factory.getProtocolRepository();
    if (protocol_repo) {
      if (!protocol_repo->findByType("OPC_UA").has_value()) {
        PulseOne::Database::Entities::ProtocolEntity entity;
        entity.setProtocolType("OPC_UA");
        entity.setDisplayName("OPC UA");
        entity.setCategory("industrial");
        entity.setDescription("OPC Unified Architecture Protocol Driver");
        entity.setDefaultPort(4840);
        protocol_repo->save(entity);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[OPCUA] DB Registration failed: " << e.what() << std::endl;
  }

  // 2. 메모리 Factory에 드라이버 생성자 등록
  PulseOne::Drivers::DriverFactory::GetInstance().RegisterDriver(
      "OPC_UA",
      []() { return std::make_unique<PulseOne::Drivers::OPCUADriver>(); });
  std::cout << "[OPCUA] Plugin Registered Successfully" << std::endl;
}

// Legacy wrapper for static linking
void RegisterOpcUaDriver() { RegisterPlugin(); }
}
#endif
