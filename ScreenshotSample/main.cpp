#include "pch.h"
#include "Display.h"
#include "Snapshot.h"
#include "ToneMapper.h"

#ifdef _DEBUG
#define D3D_DEVICE_CREATE_FLAGS (D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG)
#else
#define D3D_DEVICE_CREATE_FLAGS D3D11_CREATE_DEVICE_BGRA_SUPPORT
#endif

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Graphics::DirectX::Direct3D11;
    using namespace Windows::Graphics::Imaging;
    using namespace Windows::Storage;
    using namespace Windows::Storage::Streams;
}

namespace util
{
    using namespace robmikh::common::uwp;
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

int wmain()
{
    // Init COM
    winrt::init_apartment();
    // We don't want virtualized coordinates
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Init D3D
    auto d3dDevice = util::CreateD3DDevice(D3D_DEVICE_CREATE_FLAGS);
    auto device = CreateDirect3DDevice(d3dDevice.as<IDXGIDevice>().get());

    auto toneMapper = std::make_shared<ToneMapper>(d3dDevice);

    // Enumerate displays
    auto displays = Display::GetAllDisplays();

    // Compose our displays
    auto composedTexture = ComposeSnapshotsAsync(device, displays, toneMapper).get();

    // Save the texture to a file
    auto file = CreateLocalFileAsync(L"screenshot.png").get();
    SaveTextureToFileAsync(composedTexture, file).get();

    wprintf(L"Done!\n");
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