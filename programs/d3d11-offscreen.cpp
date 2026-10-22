// Hypothesis: an NVIDIA hardware adapter can render and read back a D3D11 target.
// Falsifier: no VendorId 0x10DE, any failed API, or unexpected readback pixels.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
using Microsoft::WRL::ComPtr;

static void check(HRESULT hr, const char* what) {
    if (FAILED(hr)) { std::printf("D3D11_FAIL %s HRESULT=0x%08lx\n", what, hr); std::exit(1); }
}

int main() {
    ComPtr<IDXGIFactory1> factory;
    check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "factory");
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; ; ++i) {
        ComPtr<IDXGIAdapter1> candidate;
        HRESULT hr = factory->EnumAdapters1(i, &candidate);
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        check(hr, "enumerate");
        DXGI_ADAPTER_DESC1 desc{};
        check(candidate->GetDesc1(&desc), "description");
        std::printf("ADAPTER VendorId=0x%04x DeviceId=0x%04x Software=%u\n",
                    desc.VendorId, desc.DeviceId, !!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE));
        if (desc.VendorId == 0x10DE && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            adapter = candidate; break;
        }
    }
    if (!adapter) { std::puts("D3D11_FAIL NVIDIA_VENDOR_ASSERT_0x10DE"); return 2; }
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level;
    check(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
          D3D11_SDK_VERSION, &device, &level, &context), "device");
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 4; desc.Height = 4; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target, staging;
    check(device->CreateTexture2D(&desc, nullptr, &target), "target");
    ComPtr<ID3D11RenderTargetView> view;
    check(device->CreateRenderTargetView(target.Get(), nullptr, &view), "view");
    const float color[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    context->ClearRenderTargetView(view.Get(), color);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    check(device->CreateTexture2D(&desc, nullptr, &staging), "staging");
    context->CopyResource(staging.Get(), target.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "readback");
    const int expected[4] = {64, 128, 191, 255};
    bool good = true;
    for (UINT y = 0; y < 4; ++y) for (UINT x = 0; x < 4; ++x)
        for (UINT c = 0; c < 4; ++c) {
            auto value = static_cast<const unsigned char*>(mapped.pData)[y * mapped.RowPitch + x * 4 + c];
            if (std::abs(static_cast<int>(value) - expected[c]) > 1) good = false;
        }
    context->Unmap(staging.Get(), 0);
    std::printf("D3D11_%s VendorId=0x10DE feature_level=0x%x pixels=16\n", good ? "PASS" : "FAIL", level);
    return good ? 0 : 3;
}
