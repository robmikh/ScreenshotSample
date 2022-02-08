#include "pch.h"
#include "Display.h"
#include "Snapshot.h"
#include "ToneMapper.h"
#include "Options.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Graphics::DirectX::Direct3D11;
    using namespace Windows::Graphics::Imaging;
    using namespace Windows::Storage;
    using namespace Windows::Storage::Streams;
    using namespace Windows::System;
}

namespace util
{
    using namespace robmikh::common::uwp;
    using namespace robmikh::common::wcli;
}

float CLEARCOLOR[] = { 0.0f, 0.0f, 0.0f, 1.0f }; // RGBA

std::future<winrt::com_ptr<ID3D11Texture2D>> ComposeSnapshotsAsync(
    winrt::IDirect3DDevice const& device,
    std::vector<Display> const& displays,
    std::shared_ptr<ToneMapper> const& toneMapper);
winrt::IAsyncOperation<winrt::StorageFile> CreateLocalFileAsync(std::wstring const& fileName);
winrt::IAsyncAction SaveTextureToFileAsync(
    winrt::com_ptr<ID3D11Texture2D> const& texture,
    winrt::StorageFile const& file);
bool ParseOptions(int argc, wchar_t* argv[]);

winrt::IAsyncAction MainAsync()
{
    // Init D3D
    uint32_t d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (Options::DxDebug())
    {
        d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }
    // These helpers can be found in the robmikh.common package:
    // CreateD3DDevice: https://github.com/robmikh/robmikh.common/blob/f2311df8de56f31410d14f55de7307464d9a673d/robmikh.common/include/robmikh.common/d3dHelpers.h#L68-L79
    // CreateDirect3DDevice: https://github.com/robmikh/robmikh.common/blob/f2311df8de56f31410d14f55de7307464d9a673d/robmikh.common/include/robmikh.common/direct3d11.interop.h#L19-L24
    auto d3dDevice = util::CreateD3DDevice(d3dFlags);
    auto device = CreateDirect3DDevice(d3dDevice.as<IDXGIDevice>().get());

    // Create our tone mapper
    auto toneMapper = std::make_shared<ToneMapper>(d3dDevice);

    // Enumerate displays
    auto displays = Display::GetAllDisplays();
    for (auto&& display : displays)
    {
        if (display.IsHDR())
        {
            wprintf(L"Found HDR display with white level: %f  and max luminance: %f\n", display.SDRWhiteLevelInNits(), display.MaxLuminance());
        }
        else
        {
            wprintf(L"Found SDR display\n");
        }
    }

    // Compose our displays
    auto composedTexture = ComposeSnapshotsAsync(device, displays, toneMapper).get();

    // Save the texture to a file
    auto file = co_await CreateLocalFileAsync(L"screenshot.png");
    co_await SaveTextureToFileAsync(composedTexture, file);
    wprintf(L"Done!\n");
    co_await winrt::Launcher::LaunchFileAsync(file);

    co_return;
}

int wmain(int argc, wchar_t* argv[])
{
    // Init COM
    winrt::init_apartment();
    // We don't want virtualized coordinates
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Parse args
    if (!ParseOptions(argc, argv))
    {
        return 0;
    }

    // Run the sample synchronously
    try
    {
        MainAsync().get();
    }
    catch (winrt::hresult_error const& error)
    {
        wprintf(L"Error:\n");
        wprintf(L"  0x%08x - %s\n", error.code().value, error.message().c_str());
    }

    return 0;
}

std::future<winrt::com_ptr<ID3D11Texture2D>> ComposeSnapshotsAsync(
    winrt::IDirect3DDevice const& device,
    std::vector<Display> const& displays,
    std::shared_ptr<ToneMapper> const& toneMapper)
{
    auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(device);
    winrt::com_ptr<ID3D11DeviceContext> d3dContext;
    d3dDevice->GetImmediateContext(d3dContext.put());

    // Determine the union of all displays
    RECT unionRect = {};
    unionRect.left = LONG_MAX;
    unionRect.top = LONG_MAX;
    unionRect.right = LONG_MIN;
    unionRect.bottom = LONG_MIN;
    for (auto&& display : displays)
    {
        auto& displayRect = display.Rect();

        if (unionRect.left > displayRect.left)
        {
            unionRect.left = displayRect.left;
        }
        if (unionRect.top > displayRect.top)
        {
            unionRect.top = displayRect.top;
        }
        if (unionRect.right < displayRect.right)
        {
            unionRect.right = displayRect.right;
        }
        if (unionRect.bottom < displayRect.bottom)
        {
            unionRect.bottom = displayRect.bottom;
        }
    }

    // Capture each display
    std::vector<std::future<Snapshot>> futures;
    for (auto&& display : displays)
    {
        auto future = Snapshot::TakeAsync(device, display, toneMapper);
        futures.push_back(std::move(future));
    }

    // Create the texture we'll compose everything to
    winrt::com_ptr<ID3D11Texture2D> composedTexture;
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = static_cast<uint32_t>(unionRect.right - unionRect.left);
    textureDesc.Height = static_cast<uint32_t>(unionRect.bottom - unionRect.top);
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    winrt::check_hresult(d3dDevice->CreateTexture2D(&textureDesc, nullptr, composedTexture.put()));
    // Clear to black
    winrt::com_ptr<ID3D11RenderTargetView> composedRenderTargetView;
    winrt::check_hresult(d3dDevice->CreateRenderTargetView(composedTexture.get(), nullptr, composedRenderTargetView.put()));
    d3dContext->ClearRenderTargetView(composedRenderTargetView.get(), CLEARCOLOR);

    // Compose our textures into one texture
    for (auto&& future : futures)
    {
        auto snapshot = co_await future;

        D3D11_TEXTURE2D_DESC desc = {};
        snapshot.Texture->GetDesc(&desc);

        auto destX = snapshot.DisplayRect.left - unionRect.left;
        auto destY = snapshot.DisplayRect.top - unionRect.top;

        D3D11_BOX region = {};
        region.left = 0;
        region.right = desc.Width;
        region.top = 0;
        region.bottom = desc.Height;
        region.back = 1;

        d3dContext->CopySubresourceRegion(composedTexture.get(), 0, destX, destY, 0, snapshot.Texture.get(), 0, &region);
    }

    co_return composedTexture;
}

winrt::IAsyncOperation<winrt::StorageFile> CreateLocalFileAsync(std::wstring const& fileName)
{
    auto currentPath = std::filesystem::current_path();
    auto folder = co_await winrt::StorageFolder::GetFolderFromPathAsync(currentPath.wstring());
    auto file = co_await folder.CreateFileAsync(fileName, winrt::CreationCollisionOption::ReplaceExisting);
    co_return file;
}

winrt::IAsyncAction SaveTextureToFileAsync(
    winrt::com_ptr<ID3D11Texture2D> const& texture,
    winrt::StorageFile const& file)
{
    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    // These helpers can be found in the robmikh.common package:
    // CopyBytesFromTexture: https://github.com/robmikh/robmikh.common/blob/f2311df8de56f31410d14f55de7307464d9a673d/robmikh.common/include/robmikh.common/d3dHelpers.h#L250-L282
    auto bytes = util::CopyBytesFromTexture(texture);

    auto stream = co_await file.OpenAsync(winrt::FileAccessMode::ReadWrite);
    auto encoder = co_await winrt::BitmapEncoder::CreateAsync(winrt::BitmapEncoder::PngEncoderId(), stream);
    encoder.SetPixelData(
        winrt::BitmapPixelFormat::Bgra8,
        winrt::BitmapAlphaMode::Premultiplied,
        desc.Width,
        desc.Height,
        1.0,
        1.0,
        bytes);
    co_await encoder.FlushAsync();

    co_return;
}

bool ParseOptions(int argc, wchar_t* argv[])
{
    // Much of this method uses helpers from the robmikh.common package.
    // I wouldn't recommend using this part, but if you're curious it can 
    // be found here: https://github.com/robmikh/robmikh.common/blob/master/robmikh.common/include/robmikh.common/wcliparse.h
    std::vector<std::wstring> args(argv + 1, argv + argc);
    if (util::impl::GetFlag(args, L"-help") || util::impl::GetFlag(args, L"/?"))
    {
        wprintf(L"ScreenshotSample.exe\n");
        wprintf(L"A sample that shows how to take and save screenshots using Windows.Graphics.Capture.\n");
        wprintf(L"\n");
        wprintf(L"Flags:\n");
        wprintf(L"  -dxDebug     (optional) Use the D3D and D2D debug layers.\n");
        wprintf(L"  -forceHDR    (optional) Force all monitors to be captured as HDR, used for debugging.\n");
        wprintf(L"  -clipHDR     (optional) Clip HDR contnet instead of tone mapping.\n");
        wprintf(L"\n");
        return false;
    }
    bool dxDebug = util::impl::GetFlag(args, L"-dxDebug") || util::impl::GetFlag(args, L"/dxDebug");
    bool forceHDR = util::impl::GetFlag(args, L"-forceHDR") || util::impl::GetFlag(args, L"/forceHDR");
    bool clipHDR = util::impl::GetFlag(args, L"-clipHDR") || util::impl::GetFlag(args, L"/clipHDR");
    if (clipHDR && forceHDR)
    {
        wprintf(L"Cannot simultaneously clip and force HDR!\n");
        return false;
    }
    Options::InitOptions(dxDebug, forceHDR, clipHDR);
    if (dxDebug)
    {
        wprintf(L"Using D3D and D2D debug layers...\n");
    }
    if (forceHDR)
    {
        wprintf(L"Forcing HDR capture for all monitors...\n");
    }
    if (clipHDR)
    {
        wprintf(L"Clipping HDR content...\n");
    }
    return true;
}