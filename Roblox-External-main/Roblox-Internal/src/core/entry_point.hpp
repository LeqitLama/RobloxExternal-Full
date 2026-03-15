#pragma once
#include <Windows.h>
#include <iostream>
#include <thread>
#include <memory>

// Core includes
#include "../core/globals.hpp"
#include "../core/configuration.hpp"
#include "../core/offsets.hpp"

// Engine includes
#include "../engine/graphics/graphics.hpp"
#include "../engine/data_model/data_model.hpp"

// Script system includes
#include "../script_execution/script_execution.hpp"
#include "../script_environment/script_environment.hpp"
#include "../script_scheduler/script_scheduler.hpp"
#include "../communication/communication.hpp"

// Overlay
#include "../overlay/overlay.hpp"

namespace entry_point
{
    void main_thread(HMODULE dll_module);
    bool initialize_engine();
    void start_services();
}
