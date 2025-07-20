// collector/include/MultiDBClient.h
#ifndef MULTI_DB_CLIENT_H
#define MULTI_DB_CLIENT_H

#include <string>

class MultiDBClient {
public:
    virtual bool connectPrimary(const std::string& host, int port, const std::string& user, const std::string& password, const std::string& dbname) = 0;
    virtual bool connectSecondary(const std::string& host, int port, const std::string& user, const std::string& password, const std::string& dbname) = 0;
    virtual bool executePrimaryQuery(const std::string& query) = 0;
    virtual bool executeSecondaryQuery(const std::string& query) = 0;
    virtual void disconnectAll() = 0;
    virtual ~MultiDBClient() {}
};

#endif // MULTI_DB_CLIENT_H