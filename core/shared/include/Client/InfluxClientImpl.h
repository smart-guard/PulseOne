#ifndef PULSEONE_CLIENT_INFLUX_CLIENT_IMPL_H
#define PULSEONE_CLIENT_INFLUX_CLIENT_IMPL_H

/**
 * @file InfluxClientImpl.h
 * @brief InfluxDB v2 Client Implementation using HttpClient (Shared Library)
 * @author PulseOne Development Team
 */

#include "Client/InfluxClient.h"
#include "Client/HttpClient.h"
#include <memory>
#include <mutex>

namespace PulseOne {
namespace Client {

/**
 * @brief InfluxDB v2 클라이언트 구현체
 * InfluxDB v2 REST API를 사용하여 데이터를 전송합니다.
 */
class InfluxClientImpl : public InfluxClient {
public:
    InfluxClientImpl();
    ~InfluxClientImpl() override;

    // InfluxClient interface implementation
    bool connect(const std::string& url, const std::string& token, 
                 const std::string& org, const std::string& bucket) override;
    
    bool writePoint(const std::string& measurement, const std::string& field, double value) override;
    
    bool writeRecord(const std::string& measurement, 
                     const std::map<std::string, std::string>& tags,
                     const std::map<std::string, double>& fields) override;
    
    std::string query(const std::string& fluxQuery) override;
    
    void disconnect() override;

private:
    std::string url_;
    std::string token_;
    std::string org_;
    std::string bucket_;
    
    std::unique_ptr<HttpClient> http_client_;
    mutable std::mutex mutex_;
    bool connected_ = false;

    // Helper to format Line Protocol
    std::string formatLineProtocol(const std::string& measurement, const std::string& field, double value);
    std::string formatLineProtocol(const std::string& measurement, 
                                 const std::map<std::string, std::string>& tags,
                                 const std::map<std::string, double>& fields);
};

} // namespace Client
} // namespace PulseOne

#endif // PULSEONE_CLIENT_INFLUX_CLIENT_IMPL_H
