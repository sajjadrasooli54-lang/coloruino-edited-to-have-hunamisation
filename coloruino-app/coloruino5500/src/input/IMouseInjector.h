#pragma once

class IMouseInjector {
public:
    virtual ~IMouseInjector() = default;

    // Relative mouse movement with humanization
    virtual void Move(int dx, int dy) = 0;

    // Left click (press + release) with randomized hold duration
    virtual void Click() = 0;

    // Silent aim: move + click + snapback (4 reports)
    virtual void SilentAim(int dx, int dy) = 0;

    // Flicker: move + click (no snapback, 3 reports)
    virtual void Flick(int dx, int dy) = 0;

    // Set cooldown for silent aim (ms)
    virtual void SetCooldown(int ms) = 0;
};

#include <memory>
extern std::unique_ptr<IMouseInjector> g_injector;