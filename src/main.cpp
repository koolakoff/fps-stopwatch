#include "display.h"
#include "renderer.h"
#include "stopwatch.h"
#include <windowsx.h>
#include <array>
#include <exception>
#include <fstream>

static std::int64_t ticks() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
static std::int64_t clockFrequency() { LARGE_INTEGER t; QueryPerformanceFrequency(&t); return t.QuadPart; }
static std::wstring number(double value, int decimals) {
    wchar_t buffer[80]; swprintf_s(buffer, L"%.*f", decimals, value); return buffer;
}
static std::wstring rateLabel(double rate) {
    return number(rate, std::abs(rate - std::round(rate)) < 0.005 ? 0 : 2) + L" Hz";
}
static bool inside(D2D1_RECT_F r, float x, float y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}
static float textWidth(IDWriteTextLayout* layout) {
    DWRITE_TEXT_METRICS metrics{}; check(layout->GetMetrics(&metrics)); return metrics.widthIncludingTrailingWhitespace;
}
static float baseline(IDWriteTextLayout* layout) {
    DWRITE_LINE_METRICS line{}; UINT32 count = 0;
    check(layout->GetLineMetrics(&line, 1, &count)); return line.baseline;
}

class App {
    Display display;
    Renderer render;
    Stopwatch watch{clockFrequency()};
    HWND window = nullptr;
    bool light = false, running = true, resizePending = false;
    bool smoke = false, testRefresh = false;
    float mouseX = -1, mouseY = -1;
    int scroll = 0;
    std::wstring notice;
    ULONGLONG noticeUntil = 0;
    D2D1_RECT_F closeRect{}, themeRect{}, resetRect{}, listRect{};
    std::vector<std::pair<int, D2D1_RECT_F>> rows;
    ComPtr<ID2D1SolidColorBrush> ink, accent, muted, hover;
    std::array<ComPtr<IDWriteTextLayout>, 10> digitsSmall, digitsLarge;
    ComPtr<IDWriteTextLayout> colon;
    ComPtr<ID2D1PathGeometry> resetIcon;
    float smallAdvance = 0, largeAdvance = 0, colonWidth = 0, smallBaseline = 0, largeBaseline = 0;
    float timerSize = 0, timerLeft = 0, timerRight = 0;
    size_t minuteDigits = 0;
    std::exception_ptr deferredError;

    void message(const std::wstring& value) { notice = value; noticeUntil = GetTickCount64() + 7000; }
    bool select(int index) {
        std::wstring error;
        if (!display.select(index, error)) { message(error); return false; }
        notice.clear(); ensureVisible(); return true;
    }
    void ensureVisible() {
        const int selected = display.index(display.current);
        const int visible = std::max(1, static_cast<int>(rows.size()));
        if (selected < scroll) scroll = selected;
        if (selected >= scroll + visible) scroll = selected - visible + 1;
    }
    void palette() {
        auto color = [](UINT32 value, float alpha = 1) { return D2D1::ColorF(value, alpha); };
        ink.Reset(); accent.Reset(); muted.Reset(); hover.Reset();
        check(render.canvas->CreateSolidColorBrush(color(light ? 0x101418 : 0xF2F0E8), &ink));
        check(render.canvas->CreateSolidColorBrush(color(light ? 0x00695C : 0x80CBC4), &accent));
        check(render.canvas->CreateSolidColorBrush(color(light ? 0x51595D : 0xA8AFB4), &muted));
        check(render.canvas->CreateSolidColorBrush(color(light ? 0x101418 : 0xF2F0E8, 0.08f), &hover));
    }
    void text(const std::wstring& value, float size, float x, float y, ID2D1Brush* brush, bool bold = false) {
        render.canvas->DrawTextLayout(D2D1::Point2F(x, y), render.label(value, size, bold), brush);
    }
    void makeDigits(float size) {
        smallAdvance = largeAdvance = 0;
        for (int i = 0; i < 10; ++i) {
            const std::wstring digit(1, wchar_t(L'0' + i));
            digitsSmall[i] = render.layout(digit, size, true);
            digitsLarge[i] = render.layout(digit, size * 1.5f, true);
            for (auto* l : {digitsSmall[i].Get(), digitsLarge[i].Get()})
                check(l->SetFontWeight(DWRITE_FONT_WEIGHT_BLACK, DWRITE_TEXT_RANGE{0, 1}));
            smallAdvance = std::max(smallAdvance, textWidth(digitsSmall[i].Get()));
            largeAdvance = std::max(largeAdvance, textWidth(digitsLarge[i].Get()));
        }
        colon = render.layout(L":", size, true); colonWidth = textWidth(colon.Get());
        smallBaseline = baseline(digitsSmall[0].Get()); largeBaseline = baseline(digitsLarge[0].Get());
    }
    void prepareDigits(float left, float right, float height, size_t minutes) {
        if (minuteDigits == minutes && timerLeft == left && timerRight == right) return;
        minuteDigits = minutes; timerLeft = left; timerRight = right;
        makeDigits(100);
        const float measured = float(minutes + 2) * smallAdvance + 3 * largeAdvance + 2 * colonWidth;
        timerSize = std::min((right - left) * 100 / measured, height * 0.36f);
        makeDigits(timerSize);
    }
    void draw(const wchar_t* capture = nullptr) {
        const float w = float(render.width), h = float(render.height);
        const float unit = std::min(w / 1920.f, h / 1080.f);
        const float margin = w * 0.03f;
        auto* c = render.canvas.Get();
        c->BeginDraw(); c->Clear(D2D1::ColorF(light ? 0xF2F0E8 : 0x101418));
        const float sideWidth = 285 * unit, rowHeight = 57 * unit;
        listRect = D2D1::RectF(margin * 0.5f, 155 * unit, margin * 0.5f + sideWidth, h - 165 * unit);
        text(L"REFRESH RATE", 19 * unit, listRect.left + 12 * unit, 108 * unit, muted.Get());
        const int visible = std::max(1, int((listRect.bottom - listRect.top) / rowHeight));
        scroll = std::clamp(scroll, 0, std::max(0, int(display.rates.size()) - visible));
        rows.clear();
        for (int n = scroll; n < std::min(int(display.rates.size()), scroll + visible); ++n) {
            const double rate = rateValue(display.rates[n]);
            const auto rect = D2D1::RectF(listRect.left, listRect.top + (n - scroll) * rowHeight,
                listRect.right, listRect.top + (n - scroll + 1) * rowHeight - 4 * unit);
            rows.push_back({n, rect});
            const bool original = std::abs(rate - display.initialRate) < 0.001;
            const bool selected = n == display.index(display.current);
            if (inside(rect, mouseX, mouseY)) c->FillRectangle(rect, hover.Get());
            if (selected) c->DrawRectangle(rect, original ? accent.Get() : ink.Get(), 1.5f * unit);
            std::wstring markers = original ? L" (d)" : L"";
            if (n == int(display.rates.size()) - 1) markers += L" (m)";
            const auto prefix = rateLabel(rate);
            auto* label = render.label(prefix + L"  \u00B7  " + number(1000 / rate, 2) + L" ms" + markers, 22 * unit, true);
            const UINT32 prefixLength = static_cast<UINT32>(prefix.size());
            check(label->SetFontSize(16 * unit, DWRITE_TEXT_RANGE{prefixLength, UINT32_MAX - prefixLength}));
            const float shrink = std::min(1.f, (sideWidth - 24 * unit) / textWidth(label));
            const auto transform = D2D1::Matrix3x2F::Scale(shrink, shrink) *
                D2D1::Matrix3x2F::Translation(rect.left + 12 * unit, rect.top + 11 * unit);
            c->SetTransform(transform);
            c->DrawTextLayout(D2D1::Point2F(0, 0), label, original ? accent.Get() : ink.Get());
            c->SetTransform(D2D1::Matrix3x2F::Identity());
        }
        if (scroll > 0) text(L"\u2191 scroll", 14 * unit, listRect.left + 12 * unit, listRect.top - 24 * unit, muted.Get());
        if (scroll + visible < int(display.rates.size()))
            text(L"\u2193 scroll", 14 * unit, listRect.left + 12 * unit, listRect.bottom + 3 * unit, muted.Get());

        // Read QPC after the static UI, immediately before drawing the digits.
        const auto time = formatTime(watch.milliseconds(ticks()));
        const size_t minutes = time.find(L':');
        const float left = listRect.right + 24 * unit, right = w * 0.92f;
        prepareDigits(left, right, h, minutes);
        const float total = float(minutes + 2) * smallAdvance + 3 * largeAdvance + 2 * colonWidth;
        float x = left + (right - left - total) / 2;
        const float base = h * 0.54f + timerSize * 0.28f;
        int separators = 0;
        for (wchar_t ch : time) {
            if (ch == L':') {
                c->DrawTextLayout(D2D1::Point2F(x, base - smallBaseline), colon.Get(), ink.Get());
                x += colonWidth; ++separators;
            } else {
                const bool big = separators == 2;
                auto* l = (big ? digitsLarge : digitsSmall)[ch - L'0'].Get();
                const float advance = big ? largeAdvance : smallAdvance;
                c->DrawTextLayout(D2D1::Point2F(x + (advance - textWidth(l)) / 2, base - (big ? largeBaseline : smallBaseline)), l, ink.Get());
                x += advance;
            }
        }
        const float buttonSize = 48 * unit;
        closeRect = D2D1::RectF(w - margin - buttonSize, 18 * unit, w - margin, 18 * unit + buttonSize);
        themeRect = D2D1::RectF(closeRect.left - buttonSize - 12 * unit, closeRect.top, closeRect.left - 12 * unit, closeRect.bottom);
        resetRect = D2D1::RectF(listRect.left, h - 135 * unit, listRect.left + sideWidth / 2, h - 75 * unit);
        for (auto rect : {resetRect, themeRect, closeRect}) if (inside(rect, mouseX, mouseY)) c->FillRectangle(rect, hover.Get());
        c->DrawRectangle(resetRect, muted.Get(), unit);
        // Circular reset arrow, drawn as geometry rather than a font glyph.
        const float resetX = (resetRect.left + resetRect.right) / 2;
        const float resetY = (resetRect.top + resetRect.bottom) / 2;
        if (!resetIcon) {
            ComPtr<ID2D1Factory> factory; c->GetFactory(&factory);
            check(factory->CreatePathGeometry(&resetIcon));
            ComPtr<ID2D1GeometrySink> sink; check(resetIcon->Open(&sink));
            sink->SetFillMode(D2D1_FILL_MODE_WINDING);
            const auto point = [](float radius, int segment) {
                const float angle = (-60.f + 300.f * float(segment) / 64.f) * 3.14159265f / 180.f;
                return D2D1::Point2F(radius * std::cos(angle), radius * std::sin(angle));
            };
            // Thick annular arc; keep the existing start/end angles and rotation.
            sink->BeginFigure(point(18, 0), D2D1_FIGURE_BEGIN_FILLED);
            for (int segment = 1; segment <= 64; ++segment) sink->AddLine(point(18, segment));
            for (int segment = 64; segment >= 0; --segment) sink->AddLine(point(10, segment));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            const auto end = point(14, 64);
            const float endAngle = 240.f * 3.14159265f / 180.f;
            const float tx = -std::sin(endAngle), ty = std::cos(endAngle);
            sink->BeginFigure(D2D1::Point2F(end.x + 8 * tx, end.y + 8 * ty), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(end.x - 7 * tx - 12 * ty, end.y - 7 * ty + 12 * tx));
            sink->AddLine(D2D1::Point2F(end.x - 7 * tx + 12 * ty, end.y - 7 * ty - 12 * tx));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            check(sink->Close());
        }
        c->SetTransform(D2D1::Matrix3x2F::Scale(unit, unit) * D2D1::Matrix3x2F::Translation(resetX, resetY));
        c->FillGeometry(resetIcon.Get(), ink.Get());
        c->SetTransform(D2D1::Matrix3x2F::Identity());
        float cx = (closeRect.left + closeRect.right) / 2, cy = (closeRect.top + closeRect.bottom) / 2;
        const float cross = 10 * unit;
        c->DrawLine(D2D1::Point2F(cx - cross, cy - cross), D2D1::Point2F(cx + cross, cy + cross), ink.Get(), 2 * unit);
        c->DrawLine(D2D1::Point2F(cx + cross, cy - cross), D2D1::Point2F(cx - cross, cy + cross), ink.Get(), 2 * unit);
        cx = (themeRect.left + themeRect.right) / 2;
        const auto circle = D2D1::Ellipse(D2D1::Point2F(cx, cy), 12 * unit, 12 * unit);
        c->PushAxisAlignedClip(D2D1::RectF(cx - 13 * unit, cy - 13 * unit, cx, cy + 13 * unit), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        c->FillEllipse(circle, ink.Get()); c->PopAxisAlignedClip(); c->DrawEllipse(circle, ink.Get(), 1.5f * unit);

        auto* interval = render.label(number(1000 / display.current, 2) + L" ms", 40 * unit, true);
        c->DrawTextLayout(D2D1::Point2F(w - margin - textWidth(interval), h - 132 * unit), interval, ink.Get());
        auto* caption = render.label(L"frame interval", 18 * unit);
        c->DrawTextLayout(D2D1::Point2F(w - margin - textWidth(caption), h - 78 * unit), caption, muted.Get());
        text(L"v1.0    Space / Click \u2014 start / pause    R \u2014 reset    \u2191\u2193 \u2014 refresh rate    M \u2014 max    D \u2014 default    I \u2014 theme    Esc / Q \u2014 exit",
            15 * unit, margin, h - 43 * unit, muted.Get());
        text(watch.running() ? L"RUNNING" : (watch.milliseconds(ticks()) ? L"PAUSED" : L"READY"),
            13 * unit, left, h - 91 * unit, muted.Get());
        if (!notice.empty() && GetTickCount64() < noticeUntil) {
            auto* label = render.label(notice, 17 * unit);
            label->SetMaxWidth(right - left); label->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            c->DrawTextLayout(D2D1::Point2F(left, 108 * unit), label, accent.Get());
        }
        check(c->EndDraw());
        if (capture) render.screenshot(capture);
        const HRESULT result = render.swap->Present(1, 0);
        if (result != DXGI_STATUS_OCCLUDED) check(result);
    }
    void key(WPARAM keyCode, bool repeat) {
        if (keyCode == VK_ESCAPE || keyCode == 'Q') { DestroyWindow(window); return; }
        if (repeat) return;
        if (keyCode == VK_SPACE) watch.toggle(ticks());
        if (keyCode == 'R') watch.reset(ticks());
        if (keyCode == 'I') { light = !light; palette(); }
        if (keyCode == 'M') select(int(display.rates.size()) - 1);
        if (keyCode == 'D') select(display.index(display.initialRate));
        if (keyCode == VK_UP) select(std::max(0, display.index(display.current) - 1));
        if (keyCode == VK_DOWN) select(std::min(int(display.rates.size()) - 1, display.index(display.current) + 1));
    }
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_KEYDOWN: key(wp, (lp & (1LL << 30)) != 0); return 0;
        case WM_MOUSEMOVE: {
            mouseX = float(GET_X_LPARAM(lp)); mouseY = float(GET_Y_LPARAM(lp));
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0}; TrackMouseEvent(&track);
            bool clickable = inside(closeRect, mouseX, mouseY) || inside(themeRect, mouseX, mouseY) || inside(resetRect, mouseX, mouseY);
            for (auto row : rows) clickable |= inside(row.second, mouseX, mouseY);
            SetCursor(LoadCursorW(nullptr, clickable ? IDC_HAND : IDC_ARROW)); return 0;
        }
        case WM_MOUSELEAVE: mouseX = mouseY = -1; return 0;
        case WM_LBUTTONUP: {
            const float x = float(GET_X_LPARAM(lp)), y = float(GET_Y_LPARAM(lp));
            if (inside(closeRect, x, y)) { DestroyWindow(window); return 0; }
            if (inside(themeRect, x, y)) { key('I', false); return 0; }
            if (inside(resetRect, x, y)) { key('R', false); return 0; }
            for (auto row : rows) if (inside(row.second, x, y)) { select(row.first); return 0; }
            key(VK_SPACE, false);
            return 0;
        }
        case WM_MOUSEWHEEL: scroll -= GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA; return 0;
        case WM_SIZE: case WM_DPICHANGED: resizePending = true; return 0;
        case WM_DISPLAYCHANGE: resizePending = true; display.refresh(); return 0;
        case WM_CLOSE: DestroyWindow(window); return 0;
        case WM_QUERYENDSESSION: display.restore(); return TRUE;
        case WM_DESTROY: running = false; PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(window, msg, wp, lp);
    }
    static LRESULT CALLBACK procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) noexcept {
        App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            app->window = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        if (!app) return DefWindowProcW(hwnd, msg, wp, lp);
        try { return app->handle(msg, wp, lp); }
        catch (...) {
            app->deferredError = std::current_exception();
            app->running = false; PostQuitMessage(1); return 0;
        }
    }
    void smokeCheck(bool condition, const char* description) {
        if (!condition) throw std::runtime_error(description);
    }
public:
    App(bool smokeTest, bool refreshTest) : smoke(smokeTest), testRefresh(refreshTest) {}
    ~App() { if (window && IsWindow(window)) DestroyWindow(window); }
    int run(HINSTANCE instance) {
        WNDCLASSEXW cls{}; cls.cbSize = sizeof(cls); cls.hInstance = instance;
        cls.lpfnWndProc = procedure; cls.lpszClassName = L"FpsTimerWindow";
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassExW(&cls)) throw std::runtime_error("Cannot register the application window.");
        const auto rect = display.monitor.rcMonitor;
        window = CreateWindowExW(WS_EX_APPWINDOW, cls.lpszClassName, L"FPS Timer", WS_POPUP,
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, this);
        if (!window) throw std::runtime_error("Cannot create the application window.");
        render.create(window, display.adapter.Get()); palette();
        ShowWindow(window, SW_SHOW); SetForegroundWindow(window);
        if (!display.canSwitch) message(L"Duplicated display: refresh switching is disabled.");
        unsigned frames = 0;
        std::int64_t pausedMs = 0;
        const auto smokeStart = GetTickCount64();
        std::ofstream report;
        if (smoke) {
            CreateDirectoryW(L"build", nullptr);
            report.open("build/smoke-test.txt");
            report << "Resolution: " << render.width << 'x' << render.height << "\nInitial Hz: " << display.initialRate << "\nModes:";
            for (const auto rate : display.rates) report << ' ' << rateValue(rate);
            report << "\n";
        }
        while (running) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) { running = false; break; }
                TranslateMessage(&msg); DispatchMessageW(&msg);
            }
            if (deferredError) std::rethrow_exception(deferredError);
            if (!running) break;
            if (resizePending) {
                RECT client; GetClientRect(window, &client);
                render.resize(UINT(client.right), UINT(client.bottom));
                minuteDigits = 0; resizePending = false;
            }
            if (smoke && GetTickCount64() - smokeStart > 20000) throw std::runtime_error("Smoke test timed out waiting for display frames.");
            if (!smoke && (IsIconic(window) || GetForegroundWindow() != window)) {
                MsgWaitForMultipleObjectsEx(0, nullptr, 100, QS_ALLINPUT, MWMO_INPUTAVAILABLE); continue;
            }
            const DWORD wait = MsgWaitForMultipleObjectsEx(1, &render.ready, 1000, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (wait == WAIT_FAILED) throw std::runtime_error("Display synchronization failed.");
            if (wait != WAIT_OBJECT_0) continue;
            const wchar_t* capture = nullptr;
            if (smoke) {
                if (frames == 2) {
                    smokeCheck(watch.milliseconds(ticks()) == 0 && !watch.running(), "Initial stopwatch state failed.");
                    capture = L"build/smoke-dark.png";
                }
                if (frames == 3) {
                    SendMessageW(window, WM_KEYDOWN, VK_SPACE, 0);
                    SendMessageW(window, WM_KEYDOWN, VK_SPACE, LPARAM(1LL << 30));
                    smokeCheck(watch.running(), "Space key/repeat handling failed.");
                }
                if (frames == 25) {
                    SendMessageW(window, WM_KEYDOWN, VK_SPACE, 0);
                    pausedMs = watch.milliseconds(ticks());
                    smokeCheck(!watch.running() && pausedMs > 0, "Pause failed.");
                }
                if (frames == 30) {
                    smokeCheck(watch.milliseconds(ticks()) == pausedMs, "Paused clock advanced.");
                    // Exercise the same mouse path as a real theme-button click.
                    const LPARAM click = MAKELPARAM(int((themeRect.left + themeRect.right) / 2), int((themeRect.top + themeRect.bottom) / 2));
                    SendMessageW(window, WM_LBUTTONUP, 0, click);
                    smokeCheck(light, "Mouse theme switch failed.");
                    smokeCheck(!watch.running() && watch.milliseconds(ticks()) == pausedMs, "Theme click changed stopwatch state.");
                    capture = L"build/smoke-light.png";
                }
                if (frames == 32) {
                    SendMessageW(window, WM_KEYDOWN, 'I', 0);
                    smokeCheck(!light, "Keyboard theme switch failed.");
                    SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(int(render.width / 2), int(render.height / 2)));
                    smokeCheck(watch.running(), "Background click did not resume the stopwatch.");
                }
                if (frames == 33) {
                    SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(int(render.width / 2), int(render.height / 2)));
                    smokeCheck(!watch.running(), "Background click did not pause the stopwatch.");
                    SendMessageW(window, WM_KEYDOWN, VK_SPACE, 0);
                    const bool wasRunning = watch.running();
                    // The selected row is a control even when its rate does not change.
                    for (const auto& activeRow : rows) if (activeRow.first == display.index(display.current)) {
                        SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(int((activeRow.second.left + activeRow.second.right) / 2), int((activeRow.second.top + activeRow.second.bottom) / 2)));
                        break;
                    }
                    smokeCheck(watch.running() == wasRunning, "Frequency click changed stopwatch state.");
                }
                if (frames == 36 && testRefresh && display.rates.size() > 1) {
                    const int selected = display.index(display.current);
                    const int next = selected + 1 < int(display.rates.size()) ? selected + 1 : selected - 1;
                    smokeCheck(select(next), "Refresh switch failed.");
                    report << "Switched Hz: " << display.current << "\n";
                }
                if (frames == 45 && testRefresh) {
                    SendMessageW(window, WM_KEYDOWN, 'D', 0);
                    smokeCheck(std::abs(display.current - display.initialRate) < 0.01, "Default refresh restore failed.");
                }
                if (frames == 50 && testRefresh && display.rates.size() > 1) {
                    // Leave a different rate active, so exit restoration is tested
                    // independently of the D shortcut. Select through the mouse path.
                    const int selected = display.index(display.current);
                    const int next = selected + 1 < int(display.rates.size()) ? selected + 1 : selected - 1;
                    for (const auto& row : rows) if (row.first == next) {
                        const LPARAM click = MAKELPARAM(int((row.second.left + row.second.right) / 2), int((row.second.top + row.second.bottom) / 2));
                        SendMessageW(window, WM_LBUTTONUP, 0, click);
                        break;
                    }
                    smokeCheck(std::abs(display.current - rateValue(display.rates[next])) < 0.01, "Mouse refresh selection failed.");
                }
                if (frames == 52 && testRefresh) capture = L"build/smoke-alternate-rate.png";
                if (frames == 54) {
                    smokeCheck(watch.running() && watch.milliseconds(ticks()) > pausedMs, "Resume failed.");
                    SendMessageW(window, WM_KEYDOWN, 'R', 0);
                    smokeCheck(watch.running() && watch.milliseconds(ticks()) < 10, "Running reset failed.");
                    SendMessageW(window, WM_KEYDOWN, VK_SPACE, 0);
                    const LPARAM click = MAKELPARAM(int((resetRect.left + resetRect.right) / 2), int((resetRect.top + resetRect.bottom) / 2));
                    SendMessageW(window, WM_LBUTTONUP, 0, click);
                    smokeCheck(!watch.running() && watch.milliseconds(ticks()) == 0, "Paused mouse reset failed.");
                    SendMessageW(window, WM_KEYDOWN, 'R', LPARAM(1LL << 30));
                    smokeCheck(!watch.running() && watch.milliseconds(ticks()) == 0, "Reset repeat changed stopwatch state.");
                }
                if (frames == 55) {
                    smokeCheck(display.restore(), "Exit display restore failed.");
                    smokeCheck(display.refresh() && std::abs(display.current - display.initialRate) < 0.01, "Restored display rate verification failed.");
                    report << "Restored Hz: " << display.current << "\n";
                    report << "Rendered frames: " << frames << "\nPause ms: " << pausedMs << "\nPASS\n";
                    SendMessageW(window, WM_KEYDOWN, 'Q', 0);
                    break;
                }
            }
            draw(capture); ++frames;
        }
        if (!display.restore()) MessageBoxW(nullptr, L"Could not restore the initial display mode. Please check Windows Display settings.", L"FPS Timer", MB_OK | MB_ICONWARNING);
        return 0;
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const bool smoke = wcsstr(commandLine, L"--smoke-test") != nullptr;
    HANDLE singleton = CreateMutexW(nullptr, FALSE, L"Local\\FpsTimer.SingleInstance");
    if (!singleton) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(L"FpsTimerWindow", nullptr)) SetForegroundWindow(existing);
        CloseHandle(singleton); return smoke ? 2 : 0;
    }
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result = 1;
    try { App app(smoke, wcsstr(commandLine, L"--test-refresh") != nullptr); result = app.run(instance); }
    catch (const std::exception& error) {
        if (smoke) { std::ofstream report("build/smoke-test.txt", std::ios::app); report << "FAIL: " << error.what() << '\n'; }
        else MessageBoxA(nullptr, error.what(), "FPS Timer", MB_OK | MB_ICONERROR);
    }
    if (SUCCEEDED(com)) CoUninitialize();
    CloseHandle(singleton); return result;
}
