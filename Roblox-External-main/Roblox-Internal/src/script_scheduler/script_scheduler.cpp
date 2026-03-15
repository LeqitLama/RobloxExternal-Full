#include "script_scheduler.hpp"
#include <chrono>

void script_scheduler_t::scheduler_loop()
{
    while (running)
    {
        std::string script;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (script_queue.empty())
            {
                // Small sleep to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            script = script_queue.front();
            script_queue.pop();
        }

        // Execute the script
        if (!script.empty())
        {
            script_execution->execute_script(script);
        }

        // Small delay between script executions
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void script_scheduler_t::add_to_queue(const std::string& script)
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    script_queue.push(script);
}

void script_scheduler_t::initialize()
{
    if (running)
        return;

    running = true;
    scheduler_thread = std::thread(&script_scheduler_t::scheduler_loop, this);
}

void script_scheduler_t::shutdown()
{
    running = false;
    if (scheduler_thread.joinable())
        scheduler_thread.join();
}

bool script_scheduler_t::is_running() const
{
    return running;
}

size_t script_scheduler_t::queue_size()
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    return script_queue.size();
}
