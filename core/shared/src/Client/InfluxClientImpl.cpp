#include "Client/InfluxClientImpl.h"
#include "Logging/LogManager.h"
#include <chrono>
#include <sstream>
#include <string>

namespace PulseOne {
namespace Client {

InfluxClientImpl::InfluxClientImpl()
    : http_client_(std::make_unique<HttpClient>()) {}

InfluxClientImpl::~InfluxClientImpl() { disconnect(); }

bool InfluxClientImpl::connect(const std::string &url, const std::string &token,
                               const std::string &org,
                               const std::string &bucket) {
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

bool InfluxClientImpl::writePoint(const std::string &measurement,
                                  const std::string &field, double value) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!connected_)
    return false;

  // API: POST /api/v2/write?org=YOUR_ORG&bucket=YOUR_BUCKET&precision=ns
  std::string path =
      "/api/v2/write?org=" + org_ + "&bucket=" + bucket_ + "&precision=s";
  std::string body = formatLineProtocol(measurement, field, value);

  auto response = http_client_->post(path, body, "text/plain");

  if (response.isSuccess()) {
    return true;
  } else {
    LogManager::getInstance().log(
        "database", LogLevel::LOG_ERROR,
        "InfluxDB write failed: " + response.error_message +
            " (Status: " + std::to_string(response.status_code) + ")");
    return false;
  }
}

bool InfluxClientImpl::writeBatch(const std::vector<std::string> &lines) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!connected_ || lines.empty())
    return false;

  // API: POST /api/v2/write?org=YOUR_ORG&bucket=YOUR_BUCKET&precision=s
  std::string path =
      "/api/v2/write?org=" + org_ + "&bucket=" + bucket_ + "&precision=s";

  std::ostringstream oss;
  for (const auto &line : lines) {
    oss << line << "\n";
  }
  std::string body = oss.str();

  auto response = http_client_->post(path, body, "text/plain");

  if (response.isSuccess()) {
    LogManager::getInstance().log("database", LogLevel::DEBUG_LEVEL,
                                  "InfluxDB batch write success: " +
                                      std::to_string(lines.size()) + " points");
    return true;
  } else {
    LogManager::getInstance().log(
        "database", LogLevel::LOG_ERROR,
        "InfluxDB batch write failed: " + response.error_message +
            " (Status: " + std::to_string(response.status_code) + ")");
    return false;
  }
}

std::string
InfluxClientImpl::formatRecord(const std::string &measurement,
                               const std::map<std::string, std::string> &tags,
                               const std::map<std::string, double> &fields) {
  // epoch_seconds=0: no timestamp appended.
  // Callers needing precision should call formatLineProtocol() directly.
  return formatLineProtocol(measurement, tags, fields, 0);
}

bool InfluxClientImpl::writeRecord(
    const std::string &measurement,
    const std::map<std::string, std::string> &tags,
    const std::map<std::string, double> &fields) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!connected_)
    return false;

  // API: POST /api/v2/write?org=YOUR_ORG&bucket=YOUR_BUCKET&precision=ns
  std::string path =
      "/api/v2/write?org=" + org_ + "&bucket=" + bucket_ + "&precision=s";
  std::string body = formatLineProtocol(measurement, tags, fields);

  auto response = http_client_->post(path, body, "text/plain");

  if (response.isSuccess()) {
    return true;
  } else {
    LogManager::getInstance().log(
        "database", LogLevel::LOG_ERROR,
        "InfluxDB record write failed: " + response.error_message +
            " (Status: " + std::to_string(response.status_code) + ")");
    return false;
  }
}

std::string InfluxClientImpl::query(const std::string &fluxQuery) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!connected_)
    return "";

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
                                  "InfluxDB query failed: " +
                                      response.error_message);
    return "";
  }
}

void InfluxClientImpl::disconnect() {
  std::lock_guard<std::mutex> lock(mutex_);
  connected_ = false;
}

// [BUG #26 FIX] InfluxDB line protocol: tag keys/values must escape spaces,
// commas, equals.
static std::string EscapeInfluxTagValue(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (char c : s) {
    if (c == ' ') {
      out += '\\ ';
    } else if (c == ',') {
      out += "\\,";
    } else if (c == '=') {
      out += "\\=";
    } else {
      out += c;
    }
  }
  return out;
}

std::string InfluxClientImpl::formatLineProtocol(const std::string &measurement,
                                                 const std::string &field,
                                                 double value,
                                                 long long epoch_seconds) {
  // Line Protocol: measurement field=value timestamp_seconds
  std::ostringstream oss;
  oss << EscapeInfluxTagValue(measurement) << " " << EscapeInfluxTagValue(field)
      << "=" << value;
  if (epoch_seconds > 0)
    oss << " " << epoch_seconds;
  return oss.str();
}

std::string InfluxClientImpl::formatLineProtocol(
    const std::string &measurement,
    const std::map<std::string, std::string> &tags,
    const std::map<std::string, double> &fields, long long epoch_seconds) {
  std::ostringstream oss;
  oss << EscapeInfluxTagValue(measurement);

  for (const auto &[key, value] : tags) {
    oss << "," << EscapeInfluxTagValue(key) << "="
        << EscapeInfluxTagValue(value);
  }

  oss << " ";

  bool first = true;
  for (const auto &[key, value] : fields) {
    if (!first)
      oss << ",";
    oss << EscapeInfluxTagValue(key) << "=" << value;
    first = false;
  }

  // [BUG #25 FIX] Caller-supplied epoch seconds = actual measurement time.
  // epoch_seconds=0 means no timestamp (InfluxDB server time used as fallback).
  if (epoch_seconds > 0)
    oss << " " << epoch_seconds;

  return oss.str();
}

} // namespace Client
} // namespace PulseOne
