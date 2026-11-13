#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

class SharedResourceService
{
public:
    explicit SharedResourceService(std::string resource_key);
    ~SharedResourceService();

    SharedResourceService(const SharedResourceService&) = delete;
    SharedResourceService& operator=(const SharedResourceService&) = delete;

    void start();
    void stop();

private:
    void run();

    std::string resource_key_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool running_{false};
    std::thread worker_;
};
