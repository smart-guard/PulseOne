// collector/include/RedisClusterClient.h
#ifndef REDIS_CLUSTER_CLIENT_H
#define REDIS_CLUSTER_CLIENT_H

#include <string>
#include <vector>

class RedisClusterClient {
public:
    virtual bool connect(const std::vector<std::string>& hosts, const std::string& password = "") = 0;
    virtual bool set(const std::string& key, const std::string& value) = 0;
    virtual std::string get(const std::string& key) = 0;
    virtual void disconnect() = 0;
    virtual ~RedisClusterClient() {}
};

#endif // REDIS_CLUSTER_CLIENT_H