// collector/include/RedisClient.h
#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <string>

class RedisClient {
public:
    virtual bool connect(const std::string& host, int port) = 0;
    virtual bool set(const std::string& key, const std::string& value) = 0;
    virtual std::string get(const std::string& key) = 0;
    virtual void disconnect() = 0;
    virtual ~RedisClient() {}
};

#endif // REDIS_CLIENT_H