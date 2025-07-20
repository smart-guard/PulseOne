// collector/include/DBClient.h
#ifndef DB_CLIENT_H
#define DB_CLIENT_H

#include <string>
#include <vector>

class DBClient {
public:
    virtual bool connect(const std::string& host, int port, const std::string& user, const std::string& password, const std::string& dbname) = 0;
    virtual bool executeQuery(const std::string& query) = 0;
    virtual std::vector<std::vector<std::string>> fetchResults() = 0;
    virtual void disconnect() = 0;
    virtual ~DBClient() {}
};

#endif // DB_CLIENT_H