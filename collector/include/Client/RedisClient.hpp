#ifndef REDISCLIENT_HPP
#define REDISCLIENT_HPP

#include <string>

class RedisClient {
public:
    virtual bool connect(const std::string& host, int port) = 0;
    virtual std::string get(const std::string& key) = 0;
    virtual bool set(const std::string& key, const std::string& value) = 0;
    virtual void disconnect() = 0;
    virtual ~RedisClient() {}
};

#endif
