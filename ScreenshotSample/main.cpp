#include "pch.h"
#include "Display.h"

namespace winrt
{
    using namespace Windows::Foundation;
}

int wmain()
{
    // Init COM
    winrt::init_apartment();
    // We don't want virtualized coordinates
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Enumerate displays
    auto displays = Display::GetAllDisplays();

    // Determine the union of all displays
    RECT unionRect = {};
    unionRect.left = LONG_MAX;
    unionRect.top = LONG_MAX;
    unionRect.right = LONG_MIN;
    unionRect.bottom = LONG_MIN;
    for (auto&& display : displays)
    {
        auto& displayRect = display.Rect();

        if (unionRect.left > displayRect.left)
        {
            unionRect.left = displayRect.left;
        }
        if (unionRect.top > displayRect.top)
        {
            unionRect.top = displayRect.top;
        }
        if (unionRect.right < displayRect.right)
        {
            unionRect.right = displayRect.right;
        }
        if (unionRect.bottom < displayRect.bottom)
        {
            unionRect.bottom = displayRect.bottom;
        }
    }

    return 0;
}
