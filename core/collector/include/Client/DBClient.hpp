#ifndef DBCLIENT_HPP
#define DBCLIENT_HPP

#include <string>

class DBClient {
public:
    virtual bool connect(const std::string& configPath) = 0;
    virtual bool insert(const std::string& query) = 0;
    virtual std::string query(const std::string& query) = 0;
    virtual void disconnect() = 0;
    virtual ~DBClient() {}
};

#endif
