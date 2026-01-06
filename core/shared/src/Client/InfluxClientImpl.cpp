#include "Client/InfluxClientImpl.h"
#include "Utils/LogManager.h"
#include <sstream>

namespace PulseOne {
namespace Client {

InfluxClientImpl::InfluxClientImpl() 
    : http_client_(std::make_unique<HttpClient>()) {
}

InfluxClientImpl::~InfluxClientImpl() {
    disconnect();
}

bool InfluxClientImpl::connect(const std::string& url, const std::string& token, 
                               const std::string& org, const std::string& bucket) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    url_ = url;
    token_ = token;
    org_ = org;
    bucket_ = bucket;
    
    http_client_->setBaseUrl(url_);
    http_client_->setBearerToken(token_);
    
    // Test connection with a simple health check or orgs query
    // For InfluxDB v2, /health is a good endpoint
    if (http_client_->testConnection("/health")) {
        connected_ = true;
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "Connected to InfluxDB at " + url_);
        return true;
    } else {
        connected_ = false;
        LogManager::getInstance().log("database", LogLevel::LOG_ERROR, 
            "Failed to connect to InfluxDB at " + url_);
        return false;
    }
}

bool InfluxClientImpl::writePoint(const std::string& measurement, const std::string& field, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) return false;

    // API: POST /api/v2/write?org=YOUR_ORG&bucket=YOUR_BUCKET&precision=ns
    std::string path = "/api/v2/write?org=" + org_ + "&bucket=" + bucket_ + "&precision=s";
    std::string body = formatLineProtocol(measurement, field, value);

    auto response = http_client_->post(path, body, "text/plain");
    
    if (response.isSuccess()) {
        return true;
    } else {
        LogManager::getInstance().log("database", LogLevel::LOG_ERROR, 
            "InfluxDB write failed: " + response.error_message + " (Status: " + std::to_string(response.status_code) + ")");
        return false;
    }
}

std::string InfluxClientImpl::query(const std::string& fluxQuery) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) return "";

    // API: POST /api/v2/query?org=YOUR_ORG
    std::string path = "/api/v2/query?org=" + org_;
    
    nlohmann::json body;
    body["query"] = fluxQuery;
    body["type"] = "flux";

    auto response = http_client_->post(path, body.dump(), "application/json");
    
    if (response.isSuccess()) {
        return response.body;
    } else {
        LogManager::getInstance().log("database", LogLevel::LOG_ERROR, 
            "InfluxDB query failed: " + response.error_message);
        return "";
    }
}

void InfluxClientImpl::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
}

std::string InfluxClientImpl::formatLineProtocol(const std::string& measurement, const std::string& field, double value) {
    // Line Protocol: measurement field=value timestamp(optional)
    // Example: mem,host=host1 used_percent=23.43234543 1556813568
    std::ostringstream oss;
    oss << measurement << " " << field << "=" << value;
    return oss.str();
}

} // namespace Client
} // namespace PulseOne
