#include "shared_resource_pool.hpp"

#include "logger.hpp"

#include <utility>

SharedResourcePool::~SharedResourcePool()
{
    shutdown();
}

bool SharedResourcePool::acquire(const std::string& key)
{
    if (key.empty()) {
        log_line("cannot acquire a shared resource with an empty key");
        return false;
    }

    const auto existing = resources_.find(key);
    if (existing != resources_.end()) {
        ++existing->second.references;
        log_line("resource '" + key + "' reused; references="
            + std::to_string(existing->second.references));
        return true;
    }

    auto service = std::make_unique<SharedResourceService>(key);
    service->start();

    Entry entry;
    entry.service = std::move(service);
    entry.references = 1;
    resources_.emplace(key, std::move(entry));
    log_line("resource '" + key + "' acquired; references=1");
    return true;
}

void SharedResourcePool::release(const std::string& key)
{
    const auto entry = resources_.find(key);
    if (entry == resources_.end()) {
        log_line("resource '" + key + "' release ignored; it is not in the pool");
        return;
    }

    if (--entry->second.references > 0) {
        log_line("resource '" + key + "' returned; references="
            + std::to_string(entry->second.references));
        return;
    }

    auto service = std::move(entry->second.service);
    resources_.erase(entry);
    log_line("last reference to resource '" + key + "' returned");
    service->stop();
}

void SharedResourcePool::shutdown()
{
    while (!resources_.empty()) {
        auto entry = resources_.begin();
        const std::string key = entry->first;
        const std::size_t references = entry->second.references;
        auto service = std::move(entry->second.service);
        resources_.erase(entry);

        if (references != 0) {
            log_line("pool shutdown is cleaning up resource '" + key
                + "' with " + std::to_string(references) + " outstanding reference(s)");
        }
        service->stop();
    }
}
