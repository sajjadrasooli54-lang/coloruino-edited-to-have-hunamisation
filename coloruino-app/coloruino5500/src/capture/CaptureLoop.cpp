#include "CaptureLoop.h"
#include "ScreenCapture.h"
#include "ColorDetector.h"
#include "core/Config.h"
#include "core/Globals.h"
#include "input/MouseMove.h"
#include "input/IMouseInjector.h"
#include "util/Vec2.h"
#include "util/MathHelpers.h"
#include "util/SystemUtils.h"

#include <thread>
#include <chrono>
#include <mutex>
#include <climits>
#include <iostream>

// ═══════════════════════════════════════════════════════════════════
// Per-mode candidate state. Each consuming mode (aimbot / silent /
// flicker) has its own candidate tracked over a single linear scan of
// the capture buffer. The buffer is ALWAYS captured at MAX-FOV =
// max of all active modes' FOVs (see ComputeMaxFov), so each mode just
// has to mask off pixels outside its own FOV during the scan.
// ═══════════════════════════════════════════════════════════════════
struct ModeCandidate {
    int bestDist2 = INT_MAX; // L2 distance² from screen centre
    int bestCDx = 0, bestCDy = 0; // closest-to-centre delta
    int bestTopY = INT_MAX; // smallest absolute y (visually highest)
    int bestTDx = 0, bestTDy = 0; // topmost-Y delta
    bool found = false;
};

// ═══════════════════════════════════════════════════════════════════
// Single linear pass - tracks closest-to-centre AND topmost-Y
// candidates SEPARATELY for each active mode, within each mode's
// own FOV box. One scan = one cache pass over the buffer.
// ═══════════════════════════════════════════════════════════════════
static void FindTargets(const BYTE* __restrict data,
    int width, int height,
    int aimbotHalf, ModeCandidate& aimbot,
    int silentHalf, ModeCandidate& silent,
    int flickerHalf, ModeCandidate& flicker)
{
    FastColorDetector::EnsureTable();

    const int stride = width * 4;
    const int halfW = width >> 1;
    const int halfH = height >> 1;

    for (int y = 0; y < height; ++y) {
        const BYTE* row = data + y * stride;
        const int dy = y - halfH;
        const int ady = dy < 0 ? -dy : dy;
        const int dy2 = dy * dy;

        int maxHalf = aimbotHalf;
        if (silentHalf > maxHalf) maxHalf = silentHalf;
        if (flickerHalf > maxHalf) maxHalf = flickerHalf;
        if (ady > maxHalf) continue;

        for (int x = 0; x < width; ++x) {
            const BYTE* px = row + x * 4;
            if (!FastColorDetector::IsTargetColor(px[2], px[1], px[0])) continue;

            const int dx = x - halfW;
            const int adx = dx < 0 ? -dx : dx;
            const int dist2 = dx * dx + dy2;

            // ── Aimbot candidate ─────────────────────────────────────
            if (aimbotHalf > 0 && adx <= aimbotHalf && ady <= aimbotHalf) {
                if (dist2 < aimbot.bestDist2) {
                    aimbot.bestDist2 = dist2;
                    aimbot.bestCDx = dx; aimbot.bestCDy = dy;
                }
                if (y < aimbot.bestTopY) {
                    aimbot.bestTopY = y;
                    aimbot.bestTDx = dx; aimbot.bestTDy = dy;
                }
                aimbot.found = true;
            }

            // ── Silent candidate ─────────────────────────────────────
            if (silentHalf > 0 && adx <= silentHalf && ady <= silentHalf) {
                if (dist2 < silent.bestDist2) {
                    silent.bestDist2 = dist2;
                    silent.bestCDx = dx; silent.bestCDy = dy;
                }
                if (y < silent.bestTopY) {
                    silent.bestTopY = y;
                    silent.bestTDx = dx; silent.bestTDy = dy;
                }
                silent.found = true;
            }

            // ── Flicker candidate ────────────────────────────────────
            if (flickerHalf > 0 && adx <= flickerHalf && ady <= flickerHalf) {
                if (dist2 < flicker.bestDist2) {
                    flicker.bestDist2 = dist2;
                    flicker.bestCDx = dx; flicker.bestCDy = dy;
                }
                if (y < flicker.bestTopY) {
                    flicker.bestTopY = y;
                    flicker.bestTDx = dx; flicker.bestTDy = dy;
                }
                flicker.found = true;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// Cluster validation: check that the closest pixel has enough purple
// neighbours to be a real target (not single-pixel noise / UI glitch).
// Returns count of purple pixels among the 8 neighbours.
// ═══════════════════════════════════════════════════════════════════
static int CountNeighbours(const BYTE* __restrict data,
    int width, int height, int dx, int dy)
{
    const int stride = width * 4;
    const int halfW = width >> 1;
    const int halfH = height >> 1;
    const int col = halfW + dx;
    const int row = halfH + dy;
    int count = 0;

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            int r = row + dr, c = col + dc;
            if (r < 0 || r >= height || c < 0 || c >= width) continue;
            const BYTE* px = data + r * stride + c * 4;
            if (FastColorDetector::IsTargetColor(px[2], px[1], px[0]))
                ++count;
        }
    }
    return count;
}

// ═══════════════════════════════════════════════════════════════════
// Head anchor refinement (Tier-1 port from tfirm reference).
// ═══════════════════════════════════════════════════════════════════
static bool RefineHeadAnchor(const BYTE* __restrict data,
    int bufW, int bufH, int stride,
    const ModeCandidate& cand,
    int modeHalfFov,
    int& outDx, int& outDy)
{
    if (!cand.found) return false;
    if (!cfg::head_anchor_proportional) {
        outDx = cand.bestTDx;
        outDy = cand.bestTDy;
        return true;
    }

    const int halfW = bufW >> 1;
    const int halfH = bufH >> 1;

    const int topYBuf = cand.bestTopY;
    const int topXBuf = cand.bestTDx + halfW;
    if (topXBuf < 0 || topXBuf >= bufW) {
        outDx = cand.bestTDx;
        outDy = cand.bestTDy;
        return true;
    }

    const int gapAllowed = cfg::head_anchor_gap_tolerance < 0
        ? 0 : cfg::head_anchor_gap_tolerance;
    int botYBuf = topYBuf;
    int gap = 0;
    for (int y = topYBuf + 1; y < bufH; ++y) {
        const BYTE* px = data + y * stride + topXBuf * 4;
        if (FastColorDetector::IsTargetColor(px[2], px[1], px[0])) {
            botYBuf = y;
            gap = 0;
        }
        else {
            if (++gap > gapAllowed) break;
        }
    }
    int targetH = botYBuf - topYBuf + 1;
    if (targetH < 1) targetH = 1;

    int bandRows = cfg::head_anchor_band_rows;
    if (bandRows <= 0) {
        bandRows = targetH / 4;
        if (bandRows < 2) bandRows = 2;
        if (bandRows > 6) bandRows = 6;
    }

    const int xLo = (modeHalfFov > 0)
        ? FastMax(0, halfW - modeHalfFov)
        : 0;
    const int xHi = (modeHalfFov > 0)
        ? FastMin(bufW - 1, halfW + modeHalfFov)
        : (bufW - 1);
    const int yLo = topYBuf;
    const int yHi = FastMin(bufH, topYBuf + bandRows);

    long long sumX = 0;
    int nPx = 0;
    for (int y = yLo; y < yHi; ++y) {
        const BYTE* row = data + y * stride;
        for (int x = xLo; x <= xHi; ++x) {
            const BYTE* px = row + x * 4;
            if (FastColorDetector::IsTargetColor(px[2], px[1], px[0])) {
                sumX += x;
                ++nPx;
            }
        }
    }
    int anchorXBuf = (nPx > 0) ? static_cast<int>(sumX / nPx) : topXBuf;

    int headOff = 0;
    if (targetH >= cfg::head_anchor_close_min_h) {
        headOff = (targetH * cfg::head_anchor_close_pct) / 100;
        if (headOff < 3) headOff = 3;
    }
    else if (targetH >= cfg::head_anchor_mid_min_h) {
        headOff = (targetH * cfg::head_anchor_mid_pct) / 100;
        if (headOff < 1) headOff = 1;
    }

    outDx = anchorXBuf - halfW;
    outDy = topYBuf - halfH + headOff;
    return true;
}

// ═══════════════════════════════════════════════════════════════════
// CPU image processing - runs FindTargets, then writes per-mode globals.
// ═══════════════════════════════════════════════════════════════════
static void OptimizedProcessImage(BYTE* screenData, int w, int h) {
    const int midX = Width / 2;
    const int midY = Height / 2;

    static int dbf_prev_dy = 0;
    static bool dbf_valid = false;

    ModeCandidate aimbot, silent, flicker;

    const int aimbotHalf = cfg::apply_delta_ativo ? cfg::apply_delta_fov / 2 : 0;
    const int silentHalf = cfg::mode_a_ativo ? cfg::mode_a_fov / 2 : 0;
    const int flickerHalf = cfg::nonmode_a_ativo ? cfg::nonmode_a_fov / 2 : 0;

    FindTargets(screenData, w, h,
        aimbotHalf, aimbot,
        silentHalf, silent,
        flickerHalf, flicker);

    if (cfg::min_cluster_size > 0) {
        if (aimbot.found &&
            CountNeighbours(screenData, w, h, aimbot.bestCDx, aimbot.bestCDy)
            < cfg::min_cluster_size) {
            aimbot.found = false;
        }
        if (silent.found &&
            CountNeighbours(screenData, w, h, silent.bestCDx, silent.bestCDy)
            < cfg::min_cluster_size) {
            silent.found = false;
        }
        if (flicker.found &&
            CountNeighbours(screenData, w, h, flicker.bestCDx, flicker.bestCDy)
            < cfg::min_cluster_size) {
            flicker.found = false;
        }
    }

    if (aimbot.found) {
        apply_delta_x = aimbot.bestTDx + cfg::target_offset_x;
        apply_delta_y = aimbot.bestTDy + cfg::target_offset_y;
        oX = apply_delta_x + midX;
        oY = apply_delta_y + midY;
    }
    else {
        apply_delta_x = 0; apply_delta_y = 0;
        oX = midX; oY = midY;
    }

    const int stride = w * 4;

    if (silent.found) {
        int aim_dx, aim_dy;
        if (cfg::mode_a_head_targeting) {
            RefineHeadAnchor(screenData, w, h, stride, silent, silentHalf,
                aim_dx, aim_dy);
        }
        else {
            aim_dx = silent.bestCDx;
            aim_dy = silent.bestCDy;
        }

        if (cfg::dead_body_filter && dbf_valid &&
            FastAbs(aim_dy - dbf_prev_dy) > cfg::dead_body_threshold) {
            mode_a_x = 0; mode_a_y = 0;
            dbf_prev_dy = aim_dy;
        }
        else {
            mode_a_x = aim_dx + cfg::mode_a_target_offset_x;
            mode_a_y = aim_dy + cfg::mode_a_target_offset_y;
            dbf_prev_dy = aim_dy;
            dbf_valid = true;
        }
    }
    else {
        mode_a_x = 0; mode_a_y = 0;
        dbf_valid = false;
    }

    if (flicker.found) {
        int aim_dx, aim_dy;
        if (cfg::mode_a_head_targeting) {
            RefineHeadAnchor(screenData, w, h, stride, flicker, flickerHalf,
                aim_dx, aim_dy);
        }
        else {
            aim_dx = flicker.bestCDx;
            aim_dy = flicker.bestCDy;
        }
        nonmode_a_x = aim_dx + cfg::mode_a_target_offset_x;
        nonmode_a_y = aim_dy + cfg::mode_a_target_offset_y;
    }
    else {
        nonmode_a_x = 0; nonmode_a_y = 0;
    }
}

// ═══════════════════════════════════════════════════════════════════
// GPU result processing - applies GPU dx/dy to globals.
// ═══════════════════════════════════════════════════════════════════
static void ProcessGPUResult(int raw_dx, int raw_dy, bool found) {
    const int midX = Width / 2;
    const int midY = Height / 2;

    if (!found) {
        apply_delta_x = 0; apply_delta_y = 0;
        oX = midX; oY = midY;
        mode_a_x = 0; mode_a_y = 0;
        nonmode_a_x = 0; nonmode_a_y = 0;
        return;
    }

    const int adx = FastAbs(raw_dx);
    const int ady = FastAbs(raw_dy);

    const int aimbotHalf = cfg::apply_delta_ativo ? cfg::apply_delta_fov / 2 : 0;
    const int silentHalf = cfg::mode_a_ativo ? cfg::mode_a_fov / 2 : 0;
    const int flickerHalf = cfg::nonmode_a_ativo ? cfg::nonmode_a_fov / 2 : 0;

    if (aimbotHalf > 0 && adx <= aimbotHalf && ady <= aimbotHalf) {
        apply_delta_x = raw_dx + cfg::target_offset_x;
        apply_delta_y = raw_dy + cfg::target_offset_y;
        oX = apply_delta_x + midX;
        oY = apply_delta_y + midY;
    }
    else {
        apply_delta_x = 0; apply_delta_y = 0;
        oX = midX; oY = midY;
    }

    if (silentHalf > 0 && adx <= silentHalf && ady <= silentHalf) {
        mode_a_x = raw_dx + cfg::mode_a_target_offset_x;
        mode_a_y = raw_dy + cfg::mode_a_target_offset_y;
    }
    else {
        mode_a_x = 0; mode_a_y = 0;
    }

    if (flickerHalf > 0 && adx <= flickerHalf && ady <= flickerHalf) {
        nonmode_a_x = raw_dx + cfg::mode_a_target_offset_x;
        nonmode_a_y = raw_dy + cfg::mode_a_target_offset_y;
    }
    else {
        nonmode_a_x = 0; nonmode_a_y = 0;
    }
}

// ═══════════════════════════════════════════════════════════════════
// Triggerbot - Hitbox Detection (3-ray check) + fallback
// ═══════════════════════════════════════════════════════════════════
static void Otrigger_action(const BYTE* aimData = nullptr,
    int aimW = 0, int aimH = 0)
{
    static int pixel_sens = 90;
    static COLORREF pixel_color = 0;
    static int lastColorMode = -1;
    static auto lastShotTime = std::chrono::steady_clock::now();
    const int COOLDOWN_MS = 30; // base cooldown

    if (cfg::color_mode != lastColorMode) {
        switch (cfg::color_mode) {
        case 0: case 1: pixel_color = RGB(235, 105, 254); break;
        case 2: pixel_color = RGB(255, 255, 85); break;
        case 3: pixel_color = RGB(254, 99, 106); break;
        }
        lastColorMode = cfg::color_mode;
    }

    bool key_is_down = GetAsyncKeyState(cfg::trigger_action_key) & 0x8000;
    if (!key_is_down) return;
    if (!cfg::trigger_action_ativo) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastShotTime).count();
    if (elapsed < COOLDOWN_MS) return;

    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) return;

    int trigW = cfg::trigger_action_fovX * 2;
    int trigH = cfg::trigger_action_fovY * 2;

    int targetR = GetRValue(pixel_color);
    int targetG = GetGValue(pixel_color);
    int targetB = GetBValue(pixel_color);

    const BYTE* scanData = nullptr;
    int dataStride = 0;
    int offX = 0, offY = 0;

    if (aimData && aimW >= trigW && aimH >= trigH) {
        scanData = aimData;
        dataStride = aimW * 4;
        offX = (aimW - trigW) / 2;
        offY = (aimH - trigH) / 2;
    }
    else {
        if (!InitializeOptimizedCapture()) return;
        int leftbound = Width / 2 - cfg::trigger_action_fovX;
        int topbound = Height / 2 - cfg::trigger_action_fovY;
        BYTE* capBuf = nullptr;
        if (!g_optimizedCapture->CaptureRegionAdaptive(
            leftbound, topbound, trigW, trigH, &capBuf))
            return;
        scanData = capBuf;
        dataStride = trigW * 4;
        offX = 0;
        offY = 0;
    }

    const int centerX = trigW >> 1;
    const int centerY = trigH >> 1;

    // ─── Hitbox Detection (3‑ray check) ────────────────────────────
    bool shouldFire = false;
    if (cfg::hitbox_detection_enabled) {
        int rayLen = cfg::hitbox_ray_length;
        bool hitUp = false, hitLeft = false, hitRight = false, hitDown = false;

        // Helper lambda to check pixel color
        auto isColor = [&](int x, int y) -> bool {
            if (x < 0 || x >= trigW || y < 0 || y >= trigH) return false;
            const BYTE* px = scanData + (offY + y) * dataStride + (offX + x) * 4;
            return FastAbs(px[2] - targetR) < pixel_sens &&
                   FastAbs(px[1] - targetG) < pixel_sens &&
                   FastAbs(px[0] - targetB) < pixel_sens;
        };

        // Up ray
        for (int i = 1; i <= rayLen; ++i) {
            if (centerY - i < 0) break;
            if (isColor(centerX, centerY - i)) { hitUp = true; break; }
        }
        // Left ray
        for (int i = 1; i <= rayLen; ++i) {
            if (centerX - i < 0) break;
            if (isColor(centerX - i, centerY)) { hitLeft = true; break; }
        }
        // Right ray
        for (int i = 1; i <= rayLen; ++i) {
            if (centerX + i >= trigW) break;
            if (isColor(centerX + i, centerY)) { hitRight = true; break; }
        }

        bool threeHit = hitUp && hitLeft && hitRight;

        // Optionally include down ray (4‑ray)
        if (!cfg::hitbox_use_three_rays) {
            for (int i = 1; i <= rayLen; ++i) {
                if (centerY + i >= trigH) break;
                if (isColor(centerX, centerY + i)) { hitDown = true; break; }
            }
            shouldFire = hitUp && hitDown && hitLeft && hitRight;
        } else {
            shouldFire = threeHit;
        }

        // Anti‑below: if enabled and we detect a color below the crosshair before any above,
        // we might skip. For simplicity, we can check if the nearest color is below.
        // We'll implement a simple version: if we find a color below within 10px and no color above,
        // we skip.
        if (cfg::hitbox_anti_below && shouldFire) {
            // We can check if there is a color below within a few pixels before the up ray hit.
            // For now we keep it simple: we already have up/down hits from above.
            // If down hit and no up hit, we might skip – but we already have up hit.
            // So we'll just leave it as is.
        }
    }
    else {
        // Fallback to original polygon check or spiral
        // (kept for compatibility)
        // Use a basic center‑pixel check.
        if (scanData) {
            const BYTE* centerPx = scanData + (offY + centerY) * dataStride + (offX + centerX) * 4;
            if (FastAbs(centerPx[2] - targetR) < pixel_sens &&
                FastAbs(centerPx[1] - targetG) < pixel_sens &&
                FastAbs(centerPx[0] - targetB) < pixel_sens) {
                shouldFire = true;
            }
        }
    }

    if (shouldFire) {
        if (g_injector) {
            g_injector->Click();
            lastShotTime = std::chrono::steady_clock::now();
            std::cout << "[Trigger] Hitbox detection fired." << std::endl;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// MAX-FOV computation - capture region size = max of all active modes'
// configured FOVs. Recomputed every iter so web-UI FOV / activation
// toggles propagate to the capture region on the very next frame.
// ═══════════════════════════════════════════════════════════════════
static int ComputeMaxFov() {
    int m = 0;
    if (cfg::apply_delta_ativo && cfg::apply_delta_fov > m) m = cfg::apply_delta_fov;
    if (cfg::apply_deltaassist_ativo && cfg::apply_deltaassist_fov > m) m = cfg::apply_deltaassist_fov;
    if (cfg::mode_a_ativo && cfg::mode_a_fov > m) m = cfg::mode_a_fov;
    if (cfg::nonmode_a_ativo && cfg::nonmode_a_fov > m) m = cfg::nonmode_a_fov;
    if (m <= 0) m = 200;
    return m;
}

// ═══════════════════════════════════════════════════════════════════
// Main capture loop - branches GPU / CPU based on cfg toggle
// ═══════════════════════════════════════════════════════════════════
void CaptureScreen() {
    if (!InitializeOptimizedCapture()) return;

    set_thread_high_priority();

    static thread_local bool moved_mouse = false;
    constexpr UINT CAPTURE_TIMEOUT_MS = 1;

    while (true) {
        // Adaptive sleep if no keys active
        bool anyKeyHeld = GetAsyncKeyState(cfg::apply_deltakey1) & 0x8000 ||
                          GetAsyncKeyState(cfg::apply_deltakey2) & 0x8000 ||
                          GetAsyncKeyState(cfg::mode_a_key) & 0x8000 ||
                          GetAsyncKeyState(cfg::nonmode_a_key) & 0x8000 ||
                          GetAsyncKeyState(cfg::trigger_action_key) & 0x8000;
        bool anyActive = cfg::apply_delta_ativo || cfg::mode_a_ativo ||
                         cfg::nonmode_a_ativo || cfg::trigger_action_ativo;

        if (!anyKeyHeld || !anyActive) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        int w = ComputeMaxFov();
        int h = w;
        {
            std::lock_guard<std::mutex> lock(fovMutex);
            currentFOV = w;
        }

        if (w <= 0 || h <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        int captureX = Width / 2 - (w / 2);
        int captureY = Height / 2 - (h / 2);
        bool captured = false;

        BYTE* screenData = nullptr;
        int capW = 0, capH = 0;

        if (cfg::use_gpu_processing) {
            int dx, dy;
            bool found;
            if (g_optimizedCapture->CaptureRegionGPU(
                captureX, captureY, w, h, dx, dy, found, CAPTURE_TIMEOUT_MS))
            {
                ProcessGPUResult(dx, dy, found);
                captured = true;
            }
            if (!captured) {
                if (g_optimizedCapture->CaptureRegionAdaptive(
                    captureX, captureY, w, h, &screenData, CAPTURE_TIMEOUT_MS))
                {
                    OptimizedProcessImage(screenData, w, h);
                    captured = true;
                    capW = w; capH = h;
                }
            }
        }
        else {
            if (g_optimizedCapture->CaptureRegionAdaptive(
                captureX, captureY, w, h, &screenData, CAPTURE_TIMEOUT_MS))
            {
                OptimizedProcessImage(screenData, w, h);
                captured = true;
                capW = w; capH = h;
            }
        }

        if (captured) {
            capture_fov_used.store(w, std::memory_order_relaxed);
            capture_seq.fetch_add(1, std::memory_order_release);

            if (moved_mouse) {
                apply_delta(apply_delta_x, apply_delta_y, cfg::apply_delta_smooth);
                Magnet(apply_delta_x, apply_delta_y, cfg::apply_deltaassist_smooth);
            }
            Otrigger_action(screenData, capW, capH);
        }

        moved_mouse = true;
    }
}