// collector/include/RPCClient.h
#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <string>

class RPCClient {
public:
    virtual bool connect(const std::string& url) = 0;
    virtual std::string callMethod(const std::string& method, const std::string& params) = 0;
    virtual void disconnect() = 0;
    virtual ~RPCClient() {}
};

#endif // RPC_CLIENT_H