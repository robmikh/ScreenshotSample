#pragma once

struct Display
{
    static std::vector<Display> GetAllDisplays();
    Display(HMONITOR handle, RECT rect, bool isHDR, float sdrWhiteLevelInNits, float maxLuminance);

    HMONITOR Handle() const { return m_handle; }
    RECT const& Rect() const { return m_rect; }
    bool IsHDR() const { return m_isHDR; }
    // Only valid if IsHDR is true
    float SDRWhiteLevelInNits() const { return m_sdrWhiteLevelInNits; }
    float MaxLuminance() const { return m_maxLuminance; }

    bool operator==(const Display& display) { return m_handle == display.m_handle; }
    bool operator!=(const Display& display) { return !(*this == display); }

private:
    HMONITOR m_handle = nullptr;
    RECT m_rect = {};
    bool m_isHDR = false;
    float m_sdrWhiteLevelInNits = 0.0f;
    float m_maxLuminance = 0.0f;
};
