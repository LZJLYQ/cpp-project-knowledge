#pragma once

#include <iostream>
#include <mutex>
#include <string>

inline void log_line(const std::string& message)
{
    static std::mutex output_mutex;
    const std::lock_guard<std::mutex> lock(output_mutex);
    std::cout << message << '\n';
}
