#include "pch.h"
#include "Options.h"

Options Options::s_options = {};

void Options::InitOptions(bool dxDebug, bool forceHDR, bool clipHDR)
{
    s_options.m_dxDebug = dxDebug;
    s_options.m_forceHDR = forceHDR;
    s_options.m_clipHDR = clipHDR;
}
