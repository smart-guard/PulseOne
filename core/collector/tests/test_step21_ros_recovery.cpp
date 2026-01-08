#include <gtest/gtest.h>
#include "Workers/Protocol/ROSWorker.h"
#include "Pipeline/PipelineManager.h"
#include "Pipeline/DataProcessingService.h"
#include "Logging/LogManager.h"
#include <thread>
#include <chrono>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using namespace PulseOne::Workers;
using namespace PulseOne::Structs;

class MockRosBridgeRecovery {
public:
    MockRosBridgeRecovery(int port) : port_(port), is_running_(false) {}
    ~MockRosBridgeRecovery() { Stop(); }

    void Start() {
        is_running_ = true;
        server_thread_ = std::make_unique<std::thread>(&MockRosBridgeRecovery::Run, this);
    }

    void Stop() {
        is_running_ = false;
        if (server_fd_ != -1) {
            shutdown(server_fd_, SHUT_RDWR);
            close(server_fd_);
            server_fd_ = -1;
        }
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (client_fd_ != -1) {
                shutdown(client_fd_, SHUT_RDWR);
                close(client_fd_);
                client_fd_ = -1;
            }
        }
        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join();
        }
    }

    void Run() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);

        if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) return;
        if (listen(server_fd_, 3) < 0) return;

        while (is_running_) {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addrlen);
            if (client_fd < 0) break;

            {
                std::lock_guard<std::mutex> lock(mtx_);
                client_fd_ = client_fd;
            }
            std::cout << "[MockServer] Client connected" << std::endl;
            
            char buffer[4096] = {0};
            while (is_running_) {
                int valread = recv(client_fd, buffer, 4096, 0);
                if (valread <= 0) break;

                std::string received(buffer, valread);
                size_t pos = 0;
                size_t next_pos;
                while ((next_pos = received.find('}', pos)) != std::string::npos) {
                    std::string part = received.substr(pos, next_pos - pos + 1);
                    pos = next_pos + 1;
                    try {
                        auto j = nlohmann::json::parse(part);
                        if (j["op"] == "subscribe") {
                            std::string topic = j["topic"];
                            std::cout << "[MockServer] Subscribed to: " << topic << std::endl;
                            {
                                std::lock_guard<std::mutex> lock(mtx_);
                                subscriptions_[topic]++;
                            }
                        }
                    } catch (...) {}
                }
            }
            {
                std::lock_guard<std::mutex> lock(mtx_);
                client_fd_ = -1;
            }
            close(client_fd);
            std::cout << "[MockServer] Client disconnected" << std::endl;
        }
    }

    int GetSubCount(const std::string& topic) {
        std::lock_guard<std::mutex> lock(mtx_);
        return subscriptions_[topic];
    }
    
    void ResetCounts() {
        std::lock_guard<std::mutex> lock(mtx_);
        subscriptions_.clear();
    }

private:
    int port_;
    int server_fd_ = -1;
    int client_fd_ = -1;
    std::atomic<bool> is_running_;
    std::unique_ptr<std::thread> server_thread_;
    std::mutex mtx_;
    std::map<std::string, int> subscriptions_;
};

TEST(RosRecoveryTest, AutomaticReconnection) {
    int port = 9100;
    auto server = std::make_unique<MockRosBridgeRecovery>(port);
    server->Start();

    DeviceInfo info;
    info.id = "robot_recovery";
    info.endpoint = "127.0.0.1";
    info.properties["port"] = std::to_string(port);
    
    auto worker = std::make_unique<ROSWorker>(info);
    DataPoint p1; p1.id = "1"; p1.address_string = "/test_topic";
    worker->AddDataPoint(p1);

    std::cout << "--- Starting Worker ---" << std::endl;
    worker->Start();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_GE(server->GetSubCount("/test_topic"), 1);
    std::cout << "--- Initial Subscription Verified ---" << std::endl;

    std::cout << "--- Killing Server ---" << std::endl;
    server->Stop();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    EXPECT_FALSE(worker->CheckConnection());

    std::cout << "--- Restarting Server ---" << std::endl;
    server = std::make_unique<MockRosBridgeRecovery>(port);
    server->ResetCounts();
    server->Start();

    // Wait for worker to reconnect (BaseDeviceWorker reconnection loop is 5s)
    std::cout << "--- Waiting for Reconnection (up to 15s) ---" << std::endl;
    bool reconnected = false;
    for (int i = 0; i < 30; ++i) {
        if (server->GetSubCount("/test_topic") >= 1) {
            reconnected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    EXPECT_TRUE(reconnected);
    EXPECT_TRUE(worker->CheckConnection());
    std::cout << "--- Recovery and Re-subscription Verified ---" << std::endl;

    worker->Stop();
}
