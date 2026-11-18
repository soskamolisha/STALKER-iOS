#!/usr/bin/env python3
"""Connect iPhone render sizes and legacy UI sprites to the in-game options."""
import argparse
from pathlib import Path

from patch_openxray_ios_renderer import replace


IOS_HEADER = "\n#if defined(__APPLE__)\n#include <TargetConditionals.h>\n#endif\n"
IOS = "#if defined(__APPLE__) && TARGET_OS_IPHONE"


def ios_include(path, anchor='#include "stdafx.h"'):
    replace(path, anchor, anchor + "\n" + IOS_HEADER)


def patch_display(engine):
    mode = engine / "Device_mode.cpp"
    ios_include(mode)
    anchor = "xr_map<u32, xr_vector<xr_token>> vid_mode_token;"
    replace(mode, anchor, anchor + "\n\n" + IOS + """
namespace
{
constexpr struct { u32 width, height; } iosRenderModes[] = {
    {800, 600}, {1024, 768}, {1280, 720}, {1280, 800}, {1366, 768},
    {1600, 900}, {1920, 1080}, {2796, 1290},
};
constexpr auto defaultIOSRenderMode = iosRenderModes[2];
}
#endif""")
    replace(mode, "    const int modeCount = SDL_GetNumDisplayModes(monitorID);", IOS + """
    for (int i = 0; i < static_cast<int>(std::size(iosRenderModes)); ++i)
    {
        string32 name;
        xr_sprintf(name, "%ux%u", iosRenderModes[i].width, iosRenderModes[i].height);
        vid_mode_token[monitorID].emplace_back(xr_strdup(name), i);
    }
#else
    const int modeCount = SDL_GetNumDisplayModes(monitorID);""")
    replace(mode, "    vid_mode_token[monitorID].emplace_back(nullptr, -1);",
        "#endif\n    vid_mode_token[monitorID].emplace_back(nullptr, -1);")
    replace(mode, "    // Changing monitor, unset fullscreen for the previous monitor", "#if !defined(__APPLE__) || !TARGET_OS_IPHONE\n"
        "    // Changing monitor, unset fullscreen for the previous monitor")
    replace(mode, "    SDL_PumpEvents();\n    UpdateWindowRects();", "#endif\n    SDL_PumpEvents();\n    UpdateWindowRects();")
    replace(mode, "    io.DisplayFramebufferScale = ImVec2{ float(dwWidth / m_rcWindowClient.w), float(dwHeight / m_rcWindowClient.h) };", IOS + """
    io.DisplaySize = {float(m_rcWindowClient.w), float(m_rcWindowClient.h)};
    if (m_rcWindowClient.w > 0 && m_rcWindowClient.h > 0)
        io.DisplayFramebufferScale = {float(dwWidth) / m_rcWindowClient.w, float(dwHeight) / m_rcWindowClient.h};
#else
    io.DisplayFramebufferScale = ImVec2{ float(dwWidth / m_rcWindowClient.w), float(dwHeight / m_rcWindowClient.h) };
#endif""")
    replace(mode, "    // Dedicated server hardcoded resolution", IOS + """
    UNUSED(windowed);
    psDeviceMode.WindowStyle = rsFullscreenBorderless;
    psDeviceMode.Monitor = 0;
    const bool available = std::any_of(std::begin(iosRenderModes), std::end(iosRenderModes), [](auto size) {
        return size.width == psDeviceMode.Width && size.height == psDeviceMode.Height;
    });
    if (!available)
    {
        psDeviceMode.Width = defaultIOSRenderMode.width;
        psDeviceMode.Height = defaultIOSRenderMode.height;
    }
#else
    // Dedicated server hardcoded resolution""")
    replace(mode, "    dwWidth = psDeviceMode.Width;\n    dwHeight = psDeviceMode.Height;",
        "#endif\n    dwWidth = psDeviceMode.Width;\n    dwHeight = psDeviceMode.Height;")

    init = engine / "Device_Initialize.cpp"
    ios_include(init)
    replace(init, "        GEnv.Render->ObtainRequiredWindowFlags(flags);", IOS + """
        flags = (flags & ~SDL_WINDOW_RESIZABLE) | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI;
#endif
        GEnv.Render->ObtainRequiredWindowFlags(flags);""")

    events = engine / "device.cpp"
    ios_include(events)
    replace(events, "                if (static_cast<int>(psDeviceMode.Width) == event.window.data1", "#if !defined(__APPLE__) || !TARGET_OS_IPHONE\n"
        "                if (static_cast<int>(psDeviceMode.Width) == event.window.data1")
    replace(events, "                psDeviceMode.Height = event.window.data2;\n\n                Reset();",
        "                psDeviceMode.Height = event.window.data2;\n\n                Reset();\n#endif")

    console = engine / "xr_ioc_cmd.cpp"
    ios_include(console)
    status = '        xr_sprintf(S, "%ux%u (%uHz)", psDeviceMode.Width, psDeviceMode.Height, psDeviceMode.RefreshRate);'
    replace(console, status, IOS + '\n        xr_sprintf(S, "%ux%u", psDeviceMode.Width, psDeviceMode.Height);\n#else\n' + status + '\n#endif')


def patch_memory(engine):
    persistent = engine / "IGame_Persistent.cpp"
    ios_include(persistent)
    replace(persistent, '    if (!strstr(Core.Params, "-noprefetch"))\n        Prefetch();', IOS + """
    // Defer the global desktop precache. The level loader still prepares the
    // textures and models it actually uses.
    Log("* iOS: skipping global object and texture prefetch");
#else
    if (!strstr(Core.Params, "-noprefetch"))
        Prefetch();
#endif""")


def patch_legacy_ui(src):
    script = src / "xrGame/ScriptXMLInit.cpp"
    ios_include(script, '#include "pch_script.h"')
    replace(script, "    CUIXmlInit::InitComboBox(m_xml, path, 0, pWnd);", "    CUIXmlInit::InitComboBox(m_xml, path, 0, pWnd);\n" + IOS + """
    if (ShadowOfChernobylMode && xr_strcmp(path, "tab_video:list_resolution") == 0)
    {
        if (!m_xml.NavigateToNode("tab_video:list_resolution:options_item", 0))
            pWnd->AssignProps("vid_mode", "mm_opt_video");
        pWnd->SetSystemDepends(CUIOptionsItem::sdVidRestart);
    }
#endif""")

    atlas = src / "xrUICore/XML/UITextureMaster.cpp"
    ios_include(atlas, '#include "pch.hpp"')
    replace(atlas, "    // Renderer has some path transformations", IOS + r"""
    if (ShadowOfChernobylMode && texture_name && xr_strnicmp(texture_name.c_str(), "ui_", 3) == 0)
    {
        if (texture_name == "ui_brokenline" && ItemExist("ui_brokenline_back"))
            return InitTexture("ui_brokenline_back", tc, shader_name);

        string_path candidate, resolved;
        strconcat(candidate, "ui" DELIMITER, texture_name.c_str());
        if (FS.exist(resolved, "$game_textures$", candidate, ".dds"))
        {
            tc->CreateShader(candidate, shader_name.c_str());
            return true;
        }
        if (!FS.exist(resolved, "$game_textures$", texture_name.c_str(), ".dds") &&
            !FS.exist(resolved, "$game_textures$", texture_name.c_str(), ".ogm"))
        {
            // A fallback checker has dimensions too. Don't mistake it for
            // an existing part of the original options or scrollbar atlas.
            tc->SetShader(ui_shader());
            return false;
        }
    }
#endif

    // Renderer has some path transformations""")

    frame = src / "xrUICore/Windows/UIFrameLineWnd.cpp"
    ios_include(frame, '#include "pch.hpp"')
    replace(frame, '    strconcat(second, texture, "_e");', '    strconcat(second, texture, "_e");\n\n' + IOS + """
    // The SoC scrollbar has one sprite in place of the three-part frame.
    if (ShadowOfChernobylMode && !fatal && CUITextureMaster::ItemExist(texture) &&
        (!CUITextureMaster::ItemExist(back) || !CUITextureMaster::ItemExist(first) || !CUITextureMaster::ItemExist(second)))
        return false;
#endif""")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("upstream", type=Path)
    src = parser.parse_args().upstream / "src"
    patch_display(src / "xrEngine")
    patch_memory(src / "xrEngine")
    patch_legacy_ui(src)


if __name__ == "__main__":
    main()
