---
description: How to use the Lua Executor
---

# Lua Executor Usage Guide

The Lua Executor is a powerful feature that allows you to run custom Lua scripts within Roblox with elevated privileges (Identity 8).

## Prerequisites
1.  **Build the Project**: Ensure you have built the solution in **Release x64** mode.
2.  **Roblox Running**: Start Roblox and join a game.

## Step-by-Step Guide

1.  **Inject the DLL**:
    *   Run `MinusInjector.exe` (located in `build/` or `Minus-Injector/x64/Release/`).
    *   This will inject `MinusInternal.dll` into the Roblox process.
    *   *Note: This is CRITICAL. The executor will NOT work without the DLL injected.*

2.  **Start the External Overlay**:
    *   Run `FreeLamaExternal.exe` (located in `build/`).
    *   The overlay should appear over the Roblox window.

3.  **Open the Menu**:
    *   Press the **INSERT** key on your keyboard to toggle the menu.

4.  **Navigate to Executor**:
    *   Click on the **Executor** tab in the left sidebar (icon looks like a terminal).

5.  **Execute a Script**:
    *   **Type Code**: Enter your Lua code in the large text editor.
    *   **Run**: Click the green **Execute** button.
    *   **Output**: Check the Output panel below the editor for status.
    *   *Tip: You can also use the Roblox Developer Console (F9) to see `print()` output.*

## Features

*   **Built-in Scripts**: Click the "Scripts" button on the right to see a list of pre-made scripts (ESP, Fly, Noclip, etc.). Click one to load it.
*   **History**: Click "History" to see your previously executed scripts.
*   **Load File**: Click "Load File" to run a `.lua` or `.txt` file from your computer.
*   **Identity 8**: Scripts run with high privileges, allowing them to access `CoreGui` and other protected services.

## Troubleshooting

*   **"Internal DLL not detected"**: Ensure you ran the injector successfully. The external tool communicates with the DLL via shared memory.
*   **Script doesn't run**: Check the Roblox Developer Console (F9) for red error messages. Your script might have a syntax error.
*   **Crash**: If Roblox crashes, the offsets might be outdated for the current game version. Check `Minus-Internal/lua_executor.h` for offsets.
