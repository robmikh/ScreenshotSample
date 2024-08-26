#pragma once
#include "Display.h"
#include "ToneMapper.h"

struct Snapshot
{
    static wil::task<Snapshot> TakeAsync(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device,
        Display const& display,
        std::shared_ptr<ToneMapper> const& toneMapper);

    winrt::com_ptr<ID3D11Texture2D> Texture;
    RECT DisplayRect = {};
};
