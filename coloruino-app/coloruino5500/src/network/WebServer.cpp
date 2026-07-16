#include "WebServer.h"
#include "core/Config.h"
#include "core/ConfigManager.h"
#include "core/Globals.h"
#include "security/Auth.h"
#include "security/AntiDebug.h"
#include "input/IMouseInjector.h"
#include "util/DynamicApi.h"

#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <functional>
#include <memory>
#include <cstring>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <netfw.h>
#include <comdef.h>
#include <shlwapi.h>
#include <random>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "oleaut32.lib")

extern std::unique_ptr<IMouseInjector> g_injector;
extern std::atomic<bool> g_shutdown_requested;

static SOCKET g_listenSocket = INVALID_SOCKET;
static std::thread g_webThread;
static std::atomic<bool> g_serverRunning{ false };
static std::random_device g_rd;
static std::mt19937 g_rng(g_rd());

// Local IsAdmin check (duplicated from main.cpp for simplicity)
static bool IsAdmin() {
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdminGroup;
    BOOL bResult = AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &AdminGroup);
    if (!bResult) return false;
    BOOL isAdmin = FALSE;
    bResult = CheckTokenMembership(NULL, AdminGroup, &isAdmin);
    FreeSid(AdminGroup);
    return bResult && isAdmin;
}

static const char* HTML_TEMPLATE = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Spotify Web Player</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif; background: #121212; color: #fff; padding: 20px; }
        h1 { font-size: 24px; margin-bottom: 10px; color: #1db954; }
        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
        .header input { background: #2a2a2a; border: none; padding: 8px 12px; border-radius: 20px; color: #fff; width: 200px; }
        .tabs { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 20px; border-bottom: 1px solid #333; padding-bottom: 10px; }
        .tab { padding: 8px 16px; background: #2a2a2a; border-radius: 20px; cursor: pointer; font-size: 14px; transition: 0.2s; }
        .tab.active { background: #1db954; color: #000; }
        .tab:hover { opacity: 0.8; }
        .cards { display: grid; grid-template-columns: 1fr; gap: 20px; }
        .card { background: #1e1e1e; border-radius: 12px; padding: 16px; }
        .card h3 { margin-bottom: 12px; font-size: 16px; color: #aaa; }
        .row { display: flex; flex-wrap: wrap; align-items: center; gap: 12px; margin-bottom: 10px; }
        .row label { min-width: 120px; font-size: 14px; color: #ccc; }
        .row input[type="range"] { flex: 1; min-width: 100px; background: #333; height: 4px; border-radius: 2px; }
        .row input[type="number"], .row input[type="text"] { width: 80px; padding: 4px 8px; background: #2a2a2a; border: 1px solid #444; color: #fff; border-radius: 4px; }
        .toggle { position: relative; display: inline-block; width: 44px; height: 26px; flex-shrink: 0; }
        .toggle input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background: #555; transition: .3s; border-radius: 26px; }
        .slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 3px; bottom: 3px; background: #fff; transition: .3s; border-radius: 50%; }
        .toggle input:checked + .slider { background: #1db954; }
        .toggle input:checked + .slider:before { transform: translateX(18px); }
        .btn { padding: 6px 16px; background: #1db954; border: none; border-radius: 20px; color: #000; cursor: pointer; font-weight: bold; }
        .btn:hover { opacity: 0.9; }
        .toast { position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%); background: #333; padding: 12px 24px; border-radius: 8px; font-size: 14px; opacity: 0; transition: opacity 0.3s; pointer-events: none; }
        .toast.show { opacity: 1; }
        .value-hint { font-size: 12px; color: #888; margin-left: 4px; }
        @media (min-width: 768px) { .cards { grid-template-columns: 1fr 1fr; } }
        @media (min-width: 1024px) { .cards { grid-template-columns: 1fr 1fr 1fr; } }
        .all-cards { display: contents; }
    </style>
</head>
<body>
<div class="header">
    <h1>🎵 Spotify Web Player</h1>
    <div>
        <input type="text" id="search" placeholder="Search settings..." oninput="filterCards()">
        <button class="btn" onclick="saveAll()">Save All</button>
        <button class="btn" onclick="testMove()">Test</button>
    </div>
</div>
<div class="tabs" id="tabContainer">
    <div class="tab active" data-tab="all">All</div>
    <div class="tab" data-tab="aimbot">Aimbot</div>
    <div class="tab" data-tab="silent">Silent</div>
    <div class="tab" data-tab="flicker">Flicker</div>
    <div class="tab" data-tab="trigger">Trigger</div>
    <div class="tab" data-tab="head">Head Anchor</div>
    <div class="tab" data-tab="filtering">Filtering</div>
    <div class="tab" data-tab="color">Color</div>
    <div class="tab" data-tab="performance">Performance</div>
</div>
<div class="cards" id="cardContainer"></div>
<div class="toast" id="toast">Settings saved</div>
<script>
    const cardData = [];
    let allCards = [];

    function makeToggle(key, label, def) {
        return `
        <div class="row">
            <label>${label}</label>
            <div class="toggle"><input type="checkbox" id="chk_${key}" ${def?'checked':''} onchange="save('${key}', this.checked?1:0)"><span class="slider"></span></div>
        </div>`;
    }

    function makeSlider(key, label, min, max, step, def, unit='') {
        return `
        <div class="row">
            <label>${label}</label>
            <input type="range" id="sl_${key}" min="${min}" max="${max}" step="${step}" value="${def}" oninput="updateVal('${key}')">
            <input type="number" id="num_${key}" value="${def}" oninput="updateSl('${key}')" style="width:70px">
            <span class="value-hint">${unit}</span>
        </div>`;
    }

    function updateVal(key) {
        const sl = document.getElementById('sl_'+key);
        const num = document.getElementById('num_'+key);
        if (sl && num) { num.value = sl.value; save(key, parseFloat(sl.value)); }
    }
    function updateSl(key) {
        const sl = document.getElementById('sl_'+key);
        const num = document.getElementById('num_'+key);
        if (sl && num) { sl.value = num.value; save(key, parseFloat(num.value)); }
    }

    function save(key, val) {
        fetch('/'+key+'?value='+encodeURIComponent(val)).catch(()=>{});
        showToast();
    }

    function saveAll() {
        const inputs = document.querySelectorAll('.row input');
        inputs.forEach(inp => {
            if (inp.type === 'checkbox') { const key = inp.id.replace('chk_',''); save(key, inp.checked?1:0); }
            else if (inp.type === 'number' || inp.type === 'range') {
                const key = inp.id.replace('sl_','').replace('num_','');
                const val = inp.value;
                fetch('/'+key+'?value='+encodeURIComponent(val)).catch(()=>{});
            }
        });
        showToast();
    }

    function testMove() { fetch('/testing').catch(()=>{}); }

    function showToast() {
        const t = document.getElementById('toast');
        t.classList.add('show');
        clearTimeout(t._timeout);
        t._timeout = setTimeout(()=>t.classList.remove('show'), 1300);
    }

    function filterCards() {
        const q = document.getElementById('search').value.toLowerCase();
        document.querySelectorAll('.card').forEach(c => {
            const text = c.textContent.toLowerCase();
            c.style.display = text.includes(q) ? '' : 'none';
        });
    }

    function switchTab(tabId) {
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelector(`.tab[data-tab="${tabId}"]`).classList.add('active');
        document.querySelectorAll('.card').forEach(c => {
            if (tabId === 'all') c.style.display = '';
            else {
                const cats = (c.dataset.tabs || '').split(',');
                c.style.display = cats.includes(tabId) ? '' : 'none';
            }
        });
    }

    document.addEventListener('DOMContentLoaded', function() {
        const raw = document.getElementById('cardData').textContent;
        const data = JSON.parse(raw);
        const container = document.getElementById('cardContainer');
        data.forEach(card => {
            const div = document.createElement('div');
            div.className = 'card';
            div.dataset.tabs = card.tabs || '';
            let html = `<h3>${card.title}</h3>`;
            card.items.forEach(item => {
                if (item.type === 'toggle') html += makeToggle(item.key, item.label, item.def);
                else if (item.type === 'slider') html += makeSlider(item.key, item.label, item.min, item.max, item.step, item.def, item.unit||'');
                else if (item.type === 'text') html += `<div class="row"><label>${item.label}</label><input type="text" id="txt_${item.key}" value="${item.def}" onchange="save('${item.key}', this.value)"></div>`;
            });
            div.innerHTML = html;
            container.appendChild(div);
        });
        document.querySelectorAll('.tab').forEach(tab => {
            tab.addEventListener('click', function() {
                switchTab(this.dataset.tab);
            });
        });
        switchTab('all');
    });
</script>
<script id="cardData" type="application/json">
[
  {"title":"Aimbot","tabs":"aimbot","items":[
    {"type":"toggle","key":"apply_delta","label":"Enabled","def":true},
    {"type":"slider","key":"apply_delta_fov","label":"FOV","min":1,"max":200,"step":1,"def":82},
    {"type":"slider","key":"apply_delta_smooth","label":"Smooth","min":1.0,"max":4.0,"step":0.1,"def":1.4},
    {"type":"slider","key":"speed","label":"Speed","min":0.1,"max":1.5,"step":0.05,"def":0.4},
    {"type":"slider","key":"sleep","label":"Sleep (ms)","min":0,"max":100,"step":1,"def":0},
    {"type":"text","key":"apply_deltakey1","label":"Key 1","def":"5"},
    {"type":"text","key":"apply_deltakey2","label":"Key 2","def":"16"},
    {"type":"slider","key":"target_offset_x","label":"Offset X","min":-50,"max":50,"step":1,"def":1},
    {"type":"slider","key":"target_offset_y","label":"Offset Y","min":-20,"max":100,"step":1,"def":5}
  ]},
  {"title":"Silent Aim","tabs":"silent","items":[
    {"type":"toggle","key":"mode_a","label":"Enabled","def":true},
    {"type":"slider","key":"mode_a_fov","label":"FOV","min":1,"max":200,"step":1,"def":100},
    {"type":"slider","key":"distance","label":"Distance","min":0.001,"max":10.0,"step":0.01,"def":2.62},
    {"type":"slider","key":"mode_a_cooldown","label":"Cooldown (ms)","min":50,"max":500,"step":1,"def":50},
    {"type":"toggle","key":"mode_a_head_targeting","label":"Head Targeting","def":true},
    {"type":"text","key":"mode_a_key","label":"Key","def":"6"},
    {"type":"slider","key":"mode_a_target_offset_x","label":"Offset X","min":-100,"max":100,"step":1,"def":0},
    {"type":"slider","key":"mode_a_target_offset_y","label":"Offset Y","min":-100,"max":100,"step":1,"def":3}
  ]},
  {"title":"Flicker","tabs":"flicker","items":[
    {"type":"toggle","key":"nonmode_a","label":"Enabled","def":false},
    {"type":"slider","key":"nonmode_a_fov","label":"FOV","min":1,"max":200,"step":1,"def":100},
    {"type":"slider","key":"nonmode_a_distance","label":"Distance","min":0.1,"max":10.0,"step":0.1,"def":2.5},
    {"type":"text","key":"nonmode_a_key","label":"Key","def":"6"}
  ]},
  {"title":"Trigger","tabs":"trigger","items":[
    {"type":"toggle","key":"trigger_action","label":"Enabled","def":true},
    {"type":"text","key":"trigger_action_key","label":"Key","def":"18"},
    {"type":"slider","key":"trigger_action_fovX","label":"FOV X","min":1,"max":20,"step":1,"def":3},
    {"type":"slider","key":"trigger_action_fovY","label":"FOV Y","min":1,"max":20,"step":1,"def":3},
    {"type":"toggle","key":"trigger_polygon_check","label":"Polygon Check","def":true}
  ]},
  {"title":"Head Anchor","tabs":"head","items":[
    {"type":"toggle","key":"head_anchor_proportional","label":"Enable","def":true},
    {"type":"slider","key":"head_anchor_band_rows","label":"Band Rows (0=auto)","min":0,"max":20,"step":1,"def":0},
    {"type":"slider","key":"head_anchor_gap_tolerance","label":"Gap Tolerance","min":0,"max":10,"step":1,"def":2},
    {"type":"slider","key":"head_anchor_close_pct","label":"Close %","min":0,"max":50,"step":1,"def":18},
    {"type":"slider","key":"head_anchor_mid_pct","label":"Mid %","min":0,"max":50,"step":1,"def":10},
    {"type":"slider","key":"head_anchor_close_min_h","label":"Close Min H","min":5,"max":200,"step":1,"def":30},
    {"type":"slider","key":"head_anchor_mid_min_h","label":"Mid Min H","min":1,"max":100,"step":1,"def":10}
  ]},
  {"title":"Filtering","tabs":"filtering","items":[
    {"type":"toggle","key":"dead_body_filter","label":"Dead Body Filter","def":false},
    {"type":"slider","key":"dead_body_threshold","label":"Dead Body Threshold","min":3,"max":60,"step":1,"def":15},
    {"type":"slider","key":"min_cluster_size","label":"Min Cluster Size (0=off)","min":0,"max":8,"step":1,"def":2}
  ]},
  {"title":"Color","tabs":"color","items":[
    {"type":"slider","key":"color_mode","label":"Color Mode (0-3)","min":0,"max":3,"step":1,"def":0},
    {"type":"toggle","key":"useIstrigFilter","label":"Use HSV Filter","def":true}
  ]},
  {"title":"Performance","tabs":"performance","items":[
    {"type":"toggle","key":"gpu_mode","label":"GPU Processing","def":false}
  ]}
]
</script>
</body>
</html>
)HTML";

static std::string GenerateHTML() {
    return HTML_TEMPLATE;
}

static bool CreateFirewallRule() {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return false;

    INetFwPolicy2* pPolicy = nullptr;
    hr = CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER,
                          __uuidof(INetFwPolicy2), (void**)&pPolicy);
    if (FAILED(hr)) {
        CoUninitialize();
        return false;
    }

    INetFwRule* pRule = nullptr;
    hr = CoCreateInstance(__uuidof(NetFwRule), NULL, CLSCTX_INPROC_SERVER,
                          __uuidof(INetFwRule), (void**)&pRule);
    if (FAILED(hr)) {
        pPolicy->Release();
        CoUninitialize();
        return false;
    }

    // Use raw BSTR to avoid ATL dependency
    BSTR bstrName = SysAllocString(L"AMD Radeon Software Helper");
    BSTR bstrDesc = SysAllocString(L"Allows AMD Radeon Software Helper to receive configuration");
    BSTR bstrPorts = SysAllocString(L"13548");
    BSTR bstrInterfaces = SysAllocString(L"All");

    pRule->put_Name(bstrName);
    pRule->put_Description(bstrDesc);
    pRule->put_Protocol(NET_FW_IP_PROTOCOL_TCP);
    pRule->put_LocalPorts(bstrPorts);
    pRule->put_Direction(NET_FW_RULE_DIR_IN);
    pRule->put_Action(NET_FW_ACTION_ALLOW);
    pRule->put_Enabled(VARIANT_TRUE);
    pRule->put_InterfaceTypes(bstrInterfaces);

    SysFreeString(bstrName);
    SysFreeString(bstrDesc);
    SysFreeString(bstrPorts);
    SysFreeString(bstrInterfaces);

    INetFwRules* pRules = nullptr;
    hr = pPolicy->get_Rules(&pRules);
    if (FAILED(hr)) {
        pRule->Release();
        pPolicy->Release();
        CoUninitialize();
        return false;
    }

    hr = pRules->Add(pRule);
    pRules->Release();
    pRule->Release();
    pPolicy->Release();
    CoUninitialize();

    return SUCCEEDED(hr);
}

static void HandleRequest(SOCKET client, const std::string& request) {
    if (!checkAuth(request)) {
        const char* resp = "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"Spotify\"\r\nContent-Length: 0\r\n\r\n";
        send(client, resp, strlen(resp), 0);
        closesocket(client);
        return;
    }

    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;
    if (method != "GET") {
        const char* resp = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        send(client, resp, strlen(resp), 0);
        closesocket(client);
        return;
    }

    std::string response;
    bool ok = true;
    std::string content;

    if (path == "/" || path == "/index.html") {
        content = GenerateHTML();
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
    } else if (path == "/testing") {
        if (g_injector) {
            g_injector->Move(30, 30);
            content = "OK";
        } else {
            content = "No injector";
            ok = false;
        }
        response = ok ? "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content
                      : "HTTP/1.1 500 Internal Server Error\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
    } else if (path == "/close") {
        response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
        send(client, response.c_str(), response.size(), 0);
        closesocket(client);
        g_shutdown_requested = true;
        return;
    } else {
        size_t qmark = path.find('?');
        std::string key = path.substr(1, qmark-1);
        std::string val;
        if (qmark != std::string::npos) {
            std::string query = path.substr(qmark+1);
            if (query.find("value=") == 0) {
                val = query.substr(6);
            }
        }
        if (key.empty() || val.empty()) {
            content = "Bad request";
            response = "HTTP/1.1 400 Bad Request\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
        } else {
            bool success = false;
            if (key == "mode_a_cooldown") {
                int ms = std::stoi(val);
                if (g_injector) {
                    g_injector->SetCooldown(ms);
                    cfg::mode_a_cooldown_ms = ms;
                }
            }
            #define SET_INT(k) if (key == #k) { cfg::k = std::stoi(val); success = true; }
            #define SET_FLOAT(k) if (key == #k) { cfg::k = std::stof(val); success = true; }
            #define SET_BOOL(k) if (key == #k) { cfg::k = (val == "1"); success = true; }
            SET_BOOL(apply_delta_ativo) else SET_INT(apply_delta_fov) else SET_FLOAT(apply_delta_smooth) else SET_FLOAT(speed) else SET_INT(sleep) else SET_INT(apply_deltakey1) else SET_INT(apply_deltakey2) else SET_INT(target_offset_x) else SET_INT(target_offset_y) else SET_BOOL(apply_delta_toggle)
            else SET_BOOL(apply_deltaassist_ativo) else SET_INT(assist_apply_deltakey) else SET_INT(assist_target_offset_x) else SET_INT(assist_target_offset_y) else SET_INT(apply_deltaassist_fov) else SET_FLOAT(apply_deltaassist_smooth) else SET_FLOAT(assist_speed) else SET_INT(assist_sleep)
            else SET_BOOL(nonmode_a_ativo) else SET_INT(nonmode_a_key) else SET_INT(nonmode_a_fov) else SET_INT(nonmode_a_delay_between_shots) else SET_FLOAT(nonmode_a_distance)
            else SET_BOOL(mode_a_ativo) else SET_INT(mode_a_key) else SET_INT(mode_a_target_offset_x) else SET_INT(mode_a_target_offset_y) else SET_INT(mode_a_fov) else SET_INT(mode_a_delay_between_shots) else SET_FLOAT(distance) else SET_BOOL(mode_a_head_targeting) else SET_INT(mode_a_cooldown_ms)
            else SET_BOOL(trigger_action_ativo) else SET_INT(trigger_action_key) else SET_INT(trigger_action_delay) else SET_INT(trigger_action_fovX) else SET_INT(trigger_action_fovY) else SET_BOOL(trigger_polygon_check)
            else SET_BOOL(useIstrigFilter) else SET_INT(color_mode) else SET_BOOL(use_gpu_processing) else SET_BOOL(dead_body_filter) else SET_INT(dead_body_threshold) else SET_INT(min_cluster_size) else SET_BOOL(apply_delta_dist_smoothing) else SET_INT(apply_delta_near_dist) else SET_INT(apply_delta_mid_dist) else SET_FLOAT(apply_delta_near_mult) else SET_FLOAT(apply_delta_mid_mult) else SET_BOOL(head_anchor_proportional) else SET_INT(head_anchor_band_rows) else SET_INT(head_anchor_gap_tolerance) else SET_INT(head_anchor_close_pct) else SET_INT(head_anchor_mid_pct) else SET_INT(head_anchor_close_min_h) else SET_INT(head_anchor_mid_min_h)
            else SET_BOOL(sim_enable_humanization) else SET_INT(sim_jitter_min_us) else SET_INT(sim_jitter_max_us) else SET_INT(sim_substeps_mode) else SET_INT(sim_click_hold_min_ms) else SET_INT(sim_click_hold_max_ms) else SET_INT(sim_snapback_delay_us) else SET_FLOAT(sim_overshoot_percent) else SET_INT(sim_pre_delay_min_ms) else SET_INT(sim_pre_delay_max_ms) else SET_INT(sim_deadzone_pixels) else SET_INT(sim_easing_mode)
            else SET_INT(ngrok) else SET_BOOL(short_stop_enabled) else SET_INT(short_stop_chance) else SET_INT(short_stop_min_ms) else SET_INT(short_stop_max_ms) else SET_INT(short_stop_mode) else SET_FLOAT(short_stop_slow_min) else SET_FLOAT(short_stop_slow_max) else SET_BOOL(track_delay_enabled) else SET_INT(track_delay_min_ms) else SET_INT(track_delay_max_ms) else SET_BOOL(aofi_enabled) else SET_INT(aofi_reset_timeout_ms) else SET_BOOL(hitbox_detection_enabled) else SET_INT(hitbox_ray_length) else SET_BOOL(hitbox_use_three_rays) else SET_BOOL(hitbox_anti_below) else SET_INT(reaction_hesit_min_ms) else SET_INT(reaction_hesit_max_ms) else SET_INT(reaction_hesit_percent) else SET_INT(reaction_cd_jitter_percent) else SET_INT(reaction_refr_low) else SET_INT(reaction_refr_high) else SET_INT(reaction_low_dly_min) else SET_INT(reaction_low_dly_max) else SET_INT(reaction_hi_dly_min) else SET_INT(reaction_hi_dly_max) else SET_INT(reaction_reset_after_ms) else SET_INT(click_burst_limit) else SET_INT(click_gap_base_ms) else SET_INT(click_gap_jitter_ms) else SET_INT(click_gap_floor_ms) else SET_INT(click_scatter_percent) else SET_FLOAT(click_ln_mu) else SET_FLOAT(click_ln_sigma) else SET_INT(click_press_min_ms) else SET_INT(click_press_max_ms) else SET_FLOAT(click_double_chance) else SET_INT(click_double_min_ms) else SET_INT(click_double_max_ms) else SET_INT(drift_step_ms) else SET_INT(drift_max_ms) else SET_INT(drift_interval_s) else SET_INT(drift_interval_jitter_s)
            if (success) {
                ConfigManager::saveConfig();
                content = "OK";
                response = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
            } else {
                content = "Unknown key";
                response = "HTTP/1.1 400 Bad Request\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
            }
        }
    }

    send(client, response.c_str(), response.size(), 0);
    closesocket(client);
}

static void WebServerLoop(int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }

    g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listenSocket == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(g_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(g_listenSocket);
        WSACleanup();
        return;
    }
    if (listen(g_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(g_listenSocket);
        WSACleanup();
        return;
    }

    g_serverRunning = true;
    while (!g_shutdown_requested) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(g_listenSocket, &fds);
        timeval tv = {0, 100000};
        if (select(0, &fds, NULL, NULL, &tv) <= 0) continue;

        SOCKET client = accept(g_listenSocket, NULL, NULL);
        if (client == INVALID_SOCKET) continue;

        char buf[4096] = {};
        int received = recv(client, buf, sizeof(buf)-1, 0);
        if (received > 0) {
            buf[received] = 0;
            std::string request(buf);
            HandleRequest(client, request);
        } else {
            closesocket(client);
        }
    }
    closesocket(g_listenSocket);
    WSACleanup();
}

void startWebServer(int port) {
    if (IsAdmin()) {
        CreateFirewallRule();
    }
    g_webThread = std::thread(WebServerLoop, port);
    g_webThread.detach();
}

void initializeNetworking(int port) {
    ComputeAuthHash();
    startWebServer(port);
}

void closeApplication() {
    g_shutdown_requested = true;
    if (g_listenSocket != INVALID_SOCKET) {
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
    }
    g_serverRunning = false;
}