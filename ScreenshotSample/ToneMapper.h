#pragma once

class ToneMapper
{
public:
    ToneMapper(winrt::com_ptr<ID3D11Device> const& d3dDevice);
    ~ToneMapper() {}

    winrt::com_ptr<ID3D11Texture2D> ProcessTexture(winrt::com_ptr<ID3D11Texture2D> const& hdrTexture, float sdrWhiteLevelInNits);

private:
    winrt::com_ptr<ID3D11Device> m_d3dDevice;
    winrt::com_ptr<ID3D11DeviceContext> m_d3dContext;
    winrt::com_ptr<ID3D11Multithread> m_d3dMultithread;
    winrt::com_ptr<ID2D1Factory1> m_d2dFactory;
    winrt::com_ptr<ID2D1Device1> m_d2dDevice;
    winrt::com_ptr<ID2D1DeviceContext5> m_d2dContext;

    winrt::com_ptr<ID2D1Effect> m_sdrWhiteScaleEffect;
    winrt::com_ptr<ID2D1Effect> m_hdrTonemapEffect;
    winrt::com_ptr<ID2D1Effect> m_inputColorManagementEffect;
    winrt::com_ptr<ID2D1Effect> m_outputColorManagementEffect;
};
