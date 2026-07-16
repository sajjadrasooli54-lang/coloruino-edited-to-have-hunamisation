#include "Config.h"

namespace cfg
{
    int color_mode = 0;

    bool apply_delta_ativo = true;
    int apply_deltakey1 = VK_XBUTTON1;
    int apply_deltakey2 = VK_SHIFT;
    int target_offset_x = 1;
    int target_offset_y = 5;
    int apply_delta_fov = 82;
    float apply_delta_smooth = 1.4f;
    float speed = 0.4f;
    int sleep = 0;

    bool apply_deltaassist_ativo = false;
    int assist_apply_deltakey = VK_MENU;
    int assist_target_offset_x = 2;
    int assist_target_offset_y = 3;
    int apply_deltaassist_fov = 1;
    float apply_deltaassist_smooth = 1.5f;
    float assist_speed = 1.0f;
    int assist_sleep = 0;

    bool nonmode_a_ativo = false;
    int nonmode_a_key = VK_XBUTTON2;
    int nonmode_a_fov = 100;
    int nonmode_a_delay_between_shots = 1;
    float nonmode_a_distance = 2.5f;

    bool mode_a_ativo = true;
    int mode_a_key = VK_XBUTTON2;
    int mode_a_target_offset_x = 0;
    int mode_a_target_offset_y = 3;
    int mode_a_fov = 100;
    int mode_a_delay_between_shots = 1;
    float distance = 2.62f;
    bool mode_a_head_targeting = true;
    int mode_a_cooldown_ms = 50;

    bool useIstrigFilter = true;

    bool trigger_action_ativo = true;
    int trigger_action_key = VK_MENU;
    int trigger_action_delay = 1;
    int trigger_action_fovX = 3;
    int trigger_action_fovY = 3;

    int menorRGB[3] = { 70, 0, 120 };
    int maiorRGB[3] = { 255, 190, 255 };
    int menorHSV[3] = { 270, 38, 40 };
    int maiorHSV[3] = { 310, 100, 100 };

    int ngrok = 1;
    bool use_gpu_processing = false;
    bool dead_body_filter = false;
    int dead_body_threshold = 15;
    int min_cluster_size = 2;
    bool trigger_polygon_check = true;
    bool apply_delta_dist_smoothing = true;
    int apply_delta_near_dist = 10;
    int apply_delta_mid_dist = 30;
    float apply_delta_near_mult = 0.4f;
    float apply_delta_mid_mult = 0.7f;

    bool head_anchor_proportional = true;
    int head_anchor_band_rows = 0;
    int head_anchor_gap_tolerance = 2;
    int head_anchor_close_pct = 18;
    int head_anchor_mid_pct = 10;
    int head_anchor_close_min_h = 30;
    int head_anchor_mid_min_h = 10;

    // Humanisation
    bool sim_enable_humanization = true;
    int sim_jitter_min_us = 100;
    int sim_jitter_max_us = 300;
    int sim_substeps_mode = 0;
    int sim_click_hold_min_ms = 20;
    int sim_click_hold_max_ms = 67;
    int sim_snapback_delay_us = 0;

    float sim_overshoot_percent = 0.10f;
    int sim_pre_delay_min_ms = 20;
    int sim_pre_delay_max_ms = 60;
    int sim_deadzone_pixels = 2;
    int sim_easing_mode = 1;

    int click_burst_limit = 15;
    int click_gap_base_ms = 120;
    int click_gap_jitter_ms = 100;
    int click_gap_floor_ms = 80;
    int click_scatter_percent = 40;
    float click_ln_mu = 4.3f;
    float click_ln_sigma = 0.35f;
    int click_press_min_ms = 50;
    int click_press_max_ms = 200;
    float click_double_chance = 1.5f;
    int click_double_min_ms = 30;
    int click_double_max_ms = 80;

    int drift_step_ms = 3;
    int drift_max_ms = 15;
    int drift_interval_s = 15;
    int drift_interval_jitter_s = 16;

    int reaction_hesit_min_ms = 60;
    int reaction_hesit_max_ms = 140;
    int reaction_hesit_percent = 1;
    int reaction_cd_jitter_percent = 10;
    int reaction_refr_low = 4;
    int reaction_refr_high = 8;
    int reaction_low_dly_min = 15;
    int reaction_low_dly_max = 45;
    int reaction_hi_dly_min = 40;
    int reaction_hi_dly_max = 120;
    int reaction_reset_after_ms = 500;

    bool hitbox_detection_enabled = true;
    int hitbox_ray_length = 100;
    bool hitbox_use_three_rays = true;
    bool hitbox_anti_below = true;

    bool short_stop_enabled = true;
    int short_stop_chance = 5;
    int short_stop_min_ms = 10;
    int short_stop_max_ms = 200;
    int short_stop_mode = 1;
    float short_stop_slow_min = 1.0f;
    float short_stop_slow_max = 3.0f;

    bool track_delay_enabled = true;
    int track_delay_min_ms = 0;
    int track_delay_max_ms = 200;

    bool aofi_enabled = true;
    int aofi_reset_timeout_ms = 2000;

    bool apply_delta_toggle = false;
}