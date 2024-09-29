#include "ColorBlindColorChanger.h"

#include "Windows.h"  // IWYU pragma: keep

#include "shellapi.h"

#include "imgui.h"
#include "imgui_helper.h"
#include "imgui_internal.h"

#include "bakkesmod/../utils/parser.h"
#include "bakkesmod/wrappers/gameobject/MapListWrapper.h"

#include "bm_helper.h"
#include "CVarManager.h"
#include "HookedEvents.h"
#include "Logger.h"
#include "PersistentManagedCVarStorage.h"

namespace {

namespace log = LOGGER;
};  // namespace

BAKKESMOD_PLUGIN(ColorBlindColorChanger, "ColorBlindColorChanger", "0.1.0", /*UNUSED*/ NULL);

/**
 * \brief do the following when your plugin is loaded
 */
void ColorBlindColorChanger::onLoad() {
      // initialize things
      HookedEvents::gameWrapper = gameWrapper;

      // set up logging necessities
      log::set_cvarmanager(cvarManager);
      log::set_loglevel(log::LOGLEVEL::INFO);

      // set a prefix to attach in front of all cvars to avoid name clashes
      CVarManager::instance().set_cvar_prefix("cbcc_");  // INCLUDE PLUGIN CVAR PREFIX HERE!!!
      CVarManager::instance().set_cvarmanager(cvarManager);

      // MAKE SECOND PARAMETER A SHORT FORM NAME OF THE PLUGIN + "_cvars"
      cvar_storage = std::make_unique<PersistentManagedCVarStorage>(this, "colorblindcolorchanger_cvars", true, true);

      MapListWrapper mlw = gameWrapper->GetMapListWrapper();
      if (mlw) {
            ArrayWrapper<MapDataWrapper> awmdw = mlw.GetSortedMaps();
            for (const auto & map : awmdw) {
                  if (!map) {
                        continue;
                  }

                  game_maps_name_filename.emplace_back(std::make_pair(map.GetLocalizedName(), map.GetName()));
            }
      }

      init_cvars();
      init_hooked_events();

      cb_enabled = gameWrapper->GetbColorBlind();
      log::log_debug("GETBCOLORBLIND/cb_enabled: {}", cb_enabled);
}

/**
 * \brief group together the initialization of cvars
 */
void ColorBlindColorChanger::init_cvars() {
      // REGISTER THE CVARS DECLARED IN THE HEADER
      CVarManager::instance().register_cvars();

#define X(name, ...) cvar_storage->AddCVar(CVarManager::instance().get_cvar_prefix() + #name);
      LIST_OF_PLUGIN_CVARS
#undef X

      CVarWrapper bcvar = CVarManager::instance().getCVM()->registerCvar(
            CVarManager::instance().get_cvar_prefix() + "global" + "_blue",
            to_string_color(default_blue_rgba),
            "Blue/Your RGBA value for global",
            false);
      CVarWrapper ocvar = CVarManager::instance().getCVM()->registerCvar(
            CVarManager::instance().get_cvar_prefix() + "global" + "_orange",
            to_string_color(default_orange_rgba),
            "Orange/Opponent RGBA value for global",
            false);
      cvar_storage->AddCVar(CVarManager::instance().get_cvar_prefix() + "global" + "_blue");
      cvar_storage->AddCVar(CVarManager::instance().get_cvar_prefix() + "global" + "_orange");

      // add a cvar for every map
      for (const auto & map_name : game_maps_name_filename) {
            CVarWrapper bcvar = CVarManager::instance().getCVM()->registerCvar(
                  CVarManager::instance().get_cvar_prefix() + map_name.second + "_blue",
                  to_string_color(default_blue_rgba),
                  "Blue/Your RGBA value for " + map_name.first,
                  false);
            CVarWrapper ocvar = CVarManager::instance().getCVM()->registerCvar(
                  CVarManager::instance().get_cvar_prefix() + map_name.second + "_orange",
                  to_string_color(default_orange_rgba),
                  "Orange/Opponent RGBA value for " + map_name.first,
                  false);
            cvar_storage->AddCVar(CVarManager::instance().get_cvar_prefix() + map_name.second + "_blue");
            cvar_storage->AddCVar(CVarManager::instance().get_cvar_prefix() + map_name.second + "_orange");
      }

      CVarManager::instance().get_cvar_enabled().addOnValueChanged([this](std::string oldValue, CVarWrapper newValue) {
            if (newValue.getBoolValue()) {
                  enable_plugin();
            } else {
                  disable_plugin();
            }
      });
}

/**
 * \brief group together the initialization of hooked events
 */
void ColorBlindColorChanger::init_hooked_events() {
      HookedEvents::AddHookedEventWithCaller<ActorWrapper>(
            "Function TAGame.GFxData_Settings_TA.SetColorBlind",
            [this](ActorWrapper unused, void * params, std::string event_name) {
                  log::log_info("CALLING {}...", event_name);
                  struct parms {
                        unsigned char unused[0x8];  // HAS THINGS BUT I DONT WANT THEM
                        bool          b;
                  } * b      = reinterpret_cast<parms *>(params);
                  cb_enabled = b->b;
            });

      HookedEvents::AddHookedEvent("Function Engine.GameInfo.PreExit", [this](std::string event_name) {
            log::log_info("CALLING {}...", event_name);
            // ASSURED CLEANUP
            onUnload();
      });
}

void ColorBlindColorChanger::hook_colorblind_color_change_events() {
      HookedEvents::AddHookedEvent(
            "Function TAGame.GFxHUD_TA.OnAllTeamsCreated",
            [this](auto... fargs) {
                  if (!globally_set) {
                        // this is because it seems only valid if blue/orange is set
                        return;
                  }
                  log::log_info("CALLING {} ...", fargs...);
                  set_colorblind_colors();
            },
            true);

      HookedEvents::AddHookedEvent(
            // this function seems to be called when a game starts, it's perfect for setting your/oppo team colors
            "Function Engine.HUD.SetShowScores",
            [this](auto... fargs) {
                  log::log_info("CALLING {} ...", fargs...);
                  set_colorblind_colors();
            },
            true);
}

void ColorBlindColorChanger::unhook_colorblind_color_change_events() {
      HookedEvents::RemoveHook("Function TAGame.GFxHUD_TA.OnAllTeamsCreated");
      HookedEvents::RemoveHook("Function Engine.HUD.SetShowScores");
}

void ColorBlindColorChanger::set_colorblind_colors() {
      gameWrapper->SetTimeout(
            [this](GameWrapper * gw) {
                  ServerWrapper sw = gameWrapper->GetCurrentGameState();
                  if (!sw) {
                        log::log_error("NO SERVER");
                        return;
                  }

                  std::string map_name_or_global = globally_set ? "global" : gameWrapper->GetCurrentMap();
                  log::log_debug("map_name_or_global_string = {}", map_name_or_global);
                  CVarWrapper bcvar = CVarManager::instance().getCVM()->getCvar(
                        CVarManager::instance().get_cvar_prefix() + map_name_or_global + "_blue");
                  CVarWrapper ocvar = CVarManager::instance().getCVM()->getCvar(
                        CVarManager::instance().get_cvar_prefix() + map_name_or_global + "_orange");

                  ArrayWrapper<TeamWrapper> awtw = sw.GetTeams();
                  if (awtw.IsNull()) {
                        log::log_error("NO ARRAYWRAPPER<TEAMWRAPPER>");
                        return;
                  }

                  int blue_team_idx   = BLUE_TEAM_IDX;
                  int orange_team_idx = ORANGE_TEAM_IDX;

                  if (colorize_option == 0) {  // blue x orange

                  } else {  // your team x oppo
                        // getting your team number
                        PlayerControllerWrapper pcw = gameWrapper->GetPlayerController();
                        if (!pcw) {
                              log::log_error("NO PCW");
                              return;
                        }

                        PriWrapper priw = pcw.GetPRI();
                        if (!priw) {
                              log::log_error("NO PRIW");
                              return;
                        }

                        CarWrapper cw = priw.GetCar();
                        if (!cw) {
                              log::log_error("NO CAR");
                              return;
                        }

                        if (cw.HasTeam()) {
                              blue_team_idx   = priw.GetTeamNum();
                              orange_team_idx = 1 - blue_team_idx;  // there are assumedly only 2 teams.
                        }
                  }

                  TeamWrapper bteam = awtw.Count() > 0 ? awtw.Get(blue_team_idx) : NULL;
                  if (bteam) {
                        bteam.SetColorBlindFontColor(bcvar.getColorValue());
                        bteam.UpdateColors();
                  } else {
                        log::log_error("NO BLUE TEAM");
                  }

                  // Modes like Knockout don't have an "orange team", so this is for protection against that.
                  TeamWrapper oteam = awtw.Count() > 1 ? awtw.Get(orange_team_idx) : NULL;
                  if (oteam) {
                        oteam.SetColorBlindFontColor(ocvar.getColorValue());
                        oteam.UpdateColors();
                  } else {
                        log::log_error("NO ORANGE TEAM");
                  }
            },
            0.25f);
}

void ColorBlindColorChanger::unset_colorblind_colors() {
      gameWrapper->SetTimeout(
            [this](GameWrapper * gw) {
                  ServerWrapper sw = gameWrapper->GetCurrentGameState();
                  if (!sw) {
                        log::log_error("NO SERVER");
                        return;
                  }

                  ArrayWrapper<TeamWrapper> awtw = sw.GetTeams();
                  if (awtw.IsNull()) {
                        log::log_error("NO ARRAYWRAPPER<TEAMWRAPPER>");
                        return;
                  }

                  TeamWrapper bteam = awtw.Count() > 0 ? awtw.Get(BLUE_TEAM_IDX) : NULL;
                  if (bteam) {
                        bteam.SetColorBlindFontColor(default_blue_rgba);
                        bteam.UpdateColors();
                  } else {
                        log::log_error("NO BLUE TEAM");
                  }

                  // Modes like Knockout don't have an "orange team", so this is for protection against that.
                  TeamWrapper oteam = awtw.Count() > 1 ? awtw.Get(ORANGE_TEAM_IDX) : NULL;
                  if (oteam) {
                        oteam.SetColorBlindFontColor(default_orange_rgba);
                        oteam.UpdateColors();
                  } else {
                        log::log_error("NO ORANGE TEAM");
                  }
            },
            0.25f);
}

void ColorBlindColorChanger::enable_plugin() {
      plugin_enabled = true;
      hook_colorblind_color_change_events();
      set_colorblind_colors();
}

void ColorBlindColorChanger::disable_plugin() {
      plugin_enabled = false;
      unhook_colorblind_color_change_events();
      unset_colorblind_colors();
}

/**
 * \brief This is for helping with IMGUI stuff
 *
 *  copied from: https://github.com/ocornut/imgui/discussions/3862
 *
 * \param width total width of items
 * \param alignment where on the line to align
 */
static inline void AlignForWidth(float width, float alignment = 0.5f) {
      float avail = ImGui::GetContentRegionAvail().x;
      float off   = (avail - width) * alignment;
      if (off > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
      }
}

/**
 * \brief https://mastodon.gamedev.place/@dougbinks/99009293355650878
 *
 * \param col_ The color the underline should be.
 */
static inline void AddUnderline(ImColor col_) {
      ImVec2 min = ImGui::GetItemRectMin();
      ImVec2 max = ImGui::GetItemRectMax();
      min.y      = max.y;
      ImGui::GetWindowDrawList()->AddLine(min, max, col_, 1.0f);
}

/**
 * \brief taken from https://gist.github.com/dougbinks/ef0962ef6ebe2cadae76c4e9f0586c69
 * "hyperlink urls"
 *
 * \param name_ The shown text.
 * \param URL_ The url accessed after clicking the shown text.
 * \param SameLineBefore_ Should use on the same line before?
 * \param SameLineAfter_ Should use on the same line after?
 */
static inline void TextURL(const char * name_, const char * URL_, uint8_t SameLineBefore_, uint8_t SameLineAfter_) {
      if (1 == SameLineBefore_) {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
      }
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 165, 255, 255));
      ImGui::Text("%s", name_);
      ImGui::PopStyleColor();
      if (ImGui::IsItemHovered()) {
            if (ImGui::IsMouseClicked(0)) {
                  // What if the URL length is greater than int but less than size_t?
                  // well then the program should crash, but this is fine.
                  const int nchar =
                        std::clamp(static_cast<int>(std::strlen(URL_)), 0, (std::numeric_limits<int>::max)() - 1);
                  wchar_t * URL = new wchar_t[nchar + 1];
                  wmemset(URL, 0, nchar + 1);
                  MultiByteToWideChar(CP_UTF8, 0, URL_, nchar, URL, nchar);
                  ShellExecuteW(NULL, L"open", URL, NULL, NULL, SW_SHOWNORMAL);

                  delete[] URL;
            }
            AddUnderline(ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
            ImGui::SetTooltip("  Open in browser\n%s", URL_);
      } else {
            AddUnderline(ImGui::GetStyle().Colors[ImGuiCol_Button]);
      }
      if (1 == SameLineAfter_) {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
      }
}

/**
 * \brief This call usually includes ImGui code that is shown and rendered (repeatedly,
 * on every frame rendered) when your plugin is selected in the plugin
 * manager. AFAIK, if your plugin doesn't have an associated *.set file for its
 * settings, this will be used instead.
 *
 */
void ColorBlindColorChanger::RenderSettings() {
      if (!cb_enabled) {
            std::string disclaimer {
                  "This plugin does not work with the color blind setting disabled "
                  "(found in Settings -> Interface -> Color blind mode)."};
            float dwidth = ImGui::CalcTextSize(disclaimer.c_str()).x;
            AlignForWidth(dwidth);
            ImGui::TextWrapped(disclaimer.c_str());
            return;
      }

      if (ImGui::Checkbox("Enable plugin", &plugin_enabled)) {
            CVarManager::instance().get_cvar_enabled().setValue(plugin_enabled);
      }

      ImGui::SameLine();
      std::string s {"Debug level? (leave OFF to spare your console)"};
      float       item_size = ImGui::CalcTextSize(s.c_str()).x + 75.0f;
      AlignForWidth(item_size, 1.0f);
      ImGui::TextUnformatted(s.c_str());

      ImGui::SameLine();
      ImGui::SetNextItemWidth(75.0f);
      if (ImGui::BeginCombo("##debug_level", debug_level, ImGuiComboFlags_NoArrowButton)) {
            for (int n = 0; n < IM_ARRAYSIZE(debug_levels); n++) {
                  bool is_selected = (debug_level == debug_levels[n]);
                  if (ImGui::Selectable(debug_levels[n], is_selected)) {
                        debug_level = debug_levels[n];
                        log::set_loglevel(static_cast<log::LOGLEVEL>(n));
                  }
                  if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                  }
            }
            ImGui::EndCombo();
      }

      ImGui::Separator();

      with_Disabled(!plugin_enabled);
      ImGui::Checkbox("Set color globally? ", &globally_set);
      ImGui::SameLine(ImGui::GetCursorPosX(), 100.0f);
      ImGui::SetNextItemWidth(200.0f);
      if (ImGui::Combo("##colorize_option", &colorize_option, color_set_choice, IM_ARRAYSIZE(color_set_choice))) {
            set_colorblind_colors();
      }

      ImGui::SameLine(740.0f, 0.0f);
      ImGui::SetNextItemWidth(150.0f);
      if (ImGui::Button("Update Colors")) {
            gameWrapper->Execute([this](GameWrapper * gw) { set_colorblind_colors(); });
      }

      ImGui::NewLine();
      static float cpos_bottom_blue   = 0.0f;
      static float cpos_bottom_orange = 0.0f;

      ImGui::Columns(3, "##settings", false);
      ImGui::SetColumnWidth(0, 400.0f);
      ImGui::SetColumnWidth(1, 340.0f);

      static size_t selected_map_idx = 0;
      maybe_Disabled(globally_set) {
            ImGui::TextUnformatted("Asterisk (*) by map name means a non-default color is set.");
            ImGui::BeginGroup();
            bool window_visible = ImGui::BeginChild(
                  ImGui::GetID(reinterpret_cast<void *>(static_cast<intptr_t>(0))),
                  ImVec2(ImGui::GetContentRegionAvailWidth(), (std::max)(cpos_bottom_orange - 50.0f, 300.0f)),
                  true,
                  NULL);
            if (window_visible) {
                  for (size_t i = 0; i < game_maps_name_filename.size(); ++i) {
                        std::string_view map_name     = game_maps_name_filename[i].first;
                        std::string_view map_filename = game_maps_name_filename[i].second;
                        bool             is_set =
                              (CVarManager::instance()
                                     .getCVM()
                                     ->getCvar(
                                           CVarManager::instance().get_cvar_prefix() + map_filename.data() + "_blue")
                                     .getColorValue()
                               != default_blue_rgba)
                              || (CVarManager::instance()
                                        .getCVM()
                                        ->getCvar(
                                              CVarManager::instance().get_cvar_prefix() + map_filename.data()
                                              + "_orange")
                                        .getColorValue()
                                  != default_orange_rgba);
                        char buf[128] = {0};
                        snprintf(
                              buf,
                              sizeof(buf),
                              "%s%s",
                              is_set ? "(*) " : "",
                              game_maps_name_filename[i].first.c_str());
                        if (ImGui::Selectable(
                                  buf,
                                  selected_map_idx == i,
                                  globally_set ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None)) {
                              selected_map_idx = i;
                        }
                  }
            }
            ImGui::EndChild();
            ImGui::EndGroup();
      }

      ImGui::NextColumn();

      if (globally_set) {
            ImGui::TextUnformatted("Setting the color for all maps:");
      } else {
            ImGui::Text("Setting the color for %s:", game_maps_name_filename[selected_map_idx].first.c_str());
      }

      std::string map_name_or_global = globally_set ? "global" : game_maps_name_filename[selected_map_idx].second;
      CVarWrapper bcvar              = CVarManager::instance().getCVM()->getCvar(
            CVarManager::instance().get_cvar_prefix() + map_name_or_global + "_blue");
      CVarWrapper ocvar = CVarManager::instance().getCVM()->getCvar(
            CVarManager::instance().get_cvar_prefix() + map_name_or_global + "_orange");

      ImGuiColorEditFlags color_select_flags =
            ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Float;

      const char * blue_txt = colorize_option == 0 ? "Blue" : "Your";
      ImGui::Text("%s team: ", blue_txt);
      const LinearColor & blue_color   = bcvar ? bcvar.getColorValue() : LinearColor {};
      float               blue_rgba[4] = {blue_color.R, blue_color.G, blue_color.B, blue_color.A};
      if (ImGui::ColorPicker4("##blue_color", blue_rgba, color_select_flags)) {
            bcvar.setValue(LinearColor {blue_rgba[0], blue_rgba[1], blue_rgba[2], blue_rgba[3]});
      }
      cpos_bottom_blue = ImGui::GetCursorPosY();

      ImGui::NewLine();

      const char * orange_txt = colorize_option == 0 ? "Orange" : "Opponent";
      ImGui::Text("%s team: ", orange_txt);
      const LinearColor & orange_color   = ocvar ? ocvar.getColorValue() : LinearColor {};
      float               orange_rgba[4] = {orange_color.R, orange_color.G, orange_color.B, orange_color.A};
      if (ImGui::ColorPicker4("##orange_color", orange_rgba, color_select_flags)) {
            ocvar.setValue(LinearColor {orange_rgba[0], orange_rgba[1], orange_rgba[2], orange_rgba[3]});
      }
      cpos_bottom_orange = ImGui::GetCursorPosY();

      ImGui::NextColumn();
      float indent_factor = 40.0f;

      ImGui::Indent(indent_factor);
      ImGui::SetNextItemWidth(100.0f);
      if (ImGui::Button("Load Map")) {
            constexpr static const char * load_map_str =
                  "start {}?Game=TAGame.GameInfo_Soccar_TA?Lan?GameTags=BotsIntro,UnlimitedTime,PlayerCount8";
            if (globally_set) {
                  gameWrapper->Execute([this](GameWrapper * gw) {
                        // needs to be called before the execute, otherwise...?
                        const std::string rmap = gw->GetRandomMap();
                        gw->ExecuteUnrealCommand(std::format(load_map_str, rmap));
                  });
            } else {
                  gameWrapper->Execute([this](GameWrapper * gw) {
                        log::log_debug(
                              "game_maps_name_filename[selected_map_idx].second: {}",
                              game_maps_name_filename[selected_map_idx].second);
                        gw->ExecuteUnrealCommand(
                              std::format(load_map_str, game_maps_name_filename[selected_map_idx].second));
                  });
            }
      }
      ImGui::Unindent(indent_factor);

      ImGui::SetCursorPosY(cpos_bottom_blue / 2.0f - 30.0f);
      ImGui::Indent(indent_factor);

      ImGui::SetNextItemWidth(25.0f);
      if (ImGui::Button("Set Default##def_blue")) {
            bcvar.setValue(default_blue_rgba);
      }

      ImGui::NewLine();
      ImGui::TextUnformatted("Copied color:");
      ImVec4 blue_temp_color = {temp_blue_copy.R, temp_blue_copy.G, temp_blue_copy.B, temp_blue_copy.A};
      ImGui::ColorButton("##copied_blue_color", blue_temp_color, color_select_flags, ImVec2(80, 50));
      ImGui::Unindent(indent_factor);
      ImGui::SetNextItemWidth(25.0f);
      if (ImGui::Button("Copy Color##blue")) {
            temp_blue_copy.R = blue_rgba[0];
            temp_blue_copy.G = blue_rgba[1];
            temp_blue_copy.B = blue_rgba[2];
            temp_blue_copy.A = blue_rgba[3];
      }

      ImGui::SameLine(0.0f, 20.0f);
      ImGui::SetNextItemWidth(25.0f);
      if (ImGui::Button("Paste Color##blue")) {
            blue_rgba[0] = temp_blue_copy.R;
            blue_rgba[1] = temp_blue_copy.G;
            blue_rgba[2] = temp_blue_copy.B;
            blue_rgba[3] = temp_blue_copy.A;
            bcvar.setValue(LinearColor {blue_rgba[0], blue_rgba[1], blue_rgba[2], blue_rgba[3]});
      }

      ImGui::SetCursorPosY(cpos_bottom_orange - (cpos_bottom_blue / 2.0f) - 30.0f);
      ImGui::Indent(indent_factor);

      ImGui::SetNextItemWidth(25.0f);
      if (ImGui::Button("Set Default##def_orange")) {
            ocvar.setValue(default_orange_rgba);
      }

      ImGui::NewLine();
      ImGui::TextUnformatted("Copied color:");
      ImVec4 orange_temp_color = {temp_orange_copy.R, temp_orange_copy.G, temp_orange_copy.B, temp_orange_copy.A};
      ImGui::ColorButton("##copied_orange_color", orange_temp_color, color_select_flags, ImVec2(80, 50));
      ImGui::Unindent(indent_factor);
      ImGui::SetNextItemWidth(25.0f);
      if (ImGui::Button("Copy Color##orange")) {
            temp_orange_copy.R = orange_rgba[0];
            temp_orange_copy.G = orange_rgba[1];
            temp_orange_copy.B = orange_rgba[2];
            temp_orange_copy.A = orange_rgba[3];
      }

      ImGui::SameLine(0.0f, 20.0f);
      ImGui::SetNextItemWidth(25.0f);
      if (ImGui::Button("Paste Color##orange")) {
            orange_rgba[0] = temp_orange_copy.R;
            orange_rgba[1] = temp_orange_copy.G;
            orange_rgba[2] = temp_orange_copy.B;
            orange_rgba[3] = temp_orange_copy.A;
            ocvar.setValue(LinearColor {orange_rgba[0], orange_rgba[1], orange_rgba[2], orange_rgba[3]});
      }

      ImGui::EndColumns();
}
/**
 * \brief  "SetImGuiContext happens when the plugin's ImGui is initialized."
 * https://wiki.bakkesplugins.com/imgui/custom_fonts/
 *
 * also:
 * "Don't call this yourself, BM will call this function with a pointer
 * to the current ImGui context" -- pluginsettingswindow.h
 * ...
 *
 * \param ctx AFAIK The pointer to the ImGui context
 */
void ColorBlindColorChanger::SetImGuiContext(uintptr_t ctx) {
      ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext *>(ctx));
}

/**
 * \brief Get the name of the plugin for the plugins tab in bakkesmod
 *
 *
 * \return The name of the plugin for the plugins tab in BakkesMod.
 */
std::string ColorBlindColorChanger::GetPluginName() {
      return "Color Blind Color Changer";
}

/*
 * for when you've inherited from BakkesMod::Plugin::PluginWindow.
 * this lets  you do "togglemenu (GetMenuName())" in BakkesMod's console...
 * ie
 * if the following GetMenuName() returns "xyz", then you can refer to your
 * plugin's window in game through "togglemenu xyz"
 */

/**
 * \brief do the following on togglemenu open
 */
// void ColorBlindColorChanger::OnOpen() {};

/**
 * \brief do the following on menu close
 */
// void ColorBlindColorChanger::OnClose() {};

/**
 * \brief (ImGui) Code called while rendering your menu window
 */
// void ColorBlindColorChanger::Render() {};

/**
 * \brief Returns the name of the menu to refer to it by
 *
 * \return The name used refered to by togglemenu
 */
// std::string ColorBlindColorChanger::GetMenuName() {
//       return "ColorBlindColorChanger";
// };

/**
 * \brief Returns a std::string to show as the title
 *
 * \return The title of the menu
 */
// std::string ColorBlindColorChanger::GetMenuTitle() {
//       return "ColorBlindColorChanger";
// };

/**
 * \brief Is it the active overlay(window)?
 *
 * \return True/False for being the active overlay
 */
// bool ColorBlindColorChanger::IsActiveOverlay() {
//       return true;
// };

/**
 * \brief Should this block input from the rest of the program?
 * (aka RocketLeague and BakkesMod windows)
 *
 * \return True/False for if bakkesmod should block input
 */
// bool ColorBlindColorChanger::ShouldBlockInput() {
//       return false;
// };

/**
 * \brief Do the following when your plugin is unloaded
 *
 *  destroy things here, don't throw
 *  don't rely on this to assuredly run when RL is closed
 */
void ColorBlindColorChanger::onUnload() {
}
