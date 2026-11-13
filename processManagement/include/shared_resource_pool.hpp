#pragma once

#include "resource_pool.hpp"
#include "service.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <string>

class SharedResourcePool final : public ISharedResourcePool
{
public:
    ~SharedResourcePool() override;

    bool acquire(const std::string& key) override;
    void release(const std::string& key) override;
    void shutdown();

private:
    struct Entry
    {
        std::unique_ptr<SharedResourceService> service;
        std::size_t references{0};
    };

    std::map<std::string, Entry> resources_;
};
