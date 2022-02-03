#pragma once

struct Display
{
    static std::vector<Display> GetAllDisplays();
    Display(HMONITOR handle);

    HMONITOR Handle() { return m_handle; }
    RECT const& Rect() { return m_rect; }

    bool operator==(const Display& display) { return m_handle == display.m_handle; }
    bool operator!=(const Display& display) { return !(*this == display); }

private:
    HMONITOR m_handle = nullptr;
    RECT m_rect = {};
};
