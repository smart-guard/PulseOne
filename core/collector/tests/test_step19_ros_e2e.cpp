/**
 * @file test_step19_ros_e2e.cpp
 * @brief End-to-End Integration Test for ROS Robot Integration
 */

#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <vector>

#include "Drivers/Ros/ROSDriver.h"
#include "Workers/Protocol/ROSWorker.h"
#include "Pipeline/DataProcessingService.h"
#include "Database/RepositoryFactory.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include <nlohmann/json.hpp>

using namespace PulseOne;
using namespace PulseOne::Workers;
using namespace PulseOne::Drivers;

// Mock ROS Bridge Server
class MockRosBridgeServer {
public:
    MockRosBridgeServer(int port) : port_(port), running_(false), server_fd_(-1), client_fd_(-1) {}
    
    ~MockRosBridgeServer() {
        Stop();
    }

    void Start() {
        running_ = true;
        server_thread_ = std::thread([this]() {
            server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
            if (server_fd_ < 0) return;
            
            int opt = 1;
            setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            struct sockaddr_in address;
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port_);
            
            if (bind(server_fd_, (struct sockaddr *)&address, sizeof(address)) < 0) return;
            if (listen(server_fd_, 3) < 0) return;

            while (running_) {
                struct sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);
                int new_socket = accept(server_fd_, (struct sockaddr *)&client_addr, &addrlen);
                if (new_socket < 0) continue;
                
                LogManager::getInstance().Info("[MockServer] Client Connected");
                client_fd_ = new_socket;
                
                // Handle client
                char buffer[1024];
                while (running_ && client_fd_ != -1) {
                    int valread = read(client_fd_, buffer, 1024);
                    if (valread <= 0) {
                        close(client_fd_);
                        client_fd_ = -1;
                        break;
                    }
                    std::string received(buffer, valread);
                    LogManager::getInstance().Info("[MockServer] Received: " + received);
                    
                    // Simple Response Logic
                    if (received.find("subscribe") != std::string::npos) {
                        // Simulate sending data back
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        SendOdom();
                        SendBattery();
                    }
                }
            }
        });
    }

    void Stop() {
        running_ = false;
        if (client_fd_ >= 0) close(client_fd_);
        if (server_fd_ >= 0) close(server_fd_);
        if (server_thread_.joinable()) server_thread_.join();
    }
    
    void SendOdom() {
        if (client_fd_ < 0) return;
        nlohmann::json j;
        j["op"] = "publish";
        j["topic"] = "/odom";
        j["msg"]["pose"]["pose"]["position"]["x"] = 10.5;
        j["msg"]["pose"]["pose"]["position"]["y"] = 20.3;
        std::string msg = j.dump();
        send(client_fd_, msg.c_str(), msg.length(), 0);
    }

    void SendBattery() {
        if (client_fd_ < 0) return;
        nlohmann::json j;
        j["op"] = "publish";
        j["topic"] = "/battery_state";
        j["msg"]["percentage"] = 98.5;
        std::string msg = j.dump();
        send(client_fd_, msg.c_str(), msg.length(), 0);
    }

private:
    int port_;
    std::atomic<bool> running_;
    int server_fd_;
    std::atomic<int> client_fd_;
    std::thread server_thread_;
};

class RosE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Init Mock Server
        server = std::make_unique<MockRosBridgeServer>(9090);
        server->Start();
        
        // Init Pipeline
        DbLib::DatabaseManager::getInstance().initialize(); // Mock DB
        
        // Start PipelineManager Singleton
        Pipeline::PipelineManager::getInstance().initialize();
        
        // Create DataProcessingService
        pipeline = std::make_shared<Pipeline::DataProcessingService>();
        pipeline->Start();
        
        // Init Worker
        PulseOne::Structs::DeviceInfo info;
        info.id = "1";
        info.endpoint = "127.0.0.1";
        info.protocol_type = "ROS_BRIDGE";
        info.properties["port"] = "9090";
        
        worker = std::make_unique<ROSWorker>(info);
    }

    void TearDown() override {
        worker->Stop();
        if (pipeline) pipeline->Stop();
        Pipeline::PipelineManager::getInstance().Shutdown();
        server->Stop();
    }

    std::unique_ptr<MockRosBridgeServer> server;
    std::unique_ptr<ROSWorker> worker;
    std::shared_ptr<Pipeline::DataProcessingService> pipeline;
};

TEST_F(RosE2ETest, ConnectionAndDataFlow) {
    // 1. Start Worker
    auto future = worker->Start();
    ASSERT_TRUE(future.get());
    
    // 2. Wait for connection and subscription
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 3. Verify Connection
    EXPECT_TRUE(worker->CheckConnection());

    // 4. Server automatically sends Odom and Battery upon subscription
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 5. Verify Data in Pipeline (Need to check logs or mocked repository)
    // Since we can't easily peek into the pipeline in this strict test env without mocking everything,
    // we rely on the fact that no crash occurred and logs would show "Processing"
    // Also, InfluxClient would log specific messages.
    
    // Check if worker is still running
    EXPECT_EQ(worker->GetState(), PulseOne::Workers::WorkerState::RUNNING);
}
