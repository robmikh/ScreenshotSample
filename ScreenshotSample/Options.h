#pragma once

class Options
{
public:
    static void InitOptions(bool dxDebug, bool forceHDR, bool clipHDR);

    static bool DxDebug() { return s_options.m_dxDebug; }
    static bool ForceHDR() { return s_options.m_forceHDR; }
    static bool ClipHDR() { return s_options.m_clipHDR; }

private:
    static Options s_options;

    bool m_dxDebug = false;
    bool m_forceHDR = false;
    bool m_clipHDR = false;
};
