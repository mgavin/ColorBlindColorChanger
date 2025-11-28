/*
 * TODO:
 *     Make it so you can add multiple functions to the hook, if it's initialized
 *       - have some sort of list of functions
 *       - return a pointer as an identifier, so specific functions can be removed from hooks
 *      Append vs Add?
 *
 */

#ifndef _HOOKEDEVENTS_HPP_
#define _HOOKEDEVENTS_HPP_

#include <regex>
#include <string>
#include <unordered_set>

#include "bakkesmod/wrappers/gamewrapper.h"

#include "Logger.h"

namespace {
namespace log = LOGGER;
}

class HookedEvents {
private:
      struct HookedEvent {
            std::string eventName;
            bool        isPost;

            HookedEvent(std::string eN, bool iP) : eventName(std::move(eN)), isPost(iP) {}
            ~HookedEvent() { isPost ? gameWrapper->UnhookEventPost(eventName) : gameWrapper->UnhookEvent(eventName); }

            static void UnhookEvent(std::string eventName, bool isPost) {
                  if (isPost) {
                        gameWrapper->UnhookEventPost(eventName);
                  } else {
                        gameWrapper->UnhookEvent(eventName);
                  }
            }
      };

      struct hook_cmp {
            bool operator()(const std::shared_ptr<HookedEvent> & rhe, const std::shared_ptr<HookedEvent> & lhe) const {
                  return ((lhe->eventName == rhe->eventName) && (lhe->isPost == rhe->isPost));
            }
      };

      struct HookedEventHash {
            std::size_t operator()(const std::shared_ptr<HookedEvent> & hooked_event) const {
                  return std::hash<std::string> {}(hooked_event->eventName + std::to_string(hooked_event->isPost));
            }
      };

      static inline std::unordered_set<std::shared_ptr<HookedEvent>, HookedEventHash, hook_cmp> hooked_events;

public:
      HookedEvents()                = delete;
      HookedEvents(HookedEvents &)  = delete;
      HookedEvents(HookedEvents &&) = delete;

      static inline std::shared_ptr<GameWrapper> gameWrapper;

      template <typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type * = nullptr>
      static void AddHookedEventWithCaller(
            const std::string &                                                 eventName,
            std::function<void(T caller, void * params, std::string eventName)> func,
            bool                                                                isPost = false,
            bool                                                                report = true);
      static void AddHookedEvent(
            const std::string &                        eventName,
            std::function<void(std::string eventName)> func,
            bool                                       isPost = false,
            bool                                       report = true);

      static void RemoveHook(std::string, bool isPost = false);
      static void RemoveHook(std::regex);
      static void RemoveAllHooks();
};

template <typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type *>
void HookedEvents::AddHookedEventWithCaller(
      const std::string &                                                 eventName,
      std::function<void(T caller, void * params, std::string eventName)> func,
      bool                                                                isPost,
      bool                                                                report) {
      if (gameWrapper == nullptr) {
            throw std::exception {
                  "gameWrapper not set for hooking functions (put "
                  "HookedEvents::gameWrapper = gameWrapper in "
                  "your OnLoad() plugin member function)"};
      }
      if (std::find_if(
                begin(hooked_events),
                end(hooked_events),
                [eventName, isPost](const std::shared_ptr<HookedEvent> & he) {
                      return ((he->eventName == eventName) && (he->isPost == isPost));
                })
          != end(hooked_events)) {
            log::log_debug(
                  "Hooked event (with caller) \"{}{}\" already exists. Cannot overwrite.",
                  eventName,
                  isPost ? "(POST)" : "");

            return;
      }

      auto f = [isPost, report, func](T caller, void * params, std::string eventName) {
            if (report) { log::log_debug("CALLING {}{}", eventName, isPost ? "... POST" : ""); }

            func(caller, params, eventName);
      };

      // just because it might already exist before we manage it.
      HookedEvent::UnhookEvent(eventName, isPost);
      if (isPost) {
            gameWrapper->HookEventWithCallerPost<T>(eventName, f);
      } else {
            gameWrapper->HookEventWithCaller<T>(eventName, f);
      }

      hooked_events.insert(
            std::shared_ptr<HookedEvent> {
                  new HookedEvent {eventName, isPost}
      });
}

inline void HookedEvents::AddHookedEvent(
      const std::string &                        eventName,
      std::function<void(std::string eventName)> func,
      bool                                       isPost,
      bool                                       report) {
      if (gameWrapper == nullptr) {
            throw std::exception {
                  "gameWrapper not set for hooking functions (put "
                  "HookedEvents::gameWrapper = gameWrapper in "
                  "your OnLoad() plugin member function)"};
      }
      if (std::find_if(
                begin(hooked_events),
                end(hooked_events),
                [eventName, isPost](const std::shared_ptr<HookedEvent> & he) {
                      return ((he->eventName == eventName) && (he->isPost == isPost));
                })
          != end(hooked_events)) {
            log::log_debug(
                  "Hooked event \"{}{}\" already exists. Cannot overwrite.",
                  eventName,
                  isPost ? "(POST)" : "");
            return;
      }

      auto f = [isPost, report, func](std::string eventName) {
            if (report) { log::log_debug("CALLING {}{}", eventName, isPost ? "... POST" : ""); }
            func(eventName);
      };

      // just because it might exist before we manage it.
      HookedEvent::UnhookEvent(eventName, isPost);
      void (GameWrapper::*hook)(std::string eventName, std::function<void(std::string eventName)> callback)
            = isPost ? &GameWrapper::HookEventPost : &GameWrapper::HookEvent;
      (gameWrapper.get()->*hook)(eventName, f);

      hooked_events.insert(std::make_shared<HookedEvent>(eventName, isPost));
}

inline void HookedEvents::RemoveHook(std::string key, bool isPost) {
      auto it = std::find_if(begin(hooked_events), end(hooked_events), [key, isPost](const auto & he) {
            return (he->eventName == key && he->isPost == isPost);
      });
      if (it != end(hooked_events)) { hooked_events.erase(it); }
}

/**
 *  \note I wanted to make this be able to search keys in the HookedEvents set with a regex string.
 *        But I forgot that paltry amount of c++ regex I was doing for another thing... which reminds me
 *        of forgetting chrono stuf... kind of ...
 *
 */
inline void HookedEvents::RemoveHook(std::regex key) {
}

/**
 * @brief You shouldn't necessarily need to call this yourself, as all hooks unhook when the underlying structure gets
 * destroyed at the end of the program, but if you want to remove them all before then explicitly, then call this.
 */
inline void HookedEvents::RemoveAllHooks() {
      hooked_events.clear();
}

#endif
