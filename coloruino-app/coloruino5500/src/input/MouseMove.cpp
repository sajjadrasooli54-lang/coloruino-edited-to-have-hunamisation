#include "MouseMove.h"
#include "core/Config.h"
#include "core/Globals.h"
#include "input/IMouseInjector.h"
#include "util/MathHelpers.h"

#include <cmath>
#include <algorithm>

extern std::unique_ptr<IMouseInjector> g_injector;

// Per-axis overflow accumulators for smooth movement
static float overflow_x = 0.0f;
static float overflow_y = 0.0f;
static float assist_overflow_x = 0.0f;
static float assist_overflow_y = 0.0f;

void apply_delta(int deltaX, int deltaY, double smooth) {
    if (!cfg::apply_delta_ativo || !g_injector) {
        overflow_x = 0.0f;
        overflow_y = 0.0f;
        return;
    }

    bool key1 = GetAsyncKeyState(cfg::apply_deltakey1) & 0x8000;
    bool key2 = GetAsyncKeyState(cfg::apply_deltakey2) & 0x8000;
    if (!key1 && !key2) {
        overflow_x = 0.0f;
        overflow_y = 0.0f;
        return;
    }

    if (deltaX == 0 && deltaY == 0) {
        overflow_x = 0.0f;
        overflow_y = 0.0f;
        return;
    }

    float dist_factor = 1.0f;
    if (cfg::apply_delta_dist_smoothing) {
        float dist2 = (float)(deltaX * deltaX + deltaY * deltaY);
        float near_dist2 = (float)(cfg::apply_delta_near_dist * cfg::apply_delta_near_dist);
        float mid_dist2 = (float)(cfg::apply_delta_mid_dist * cfg::apply_delta_mid_dist);
        if (dist2 < near_dist2) {
            dist_factor = cfg::apply_delta_near_mult;
        } else if (dist2 < mid_dist2) {
            dist_factor = cfg::apply_delta_mid_mult;
        }
    }

    float effective_smooth = (float)smooth;
    if (effective_smooth < 1.0f) effective_smooth = 1.0f;

    float move_x = ((float)deltaX / effective_smooth) * cfg::speed * dist_factor + overflow_x;
    float move_y = ((float)deltaY / effective_smooth) * cfg::speed * dist_factor + overflow_y;

    int outX = (int)move_x;
    int outY = (int)move_y;

    overflow_x = move_x - (float)outX;
    overflow_y = move_y - (float)outY;

    if (outX != 0 || outY != 0) {
        g_injector->Move(outX, outY);
    }
}

void Magnet(int deltaX, int deltaY, double smooth) {
    if (!cfg::apply_deltaassist_ativo || !g_injector) {
        assist_overflow_x = 0.0f;
        assist_overflow_y = 0.0f;
        return;
    }

    static bool keyPressProcessed = false;
    static bool key_ativa = false;
    bool key_down = GetAsyncKeyState(cfg::assist_apply_deltakey) & 0x8000;

    if (key_down && !keyPressProcessed) {
        key_ativa = !key_ativa;
        keyPressProcessed = true;
    }
    if (!key_down) {
        keyPressProcessed = false;
    }

    if (!key_ativa) {
        assist_overflow_x = 0.0f;
        assist_overflow_y = 0.0f;
        return;
    }

    if (deltaX == 0 && deltaY == 0) {
        assist_overflow_x = 0.0f;
        assist_overflow_y = 0.0f;
        return;
    }

    float effective_smooth = (float)smooth;
    if (effective_smooth < 1.0f) effective_smooth = 1.0f;

    float move_x = ((float)deltaX / effective_smooth) * cfg::assist_speed + assist_overflow_x;
    float move_y = ((float)deltaY / effective_smooth) * cfg::assist_speed + assist_overflow_y;

    int outX = (int)move_x;
    int outY = (int)move_y;

    assist_overflow_x = move_x - (float)outX;
    assist_overflow_y = move_y - (float)outY;

    if (outX != 0 || outY != 0) {
        g_injector->Move(outX, outY);
    }
}

void SnapShoot_P(int deltaX, int deltaY) {
    if (!cfg::mode_a_ativo || !g_injector) return;
    float mult = cfg::distance > 0.0f ? cfg::distance : 1.0f;
    int moveX = (int)((float)deltaX * mult);
    int moveY = (int)((float)deltaY * mult);
    if (moveX != 0 || moveY != 0) {
        g_injector->SilentAim(moveX, moveY);
    }
}

void SnapShoot_F(int deltaX, int deltaY) {
    if (!cfg::nonmode_a_ativo || !g_injector) return;
    float mult = cfg::nonmode_a_distance > 0.0f ? cfg::nonmode_a_distance : 1.0f;
    int moveX = (int)((float)deltaX * mult);
    int moveY = (int)((float)deltaY * mult);
    if (moveX != 0 || moveY != 0) {
        g_injector->Flick(moveX, moveY);
    }
}