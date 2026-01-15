#ifndef INFLUX_CLIENT_H
#define INFLUX_CLIENT_H

#include <string>
#include <map>
#include <vector>

namespace PulseOne {
namespace Client {

class InfluxClient {
public:
    virtual bool connect(const std::string& url, const std::string& token, const std::string& org, const std::string& bucket) = 0;
    virtual bool writePoint(const std::string& measurement, const std::string& field, double value) = 0;
    virtual bool writeRecord(const std::string& measurement, 
                             const std::map<std::string, std::string>& tags,
                             const std::map<std::string, double>& fields) = 0;
    virtual bool writeBatch(const std::vector<std::string>& lines) = 0;
    virtual std::string formatRecord(const std::string& measurement,
                                   const std::map<std::string, std::string>& tags,
                                   const std::map<std::string, double>& fields) = 0;
    virtual std::string query(const std::string& fluxQuery) = 0;
    virtual void disconnect() = 0;
    virtual ~InfluxClient() {}
};

} // namespace Client
} // namespace PulseOne

#endif // INFLUX_CLIENT_H