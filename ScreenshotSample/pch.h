#pragma once

// Windows
#include <windows.h>

// Must come before C++/WinRT
#include <wil/cppwinrt.h>

// WinRT
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

// WIL
#include <wil/resource.h>

// DirectX
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <d2d1_3.h>
#include <wincodec.h>

// STL
#include <memory>
#include <filesystem>
#include <chrono>
#include <vector>
#include <future>
#include <string>

// robmikh.common
#include <robmikh.common/direct3d11.interop.h>
#include <robmikh.common/d3dHelpers.h>
#include <robmikh.common/graphics.interop.h>
#include <robmikh.common/d3dHelpers.desktop.h>
#include <robmikh.common/capture.desktop.interop.h>
