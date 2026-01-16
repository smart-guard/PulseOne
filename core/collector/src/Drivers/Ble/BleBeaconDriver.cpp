#include "Drivers/Ble/BleBeaconDriver.h"
#include "Logging/LogManager.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>

#if HAS_BLUETOOTH
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif

namespace PulseOne {
namespace Drivers {
namespace Ble {

BleBeaconDriver::BleBeaconDriver()
    : is_connected_(false)
#if HAS_BLUETOOTH
      ,
      hci_socket_(-1), device_id_(-1), is_scanning_(false)
#endif
{
}

BleBeaconDriver::~BleBeaconDriver() { Disconnect(); }

bool BleBeaconDriver::Initialize(const Structs::DriverConfig &config) {
  (void)config;
#if HAS_BLUETOOTH
  LogManager::getInstance().Info("[BLE] Initialized with Native BlueZ Support");
#else
  LogManager::getInstance().Info("[BLE] Initialized in Simulation Mode");
#endif
  return true;
}

bool BleBeaconDriver::Connect() {
#if HAS_BLUETOOTH
  // Open HCI Socket
  if (OpenHciSocket() < 0) {
    LogManager::getInstance().Error("[BLE] Failed to open HCI socket");
    return false;
  }

  // Enable scanning
  if (EnableScanning(true) < 0) {
    LogManager::getInstance().Error("[BLE] Failed to enable scanning");
    CloseHciSocket();
    return false;
  }

  // Start scan thread
  is_scanning_ = true;
  scan_thread_ = std::thread(&BleBeaconDriver::ScanLoop, this);

  is_connected_ = true;
  NotifyStatusChange(Enums::ConnectionStatus::CONNECTED);
  LogManager::getInstance().Info("[BLE] Connected and scanning started");
  return true;
#else
  // Simulation Mode
  is_connected_ = true;
  NotifyStatusChange(Enums::ConnectionStatus::CONNECTED);
  return true;
#endif
}

bool BleBeaconDriver::Disconnect() {
#if HAS_BLUETOOTH
  is_scanning_ = false;
  if (scan_thread_.joinable()) {
    scan_thread_.join();
  }

  EnableScanning(false);
  CloseHciSocket();
#endif

  is_connected_ = false;
  NotifyStatusChange(Enums::ConnectionStatus::DISCONNECTED);
  return true;
}

bool BleBeaconDriver::IsConnected() const { return is_connected_; }

bool BleBeaconDriver::Start() { return Connect(); }

bool BleBeaconDriver::Stop() { return Disconnect(); }

bool BleBeaconDriver::WriteValue(const Structs::DataPoint &point,
                                 const Structs::DataValue &value) {
  (void)point;
  (void)value;
  return false; // BLE Beacons are read-only
}

Enums::ProtocolType BleBeaconDriver::GetProtocolType() const {
  return Enums::ProtocolType::BLE_BEACON;
}

Structs::DriverStatus BleBeaconDriver::GetStatus() const {
  if (is_connected_)
    return Enums::DriverStatus::RUNNING;
  return Enums::DriverStatus::STOPPED;
}

Structs::ErrorInfo BleBeaconDriver::GetLastError() const { return last_error_; }

void BleBeaconDriver::SimulateBeaconDiscovery(const std::string &uuid,
                                              int rssi) {
  std::lock_guard<std::mutex> lock(data_mutex_);
  discovered_rssi_[uuid] = rssi;
}

bool BleBeaconDriver::ReadValues(
    const std::vector<Structs::DataPoint> &points,
    std::vector<Structs::TimestampedValue> &out_values) {
  if (!is_connected_)
    return false;

  std::lock_guard<std::mutex> lock(data_mutex_);

  for (const auto &point : points) {
    Structs::TimestampedValue val;
    val.timestamp = std::chrono::system_clock::now();

    auto it = discovered_rssi_.find(point.address_string);
    if (it != discovered_rssi_.end()) {
      val.value = it->second;
      val.quality = Enums::DataQuality::GOOD;
    } else {
      // Simulation fallback if not using BlueZ (or if BlueZ hasn't found it
      // yet)
#if !HAS_BLUETOOTH
      if (point.address_string == "74278BDA-B644-4520-8F0C-720EAF059935") {
        val.value = -65; // Simulated "Good" RSSI
        val.quality = Enums::DataQuality::GOOD;
        out_values.push_back(val);
        UpdateReadStats(true);
        continue;
      }
#endif
      val.value = 0;
      val.quality = Enums::DataQuality::BAD;
    }

    out_values.push_back(val);
    UpdateReadStats(true); // Always return success for the operation itself
  }

  return true;
}

// =========================================================================
// Native BlueZ Implementation
// =========================================================================

#if HAS_BLUETOOTH
int BleBeaconDriver::OpenHciSocket() {
  device_id_ = hci_get_route(NULL);
  if (device_id_ < 0) {
    device_id_ = 0; // Default to 0 if detection fails
  }

  hci_socket_ = hci_open_dev(device_id_);
  if (hci_socket_ < 0) {
    // perror("Failed to open HCI device");
    return -1;
  }
  return 0;
}

void BleBeaconDriver::CloseHciSocket() {
  if (hci_socket_ >= 0) {
    close(hci_socket_);
    hci_socket_ = -1;
  }
}

int BleBeaconDriver::EnableScanning(bool enable) {
  if (hci_socket_ < 0)
    return -1;

  // Set scan parameters
  if (enable) {
    // Default: Interval=0x10 (10ms), Window=0x10 (10ms)
    uint16_t interval = 0x10;
    uint16_t window = 0x10;

    // Override from config (JSON)
    // Unit: ms. HCI Unit: 0.625ms
    if (config_.properties.count("scan_interval_ms")) {
      int ms = std::stoi(config_.properties.at("scan_interval_ms"));
      interval = (uint16_t)(ms / 0.625);
    }
    if (config_.properties.count("scan_window_ms")) {
      int ms = std::stoi(config_.properties.at("scan_window_ms"));
      window = (uint16_t)(ms / 0.625);
    }

    // Validation
    if (window > interval)
      window = interval;

    // Active Scan (0x01)
    hci_le_set_scan_parameters(hci_socket_, 0x01, interval, window, 0x00, 0x00,
                               1000);
    hci_le_set_scan_enable(hci_socket_, 0x01, 0x00, 1000);

    // Set socket filter to receive events
    struct hci_filter flt;
    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_set_event(EVT_LE_META_EVENT, &flt);
    setsockopt(hci_socket_, SOL_HCI, HCI_FILTER, &flt, sizeof(flt));
  } else {
    hci_le_set_scan_enable(hci_socket_, 0x00, 0x00, 1000);
  }
  return 0;
}

void BleBeaconDriver::ScanLoop() {
  unsigned char buf[HCI_MAX_EVENT_SIZE];
  int len;

  LogManager::getInstance().Info("[BLE] Scan Loop Started");

  while (is_scanning_) {
    // Use select/poll to allow timeout/interruption
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(hci_socket_, &rset);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int n = select(hci_socket_ + 1, &rset, NULL, NULL, &timeout);

    if (n < 0) {
      if (errno == EINTR)
        continue;
      break; // Error
    }

    if (n == 0)
      continue; // Timeout

    len = read(hci_socket_, buf, sizeof(buf));
    if (len < 0)
      continue;

    // Parse HCI Event
    evt_le_meta_event *meta =
        (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));

    if (meta->subevent == EVT_LE_ADVERTISING_REPORT) {
      le_advertising_info *info = (le_advertising_info *)(meta->data + 1);

      // Iterate through received reports (usually one)
      // Note: In older BlueZ headers or depending on struct alignment, this
      // might need care.

      // Simple Parsing Strategy for iBeacon in the raw packet
      // The manufacturer specific data is inside info->data

      int offset = 0;
      while (offset < info->length) {
        int len = info->data[offset];
        if (len == 0)
          break;
        if (offset + 1 + len > info->length)
          break;

        int type = info->data[offset + 1];

        if (type == 0xFF) { // Manufacturer Data
          // Check for iBeacon pattern: Apple (0x004C) + Type (0x02) + Len
          // (0x15) Data layout:
          // [Len][Type][CompanyID_LO][CompanyID_HI][BeaconType][BeaconLen][UUID...][Major][Minor][TxPower]
          // Offsets:      0    1     2             3              4 5
          // 6..21   22..23  24..25  26

          if (len >= 26 && info->data[offset + 2] == 0x4C &&
              info->data[offset + 3] == 0x00 &&
              info->data[offset + 4] == 0x02 &&
              info->data[offset + 5] == 0x15) {

            // UUID (16 bytes)
            // Starts at offset + 6
            char uuid_str[37];
            const uint8_t *u = &info->data[offset + 6];
            sprintf(uuid_str,
                    "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%"
                    "02X%02X%02X",
                    u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8], u[9],
                    u[10], u[11], u[12], u[13], u[14], u[15]);

            // Packet RSSI is generally appended at the end of the data part of
            // le_advertising_info structure in the buffer, or contained within
            // `le_advertising_info` depending on event type. For
            // `EVT_LE_ADVERTISING_REPORT`, the struct is: uint8_t evt_type;
            // uint8_t bdaddr_type;
            // bdaddr_t bdaddr;
            // uint8_t length;
            // uint8_t data[0];
            // ... followed by RSSI byte.

            // Safe RSSI access:
            signed char packet_rssi =
                *(signed char *)(&info->data[info->length]);

            // Update Map
            {
              std::lock_guard<std::mutex> lock(data_mutex_);
              discovered_rssi_[std::string(uuid_str)] = (int)packet_rssi;
            }
          }
        }

        offset += len + 1;
      }
    }
  }
}
#endif

} // namespace Ble
} // namespace Drivers
} // namespace PulseOne
