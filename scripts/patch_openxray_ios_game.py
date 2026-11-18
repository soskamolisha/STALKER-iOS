#!/usr/bin/env python3
"""Adapt the embedded engine's menus and launch log for Shadow of Chernobyl."""
import argparse
from pathlib import Path
import re

from patch_openxray_ios_renderer import replace


def patch_settings(src):
    script = src / "xrGame/ScriptXMLInit.cpp"
    replace(script, '#include "xrUICore/TabControl/UITabControl.h"',
        '#include "xrUICore/TabControl/UITabControl.h"\n'
        '#include "xrUICore/TabControl/UITabButton.h"\n'
        '#include "xrEngine/defines.h"')
    replace(script, "    CUIXmlInit::InitTabControl(m_xml, path, 0, pWnd);", """    const bool legacyMain = ShadowOfChernobylMode && xr_strcmp(path, "main_dialog:tab") == 0;
    const bool legacyAO = ShadowOfChernobylMode && xr_strcmp(path, "video_adv:radio_tab_ao_options") == 0;
    if (legacyMain || legacyAO)
        Msg("* ShoC options tab: %s (%s)", path, m_xml.m_xml_file_name);
    CUIXmlInit::InitTabControl(m_xml, path, 0, pWnd, !legacyAO, legacyMain || legacyAO);

    if (legacyMain || legacyAO)
    {
        static constexpr pcstr mainIds[] = {"video", "sound", "gameplay", "controls"};
        static constexpr pcstr aoIds[] = {"disabled", "default", "hdao", "hbao"};
        const auto& ids = legacyMain ? mainIds : aoIds;
        const auto tab = m_xml.NavigateToNode(path, 0);
        auto& buttons = *pWnd->GetButtonsVector();
        for (size_t i = 0; i < buttons.size(); ++i)
        {
            auto* button = buttons[i];
            if (legacyMain && button->m_btn_id == "game")
                button->m_btn_id = "gameplay";
            if (!button->IsIdDefaultAssigned())
                continue;

            const auto node = m_xml.NavigateToNode(tab, "button", i);
            const pcstr label = node ? m_xml.Read(node, "text", 0, "") : "";
            pcstr id = i < std::size(ids) ? ids[i] : nullptr;
            if (legacyMain)
            {
                if (xr_strcmp(label, "ui_mm_game") == 0)
                    id = "gameplay";
                for (const auto candidate : mainIds)
                {
                    string64 key;
                    xr_sprintf(key, "ui_mm_%s", candidate);
                    if (xr_strcmp(label, key) == 0)
                        id = candidate;
                }
            }
            if (id)
                button->m_btn_id = id;
        }
        if (legacyAO && buttons.empty())
        {
            Msg("* ShoC options: ambient occlusion controls are absent from this layout");
            pWnd->Show(false);
            pWnd->Enable(false);
        }
    }""")

    tab = src / "xrUICore/TabControl/UITabControl.cpp"
    for method in ("SetCurrentOptValue", "SaveOptValue"):
        start = f"void CUITabControl::{method}()\n{{"
        replace(tab, start, start + "\n    if (m_TabsArr.empty())\n        return;\n")


def patch_new_game(src):
    menu = src / "xrGame/ui/UIMMShniaga.cpp"
    source = menu.read_text()
    selector = re.search(r"void CUIMMShniaga::SelectBtn\(CUIWindow\* btn\)\n\{.*?\n\}", source, re.DOTALL)
    if selector is None:
        raise ValueError(f"Missing button selector in {menu}")
    replace(menu, selector[0], """void CUIMMShniaga::SelectBtn(CUIWindow* btn)
{
    const xr_vector<CUIStatic*>* buttons = nullptr;
    switch (m_page)
    {
    case epi_main: buttons = &m_buttons; break;
    case epi_new_game: buttons = &m_buttons_new; break;
    case epi_new_network_game: buttons = &m_buttons_new_network; break;
    case epi_none: return;
    }
    if (!buttons)
        return;

    for (size_t i = 0; i < buttons->size(); ++i)
    {
        if ((*buttons)[i] == btn)
        {
            SelectBtn(static_cast<int>(i));
            return;
        }
    }
}""")
    for buttons in ("m_buttons", "m_buttons_new", "m_buttons_new_network"):
        replace(menu, f"    SelectBtn({buttons}[0]);",
            f"    m_selected = nullptr;\n    m_selected_btn = -1;\n    if (!{buttons}.empty())\n"
            f"        SelectBtn({buttons}.front());")
    replace(menu, "void CUIMMShniaga::OnBtnClick()\n{", """void CUIMMShniaga::OnBtnClick()
{
    if (!m_selected)
        return;
    if (ShadowOfChernobylMode && (m_page == epi_new_game ||
        xr_strcmp(m_selected->WindowName(), "btn_new_game") == 0))
        Msg("* ShoC game start: menu button %s", m_selected->WindowName().c_str());""")
    replace(menu, '#include "UIMMShniaga.h"',
        '#include "UIMMShniaga.h"\n#include "xrEngine/defines.h"')

    persistent = src / "xrEngine/IGame_Persistent.cpp"
    start = "    if (E == eStart)\n    {"
    replace(persistent, start, start + '\n        if (ShadowOfChernobylMode)\n            Msg("* ShoC game start: entering KERNEL:start");')
    start_level = "        Start(op_server);\n        g_pGameLevel->net_Start(op_server, op_client);"
    replace(persistent, start_level, """        if (ShadowOfChernobylMode)
            Msg("* ShoC game start: loading campaign");
        Start(op_server);
        if (ShadowOfChernobylMode)
            Msg("* ShoC game start: starting level");
        g_pGameLevel->net_Start(op_server, op_client);
        if (ShadowOfChernobylMode)
            Msg("* ShoC game start: level start returned");""")


def patch_fatal_log(src):
    debug = src / "xrCore/xrDebug.cpp"
    replace(debug, '#if defined(XR_PLATFORM_LINUX) || defined(XR_PLATFORM_APPLE) || defined(XR_PLATFORM_BSD)',
        '#if defined(__APPLE__)\n#include <TargetConditionals.h>\n#endif\n\n'
        '#if defined(XR_PLATFORM_LINUX) || defined(XR_PLATFORM_APPLE) || defined(XR_PLATFORM_BSD)')
    replace(debug, "    buffer = assertionInfo;\n#if defined(XR_PLATFORM_WINDOWS)",
        "#if !defined(__APPLE__) || !TARGET_OS_IPHONE\n    buffer = assertionInfo;\n#endif\n#if defined(XR_PLATFORM_WINDOWS)")
    start = "    FlushLog();\n\n    if (ShowErrorMessage)"
    replace(debug, start, "#if defined(__APPLE__) && TARGET_OS_IPHONE\n    Log(message.c_str());\n#endif\n" + start)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("upstream", type=Path)
    src = parser.parse_args().upstream / "src"
    patch_settings(src)
    patch_new_game(src)
    patch_fatal_log(src)


if __name__ == "__main__":
    main()
