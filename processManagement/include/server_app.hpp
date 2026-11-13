#pragma once

#include "resource_pool.hpp"
#include "session_transport.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <string>

class ServerApp
{
public:
    ServerApp(ISharedResourcePool& resources, ISessionTransportFactory& transport_factory);
    ~ServerApp();

    ServerApp(const ServerApp&) = delete;
    ServerApp& operator=(const ServerApp&) = delete;

    bool startSession(const std::string& session_id, const std::string& resource_key);
    bool stopSession(const std::string& session_id);
    void reload();
    void stop();

    std::size_t sessionCount() const;

private:
    struct Session
    {
        std::string resource_key;
        std::unique_ptr<ISessionTransport> transport;
    };

    ISharedResourcePool& resources_;
    ISessionTransportFactory& transport_factory_;
    std::map<std::string, Session> sessions_;
    unsigned int configuration_generation_{1};
};
