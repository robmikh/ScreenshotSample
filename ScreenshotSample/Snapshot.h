#pragma once
#include "Display.h"

struct Snapshot
{
    static std::future<Snapshot> TakeAsync(
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice const& device,
        Display const& display);

    winrt::com_ptr<ID3D11Texture2D> Texture;
    RECT DisplayRect = {};
};
