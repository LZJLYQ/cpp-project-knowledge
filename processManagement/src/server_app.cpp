#include "server_app.hpp"

#include "logger.hpp"

#include <exception>
#include <utility>

ServerApp::ServerApp(
    ISharedResourcePool& resources,
    ISessionTransportFactory& transport_factory)
    : resources_(resources),
      transport_factory_(transport_factory)
{
}

ServerApp::~ServerApp()
{
    stop();
}

bool ServerApp::startSession(
    const std::string& session_id,
    const std::string& resource_key)
{
    if (session_id.empty() || resource_key.empty()) {
        log_line("session start rejected: session ID and resource key are required");
        return false;
    }
    if (sessions_.find(session_id) != sessions_.end()) {
        log_line("session '" + session_id + "' already exists");
        return false;
    }
    if (!resources_.acquire(resource_key)) {
        log_line("session '" + session_id + "' could not acquire resource '"
            + resource_key + "'");
        return false;
    }

    try {
        auto transport = transport_factory_.create();
        if (transport == nullptr) {
            resources_.release(resource_key);
            log_line("session '" + session_id + "' failed to start its transport");
            return false;
        }
        if (!transport->start(session_id)) {
            transport->stop();
            resources_.release(resource_key);
            log_line("session '" + session_id + "' failed to start its transport");
            return false;
        }

        Session session;
        session.resource_key = resource_key;
        session.transport = std::move(transport);
        const auto result = sessions_.emplace(session_id, std::move(session));
        if (!result.second) {
            resources_.release(resource_key);
            return false;
        }
    } catch (const std::exception& error) {
        resources_.release(resource_key);
        log_line("session '" + session_id + "' setup failed: " + error.what());
        return false;
    } catch (...) {
        resources_.release(resource_key);
        log_line("session '" + session_id + "' setup failed with an unknown error");
        return false;
    }

    log_line("session '" + session_id + "' started with resource '"
        + resource_key + "'");
    return true;
}

bool ServerApp::stopSession(const std::string& session_id)
{
    const auto entry = sessions_.find(session_id);
    if (entry == sessions_.end()) {
        return true;
    }

    Session session = std::move(entry->second);
    sessions_.erase(entry);

    session.transport->stop();
    resources_.release(session.resource_key);
    log_line("session '" + session_id + "' stopped and returned its resource");
    return true;
}

void ServerApp::reload()
{
    ++configuration_generation_;
    log_line("application reload applied; configuration generation="
        + std::to_string(configuration_generation_));
}

void ServerApp::stop()
{
    while (!sessions_.empty()) {
        stopSession(sessions_.begin()->first);
    }
}

std::size_t ServerApp::sessionCount() const
{
    return sessions_.size();
}
