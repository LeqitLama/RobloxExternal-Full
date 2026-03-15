#pragma once
#include <cstdint>

// Roblox Version: version-b130242ed064436f
// Flat Wrapper for Compatibility with existing code
namespace Offsets {
    // -- Global / Engine --
    inline constexpr uintptr_t FakeDataModelPointer = 0x81C2C38;
    inline constexpr uintptr_t FakeDataModelToDataModel = 0x1C0;
    inline constexpr uintptr_t VisualEnginePointer = 0x7D78148;
    inline constexpr uintptr_t viewmatrix = 0x140; // VisualEngine -> RenderView -> ViewMatrix

    // -- Instance / Hierarchy --
    inline constexpr uintptr_t Name = 0xB0;
    inline constexpr uintptr_t Children = 0x70;    // ChildrenStart
    inline constexpr uintptr_t ChildrenEnd = 0x8;
    inline constexpr uintptr_t ClassDescriptor = 0x18;
    inline constexpr uintptr_t Parent = 0x68;

    // -- Player --
    inline constexpr uintptr_t LocalPlayer = 0x130;
    inline constexpr uintptr_t Team = 0x2A0;
    inline constexpr uintptr_t UserId = 0x2D8;
    inline constexpr uintptr_t DisplayName = 0x130;
    inline constexpr uintptr_t ModelInstance = 0x398; // The Character Model
    inline constexpr uintptr_t PlayerMouse = 0xF78;
    inline constexpr uintptr_t MousePosition = 0xEC;

    // -- Humanoid --
    inline constexpr uintptr_t Health = 0x194;
    inline constexpr uintptr_t MaxHealth = 0x1B4;
    inline constexpr uintptr_t WalkSpeed = 0x1D4;
    inline constexpr uintptr_t JumpPower = 0x1B0;
    inline constexpr uintptr_t HumanoidRootPart = 0x4C0; // Defaulting to RootPartR6
    inline constexpr uintptr_t HumanoidState = 0x8D8;

    // -- Part / Primitive --
    inline constexpr uintptr_t Primitive = 0x148;
    inline constexpr uintptr_t Position = 0xE4;   // Relative to Primitive
    inline constexpr uintptr_t Velocity = 0xF0;   // AssemblyLinearVelocity
    inline constexpr uintptr_t Rotation = 0xC8;   // CFrame Rotation updated from 0xC0
    inline constexpr uintptr_t Size = 0x1B0;

    // -- Camera --
    inline constexpr uintptr_t Camera = 0x468;
    inline constexpr uintptr_t CameraRotation = 0xF8;
    inline constexpr uintptr_t FieldOfView = 0x160;

    // -- Misc --
    inline constexpr uintptr_t StringLength = 0x10;
}