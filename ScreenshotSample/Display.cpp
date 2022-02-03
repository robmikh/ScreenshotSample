#include "pch.h"
#include "Display.h"

std::vector<Display> Display::GetAllDisplays()
{
    std::vector<Display> displays;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hmon, HDC, LPRECT, LPARAM lparam)
    {
        auto& displays = *reinterpret_cast<std::vector<Display>*>(lparam);
        displays.push_back(Display(hmon));

        return TRUE;
    }, reinterpret_cast<LPARAM>(&displays));
    return displays;
}

Display::Display(HMONITOR handle)
{
    m_handle = handle;
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    winrt::check_bool(GetMonitorInfoW(m_handle, &monitorInfo));
    m_rect = monitorInfo.rcMonitor;
}
