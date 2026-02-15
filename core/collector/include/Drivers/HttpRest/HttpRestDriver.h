#ifndef PULSEONE_DRIVERS_HTTPREST_DRIVER_H
#define PULSEONE_DRIVERS_HTTPREST_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace PulseOne {
namespace Drivers {

/**
 * @brief HTTP/REST Protocol Driver
 *
 * Configuration-driven driver for REST API integration.
 * Supports dynamic API endpoints with JSONPath-based data extraction.
 *
 * Features:
 * - GET, POST, PUT, DELETE methods
 * - Custom headers (Authorization, API keys, etc.)
 * - JSONPath for flexible data mapping
 * - Retry logic with exponential backoff
 * - Token refresh mechanism
 */
class HttpRestDriver : public IProtocolDriver {
public:
  HttpRestDriver();
  virtual ~HttpRestDriver();

  // IProtocolDriver Implementation
  bool Initialize(const DriverConfig &config) override;
  bool Connect() override;
  bool Disconnect() override;
  bool IsConnected() const override;

  bool Start() override;
  bool Stop() override;

  DriverStatus GetStatus() const override;
  ErrorInfo GetLastError() const override;
  const DriverStatistics &GetStatistics() const override;
  void ResetStatistics() override;

  bool ReadValues(const std::vector<DataPoint> &points,
                  std::vector<TimestampedValue> &values) override;

  bool WriteValue(const DataPoint &point, const DataValue &value) override;

  std::string GetProtocolType() const override;

private:
  // Configuration
  std::string endpoint_url_;
  std::string http_method_; // GET, POST, PUT, DELETE
  std::map<std::string, std::string> headers_;
  std::string request_body_template_;
  int timeout_ms_;
  int retry_count_;

  // State
  std::atomic<bool> is_connected_;
  std::atomic<DriverStatus> status_;
  mutable std::mutex driver_mutex_;
  mutable std::mutex error_mutex_;
  ErrorInfo last_error_;

  // Statistics
  DriverStatistics driver_statistics_;

  // HTTP Client
  struct CurlContext;
  std::unique_ptr<CurlContext> curl_context_;

  // Helper Methods
  bool ParseDriverConfig(const DriverConfig &config);
  bool InitializeHttpClient();
  void CleanupHttpClient();

  std::string ExecuteHttpRequest(const std::string &url,
                                 const std::string &method,
                                 const std::string &body = "");

  DataValue ExtractValueFromJson(const std::string &json_response,
                                 const std::string &jsonpath,
                                 const std::string &data_type);

  std::string BuildRequestBody(const DataPoint &point, const DataValue &value);

  void SetError(const std::string &message,
                Structs::ErrorCode code = Structs::ErrorCode::DEVICE_ERROR);
  void UpdateStatistics(const std::string &operation, bool success,
                        double duration_ms = 0.0);

  // CURL Callbacks
  static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                              void *userp);
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_HTTPREST_DRIVER_H
