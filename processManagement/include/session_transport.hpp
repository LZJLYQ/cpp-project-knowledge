#pragma once

#include <memory>
#include <string>

class ISessionTransport
{
public:
    virtual ~ISessionTransport() = default;

    // A failed or partially completed start must be safe to clean up with stop().
    virtual bool start(const std::string& session_id) = 0;
    virtual void stop() = 0;
};

class ISessionTransportFactory
{
public:
    virtual ~ISessionTransportFactory() = default;

    virtual std::unique_ptr<ISessionTransport> create() = 0;
};
