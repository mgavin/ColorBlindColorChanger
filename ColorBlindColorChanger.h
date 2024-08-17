// UPPERCASE THE FOLLOWING MACRO
#ifndef __COLORBLINDCOLORCHANGER_H__
#define __COLORBLINDCOLORCHANGER_H__

#include <memory>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginsettingswindow.h"
// #include "bakkesmod/plugin/pluginwindow.h"

#include "imgui.h"
#include "imgui_internal.h"

// #include "bm_helper.h"
// #include "imgui_helper.h"

#include "PersistentManagedCVarStorage.h"

// registerCvar([req] name,[req] default_value,[req] description, searchable, has_min,
// min, has_max, max, save_to_cfg)
#define LIST_OF_PLUGIN_CVARS                          \
      X(enabled, "1", "Is the plugin enabled?", true) \
      // X()

#include "CVarManager.h"

class ColorBlindColorChanger :
      public BakkesMod::Plugin::BakkesModPlugin,
      public BakkesMod::Plugin::PluginSettingsWindow {
private:
      const ImColor col_white = ImColor {
            ImVec4 {1.0f, 1.0f, 1.0f, 1.0f}
      };

      // data members
      std::unique_ptr<PersistentManagedCVarStorage> cvar_storage;

      bool plugin_enabled = false;
      bool cb_enabled     = false;  // is the COLOR BLIND SETTING enabled?!

      const int BLUE_TEAM_IDX   = 0;  // just to designate these more easily as being blue/orange
      const int ORANGE_TEAM_IDX = 1;

      // helper functions
      void init_cvars();
      void init_hooked_events();

      void enable_plugin();
      void disable_plugin();

      void hook_colorblind_color_change_events();
      void unhook_colorblind_color_change_events();

public:
      // honestly, for the sake of inheritance,
      // the member access in this class doesn't matter.
      // (since it's not expected to be inherited from)

      void onLoad() override;
      void onUnload() override;

      void        RenderSettings() override;
      std::string GetPluginName() override;
      void        SetImGuiContext(uintptr_t ctx) override;

      //
      // inherit from
      //				public BakkesMod::Plugin::PluginWindow
      //	 for the following
      // void        OnOpen() override;
      // void        OnClose() override;
      // void        Render() override;
      // std::string GetMenuName() override;
      // std::string GetMenuTitle() override;
      // bool        IsActiveOverlay() override;
      // bool        ShouldBlockInput() override;
};

#endif
