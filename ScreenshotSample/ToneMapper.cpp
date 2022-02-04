#include "pch.h"
#include "ToneMapper.h"

namespace util
{
    using namespace robmikh::common::uwp;
}

static const float sc_DefaultSdrDispMaxNits = 203.0f; // Based on BT.2100 recommended SDR viewing conditions.

// DEBUG, REMOVE AND UPDATE robmikh.common
inline auto CreateD2DFactory()
{
    D2D1_FACTORY_OPTIONS options{};

    #ifdef _DEBUG
    	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
    #endif

    winrt::com_ptr<ID2D1Factory1> factory;
    winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, factory.put()));
    return factory;
}

ToneMapper::ToneMapper(winrt::com_ptr<ID3D11Device> const& d3dDevice)
{
    m_d3dDevice = d3dDevice;
    m_d3dDevice->GetImmediateContext(m_d3dContext.put());
    m_d3dMultithread = m_d3dDevice.as<ID3D11Multithread>();
    m_d2dFactory = /*util::*/CreateD2DFactory();
    auto d2dDevice = util::CreateD2DDevice(m_d2dFactory, d3dDevice);
    m_d2dDevice = d2dDevice.as<ID2D1Device1>();
    winrt::com_ptr<ID2D1DeviceContext1> d2dContext;
    winrt::check_hresult(m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2dContext.put()));
    m_d2dContext = d2dContext.as<ID2D1DeviceContext5>();

    winrt::check_hresult(m_d2dContext->CreateEffect(CLSID_D2D1WhiteLevelAdjustment, m_sdrWhiteScaleEffect.put()));
    winrt::check_hresult(m_d2dContext->CreateEffect(CLSID_D2D1HdrToneMap, m_hdrTonemapEffect.put()));
    winrt::check_hresult(m_d2dContext->CreateEffect(CLSID_D2D1ColorManagement, m_colorManagementEffect.put()));

    m_sdrWhiteScaleEffect->SetInputEffect(0, m_hdrTonemapEffect.get());
    m_hdrTonemapEffect->SetInputEffect(0, m_colorManagementEffect.get());

    // Setup the while scale effect
    winrt::check_hresult(m_sdrWhiteScaleEffect->SetValue(D2D1_WHITELEVELADJUSTMENT_PROP_OUTPUT_WHITE_LEVEL, D2D1_SCENE_REFERRED_SDR_WHITE_LEVEL));

    // Setup the tone map effect
    winrt::check_hresult(m_hdrTonemapEffect->SetValue(D2D1_HDRTONEMAP_PROP_OUTPUT_MAX_LUMINANCE, sc_DefaultSdrDispMaxNits));
    winrt::check_hresult(m_hdrTonemapEffect->SetValue(D2D1_HDRTONEMAP_PROP_DISPLAY_MODE, D2D1_HDRTONEMAP_DISPLAY_MODE_SDR));

    // Setup the color management effect
    winrt::check_hresult(m_colorManagementEffect->SetValue(D2D1_COLORMANAGEMENT_PROP_QUALITY, D2D1_COLORMANAGEMENT_QUALITY_BEST));

    winrt::com_ptr<ID2D1ColorContext1> inputColorContext;
    winrt::check_hresult(m_d2dContext->CreateColorContextFromDxgiColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, inputColorContext.put()));
    winrt::check_hresult(m_colorManagementEffect->SetValue(D2D1_COLORMANAGEMENT_PROP_SOURCE_COLOR_CONTEXT, inputColorContext.get()));

    winrt::com_ptr<ID2D1ColorContext1> outputColorContext;
    winrt::check_hresult(m_d2dContext->CreateColorContextFromDxgiColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, outputColorContext.put()));
    winrt::check_hresult(m_colorManagementEffect->SetValue(D2D1_COLORMANAGEMENT_PROP_DESTINATION_COLOR_CONTEXT, outputColorContext.get()));
}

winrt::com_ptr<ID3D11Texture2D> ToneMapper::ProcessTexture(winrt::com_ptr<ID3D11Texture2D> const& hdrTexture, float sdrWhiteLevelInNits)
{
    auto multithreadLock = util::D3D11DeviceLock(m_d3dMultithread.get());

    D3D11_TEXTURE2D_DESC desc = {};
    hdrTexture->GetDesc(&desc);
    auto dxgiSurface = hdrTexture.as<IDXGISurface>();

    // Create a D2D image from our texture
    winrt::com_ptr<ID2D1ImageSource> d2dImageSource;
    std::vector<IDXGISurface*> surfaces = { dxgiSurface.get() };
    winrt::check_hresult(m_d2dContext->CreateImageSourceFromDxgi(
        surfaces.data(),
        surfaces.size(),
        // D2D doesn't support DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
        // for image sources. We account for this in the color management
        // effect.
        DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,
        D2D1_IMAGE_SOURCE_FROM_DXGI_OPTIONS_NONE,
        d2dImageSource.put()));

    winrt::check_hresult(m_sdrWhiteScaleEffect->SetValue(D2D1_WHITELEVELADJUSTMENT_PROP_INPUT_WHITE_LEVEL, sdrWhiteLevelInNits));
    m_colorManagementEffect->SetInput(0, d2dImageSource.get());

    winrt::com_ptr<ID2D1Image> effectImage;
    m_sdrWhiteScaleEffect->GetOutput(effectImage.put());

    // Create our output texture
    winrt::com_ptr<ID3D11Texture2D> outputTexture;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    winrt::check_hresult(m_d3dDevice->CreateTexture2D(&desc, nullptr, outputTexture.put()));

    // Create a render target
    auto outputDxgiSurface = outputTexture.as<IDXGISurface>();
    winrt::com_ptr<ID2D1Bitmap1> d2dTargetBitmap;
    winrt::check_hresult(m_d2dContext->CreateBitmapFromDxgiSurface(outputDxgiSurface.get(), nullptr, d2dTargetBitmap.put()));

    // Set the render target as our current target
    m_d2dContext->SetTarget(d2dTargetBitmap.get());

    // Draw to the render target
    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(0, 0));
    m_d2dContext->DrawImage(effectImage.get(), D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    winrt::check_hresult(m_d2dContext->EndDraw());
    //winrt::check_hresult(m_d2dContext->Flush());

    m_d2dContext->SetTarget(nullptr);

    return outputTexture;
}
