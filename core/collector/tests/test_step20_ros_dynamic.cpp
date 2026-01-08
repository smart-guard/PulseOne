#include <gtest/gtest.h>
#include "Workers/Protocol/ROSWorker.h"
#include "Drivers/Ros/ROSDriver.h"
#include "DatabaseManager.hpp"
#include "Pipeline/PipelineManager.h"
#include "Pipeline/DataProcessingService.h"
#include <thread>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace PulseOne::Workers;
using namespace PulseOne::Structs;

// Mock server to verify subscriptions
class MockRosBridgeServer {
public:
    MockRosBridgeServer(int port) : port_(port), running_(false), server_fd_(-1) {}
    
    void Start() {
        running_ = true;
        thread_ = std::thread([this]() {
            server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
            int opt = 1;
            setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            
            struct sockaddr_in address;
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port_);
            
            bind(server_fd_, (struct sockaddr*)&address, sizeof(address));
            listen(server_fd_, 3);
            
            while (running_) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len);
                if (client_fd < 0) {
                    if (running_) std::cerr << "[MockServer] Accept failed: " << errno << std::endl;
                    break;
                }
                
                std::cout << "[MockServer] Client connected: " << client_fd << std::endl;
                
                char buffer[4096] = {0};
                while (running_) {
                    int valread = recv(client_fd, buffer, sizeof(buffer)-1, 0);
                    if (valread <= 0) {
                        std::cout << "[MockServer] Client disconnected or error" << std::endl;
                        break;
                    }
                    
                    std::string received(buffer, valread);
                    std::cout << "[MockServer] Received " << valread << " bytes" << std::endl;
                    
                    // Simple split by } to handle multiple JSON messages in one packet
                    size_t pos = 0;
                    size_t next_pos;
                    while ((next_pos = received.find('}', pos)) != std::string::npos) {
                        std::string part = received.substr(pos, next_pos - pos + 1);
                        pos = next_pos + 1;
                        
                        try {
                            auto j = nlohmann::json::parse(part);
                            if (j["op"] == "subscribe") {
                                std::string topic = j["topic"];
                                std::cout << "[MockServer] Subscription to: " << topic << std::endl;
                                {
                                    std::lock_guard<std::mutex> lock(mtx_);
                                    received_topics_.push_back(topic);
                                }
                                // Echo back a message for this topic
                                nlohmann::json msg;
                                msg["op"] = "publish";
                                msg["topic"] = topic;
                                if (topic == "/custom_battery") msg["msg"]["percentage"] = 85.5;
                                else if (topic == "/custom_odom") {
                                    msg["msg"]["pose"]["pose"]["position"]["x"] = 10.1;
                                    msg["msg"]["pose"]["pose"]["position"]["y"] = 20.2;
                                }
                                std::string response = msg.dump();
                                send(client_fd, response.c_str(), response.length(), 0);
                            }
                        } catch (const std::exception& e) {
                            // Partial or invalid JSON, skip for now
                        }
                    }
                }
                close(client_fd);
            }
        });
    }
    
    void Stop() {
        running_ = false;
        if (server_fd_ >= 0) {
            shutdown(server_fd_, SHUT_RDWR);
            close(server_fd_);
        }
        if (thread_.joinable()) thread_.join();
    }
    
    std::vector<std::string> GetReceivedTopics() {
        std::lock_guard<std::mutex> lock(mtx_);
        return received_topics_;
    }

private:
    int port_;
    std::atomic<bool> running_;
    int server_fd_;
    std::thread thread_;
    std::vector<std::string> received_topics_;
    std::mutex mtx_;
};

class RosDynamicTest : public ::testing::Test {
protected:
    void SetUp() override {
        server = std::make_unique<MockRosBridgeServer>(9099);
        server->Start();
        
        PulseOne::Pipeline::PipelineManager::getInstance().initialize();
        pipeline = std::make_shared<PulseOne::Pipeline::DataProcessingService>();
        pipeline->Start();
        
        DeviceInfo info;
        info.id = "robot_1";
        info.endpoint = "127.0.0.1";
        info.protocol_type = "ROS_BRIDGE";
        info.properties["port"] = "9099";
        
        worker = std::make_unique<ROSWorker>(info);
        
        // Add Dynamic Points
        DataPoint p1; p1.id = "101"; p1.name = "Battery"; p1.address_string = "/custom_battery";
        DataPoint p2; p2.id = "201"; p2.name = "PosX"; p2.address_string = "/custom_odom";
        DataPoint p3; p3.id = "202"; p3.name = "PosY"; p3.address_string = "/custom_odom";
        
        worker->AddDataPoint(p1);
        worker->AddDataPoint(p2);
        worker->AddDataPoint(p3);
    }

    void TearDown() override {
        worker->Stop();
        pipeline->Stop();
        PulseOne::Pipeline::PipelineManager::getInstance().Shutdown();
        server->Stop();
    }

    std::unique_ptr<MockRosBridgeServer> server;
    std::unique_ptr<ROSWorker> worker;
    std::shared_ptr<PulseOne::Pipeline::DataProcessingService> pipeline;
};

TEST_F(RosDynamicTest, DynamicSubscription) {
    auto future = worker->Start();
    ASSERT_TRUE(future.get());
    
    // Wait for subscriptions to hit mock server
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    auto topics = server->GetReceivedTopics();
    EXPECT_EQ(topics.size(), static_cast<size_t>(2)); // /custom_battery, /custom_odom (p2, p3 share same topic)
    
    bool has_battery = false;
    bool has_odom = false;
    for (const auto& t : topics) {
        if (t == "/custom_battery") has_battery = true;
        if (t == "/custom_odom") has_odom = true;
    }
    EXPECT_TRUE(has_battery);
    EXPECT_TRUE(has_odom);
}
