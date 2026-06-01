#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "lluuid.h"

// Async, non-blocking translator using LibreTranslate (local HTTP server).
// Incoming messages are translated to the viewer's UI language.
// Outgoing messages are translated to the partner's detected language.
class FSollamaTranslator
{
public:
    // Blocking — only call from worker threads.
    static std::string translateIncoming(const std::string& text, const LLUUID& session_id);
    static std::string translateOutgoing(const std::string& text, const LLUUID& session_id);
    static std::string translateOutgoingNearby(const std::string& text);

    // Non-blocking: run on worker thread, call callback on main thread.
    static void translateIncomingAsync(const std::string& text, const LLUUID& session_id,
                                       std::function<void(std::string)> callback);
    static void translateOutgoingAsync(const std::string& text, const LLUUID& session_id,
                                       std::function<void(std::string)> callback);
    static void translateOutgoingNearbyAsync(const std::string& text,
                                             std::function<void(std::string)> callback);

    static bool isEnabled(const LLUUID& session_id);
    static void setEnabled(const LLUUID& session_id, bool enabled);

    static bool isNearbyChatEnabled();
    static void setNearbyChatEnabled(bool enabled);

    static std::string getNearbyChatTargetLanguage();
    static void        setNearbyChatTargetLanguage(const std::string& lang);

    // Auto-detect: tracks the most used language in nearby chat incoming messages.
    static void        recordNearbyLanguage(const std::string& lang);
    static std::string getMostUsedNearbyLanguage();

    // Explicit partner language override per session ("" = auto-detect from incoming).
    static std::string getTargetLanguage(const LLUUID& session_id);
    static void        setTargetLanguage(const LLUUID& session_id, const std::string& lang);

    // Called by gIdleCallbacks each frame to dispatch finished translations to main thread.
    static void drainCallbacks(void*);

private:
    // Returns translated text and sets detected_lang (ISO 639-1 code).
    static std::string callLibreTranslate(const std::string& text,
                                          const std::string& source,
                                          const std::string& target,
                                          std::string* detected_lang = nullptr);
    static std::string jsonEscape(const std::string& text);
    static void        scheduleMainThread(std::function<void()> fn);
    static void        registerIdleCallback();
    static std::string viewerLanguage(); // returns LLUI::getLanguage() ISO code

    static std::map<LLUUID, bool>        sSessionEnabled;
    static std::map<LLUUID, std::string> sSessionPartnerLanguage; // detected partner language
    static std::map<LLUUID, std::string> sSessionTargetLanguage;  // explicit override ("" = auto)
    static bool                          sNearbyChatEnabled;
    static std::string                   sNearbyTargetLanguage;
    static bool                          sIdleRegistered;
    static std::mutex                    sQueueMutex;
    static std::mutex                    sLanguageMutex;
    static std::mutex                    sNearbyLangMutex;
    static std::map<std::string, int>    sNearbyLanguageCount;
    static std::vector<std::function<void()>> sMainThreadQueue;
};
