#include "ColorBlindColorChanger.h"

#include "Windows.h"  // IWYU pragma: keep

#include "shellapi.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "bm_helper.h"
#include "CVarManager.h"
#include "HookedEvents.h"
#include "Logger.h"
#include "PersistentManagedCVarStorage.h"

namespace {
#ifdef _WIN32
// ERROR macro is defined in Windows header
// To avoid conflict between these macro and declaration of ERROR / DEBUG in SEVERITY enum
// We save macro and undef it
#pragma push_macro("ERROR")
#pragma push_macro("DEBUG")
#undef ERROR
#undef DEBUG
#endif

namespace log = LOGGER;
};  // namespace

BAKKESMOD_PLUGIN(ColorBlindColorChanger, "ColorBlindColorChanger", "1.0.0", /*UNUSED*/ NULL);

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

      init_cvars();
      init_hooked_events();

      cb_enabled = gameWrapper->GetbColorBlind();
      log::LOG(log::LOGLEVEL::INFO, "GETBCOLORBLIND/cb_enabled: {}", cb_enabled);
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
}

/**
 * \brief group together the initialization of hooked events
 */
void ColorBlindColorChanger::init_hooked_events() {
      HookedEvents::AddHookedEventWithCaller<ActorWrapper>(
            "Function TAGame.GFxData_Settings_TA.SetColorBlind",
            [this](ActorWrapper unused, void * params, std::string event_name) {
                  log::LOG(log::LOGLEVEL::INFO, "CALLING {}...", event_name);
                  struct parms {
                        unsigned char unused[0x8];  // HAS THINGS BUT I DONT WANT THEM
                        bool          b;
                  } * b      = reinterpret_cast<parms *>(params);
                  cb_enabled = b->b;
            });

      HookedEvents::AddHookedEvent("Function Engine.GameInfo.PreExit", [this](std::string event_name) {
            log::LOG(log::LOGLEVEL::INFO, "CALLING {}...", event_name);
            // ASSURED CLEANUP
            onUnload();
      });
}

void ColorBlindColorChanger::hook_colorblind_color_change_events() {
      // set team
      HookedEvents::AddHookedEvent(
            "Function TAGame.GFxHUD_TA.SetTeam",
            [this](auto... fargs) {
                  log::LOG(log::LOGLEVEL::INFO, "CALLING {} ...", fargs...);

                  gameWrapper->SetTimeout(
                        [this](GameWrapper * gw) {
                              ServerWrapper sw = gameWrapper->GetCurrentGameState();
                              if (!sw) {
                                    log::LOG(log::LOGLEVEL::DEBUG, "NO SERVER");
                                    return;
                              }

                              /************ This section is for "blue team" vs "orange team" *********/
                              ArrayWrapper<TeamWrapper> awtw = sw.GetTeams();
                              if (awtw.IsNull()) {
                                    log::LOG(log::LOGLEVEL::DEBUG, "NO ARRAYWRAPPER<TEAMWRAPPER>");
                                    return;
                              }

                              int blueteam   = 0;
                              int orangeteam = 1;

                              TeamWrapper bteam = sw.GetTeams().Get(blueteam);
                              if (bteam) {
                                    bteam.SetColorBlindFontColor(LinearColor(0.0f, 0.9f, 0.6f, 1.0f));
                                    bteam.UpdateColors();
                              } else {
                                    log::LOG(log::LOGLEVEL::DEBUG, "NO BLEU TEAM");
                              }

                              if (static_cast<PlaylistId>(sw.GetPlaylist().GetPlaylistId())
                                  == bm_helper::playlist_str_ids.at("Knockout")) {
                                    log::LOG(log::LOGLEVEL::DEBUG, "THERE IS NO ORANGE TEAM IN KNOCKOUT!");
                                    return;
                              }

                              TeamWrapper oteam = sw.GetTeams().Get(orangeteam);
                              if (oteam) {
                                    oteam.SetColorBlindFontColor(LinearColor(1.0f, 0.0f, 0.0f, 1.0f));
                                    oteam.UpdateColors();
                              } else {
                                    log::LOG(log::LOGLEVEL::DEBUG, "NO ORANGE TEAM");
                              }
                        },
                        0.25f);
                  /************End: This section is for "my team" vs "other team" *********/
                  /************ This section is for "my team" vs "other team" *********/
                  // it works!
                  // PlayerControllerWrapper pcw = gameWrapper->GetPlayerController();
                  // if (!pcw) {
                  //      LOG("NO PCW");
                  //      return;
                  //}

                  // PriWrapper priw = pcw.GetPRI();
                  // if (!priw) {
                  //       LOG("NO PRI");
                  //       return;
                  // }

                  // ArrayWrapper<TeamWrapper> awtw = sw.GetTeams();
                  // if (awtw.IsNull()) {
                  //       LOG("NO ARRAYWRAPPER<TEAMWRAPPER>");
                  //       return;
                  // }

                  // int myteam    = priw.GetTeamNum();
                  // int otherteam = 1 - myteam;

                  // TeamWrapper mteam = awtw.Get(myteam);
                  // if (!mteam) {
                  //       LOG("NO MY TEAM");
                  //       return;
                  // }
                  // TeamWrapper oteam = awtw.Get(otherteam);
                  // if (!oteam) {
                  //       LOG("NO OTHER TEAM");
                  //       return;
                  // }

                  // mteam.SetColorBlindFontColor(LinearColor(0.0f, 0.5f, 0.0f, 1.0f));
                  // mteam.UpdateColors();
                  // oteam.SetColorBlindFontColor(LinearColor(0.8f, 0.0f, 0.0f, 1.0f));
                  // oteam.UpdateColors();
                  /************End: This section is for "my team" vs "other team" *********/
            },
            true);
}

void ColorBlindColorChanger::unhook_colorblind_color_change_events() {
      HookedEvents::RemoveHook("");
}

void ColorBlindColorChanger::enable_plugin() {
      plugin_enabled = true;
      hook_colorblind_color_change_events();
}

void ColorBlindColorChanger::disable_plugin() {
      plugin_enabled = false;
      unhook_colorblind_color_change_events();
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
                  "This plugin does not work with the color blind setting"
                  " (found in Settings -> Interface -> Color blind mode) "
                  "disabled."};
            float dwidth = ImGui::CalcTextSize(disclaimer.c_str()).x;
            AlignForWidth(dwidth);
            ImGui::TextWrapped(disclaimer.c_str());
            return;
      }
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
//       return "$safeprojectname";
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

#ifdef _WIN32
// We restore the ERROR Windows macro
#pragma pop_macro("ERROR")
#pragma pop_macro("DEBUG")
#endif
