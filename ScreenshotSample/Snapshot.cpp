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
    Display const& display)
{
    auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(device);
    winrt::com_ptr<ID3D11DeviceContext> d3dContext;
    d3dDevice->GetImmediateContext(d3dContext.put());

    auto displayHandle = display.Handle();
    auto displayRect = display.Rect();

    auto item = util::CreateCaptureItemForMonitor(displayHandle);
    // TODO: Capture in FP16 if HDR
    auto framePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
        device,
        winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized,
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

    // TODO: Tonemap if HDR

    co_return Snapshot{ captureTexture, displayRect };
}
