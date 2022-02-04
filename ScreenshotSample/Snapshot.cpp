#include "pch.h"
#include "Snapshot.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::DirectX::Direct3D11;
}

namespace util
{
    using namespace robmikh::common::desktop;
    using namespace robmikh::common::uwp;
}

std::future<Snapshot> Snapshot::TakeAsync(
    winrt::IDirect3DDevice const& device, 
    Display const& display,
    std::shared_ptr<ToneMapper> const& toneMapper)
{
    auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(device);
    winrt::com_ptr<ID3D11DeviceContext> d3dContext;
    d3dDevice->GetImmediateContext(d3dContext.put());

    auto displayHandle = display.Handle();
    auto displayRect = display.Rect();
    auto isHDR = display.IsHDR();
    auto sdrWhiteLevel = display.SDRWhiteLevelInNits();

    // DEBUG, PLEASE REMOVE
    isHDR = true;

    auto hdrToneMapper = toneMapper;

    auto capturePixelFormat = display.IsHDR() ? winrt::DirectXPixelFormat::R16G16B16A16Float : winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized;


    auto item = util::CreateCaptureItemForMonitor(displayHandle);
    auto framePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
        device,
        capturePixelFormat,
        1,
        item.Size());
    auto session = framePool.CreateCaptureSession(item);

    winrt::com_ptr<ID3D11Texture2D> captureTexture;
    wil::shared_event captureEvent(wil::EventOptions::ManualReset);
    framePool.FrameArrived([session, d3dDevice, d3dContext, captureEvent, &captureTexture](auto&& framePool, auto&&) -> void
        {
            auto frame = framePool.TryGetNextFrame();
            auto surface = frame.Surface();
            auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(surface);

            framePool.Close();
            session.Close();

            captureTexture.copy_from(frameTexture.get());
            captureEvent.SetEvent();
        });
    session.StartCapture();

    co_await winrt::resume_on_signal(captureEvent.get());

    winrt::com_ptr<ID3D11Texture2D> resultTexture;
    if (isHDR)
    {
        // Tonemap the texture
        resultTexture.copy_from(hdrToneMapper->ProcessTexture(captureTexture).get());
    }
    else
    {
        // If we captured an SDR display, we can directly use the capture
        resultTexture.copy_from(captureTexture.get());
    }

    co_return Snapshot{ resultTexture, displayRect };
}
