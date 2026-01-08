#include "Drivers/Ble/BleBeaconDriver.h"
#include "Drivers/Common/DriverFactory.h"

#include <nlohmann/json.hpp>
#include <random>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <sstream>
#include <iomanip>

// Platform-specific headers
#if defined(__linux__)
    #include <bluetooth/bluetooth.h>
    #include <bluetooth/hci.h>
    #include <bluetooth/hci_lib.h>
    #include <unistd.h>
    #include <sys/socket.h>
#elif defined(_WIN32)
    #include <winrt/Windows.Foundation.h>
    #include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
    #pragma comment(lib, "windowsapp")
    using namespace winrt;
    using namespace Windows::Devices::Bluetooth::Advertisement;
#endif

namespace PulseOne {
namespace Drivers {
namespace Ble {

BleBeaconDriver::BleBeaconDriver() {
}

BleBeaconDriver::~BleBeaconDriver() {
    Disconnect();
}

bool BleBeaconDriver::Initialize(const PulseOne::Structs::DriverConfig& config) {
    config_ = config; 
    if (config.properties.count("adapter")) {
        adapter_name_ = config.properties.at("adapter");
    }
    
    if (config.properties.count("scan_window_ms")) {
        try {
            scan_window_ms_ = std::stoi(config.properties.at("scan_window_ms"));
        } catch (...) {
            scan_window_ms_ = 100; // Default
        }
    }
        
    Logger().Info("BleBeaconDriver initialized on adapter: " + adapter_name_);
    is_initialized_ = true;
    return true;
}

bool BleBeaconDriver::Connect() {
    if (!is_initialized_) return false;
    if (is_connected_) return true;

    stop_scanning_ = false;
    scanning_thread_ = std::thread(&BleBeaconDriver::ScanningLoop, this);
    is_connected_ = true;
    Logger().Info("BleBeaconDriver scanning started");
    NotifyStatusChange(PulseOne::Enums::ConnectionStatus::CONNECTED);
    return true;
}

bool BleBeaconDriver::Disconnect() {
    if (!is_connected_) return true;

    stop_scanning_ = true;
    if (scanning_thread_.joinable()) {
        scanning_thread_.join();
    }
    is_connected_ = false;
    Logger().Info("BleBeaconDriver scanning stopped");
    NotifyStatusChange(PulseOne::Enums::ConnectionStatus::DISCONNECTED);
    return true;
}

bool BleBeaconDriver::IsConnected() const {
    return is_connected_;
}

PulseOne::Enums::DriverStatus BleBeaconDriver::GetStatus() const {
    if (!is_initialized_) return PulseOne::Enums::DriverStatus::UNINITIALIZED;
    if (is_connected_) return PulseOne::Enums::DriverStatus::RUNNING;
    return PulseOne::Enums::DriverStatus::STOPPED;
}

bool BleBeaconDriver::ReadValues(const std::vector<PulseOne::Structs::DataPoint>& points, 
                                std::vector<PulseOne::Structs::TimestampedValue>& values) {
    std::lock_guard<std::mutex> lock(beacons_mutex_);
    auto now = std::chrono::system_clock::now();

    for (const auto& point : points) {
        PulseOne::Structs::TimestampedValue tv;
        tv.point_id = std::stoi(point.id); 
        tv.timestamp = now;
        tv.quality = PulseOne::Enums::DataQuality::BAD;

        auto it = beacons_.find(point.address_string);
        if (it != beacons_.end()) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.last_seen).count();
            if (duration <= stale_timeout_ms_) {
                tv.value = PulseOne::Structs::DataValue(static_cast<int>(it->second.rssi));
                tv.quality = PulseOne::Enums::DataQuality::GOOD;
            } else {
                 beacons_.erase(it);
                 Logger().Warn("Beacon stale, removed: " + point.address_string);
            }
        }
        values.push_back(tv);
    }
    return true; 
}

bool BleBeaconDriver::WriteValue(const PulseOne::Structs::DataPoint& point, 
                                const PulseOne::Structs::DataValue& value) {
    Logger().Warn("Write not supported for BLE Beacons");
    return false;
}

void BleBeaconDriver::SimulateBeaconDiscovery(const std::string& uuid, int rssi) {
    std::lock_guard<std::mutex> lock(beacons_mutex_);
    BeaconInfo info;
    info.uuid = uuid;
    info.rssi = rssi;
    info.last_seen = std::chrono::system_clock::now();
    beacons_[uuid] = info;
}

void BleBeaconDriver::ScanningLoop() {
    bool use_simulation = false;

#if defined(__linux__)
    // =========================================================================
    // 🐧 Linux Implementation: BlueZ
    // =========================================================================
    int dev_id = hci_get_route(NULL);
    int sock = hci_open_dev(dev_id);
    if (dev_id < 0 || sock < 0) {
        Logger().Error("Failed to open BLE adapter (Linux)");
        Logger().Warn("Falling back to simulation mode (No HW found)");
        use_simulation = true;
    } else {
        // Set filter
        struct hci_filter nf;
        hci_filter_clear(&nf);
        hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
        hci_filter_set_event(EVT_LE_META_EVENT, &nf);
        setsockopt(sock, SOL_HCI, HCI_FILTER, &nf, sizeof(nf));

        // Enable scanning
        int enable = 1;
        setsockopt(sock, SOL_HCI, HCI_FILTER, &enable, sizeof(enable));
        
        // Start LE Scan
        le_set_scan_enable_cp scan_cp;
        memset(&scan_cp, 0, sizeof(scan_cp));
        scan_cp.enable = 0x01; 
        scan_cp.filter_dup = 0x00; 
        
        struct hci_request rq;
        memset(&rq, 0, sizeof(rq));
        rq.ogf = OGF_LE_CTL;
        rq.ocf = OCF_LE_SET_SCAN_ENABLE;
        rq.cparam = &scan_cp;
        rq.clen = LE_SET_SCAN_ENABLE_CP_SIZE;
        rq.rparam = NULL;
        rq.rlen = 0;
        
        if (hci_send_req(sock, &rq, 1000) < 0) {
             Logger().Warn("Failed to enable LE scanning");
        }

        uint8_t buf[HCI_MAX_EVENT_SIZE];
        
        while (!stop_scanning_ && !use_simulation) {
            struct timeval timeout; 
            timeout.tv_sec = 1; timeout.tv_usec = 0;
            
            fd_set reads;
            FD_ZERO(&reads);
            FD_SET(sock, &reads);

            int result = select(sock + 1, &reads, NULL, NULL, &timeout);
            if (result < 0) {
                if (errno != EINTR) {
                     Logger().Error("HCI select error. Stopping scan.");
                     break;
                }
            }
            if (result > 0 && FD_ISSET(sock, &reads)) {
                int len = read(sock, buf, sizeof(buf));
                if (len > 0) {
                    evt_le_meta_event *meta = (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));
                    if (meta->subevent == EVT_LE_ADVERTISING_REPORT) {
                        uint8_t reports_count = meta->data[0];
                        void *offset = meta->data + 1;

                        while (reports_count--) {
                            le_advertising_info *info = (le_advertising_info *)offset;
                            char addr[18];
                            ba2str(&info->bdaddr, addr);
                            
                            std::string mac_str(addr);
                            for(auto & c: mac_str) c = toupper(c);

                            // 1. Extract Real RSSI
                            // RSSI is at the end of the data payload
                            int8_t rssi = (int8_t)info->data[info->length];

                            // 2. Parse iBeacon UUID
                            std::string uuid_str = mac_str; // Default to MAC
                            
                            // Check for iBeacon pattern in payload
                            // Format: [Len] [Type] [CompanyID_L] [CompanyID_H] [Type] [Len] [UUID...]
                            // Standard iBeacon: 0x02 0x01 ... 0x1A 0xFF 0x4C 0x00 0x02 0x15 [UUID 16B] ...
                            
                            // Simple scan for Apple Manufacturer Data (0x4C00) and Type 0x0215
                            uint8_t *payload = info->data;
                            uint8_t payload_len = info->length;
                            
                            int idx = 0;
                            while (idx < payload_len) {
                                uint8_t field_len = payload[idx];
                                if (field_len == 0 || idx + field_len >= payload_len) break;
                                
                                uint8_t field_type = payload[idx + 1];
                                if (field_type == 0xFF) { // Manufacturer Specific Data
                                    // Check length (at least 25 bytes for iBeacon: 2 Company + 23 Data)
                                    // Field Len includes Type byte, so 26 bytes total?
                                    // iBeacon: 0x1A (26) 0xFF 0x4C 0x00 0x02 0x15 [UUID] ...
                                    if (field_len >= 25 && 
                                        payload[idx + 2] == 0x4C && payload[idx + 3] == 0x00 &&
                                        payload[idx + 4] == 0x02 && payload[idx + 5] == 0x15) {
                                        
                                        // Found iBeacon! Extract UUID (16 bytes starting at idx+6)
                                        std::stringstream ss;
                                        ss << std::hex << std::uppercase << std::setfill('0');
                                        uint8_t *uuid_raw = &payload[idx + 6];
                                        
                                        // 8-4-4-4-12 Format
                                        // 0-3
                                        for(int i=0; i<4; i++) ss << std::setw(2) << (int)uuid_raw[i];
                                        ss << "-";
                                        // 4-5
                                        for(int i=4; i<6; i++) ss << std::setw(2) << (int)uuid_raw[i];
                                        ss << "-";
                                        // 6-7
                                        for(int i=6; i<8; i++) ss << std::setw(2) << (int)uuid_raw[i];
                                        ss << "-";
                                        // 8-9
                                        for(int i=8; i<10; i++) ss << std::setw(2) << (int)uuid_raw[i];
                                        ss << "-";
                                        // 10-15
                                        for(int i=10; i<16; i++) ss << std::setw(2) << (int)uuid_raw[i];
                                        
                                        uuid_str = ss.str();
                                        break; 
                                    }
                                }
                                idx += field_len + 1;
                            }
                            
                            SimulateBeaconDiscovery(uuid_str, (int)rssi);
                            
                            // Move to next report
                            offset = (void *)(info->data + info->length + 2); // +2 for Length byte? No, RSSI byte.
                            // Structure: [bdaddr 6] [length 1] [data L] [rssi 1]
                            // sizeof(le_advertising_info) usually includes bdaddr + length (7 bytes).
                            // But data is flexible.
                            // Pointer arithmetic: info points to start.
                            // Next report = current start + packet size.
                            // LE_ADVERTISING_INFO_SIZE is not always constant due to data[0].
                            // BlueZ struct: bdaddr (6), length (1), data (length), rssi (1). -> Total 1 + 6 + 1 + len + 1 = 9 + len ??
                            // Actually it is usually safe to increment by info->length + sizeof(le_advertising_info) equivalent.
                            // Let's rely on standard offset calculation: (uint8_t*)info + 8 (header) + info->length + 1 (rssi).
                            // Wait, header is bdaddr(6) + length(1) = 7.
                            offset = (uint8_t*)info + 1 + 6 + 1 + info->length + 1; // event_type(1)+bdaddr(6)+length(1)+data(len)+rssi(1) = 9 + len
                        }
                    }
                }
            }
            
            // Stale Check
            static auto last_cleanup = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_cleanup).count() > 5) {
                 CleanUpStaleBeacons();
                 last_cleanup = now;
            }
        }
        
        // Disable scanning
        if (!use_simulation) {
            scan_cp.enable = 0x00;
            hci_send_req(sock, &rq, 1000);
            close(sock);
        }
    }

#elif defined(_WIN32)
    // =========================================================================
    // 🪟 Windows Implementation: C++/WinRT
    // =========================================================================
    Logger().Info("Initializing Windows BLE Watcher (WinRT)...");
    
    try {
        winrt::init_apartment();
        
        BluetoothLEAdvertisementWatcher watcher;
        
        watcher.Received([this](BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementReceivedEventArgs args) {
             int16_t rssi = args.RawSignalStrengthInDBm();
             // 1. MAC Address
             uint64_t addr = args.BluetoothAddress();
             std::stringstream ss;
             ss << std::hex << std::uppercase << std::setfill('0');
             for (int i = 5; i >= 0; i--) {
                 ss << std::setw(2) << ((addr >> (i * 8)) & 0xFF);
                 if (i > 0) ss << ":";
             }
             std::string mac_str = ss.str();
             std::string uuid_str = mac_str; // Default fallback

             // 2. Parse iBeacon UUID (Manufacturer Data)
             try {
                 auto manufacturerSections = args.Advertisement().ManufacturerData();
                 for (auto const& section : manufacturerSections) {
                     if (section.CompanyId() == 0x004C) { // Apple
                         auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(section.Data());
                         
                         // Check length for iBeacon (Type + Len + UUID + Major + Minor + TxPower = 1+1+16+2+2+1 = 23 bytes)
                         if (reader.UnconsumedBufferLength() >= 23) {
                             uint8_t type = reader.ReadByte();
                             uint8_t len = reader.ReadByte();
                             
                             if (type == 0x02 && len == 0x15) {
                                 // Extract 16-byte UUID
                                 std::stringstream uuid_ss;
                                 uuid_ss << std::hex << std::uppercase << std::setfill('0');
                                 
                                 uint8_t uuid_bytes[16];
                                 reader.ReadBytes(uuid_bytes);
                                 
                                 // Format: 8-4-4-4-12
                                 for(int i=0; i<4; i++) uuid_ss << std::setw(2) << (int)uuid_bytes[i];
                                 uuid_ss << "-";
                                 for(int i=4; i<6; i++) uuid_ss << std::setw(2) << (int)uuid_bytes[i];
                                 uuid_ss << "-";
                                 for(int i=6; i<8; i++) uuid_ss << std::setw(2) << (int)uuid_bytes[i];
                                 uuid_ss << "-";
                                 for(int i=8; i<10; i++) uuid_ss << std::setw(2) << (int)uuid_bytes[i];
                                 uuid_ss << "-";
                                 for(int i=10; i<16; i++) uuid_ss << std::setw(2) << (int)uuid_bytes[i];
                                 
                                 uuid_str = uuid_ss.str();
                                 break; // Found iBeacon, stop searching
                             }
                         }
                     }
                 }
             } catch (...) {
                 // Ignore parsing errors, keep MAC
             }
             
             this->SimulateBeaconDiscovery(uuid_str, (int)rssi);
        });
        
        watcher.ScanningMode(BluetoothLEScanningMode::Active);
        watcher.Start();
        Logger().Info("Windows BLE Watcher Started.");
        
        while (!stop_scanning_ && !use_simulation) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
             // Cleanup check
            static auto last_cleanup = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_cleanup).count() > 5) {
                 CleanUpStaleBeacons();
                 last_cleanup = now;
            }
        }
        
        watcher.Stop();
        Logger().Info("Windows BLE Watcher Stopped.");
        winrt::uninit_apartment();
        
    } catch (const winrt::hresult_error& ex) {
        Logger().Error("WinRT Exception: " + std::string(winrt::to_string(ex.message()).c_str()));
        use_simulation = true;
    } catch (...) {
         Logger().Error("Exception in BLE Loop");
         use_simulation = true;
    }

#else
    // Other platforms forcing simulation
    use_simulation = true;
#endif

    if (use_simulation || stop_scanning_) {
        // =========================================================================
        // 🔂 Shared Simulation / Fallback Loop
        // =========================================================================
        if (use_simulation && !stop_scanning_) {
            Logger().Info("Starting BLE Simulation Loop (Fallback/Sim Mode)");
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> rssi_dist(-95, -40);

            std::vector<std::string> test_uuids = {
                "74278BDA-B644-4520-8F0C-720EAF059935", // Lobby
                "E2C56DB5-DFFB-48D2-B060-D0F5A71096E0"  // Asset
            };

            while (!stop_scanning_) {
                for (const auto& uuid : test_uuids) {
                    int rssi = rssi_dist(gen);
                    SimulateBeaconDiscovery(uuid, rssi);
                }
                CleanUpStaleBeacons();
                std::this_thread::sleep_for(std::chrono::milliseconds(scan_window_ms_));
            }
        }
    }
}

void BleBeaconDriver::CleanUpStaleBeacons() {
    std::lock_guard<std::mutex> lock(beacons_mutex_);
    auto now = std::chrono::system_clock::now();
    for (auto it = beacons_.begin(); it != beacons_.end(); ) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.last_seen).count();
        if (duration > stale_timeout_ms_) {
            it = beacons_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Ble
} // namespace Drivers
} // namespace PulseOne

// =============================================================================
// 🔥 Plugin Registration
// =============================================================================
#ifndef TEST_BUILD
extern "C" {
#ifdef _WIN32
    __declspec(dllexport) void RegisterPlugin() {
#else
    void RegisterPlugin() {
#endif
        PulseOne::Drivers::DriverFactory::GetInstance().RegisterDriver("BLE_BEACON", []() {
            return std::make_unique<PulseOne::Drivers::Ble::BleBeaconDriver>();
        });
        std::cout << "[BleBeaconDriver] Plugin Registered Successfully" << std::endl;
    }
}
#endif
