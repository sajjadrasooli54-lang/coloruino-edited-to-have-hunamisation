#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace cfg
{
    extern int color_mode;

    extern bool apply_delta_ativo;
    extern int apply_deltakey1;
    extern int apply_deltakey2;
    extern int target_offset_x;
    extern int target_offset_y;
    extern int apply_delta_fov;
    extern float apply_delta_smooth;
    extern float speed;
    extern int sleep;

    extern bool apply_deltaassist_ativo;
    extern int assist_apply_deltakey;
    extern int assist_target_offset_x;
    extern int assist_target_offset_y;
    extern int apply_deltaassist_fov;
    extern float apply_deltaassist_smooth;
    extern float assist_speed;
    extern int assist_sleep;

    extern bool nonmode_a_ativo;
    extern int nonmode_a_key;
    extern int nonmode_a_fov;
    extern int nonmode_a_delay_between_shots;
    extern float nonmode_a_distance;

    extern bool mode_a_ativo;
    extern int mode_a_key;
    extern int mode_a_target_offset_x;
    extern int mode_a_target_offset_y;
    extern int mode_a_fov;
    extern int mode_a_delay_between_shots;
    extern float distance;
    extern bool mode_a_head_targeting;
    extern int mode_a_cooldown_ms;

    extern bool useIstrigFilter;

    extern bool trigger_action_ativo;
    extern int trigger_action_key;
    extern int trigger_action_delay;
    extern int trigger_action_fovX;
    extern int trigger_action_fovY;

    extern int menorRGB[3];
    extern int maiorRGB[3];
    extern int menorHSV[3];
    extern int maiorHSV[3];

    extern int ngrok;
    extern bool use_gpu_processing;
    extern bool dead_body_filter;
    extern int dead_body_threshold;
    extern int min_cluster_size;
    extern bool trigger_polygon_check;
    extern bool apply_delta_dist_smoothing;
    extern int apply_delta_near_dist;
    extern int apply_delta_mid_dist;
    extern float apply_delta_near_mult;
    extern float apply_delta_mid_mult;
    extern bool head_anchor_proportional;
    extern int head_anchor_band_rows;
    extern int head_anchor_gap_tolerance;
    extern int head_anchor_close_pct;
    extern int head_anchor_mid_pct;
    extern int head_anchor_close_min_h;
    extern int head_anchor_mid_min_h;

    // ─── Simulated Mouse Injector Humanisation ──────────────────────
    extern bool sim_enable_humanization;
    extern int sim_jitter_min_us;
    extern int sim_jitter_max_us;
    extern int sim_substeps_mode;
    extern int sim_click_hold_min_ms;
    extern int sim_click_hold_max_ms;
    extern int sim_snapback_delay_us;

    // ─── New humanisation parameters ────────────────────────────────
    extern float sim_overshoot_percent;      // 0.0 - 0.3, fraction to overshoot in silent aim
    extern int sim_pre_delay_min_ms;         // min random delay before movement (ms)
    extern int sim_pre_delay_max_ms;         // max random delay before movement (ms)
    extern int sim_deadzone_pixels;          // if offset is <= this, don't move
    extern int sim_easing_mode;              // 0=linear, 1=easeInOut, 2=easeOut

    // ─── Advanced Click Humanization ──────────────────────────────
    extern int click_burst_limit;
    extern int click_gap_base_ms;
    extern int click_gap_jitter_ms;
    extern int click_gap_floor_ms;
    extern int click_scatter_percent;
    extern float click_ln_mu;
    extern float click_ln_sigma;
    extern int click_press_min_ms;
    extern int click_press_max_ms;
    extern float click_double_chance;
    extern int click_double_min_ms;
    extern int click_double_max_ms;

    // ─── Session Drift ──────────────────────────────────────────────
    extern int drift_step_ms;
    extern int drift_max_ms;
    extern int drift_interval_s;
    extern int drift_interval_jitter_s;

    // ─── Reaction Model ─────────────────────────────────────────────
    extern int reaction_hesit_min_ms;
    extern int reaction_hesit_max_ms;
    extern int reaction_hesit_percent;
    extern int reaction_cd_jitter_percent;
    extern int reaction_refr_low;
    extern int reaction_refr_high;
    extern int reaction_low_dly_min;
    extern int reaction_low_dly_max;
    extern int reaction_hi_dly_min;
    extern int reaction_hi_dly_max;
    extern int reaction_reset_after_ms;

    // ─── Hitbox Detection ──────────────────────────────────────────
    extern bool hitbox_detection_enabled;
    extern int hitbox_ray_length;
    extern bool hitbox_use_three_rays;
    extern bool hitbox_anti_below;

    // ─── Short Stop ──────────────────────────────────────────────────
    extern bool short_stop_enabled;
    extern int short_stop_chance;
    extern int short_stop_min_ms;
    extern int short_stop_max_ms;
    extern int short_stop_mode;
    extern float short_stop_slow_min;
    extern float short_stop_slow_max;

    // ─── Track Delay ────────────────────────────────────────────────
    extern bool track_delay_enabled;
    extern int track_delay_min_ms;
    extern int track_delay_max_ms;

    // ─── AoFI ──────────────────────────────────────────────────────────
    extern bool aofi_enabled;
    extern int aofi_reset_timeout_ms;

    // ─── New: Toggle/Hold option for Aimbot ──────────────────────────
    extern bool apply_delta_toggle;  // true = toggle mode, false = hold mode (default)
}