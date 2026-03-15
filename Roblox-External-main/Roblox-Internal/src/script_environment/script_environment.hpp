#pragma once
#include <iostream>
#include <vector>
#include <memory>

#include "../core/globals.hpp"

// Base class for custom Lua libraries
class library_t
{
public:
    virtual ~library_t() = default;
    virtual void initialize(lua_State* L) = 0;
    virtual const char* get_name() const = 0;
};

class script_environment_t
{
private:
    std::vector<std::unique_ptr<library_t>> libraries;

public:
    void initialize();
    void register_library(std::unique_ptr<library_t> library);
    void push_globals(lua_State* L);
};

inline auto script_environment = std::make_unique<script_environment_t>();
