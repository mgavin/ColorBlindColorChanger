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

      bool                                             plugin_enabled = false;
      bool                                             cb_enabled     = false;  // is the COLOR BLIND SETTING enabled?!
      bool                                             globally_set   = true;
      std::vector<std::string>                         game_maps_list;
      std::vector<std::pair<std::string, std::string>> game_maps_name_filename;
      int                                              colorize_option = 0;

      static inline const int         BLUE_TEAM_IDX   = 0;  // just to designate these more easily as being blue/orange
      static inline const int         ORANGE_TEAM_IDX = 1;
      static inline const LinearColor default_blue_rgba   = {0.0f, 0.1f, 0.45f, 1.0f};
      static inline const LinearColor default_orange_rgba = {1.0f, 0.4f, 0.0f, 1.0f};
      static inline const char *      color_set_choice[2] = {
            "Set Blue / Orange team color",
            "Set Your / Opponent team color"};

      // helper functions
      void init_cvars();
      void init_hooked_events();

      void enable_plugin();
      void disable_plugin();

      void hook_colorblind_color_change_events();
      void unhook_colorblind_color_change_events();

      void set_colorblind_colors();

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
