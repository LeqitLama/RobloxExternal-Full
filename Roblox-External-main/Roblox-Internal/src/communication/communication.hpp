#pragma once
#include <Windows.h>
#include <string>
#include <memory>
#include <functional>

#include "../core/configuration.hpp"

class communication_t
{
private:
    HANDLE pipe_handle = INVALID_HANDLE_VALUE;
    bool connected = false;
    std::function<void(const std::string&)> script_callback;

public:
    bool setup();
    void receive_data();
    void shutdown();
    bool is_connected() const;
    void set_script_callback(std::function<void(const std::string&)> callback);
    bool send_response(const std::string& response);
};

inline auto communication = std::make_unique<communication_t>();
