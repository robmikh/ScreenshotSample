#include "pch.h"
#include "Display.h"

std::vector<DISPLAYCONFIG_PATH_INFO> GetDisplayConfigPathInfos()
{
    uint32_t numPaths = 0;
    uint32_t numModes = 0;
    winrt::check_win32(GetDisplayConfigBufferSizes(
        QDC_ONLY_ACTIVE_PATHS,
        &numPaths,
        &numModes));
    std::vector<DISPLAYCONFIG_PATH_INFO> pathInfos(numPaths, DISPLAYCONFIG_PATH_INFO{});
    std::vector<DISPLAYCONFIG_MODE_INFO> modeInfos(numModes, DISPLAYCONFIG_MODE_INFO{});
    winrt::check_win32(QueryDisplayConfig(
        QDC_ONLY_ACTIVE_PATHS,
        &numPaths,
        pathInfos.data(),
        &numModes,
        modeInfos.data(),
        nullptr));
    pathInfos.resize(numPaths);
    return pathInfos;
}

struct DisplayHDRInfo
{
    bool IsHDR = false;
    // Only valid if IsHDR is true
    float SDRWhiteLevelInNits = 0.0f;
};

std::vector<Display> Display::GetAllDisplays()
{
    // Get all the display handles
    std::vector<HMONITOR> displayHandles;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hmon, HDC, LPRECT, LPARAM lparam)
    {
        auto& displayHandles = *reinterpret_cast<std::vector<HMONITOR>*>(lparam);
        displayHandles.push_back(hmon);

        return TRUE;
    }, reinterpret_cast<LPARAM>(&displayHandles));

    // Get all the display config path infos
    auto pathInfos = GetDisplayConfigPathInfos();

    // Build a mapping of device names to HDR information
    std::map<std::wstring, DisplayHDRInfo> namesToHDRInfos;
    for (auto&& pathInfo : pathInfos)
    {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME deviceName = {};
        deviceName.header.size = sizeof(deviceName);
        deviceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        deviceName.header.adapterId = pathInfo.sourceInfo.adapterId;
        deviceName.header.id = pathInfo.sourceInfo.id;
        winrt::check_win32(DisplayConfigGetDeviceInfo(&deviceName.header));
        std::wstring name(deviceName.viewGdiDeviceName);

        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO colorInfo = {};
        colorInfo.header.size = sizeof(colorInfo);
        colorInfo.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
        colorInfo.header.adapterId = pathInfo.targetInfo.adapterId;
        colorInfo.header.id = pathInfo.targetInfo.id;
        winrt::check_win32(DisplayConfigGetDeviceInfo(&colorInfo.header));
        bool isHDR = colorInfo.advancedColorEnabled && !colorInfo.wideColorEnforced;

        float sdrWhiteLevelInNits = 0.0f;
        if (isHDR)
        {
            DISPLAYCONFIG_SDR_WHITE_LEVEL whiteLevel = {};
            whiteLevel.header.size = sizeof(whiteLevel);
            whiteLevel.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
            whiteLevel.header.adapterId = pathInfo.targetInfo.adapterId;
            whiteLevel.header.id = pathInfo.targetInfo.id;
            winrt::check_win32(DisplayConfigGetDeviceInfo(&whiteLevel.header));
            sdrWhiteLevelInNits = static_cast<float>((whiteLevel.SDRWhiteLevel / 1000.0) * 80.0);
        }

        namesToHDRInfos.insert({ name, { isHDR, sdrWhiteLevelInNits } });
    }

    // Go through each display and find the matching hdr info
    std::vector<Display> displays;
    for (auto&& displayHandle : displayHandles)
    {
        MONITORINFOEXW monitorInfo = {};
        monitorInfo.cbSize = sizeof(monitorInfo);
        winrt::check_bool(GetMonitorInfoW(displayHandle, &monitorInfo));
        std::wstring name(monitorInfo.szDevice);

        auto hdrInfo = namesToHDRInfos[name];
        displays.push_back(Display(displayHandle, monitorInfo.rcMonitor, hdrInfo.IsHDR, hdrInfo.SDRWhiteLevelInNits));
    }

    return displays;
}

Display::Display(HMONITOR handle, RECT rect, bool isHDR, float sdrWhiteLevelInNits)
{
    m_handle = handle;
    m_rect = rect;
    m_isHDR = isHDR;
    m_sdrWhiteLevelInNits = sdrWhiteLevelInNits;
}
