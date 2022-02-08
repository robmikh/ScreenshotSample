#include "pch.h"
#include "Snapshot.h"
#include "Options.h"

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

    // Get the information we need from the display
    auto displayHandle = display.Handle();
    auto displayRect = display.Rect();
    auto isHDR = display.IsHDR();
    auto sdrWhiteLevel = display.SDRWhiteLevelInNits();
    auto maxLuminance = display.MaxLuminance();

    // Debug options
    if (!isHDR && Options::ForceHDR())
    {
        isHDR = true;
        sdrWhiteLevel = D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL;
    }

    if (Options::ClipHDR())
    {
        isHDR = false;
        sdrWhiteLevel = 0.0f;
    }

    // Grab a reference to the tone mapper so that it
    // survives the comming coroutines.
    auto hdrToneMapper = toneMapper;

    // HDR captures use an FP16 pixel format, SDR uses BGRA8
    auto capturePixelFormat = isHDR ? winrt::DirectXPixelFormat::R16G16B16A16Float : winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized;

    // Setup our capture objects. If you want, this is where you 
    // should adjust any properties of the GraphicsCaptureSession
    // (e.g. IsCursorCaptureEnabled, IsBorderRequired).
    // The CreateCaptureItemForMonitor helper can be found here: https://github.com/robmikh/robmikh.common/blob/f2311df8de56f31410d14f55de7307464d9a673d/robmikh.common/include/robmikh.common/capture.desktop.interop.h#L16-L23
    auto item = util::CreateCaptureItemForMonitor(displayHandle);
    auto framePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
        device,
        capturePixelFormat,
        1,
        item.Size());
    auto session = framePool.CreateCaptureSession(item);

    // Get one frame and then end the capture.
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

    // Wait for the next frame to show up.
    co_await winrt::resume_on_signal(captureEvent.get());

    // The caller is expecting a BGRA8 texture. If we captured in HDR,
    // tone map the texture and give the result back.
    winrt::com_ptr<ID3D11Texture2D> resultTexture;
    if (isHDR)
    {
        // Tonemap the texture
        resultTexture.copy_from(hdrToneMapper->ProcessTexture(captureTexture, sdrWhiteLevel, maxLuminance).get());
    }
    else
    {
        // If we captured an SDR display, we can directly use the capture
        resultTexture.copy_from(captureTexture.get());
    }

    co_return Snapshot{ resultTexture, displayRect };
}
