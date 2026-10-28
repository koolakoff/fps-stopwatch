#pragma once
#include <windows.h>
#include <dxgi1_3.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

inline double rateValue(DXGI_RATIONAL r) { return r.Denominator ? double(r.Numerator) / r.Denominator : 0; }
inline double rateValue(DISPLAYCONFIG_RATIONAL r) { return r.Denominator ? double(r.Numerator) / r.Denominator : 0; }
inline bool sameAdapter(LUID a, LUID b) { return a.HighPart == b.HighPart && a.LowPart == b.LowPart; }

struct DisplayConfig {
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    bool read() {
        for (int attempt = 0; attempt < 5; ++attempt) {
            UINT32 p = 0, m = 0;
            if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &p, &m) != ERROR_SUCCESS) return false;
            paths.resize(p); modes.resize(m);
            LONG result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &p, paths.data(), &m, modes.data(), nullptr);
            if (result == ERROR_INSUFFICIENT_BUFFER) continue;
            if (result != ERROR_SUCCESS) return false;
            paths.resize(p); modes.resize(m); return true;
        }
        return false;
    }
    int find(const std::wstring& name) const {
        for (size_t i = 0; i < paths.size(); ++i) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            source.header.size = sizeof(source);
            source.header.adapterId = paths[i].sourceInfo.adapterId;
            source.header.id = paths[i].sourceInfo.id;
            if (DisplayConfigGetDeviceInfo(&source.header) == ERROR_SUCCESS && name == source.viewGdiDeviceName)
                return static_cast<int>(i);
        }
        return -1;
    }
    LONG apply(bool validate = false) {
        return SetDisplayConfig(static_cast<UINT32>(paths.size()), paths.data(),
            static_cast<UINT32>(modes.size()), modes.data(),
            (validate ? SDC_VALIDATE : SDC_APPLY) | SDC_USE_SUPPLIED_DISPLAY_CONFIG);
    }
};

// Owns the original configuration and restores transient changes on every normal
// return/exception path. No mode is written to the Windows persistence database.
class Display {
public:
    MONITORINFOEXW monitor{};
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    std::vector<DXGI_RATIONAL> rates;
    double current = 0, initialRate = 0;
    DisplayConfig initial;
    bool changed = false, canSwitch = false;

    Display() {
        monitor.cbSize = sizeof(monitor);
        const HMONITOR handle = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
        if (!GetMonitorInfoW(handle, &monitor)) throw std::runtime_error("Cannot read the primary monitor.");
        if (!initial.read()) throw std::runtime_error("Cannot read the active display configuration.");
        const int path = initial.find(monitor.szDevice);
        if (path < 0) throw std::runtime_error("Cannot find the primary display path.");
        auto original = initial.paths[path].targetInfo.refreshRate;
        current = initialRate = rateValue(original);
        if (current <= 1) throw std::runtime_error("The monitor did not report a valid refresh rate.");
        rates.push_back({original.Numerator, original.Denominator});
        canSwitch = std::count_if(initial.paths.begin(), initial.paths.end(), [&](const auto& p) {
            return p.sourceInfo.id == initial.paths[path].sourceInfo.id &&
                sameAdapter(p.sourceInfo.adapterId, initial.paths[path].sourceInfo.adapterId);
        }) == 1;
        Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) throw std::runtime_error("Cannot create DXGI factory.");
        for (UINT a = 0; ; ++a) {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> candidate;
            HRESULT hr = factory->EnumAdapters1(a, &candidate);
            if (hr == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(hr)) throw std::runtime_error("Cannot enumerate graphics adapters.");
            for (UINT o = 0; ; ++o) {
                Microsoft::WRL::ComPtr<IDXGIOutput> output;
                hr = candidate->EnumOutputs(o, &output);
                if (hr == DXGI_ERROR_NOT_FOUND) break;
                if (FAILED(hr)) throw std::runtime_error("Cannot enumerate display outputs.");
                DXGI_OUTPUT_DESC description{};
                if (FAILED(output->GetDesc(&description)) || description.Monitor != handle) continue;
                adapter = candidate;
                UINT count = 0;
                if (FAILED(output->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &count, nullptr))) continue;
                std::vector<DXGI_MODE_DESC> modes(count);
                if (FAILED(output->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &count, modes.data()))) continue;
                const bool rotated = description.Rotation == DXGI_MODE_ROTATION_ROTATE90 || description.Rotation == DXGI_MODE_ROTATION_ROTATE270;
                const UINT width = UINT(monitor.rcMonitor.right - monitor.rcMonitor.left);
                const UINT height = UINT(monitor.rcMonitor.bottom - monitor.rcMonitor.top);
                for (UINT i = 0; i < count; ++i) {
                    const auto& m = modes[i];
                    if (m.Width != (rotated ? height : width) || m.Height != (rotated ? width : height) ||
                        m.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST ||
                        m.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST || rateValue(m.RefreshRate) <= 1) continue;
                    if (std::none_of(rates.begin(), rates.end(), [&](auto r) {
                        return std::abs(rateValue(r) - rateValue(m.RefreshRate)) < 0.001;
                    })) rates.push_back(m.RefreshRate);
                }
            }
        }
        std::sort(rates.begin(), rates.end(), [](auto a, auto b) { return rateValue(a) < rateValue(b); });
    }
    ~Display() { restore(); }
    int index(double value) const {
        int best = 0;
        for (int i = 1; i < static_cast<int>(rates.size()); ++i)
            if (std::abs(rateValue(rates[i]) - value) < std::abs(rateValue(rates[best]) - value)) best = i;
        return best;
    }
    bool refresh() {
        DisplayConfig config;
        if (!config.read()) return false;
        const int p = config.find(monitor.szDevice);
        if (p < 0) return false;
        current = rateValue(config.paths[p].targetInfo.refreshRate);
        return current > 1;
    }
    bool select(int i, std::wstring& error) {
        if (i < 0 || i >= static_cast<int>(rates.size())) return false;
        if (std::abs(current - rateValue(rates[i])) < 0.001) return true;
        if (!canSwitch) { error = L"Refresh switching is unavailable for duplicated displays."; return false; }
        DisplayConfig before;
        if (!before.read()) { error = L"Cannot read display configuration."; return false; }
        auto next = before;
        const int p = next.find(monitor.szDevice);
        if (p < 0) { error = L"Display is disconnected."; return false; }
        next.paths[p].targetInfo.refreshRate = {rates[i].Numerator, rates[i].Denominator};
        // Preserve the source resolution/position and let Windows find target
        // timings for this exact rational frequency. Do not allow source changes.
        next.paths[p].targetInfo.modeInfoIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
        next.paths[p].targetInfo.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;
        LONG result = next.apply(true);
        if (result != ERROR_SUCCESS) { error = L"Windows rejected this refresh rate (" + std::to_wstring(result) + L")."; return false; }
        changed = true;
        result = next.apply();
        DEVMODEW actual{}; actual.dmSize = sizeof(actual);
        const bool valid = result == ERROR_SUCCESS && refresh() &&
            EnumDisplaySettingsW(monitor.szDevice, ENUM_CURRENT_SETTINGS, &actual) &&
            actual.dmPelsWidth == DWORD(monitor.rcMonitor.right - monitor.rcMonitor.left) &&
            actual.dmPelsHeight == DWORD(monitor.rcMonitor.bottom - monitor.rcMonitor.top) &&
            std::abs(current - rateValue(rates[i])) < 0.01;
        if (!valid) {
            const LONG rollback = before.apply();
            refresh();
            error = rollback == ERROR_SUCCESS ? L"Refresh change failed. Previous mode restored."
                : L"Refresh change failed. Check Windows Display settings.";
            return false;
        }
        return true;
    }
    bool restore() noexcept {
        if (!changed) return true;
        if (initial.apply() != ERROR_SUCCESS) return false;
        changed = false; current = initialRate; return true;
    }
};
