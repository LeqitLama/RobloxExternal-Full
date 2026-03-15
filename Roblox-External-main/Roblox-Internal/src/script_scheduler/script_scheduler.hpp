#pragma once
#include <iostream>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <memory>

#include "../script_execution/script_execution.hpp"

class script_scheduler_t
{
private:
    std::queue<std::string> script_queue;
    std::mutex queue_mutex;
    bool running = false;
    std::thread scheduler_thread;

    void scheduler_loop();

public:
    void add_to_queue(const std::string& script);
    void initialize();
    void shutdown();
    bool is_running() const;
    size_t queue_size();
};

inline auto script_scheduler = std::make_unique<script_scheduler_t>();
