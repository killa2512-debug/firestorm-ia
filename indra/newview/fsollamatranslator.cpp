#include "llviewerprecompiledheaders.h"
#include "fsollamatranslator.h"

#include <curl/curl.h>
#include <thread>

#include "llcallbacklist.h"
#include "llui.h"

// Static members
std::map<LLUUID, bool>             FSollamaTranslator::sSessionEnabled;
std::map<LLUUID, std::string>      FSollamaTranslator::sSessionPartnerLanguage;
std::map<LLUUID, std::string>      FSollamaTranslator::sSessionTargetLanguage;
bool                               FSollamaTranslator::sNearbyChatEnabled = false;
std::string                        FSollamaTranslator::sNearbyTargetLanguage = "";
bool                               FSollamaTranslator::sIdleRegistered    = false;
std::mutex                         FSollamaTranslator::sQueueMutex;
std::mutex                         FSollamaTranslator::sLanguageMutex;
std::mutex                         FSollamaTranslator::sNearbyLangMutex;
std::map<std::string, int>         FSollamaTranslator::sNearbyLanguageCount;
std::vector<std::function<void()>> FSollamaTranslator::sMainThreadQueue;

// ---------------------------------------------------------------------------
// Public API — state
// ---------------------------------------------------------------------------

bool FSollamaTranslator::isEnabled(const LLUUID& session_id)
{
    auto it = sSessionEnabled.find(session_id);
    return it != sSessionEnabled.end() && it->second;
}

void FSollamaTranslator::setEnabled(const LLUUID& session_id, bool enabled)
{
    sSessionEnabled[session_id] = enabled;
}

bool FSollamaTranslator::isNearbyChatEnabled()
{
    return sNearbyChatEnabled;
}

void FSollamaTranslator::setNearbyChatEnabled(bool enabled)
{
    sNearbyChatEnabled = enabled;
}

std::string FSollamaTranslator::getNearbyChatTargetLanguage()
{
    return sNearbyTargetLanguage;
}

void FSollamaTranslator::setNearbyChatTargetLanguage(const std::string& lang)
{
    sNearbyTargetLanguage = lang;
}

void FSollamaTranslator::recordNearbyLanguage(const std::string& lang)
{
    if (lang.empty()) return;
    std::lock_guard<std::mutex> lock(sNearbyLangMutex);
    sNearbyLanguageCount[lang]++;
    // Rolling window: when total exceeds 30, decay all counts by 1 and remove zeros.
    int total = 0;
    for (auto& p : sNearbyLanguageCount) total += p.second;
    if (total > 30)
    {
        for (auto& p : sNearbyLanguageCount) p.second--;
        for (auto it = sNearbyLanguageCount.begin(); it != sNearbyLanguageCount.end(); )
            it = (it->second <= 0) ? sNearbyLanguageCount.erase(it) : ++it;
    }
}

std::string FSollamaTranslator::getMostUsedNearbyLanguage()
{
    std::lock_guard<std::mutex> lock(sNearbyLangMutex);
    std::string best = "en";
    int bestCount = 0;
    for (auto& p : sNearbyLanguageCount)
    {
        if (p.second > bestCount)
        {
            bestCount = p.second;
            best = p.first;
        }
    }
    return best;
}

std::string FSollamaTranslator::getTargetLanguage(const LLUUID& session_id)
{
    std::lock_guard<std::mutex> lock(sLanguageMutex);
    auto it = sSessionTargetLanguage.find(session_id);
    return (it != sSessionTargetLanguage.end()) ? it->second : "";
}

void FSollamaTranslator::setTargetLanguage(const LLUUID& session_id, const std::string& lang)
{
    std::lock_guard<std::mutex> lock(sLanguageMutex);
    sSessionTargetLanguage[session_id] = lang;
}

// ---------------------------------------------------------------------------
// Viewer language
// ---------------------------------------------------------------------------

std::string FSollamaTranslator::viewerLanguage()
{
    // LLUI::getLanguage() returns ISO 639-1 codes like "es", "en", "pt", "fr"
    // which match LibreTranslate language codes directly.
    std::string lang = LLUI::getLanguage();
    if (lang.empty() || lang == "default") return "es";
    // Take only the first two chars (e.g. "en-US" → "en")
    return lang.substr(0, 2);
}

// ---------------------------------------------------------------------------
// Blocking translate — call only from worker threads
// ---------------------------------------------------------------------------

std::string FSollamaTranslator::translateIncoming(const std::string& text, const LLUUID& session_id)
{
    if (text.empty()) return text;

    std::string my_lang = viewerLanguage();

    // Check if user manually set the partner's language.
    std::string partner_lang;
    {
        std::lock_guard<std::mutex> lock(sLanguageMutex);
        auto it = sSessionTargetLanguage.find(session_id);
        if (it != sSessionTargetLanguage.end() && !it->second.empty())
            partner_lang = it->second;
    }

    std::string source = partner_lang.empty() ? "auto" : partner_lang;
    std::string detected;
    std::string translated = callLibreTranslate(text, source, my_lang, &detected);

    // Save detected partner language for outgoing use.
    if (!detected.empty() && detected != my_lang)
    {
        if (!session_id.isNull())
        {
            std::lock_guard<std::mutex> lock(sLanguageMutex);
            sSessionPartnerLanguage[session_id] = detected;
        }
        else
        {
            // Nearby chat: record for auto-detect outgoing language.
            recordNearbyLanguage(detected);
        }
    }

    // If already in the user's language, return original unchanged.
    if (translated == text || translated.empty())
        return text;

    return translated;
}

std::string FSollamaTranslator::translateOutgoing(const std::string& text, const LLUUID& session_id)
{
    if (text.empty()) return text;

    std::string my_lang = viewerLanguage();

    // Determine target: explicit override → detected from incoming → English fallback.
    std::string target;
    {
        std::lock_guard<std::mutex> lock(sLanguageMutex);
        auto it = sSessionTargetLanguage.find(session_id);
        if (it != sSessionTargetLanguage.end() && !it->second.empty())
            target = it->second;
        else
        {
            auto it2 = sSessionPartnerLanguage.find(session_id);
            if (it2 != sSessionPartnerLanguage.end() && !it2->second.empty())
                target = it2->second;
        }
    }
    if (target.empty() || target == my_lang)
        target = "en";

    std::string translated = callLibreTranslate(text, my_lang, target);
    return (translated.empty() || translated == text) ? text : translated;
}

std::string FSollamaTranslator::translateOutgoingNearby(const std::string& text)
{
    if (text.empty()) return text;
    std::string my_lang = viewerLanguage();
    std::string target = sNearbyTargetLanguage.empty() ? "en" : sNearbyTargetLanguage;
    if (my_lang == target) return text;
    std::string translated = callLibreTranslate(text, my_lang, target);
    return (translated.empty()) ? text : translated;
}

// ---------------------------------------------------------------------------
// Async versions
// ---------------------------------------------------------------------------

void FSollamaTranslator::translateIncomingAsync(const std::string& text, const LLUUID& session_id,
                                                std::function<void(std::string)> callback)
{
    registerIdleCallback();
    std::thread([text, session_id, callback]() {
        std::string result = translateIncoming(text, session_id);
        scheduleMainThread([result, callback]() { callback(result); });
    }).detach();
}

void FSollamaTranslator::translateOutgoingAsync(const std::string& text, const LLUUID& session_id,
                                                std::function<void(std::string)> callback)
{
    registerIdleCallback();
    std::thread([text, session_id, callback]() {
        std::string result = translateOutgoing(text, session_id);
        scheduleMainThread([result, callback]() { callback(result); });
    }).detach();
}

void FSollamaTranslator::translateOutgoingNearbyAsync(const std::string& text,
                                                      std::function<void(std::string)> callback)
{
    registerIdleCallback();
    std::thread([text, callback]() {
        std::string result = translateOutgoingNearby(text);
        scheduleMainThread([result, callback]() { callback(result); });
    }).detach();
}

// ---------------------------------------------------------------------------
// Main-thread callback queue
// ---------------------------------------------------------------------------

void FSollamaTranslator::scheduleMainThread(std::function<void()> fn)
{
    std::lock_guard<std::mutex> lock(sQueueMutex);
    sMainThreadQueue.push_back(std::move(fn));
}

void FSollamaTranslator::drainCallbacks(void*)
{
    std::vector<std::function<void()>> pending;
    {
        std::lock_guard<std::mutex> lock(sQueueMutex);
        pending.swap(sMainThreadQueue);
    }
    for (auto& fn : pending)
        fn();
}

void FSollamaTranslator::registerIdleCallback()
{
    if (sIdleRegistered) return;
    sIdleRegistered = true;
    gIdleCallbacks.addFunction(drainCallbacks, nullptr);
}

// ---------------------------------------------------------------------------
// LibreTranslate HTTP
// ---------------------------------------------------------------------------

static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* output)
{
    output->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string FSollamaTranslator::callLibreTranslate(const std::string& text,
                                                    const std::string& source,
                                                    const std::string& target,
                                                    std::string* detected_lang)
{
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string body =
        "{\"q\":\"" + jsonEscape(text) + "\","
        "\"source\":\"" + source + "\","
        "\"target\":\"" + target + "\"}";

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:5000/translate");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "";

    // Parse "translatedText":"..."
    std::string translated;
    const std::string key = "\"translatedText\":\"";
    auto pos = response.find(key);
    if (pos != std::string::npos)
    {
        pos += key.size();
        for (size_t i = pos; i < response.size(); ++i)
        {
            char c = response[i];
            if (c == '\\' && i + 1 < response.size())
            {
                char next = response[i + 1];
                if      (next == '"')  { translated += '"';  ++i; }
                else if (next == 'n')  { translated += '\n'; ++i; }
                else if (next == 't')  { translated += '\t'; ++i; }
                else if (next == '\\') { translated += '\\'; ++i; }
                else                   { translated += c; }
            }
            else if (c == '"') { break; }
            else               { translated += c; }
        }
    }

    // Parse detected language: "detectedLanguage":{"confidence":...,"language":"pt"}
    if (detected_lang)
    {
        const std::string dkey = "\"language\":\"";
        auto dpos = response.find(dkey);
        if (dpos != std::string::npos)
        {
            dpos += dkey.size();
            auto dend = response.find('"', dpos);
            if (dend != std::string::npos)
                *detected_lang = response.substr(dpos, dend - dpos);
        }
    }

    LL_WARNS("FSTranslate") << "LibreTranslate " << source << "→" << target
        << " | in: " << text.substr(0, 40)
        << " | out: " << translated.substr(0, 40)
        << (detected_lang && !detected_lang->empty() ? " | detected: " + *detected_lang : "")
        << LL_ENDL;

    return translated;
}

// ---------------------------------------------------------------------------
// JSON helper
// ---------------------------------------------------------------------------

std::string FSollamaTranslator::jsonEscape(const std::string& text)
{
    std::string out;
    out.reserve(text.size() + 16);
    for (char c : text)
    {
        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}
