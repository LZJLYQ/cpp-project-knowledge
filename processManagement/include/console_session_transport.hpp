#pragma once

#include "session_transport.hpp"

class ConsoleSessionTransport final : public ISessionTransport
{
public:
    ~ConsoleSessionTransport() override;

    bool start(const std::string& session_id) override;
    void stop() override;

private:
    bool running_{false};
    std::string session_id_;
};

class ConsoleSessionTransportFactory final : public ISessionTransportFactory
{
public:
    std::unique_ptr<ISessionTransport> create() override;
};
