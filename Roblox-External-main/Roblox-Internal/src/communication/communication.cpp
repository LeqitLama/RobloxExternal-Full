#include "communication.hpp"
#include "../script_scheduler/script_scheduler.hpp"
#include <iostream>

bool communication_t::setup()
{
    // Create named pipe for communication with external UI
    pipe_handle = CreateNamedPipeA(
        configuration::pipe_name,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,          // Max instances
        4096,       // Output buffer size
        4096,       // Input buffer size
        0,          // Default timeout
        nullptr     // Security attributes
    );

    if (pipe_handle == INVALID_HANDLE_VALUE)
    {
        // Fallback: try to connect to existing pipe as client
        pipe_handle = CreateFileA(
            configuration::pipe_name,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (pipe_handle == INVALID_HANDLE_VALUE)
            return false;
    }

    connected = true;
    return true;
}

void communication_t::receive_data()
{
    if (!connected || pipe_handle == INVALID_HANDLE_VALUE)
        return;

    char buffer[8192];
    DWORD bytes_read;

    while (connected)
    {
        // Wait for client connection if we're the server
        ConnectNamedPipe(pipe_handle, nullptr);

        // Read incoming data
        if (ReadFile(pipe_handle, buffer, sizeof(buffer) - 1, &bytes_read, nullptr))
        {
            buffer[bytes_read] = '\0';
            std::string script(buffer);

            // Add to script queue
            if (!script.empty())
            {
                if (script_callback)
                    script_callback(script);
                else
                    script_scheduler->add_to_queue(script);
            }

            // Send acknowledgment
            send_response("OK");
        }
        else
        {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE)
            {
                // Client disconnected, wait for new connection
                DisconnectNamedPipe(pipe_handle);
                continue;
            }
            break;
        }
    }
}

void communication_t::shutdown()
{
    connected = false;
    if (pipe_handle != INVALID_HANDLE_VALUE)
    {
        DisconnectNamedPipe(pipe_handle);
        CloseHandle(pipe_handle);
        pipe_handle = INVALID_HANDLE_VALUE;
    }
}

bool communication_t::is_connected() const
{
    return connected;
}

void communication_t::set_script_callback(std::function<void(const std::string&)> callback)
{
    script_callback = callback;
}

bool communication_t::send_response(const std::string& response)
{
    if (!connected || pipe_handle == INVALID_HANDLE_VALUE)
        return false;

    DWORD bytes_written;
    return WriteFile(pipe_handle, response.c_str(), static_cast<DWORD>(response.size()), &bytes_written, nullptr);
}
