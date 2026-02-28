#ifndef PULSEONE_DRIVERS_OPCUA_DRIVER_H
#define PULSEONE_DRIVERS_OPCUA_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/OPCUA/open62541.h" // Amalgamation header
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace PulseOne {
namespace Drivers {

/**
 * @brief OPC-UA Protocol Driver
 *
 * Implements OPC-UA client using open62541 stack.
 * Features:
 * - Connection management
 * - Variable NodeID Read/Write
 */
class OPCUADriver : public IProtocolDriver {
public:
  OPCUADriver();
  virtual ~OPCUADriver();

  // IProtocolDriver Implementation
  bool Initialize(const PulseOne::Structs::DriverConfig &config) override;
  bool Connect() override;
  bool Disconnect() override;
  bool IsConnected() const override;

  bool Start() override;
  bool Stop() override;

  PulseOne::Structs::DriverStatus GetStatus() const override;
  PulseOne::Structs::ErrorInfo GetLastError() const override;
  // Statistics handled by base class IProtocolDriver

  bool
  ReadValues(const std::vector<PulseOne::Structs::DataPoint> &points,
             std::vector<PulseOne::Structs::TimestampedValue> &values) override;

  bool WriteValue(const PulseOne::Structs::DataPoint &point,
                  const PulseOne::Structs::DataValue &value) override;

  std::string GetProtocolType() const override;

  // Node Browsing
  std::vector<PulseOne::Structs::DataPoint> DiscoverPoints() override;

private:
  // Internal helper methods
  void UpdateStatus(PulseOne::Structs::DriverStatus status);
  void SetError(const std::string &message,
                PulseOne::Structs::ErrorCode code =
                    PulseOne::Structs::ErrorCode::DEVICE_ERROR);

  // Member variables
  UA_Client *client_;
  std::string endpoint_url_;
  std::string start_node_id_; // Optional
  std::atomic<bool> is_running_;
  PulseOne::Structs::DriverStatus status_;

  mutable std::mutex client_mutex_;

  std::string username_;
  std::string password_;

  // Config
  uint32_t timeout_ms_ = 5000;

  // =======================================================================
  // 🚀 High-Performance Subscription & Caching
  // =======================================================================

  // Internal Cache: NodeID -> Latest Value
  // This allows ReadValues to resolve instantly without network blocking.
  mutable std::mutex cache_mutex_;
  std::map<std::string, PulseOne::Structs::TimestampedValue> value_cache_;

  // Subscription Management
  UA_UInt32 subscription_id_ = 0;
  std::mutex pending_mutex_;
  std::vector<std::string>
      pending_monitored_items_;           // NodeIDs waiting to be monitored
  std::set<std::string> monitored_nodes_; // NodeIDs already monitored
  std::vector<std::unique_ptr<std::string>>
      monitored_contexts_; // [CRITICAL FIX] Callback Context 생명주기 관리
                           // (Memory Leak 차단)

  // Background Thread for Async Callbacks
  std::thread background_thread_;
  std::atomic<bool> stop_thread_{false};

  // Internal Methods
  void BackgroundLoop();
  void CreateSubscription();
  void SyncMonitoredItems();

  // Static Callback for open62541
  static void DataChangeNotificationCallback(UA_Client *client, UA_UInt32 subId,
                                             void *subContext, UA_UInt32 monId,
                                             void *monContext,
                                             UA_DataValue *value);

  void BrowseRecursive(UA_NodeId nodeId,
                       std::vector<PulseOne::Structs::DataPoint> &points,
                       int depth = 0);
  static const int MAX_BROWSE_DEPTH = 3;
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_OPCUA_DRIVER_H
