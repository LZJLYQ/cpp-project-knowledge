#pragma once

#include <string>

class ISharedResourcePool
{
public:
    virtual ~ISharedResourcePool() = default;

    virtual bool acquire(const std::string& key) = 0;
    virtual void release(const std::string& key) = 0;
};
