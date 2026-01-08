// =============================================================================
// HTTP/REST Driver Test
// Tests configuration-driven REST API integration with JSONPath
// =============================================================================

#include <gtest/gtest.h>
#include "Drivers/HttpRest/HttpRestDriver.h"
#include "Workers/Protocol/HttpRestWorker.h"
#include <httplib.h>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

using namespace PulseOne::Drivers;
using namespace PulseOne::Structs;
using json = nlohmann::json;

// =============================================================================
// Mock HTTP Server
// =============================================================================

class MockHttpServer {
public:
    MockHttpServer(int port = 8080) : port_(port), running_(false) {}
    
    ~MockHttpServer() {
        Stop();
    }
    
    void Start() {
        running_ = true;
        server_thread_ = std::thread([this]() {
            httplib::Server svr;
            
            // GET /api/sensors - Returns sensor data
            svr.Get("/api/sensors", [](const httplib::Request&, httplib::Response& res) {
                json response = {
                    {"status", "ok"},
                    {"data", {
                        {"temperature", 25.5},
                        {"humidity", 60.0},
                        {"pressure", 1013.25}
                    }}
                };
                res.set_content(response.dump(), "application/json");
            });
            
            // POST /api/control - Accepts control commands
            svr.Post("/api/control", [](const httplib::Request& req, httplib::Response& res) {
                try {
                    json body = json::parse(req.body);
                    json response = {
                        {"status", "success"},
                        {"received", body}
                    };
                    res.set_content(response.dump(), "application/json");
                } catch (...) {
                    res.status = 400;
                    res.set_content("{\"error\": \"Invalid JSON\"}", "application/json");
                }
            });
            
            // GET /api/weather - Complex nested response
            svr.Get("/api/weather", [](const httplib::Request&, httplib::Response& res) {
                json response = {
                    {"location", "Seoul"},
                    {"current", {
                        {"temp_c", 15.2},
                        {"temp_f", 59.4},
                        {"condition", "Cloudy"}
                    }},
                    {"forecast", {
                        {"today", {
                            {"max", 18.0},
                            {"min", 12.0}
                        }}
                    }}
                };
                res.set_content(response.dump(), "application/json");
            });
            
            svr.listen("127.0.0.1", port_);
        });
        
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void Stop() {
        if (running_) {
            running_ = false;
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
        }
    }

private:
    int port_;
    bool running_;
    std::thread server_thread_;
};

// =============================================================================
// Test Fixture
// =============================================================================

class HttpRestDriverTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_ = std::make_unique<MockHttpServer>(8080);
        server_->Start();
        
        // Wait for server to be ready
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void TearDown() override {
        server_->Stop();
    }
    
    std::unique_ptr<MockHttpServer> server_;
};

// =============================================================================
// Basic Tests
// =============================================================================

TEST_F(HttpRestDriverTest, InitializeAndConnect) {
    HttpRestDriver driver;
    
    DriverConfig config;
    config.endpoint = "http://127.0.0.1:8080/api/sensors";
    config.protocol_params["http_method"] = "GET";
    config.protocol_params["timeout_ms"] = "5000";
    
    ASSERT_TRUE(driver.Initialize(config));
    ASSERT_TRUE(driver.Connect());
    ASSERT_TRUE(driver.IsConnected());
    
    EXPECT_EQ(driver.GetProtocolType(), ProtocolType::HTTP_REST);
}

TEST_F(HttpRestDriverTest, ReadValuesWithJSONPath) {
    HttpRestDriver driver;
    
    DriverConfig config;
    config.endpoint = "http://127.0.0.1:8080/api/sensors";
    config.protocol_params["http_method"] = "GET";
    
    ASSERT_TRUE(driver.Initialize(config));
    ASSERT_TRUE(driver.Connect());
    
    // Configure data points with JSONPath
    std::vector<DataPoint> points;
    
    DataPoint temp_point;
    temp_point.id = "1";
    temp_point.name = "temperature";
    temp_point.address_string = "$.data.temperature";
    temp_point.protocol_params["data_type"] = "double";
    points.push_back(temp_point);
    
    DataPoint humidity_point;
    humidity_point.id = "2";
    humidity_point.name = "humidity";
    humidity_point.address_string = "$.data.humidity";
    humidity_point.protocol_params["data_type"] = "double";
    points.push_back(humidity_point);
    
    // Read values
    std::vector<TimestampedValue> values;
    ASSERT_TRUE(driver.ReadValues(points, values));
    
    ASSERT_EQ(values.size(), 2UL);
    
    // Check temperature
    EXPECT_NEAR(std::get<double>(values[0].value), 25.5, 0.01);
    EXPECT_EQ(values[0].quality, DataQuality::GOOD);
    
    // Check humidity
    EXPECT_NEAR(std::get<double>(values[1].value), 60.0, 0.01);
    EXPECT_EQ(values[1].quality, DataQuality::GOOD);
}

TEST_F(HttpRestDriverTest, WriteValuePOST) {
    HttpRestDriver driver;
    
    DriverConfig config;
    config.endpoint = "http://127.0.0.1:8080/api/control";
    config.protocol_params["http_method"] = "POST";
    
    ASSERT_TRUE(driver.Initialize(config));
    ASSERT_TRUE(driver.Connect());
    
    DataPoint control_point;
    control_point.id = "1";
    control_point.name = "setpoint";
    control_point.protocol_params["http_method"] = "POST";
    
    DataValue value = 75.5;
    
    ASSERT_TRUE(driver.WriteValue(control_point, value));
}

TEST_F(HttpRestDriverTest, NestedJSONPath) {
    HttpRestDriver driver;
    
    DriverConfig config;
    config.endpoint = "http://127.0.0.1:8080/api/weather";
    config.protocol_params["http_method"] = "GET";
    
    ASSERT_TRUE(driver.Initialize(config));
    ASSERT_TRUE(driver.Connect());
    
    std::vector<DataPoint> points;
    
    DataPoint temp_point;
    temp_point.id = "1";
    temp_point.name = "current_temp";
    temp_point.address_string = "$.current.temp_c";
    temp_point.protocol_params["data_type"] = "double";
    points.push_back(temp_point);
    
    DataPoint max_temp_point;
    max_temp_point.id = "2";
    max_temp_point.name = "forecast_max";
    max_temp_point.address_string = "$.forecast.today.max";
    max_temp_point.protocol_params["data_type"] = "double";
    points.push_back(max_temp_point);
    
    std::vector<TimestampedValue> values;
    ASSERT_TRUE(driver.ReadValues(points, values));
    
    ASSERT_EQ(values.size(), 2UL);
    EXPECT_NEAR(std::get<double>(values[0].value), 15.2, 0.01);
    EXPECT_NEAR(std::get<double>(values[1].value), 18.0, 0.01);
}

TEST_F(HttpRestDriverTest, ErrorHandling) {
    HttpRestDriver driver;
    
    DriverConfig config;
    config.endpoint = "http://127.0.0.1:8080/api/nonexistent";
    config.protocol_params["http_method"] = "GET";
    config.protocol_params["retry_count"] = "1";  // Reduce retries for faster test
    
    ASSERT_TRUE(driver.Initialize(config));
    
    // Connection should fail for non-existent endpoint
    // But driver should handle gracefully
    std::vector<DataPoint> points;
    DataPoint p;
    p.id = "1";
    p.address_string = "$.data";
    points.push_back(p);
    
    std::vector<TimestampedValue> values;
    // Should return false but not crash
    bool result = driver.ReadValues(points, values);
    
    // Either succeeds with empty data or fails gracefully
    if (result) {
        EXPECT_TRUE(values.empty() || values[0].quality == DataQuality::BAD);
    }
}

TEST_F(HttpRestDriverTest, Statistics) {
    HttpRestDriver driver;
    
    DriverConfig config;
    config.endpoint = "http://127.0.0.1:8080/api/sensors";
    config.protocol_params["http_method"] = "GET";
    
    ASSERT_TRUE(driver.Initialize(config));
    ASSERT_TRUE(driver.Connect());
    
    std::vector<DataPoint> points;
    DataPoint p;
    p.id = "1";
    p.address_string = "$.data.temperature";
    points.push_back(p);
    
    std::vector<TimestampedValue> values;
    
    // Perform multiple reads
    driver.ReadValues(points, values);
    driver.ReadValues(points, values);
    driver.ReadValues(points, values);
    
    const auto& stats = driver.GetStatistics();
    EXPECT_GE(stats.total_reads.load(), 3);
    EXPECT_GE(stats.successful_reads.load(), 3);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
