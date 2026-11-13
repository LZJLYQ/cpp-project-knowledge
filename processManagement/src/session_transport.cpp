#include "console_session_transport.hpp"

#include "logger.hpp"

ConsoleSessionTransport::~ConsoleSessionTransport()
{
    stop();
}

bool ConsoleSessionTransport::start(const std::string& session_id)
{
    if (running_ || session_id.empty()) {
        return false;
    }

    session_id_ = session_id;
    running_ = true;
    log_line("session transport '" + session_id_ + "' started");
    return true;
}

void ConsoleSessionTransport::stop()
{
    if (!running_) {
        return;
    }

    log_line("session transport '" + session_id_ + "' stopped");
    running_ = false;
}

std::unique_ptr<ISessionTransport> ConsoleSessionTransportFactory::create()
{
    return std::make_unique<ConsoleSessionTransport>();
}
