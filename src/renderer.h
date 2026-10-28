#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;
inline void check(HRESULT hr) {
    if (FAILED(hr)) {
        char text[80];
        std::snprintf(text, sizeof(text), "Graphics operation failed (0x%08lX).", static_cast<unsigned long>(hr));
        throw std::runtime_error(text);
    }
}

class Renderer {
public:
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> immediate;
    ComPtr<ID2D1DeviceContext> canvas;
    ComPtr<IDXGISwapChain2> swap;
    ComPtr<ID2D1Bitmap1> target;
    ComPtr<IDWriteFactory> textFactory;
    HANDLE ready = nullptr;
    UINT width = 0, height = 0;
    std::map<std::wstring, ComPtr<IDWriteTextLayout>> labels;
    ~Renderer() { if (ready) CloseHandle(ready); }

    void create(HWND window, IDXGIAdapter1* adapter) {
        D3D_FEATURE_LEVEL level;
        check(D3D11CreateDevice(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
            nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &device, &level, &immediate));
        ComPtr<IDXGIDevice> dxgiDevice; check(device.As(&dxgiDevice));
        ComPtr<ID2D1Factory1> factory;
        check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&factory)));
        ComPtr<ID2D1Device> d2d; check(factory->CreateDevice(dxgiDevice.Get(), &d2d));
        check(d2d->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &canvas));
        canvas->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
        canvas->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(textFactory.GetAddressOf())));
        ComPtr<IDXGIAdapter> dxgiAdapter; check(dxgiDevice->GetAdapter(&dxgiAdapter));
        ComPtr<IDXGIFactory2> dxgiFactory; check(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2; desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        ComPtr<IDXGISwapChain1> chain;
        check(dxgiFactory->CreateSwapChainForHwnd(device.Get(), window, &desc, nullptr, nullptr, &chain));
        check(chain.As(&swap));
        check(dxgiFactory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
        check(swap->SetMaximumFrameLatency(1));
        ready = swap->GetFrameLatencyWaitableObject();
        if (!ready) throw std::runtime_error("Cannot create the display synchronization handle.");
        RECT rect; GetClientRect(window, &rect); resize(UINT(rect.right), UINT(rect.bottom));
    }
    void resize(UINT w, UINT h) {
        if (!swap || !w || !h || (w == width && h == height)) return;
        canvas->SetTarget(nullptr); target.Reset(); immediate->ClearState(); immediate->Flush();
        check(swap->ResizeBuffers(2, w, h, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
        width = w; height = h; labels.clear();
        ComPtr<IDXGISurface> surface; check(swap->GetBuffer(0, IID_PPV_ARGS(&surface)));
        const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96, 96);
        check(canvas->CreateBitmapFromDxgiSurface(surface.Get(), &props, &target));
        canvas->SetTarget(target.Get());
    }
    ComPtr<IDWriteTextLayout> layout(const std::wstring& text, float size, bool bold = false) {
        ComPtr<IDWriteTextFormat> format;
        check(textFactory->CreateTextFormat(L"Segoe UI", nullptr, bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-US", &format));
        check(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
        ComPtr<IDWriteTextLayout> result;
        check(textFactory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format.Get(), 10000, 10000, &result));
        return result;
    }
    IDWriteTextLayout* label(const std::wstring& text, float size, bool bold = false) {
        const auto key = text + L"|" + std::to_wstring(size) + (bold ? L"b" : L"n");
        auto found = labels.find(key);
        if (found != labels.end()) return found->second.Get();
        return labels.emplace(key, layout(text, size, bold)).first->second.Get();
    }
    // Developer smoke tests capture the application's rendered back buffer,
    // not the user's desktop. WIC is included in Windows; no image DLL ships.
    void screenshot(const wchar_t* path) {
        ComPtr<ID3D11Texture2D> source;
        check(swap->GetBuffer(0, IID_PPV_ARGS(&source)));
        D3D11_TEXTURE2D_DESC desc{}; source->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> copy; check(device->CreateTexture2D(&desc, nullptr, &copy));
        immediate->CopyResource(copy.Get(), source.Get());
        ComPtr<IWICImagingFactory> wic;
        check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)));
        ComPtr<IWICStream> stream; check(wic->CreateStream(&stream));
        check(stream->InitializeFromFilename(path, GENERIC_WRITE));
        ComPtr<IWICBitmapEncoder> encoder; check(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
        check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
        ComPtr<IWICBitmapFrameEncode> frame; check(encoder->CreateNewFrame(&frame, nullptr));
        check(frame->Initialize(nullptr)); check(frame->SetSize(width, height));
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        check(frame->SetPixelFormat(&format));
        D3D11_MAPPED_SUBRESOURCE data{};
        check(immediate->Map(copy.Get(), 0, D3D11_MAP_READ, 0, &data));
        const HRESULT write = frame->WritePixels(height, data.RowPitch, data.RowPitch * height, static_cast<BYTE*>(data.pData));
        immediate->Unmap(copy.Get(), 0);
        check(write); check(frame->Commit()); check(encoder->Commit());
    }
};
