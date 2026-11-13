#include "service.hpp"

#include "logger.hpp"

#include <chrono>
#include <exception>
#include <string>
#include <utility>

SharedResourceService::SharedResourceService(std::string resource_key)
    : resource_key_(std::move(resource_key))
{
}

SharedResourceService::~SharedResourceService()
{
    stop();
}

void SharedResourceService::start()
{
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            return;
        }
        running_ = true;
    }

    try {
        worker_ = std::thread(&SharedResourceService::run, this);
    } catch (const std::exception&) {
        const std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        throw;
    }

    log_line("shared resource '" + resource_key_ + "' started");
}

void SharedResourceService::stop()
{
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }

    condition_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
        log_line("shared resource '" + resource_key_ + "' stopped");
    }
}

void SharedResourceService::run()
{
    std::unique_lock<std::mutex> lock(mutex_);
    log_line("worker for resource '" + resource_key_ + "' started");

    while (running_) {
        const bool stopping = condition_.wait_for(
            lock, std::chrono::seconds(3), [this] { return !running_; });

        if (stopping) {
            break;
        }
        log_line("worker heartbeat for resource '" + resource_key_ + "'");
    }

    lock.unlock();
    log_line("worker for resource '" + resource_key_ + "' exiting");
}
