#include "win_byte_fix.h"
#include "AuthManager.h"
#include "SupabaseConfig.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <chrono>

#ifdef TARGET_OSX
#include <sys/stat.h>
#include <unistd.h>
#endif

// ─── Paths ──────────────────────────────────────────────────────────────────

static std::string getHomeDir() {
    const char* h = getenv("HOME");
#ifdef TARGET_WIN32
    if (!h) h = getenv("USERPROFILE");
#endif
    return h ? std::string(h) : std::string("/tmp");
}

std::string AuthManager::getVsDir() const {
    return getHomeDir() + "/.virtualstage";
}

std::string AuthManager::getSessionPath() const {
    return getVsDir() + "/session.json";
}

void AuthManager::ensureVsDir() const {
    std::string d = getVsDir();
#ifdef TARGET_OSX
    mkdir(d.c_str(), 0700);
#elif defined(TARGET_WIN32)
    system(("mkdir \"" + d + "\" 2>nul").c_str());
#endif
}

// ─── Session persistence ─────────────────────────────────────────────────────

void AuthManager::loadSession() {
    std::lock_guard<std::mutex> lock(sessionMutex);
    std::string path = getSessionPath();

    std::ifstream f(path);
    if (!f.is_open()) {
        authenticated = false;
        return;
    }

    ofJson j;
    try {
        j = ofJson::parse(f);
    } catch (...) {
        authenticated = false;
        return;
    }

    try {
        session.accessToken  = j.value("access_token",  std::string());
        session.refreshToken = j.value("refresh_token", std::string());
        session.email        = j.value("email",         std::string());
        session.userId       = j.value("user_id",       std::string());
        session.expiresAt    = j.value("expires_at",    (long long)0);
    } catch (...) {
        authenticated = false;
        return;
    }

    // Session file exists → offline mode is OK regardless of token expiry
    authenticated = session.valid();
}

void AuthManager::saveSession() {
    ensureVsDir();
    ofJson j;
    j["access_token"]  = session.accessToken;
    j["refresh_token"] = session.refreshToken;
    j["email"]         = session.email;
    j["user_id"]       = session.userId;
    j["expires_at"]    = session.expiresAt;

    std::ofstream f(getSessionPath());
    if (f.is_open()) {
        f << j.dump(4);
    }
}

void AuthManager::logout() {
    std::lock_guard<std::mutex> lock(sessionMutex);
    session = Session();
    authenticated = false;
    std::remove(getSessionPath().c_str());
}

// ─── HTTP helper ─────────────────────────────────────────────────────────────

bool AuthManager::httpPost(const std::string& url,
                            const std::string& extraHeaders,
                            const std::string& jsonBody,
                            ofJson& outResponse,
                            std::string& outError) {
    ensureVsDir();
    std::string bodyFile = getVsDir() + "/auth_body.json";
    std::string respFile = getVsDir() + "/auth_resp.json";

    // Write body to file (avoids shell-escaping issues with passwords)
    {
        std::ofstream bf(bodyFile);
        if (!bf.is_open()) {
            outError = "Could not write temp file";
            return false;
        }
        bf << jsonBody;
    }

#ifdef TARGET_OSX
    std::string cmd = "curl -s -X POST "
        "-H \"apikey: " + std::string(SUPABASE_ANON_KEY) + "\" "
        "-H \"Content-Type: application/json\" ";

    if (!extraHeaders.empty()) {
        cmd += extraHeaders + " ";
    }

    cmd += "-d @\"" + bodyFile + "\" "
           "\"" + url + "\" "
           "-o \"" + respFile + "\" 2>/dev/null";

#elif defined(TARGET_WIN32)
    std::string cmd = "powershell -Command \""
        "$h = @{'apikey'='" + std::string(SUPABASE_ANON_KEY) + "';"
        "'Content-Type'='application/json'};";
    if (!extraHeaders.empty()) {
        cmd += extraHeaders;
    }
    cmd += "Invoke-RestMethod -Method POST -Uri '" + url + "' "
           "-Headers $h "
           "-InFile '" + bodyFile + "' "
           "-OutFile '" + respFile + "'\"";
#endif

    system(cmd.c_str());
    std::remove(bodyFile.c_str());

    std::ifstream rf(respFile);
    if (!rf.is_open()) {
        outError = "Network error or no internet connection";
        return false;
    }
    try {
        outResponse = ofJson::parse(rf);
    } catch (...) {
        outError = "Invalid response from server";
        std::remove(respFile.c_str());
        return false;
    }
    rf.close();
    std::remove(respFile.c_str());
    return true;
}

// ─── Auth operations ─────────────────────────────────────────────────────────

bool AuthManager::login(const std::string& email, const std::string& password,
                         std::string& outError) {
    ofJson body, resp;
    body["email"]    = email;
    body["password"] = password;

    std::string url = std::string(SUPABASE_URL) + "/auth/v1/token?grant_type=password";

    if (!httpPost(url, "", body.dump(), resp, outError)) {
        return false;
    }

    // Check for error in response
    if (resp.contains("error") && !resp["error"].is_null()) {
        std::string code = resp.value("error", std::string());
        std::string desc = resp.value("error_description", std::string());
        outError = desc.empty() ? code : desc;
        return false;
    }
    if (resp.contains("msg") && !resp.contains("access_token")) {
        outError = resp.value("msg", "Login failed");
        return false;
    }

    std::lock_guard<std::mutex> lock(sessionMutex);
    session.accessToken  = resp.value("access_token",  std::string());
    session.refreshToken = resp.value("refresh_token", std::string());
    session.email        = email;

    if (resp.contains("user") && !resp["user"].is_null()) {
        session.userId = resp["user"].value("id", std::string());
    }

    long long expiresIn = resp.value("expires_in", (long long)3600);
    auto now = std::chrono::system_clock::now();
    session.expiresAt = std::chrono::duration_cast<std::chrono::seconds>(
                            now.time_since_epoch()).count() + expiresIn;

    if (!session.valid()) {
        outError = "Invalid response from server";
        return false;
    }

    saveSession();
    authenticated = true;
    return true;
}

bool AuthManager::signup(const std::string& email, const std::string& password,
                          std::string& outError, bool& outNeedsEmailConfirm) {
    outNeedsEmailConfirm = false;

    ofJson body, resp;
    body["email"]    = email;
    body["password"] = password;

    std::string url = std::string(SUPABASE_URL) + "/auth/v1/signup";

    if (!httpPost(url, "", body.dump(), resp, outError)) {
        return false;
    }

    // Check for errors
    if (resp.contains("error") && !resp["error"].is_null()) {
        outError = resp.value("error_description", resp.value("error", "Signup failed"));
        return false;
    }
    if (resp.contains("code") && resp.contains("msg") && !resp.contains("access_token")) {
        int code = resp.value("code", 0);
        if (code == 422) {
            outError = "Email already registered. Please sign in.";
        } else {
            outError = resp.value("msg", "Signup failed");
        }
        return false;
    }

    // If response has an access_token, user is logged in immediately
    std::string at = resp.value("access_token", std::string());
    if (!at.empty()) {
        std::lock_guard<std::mutex> lock(sessionMutex);
        session.accessToken  = at;
        session.refreshToken = resp.value("refresh_token", std::string());
        session.email        = email;
        if (resp.contains("user") && !resp["user"].is_null()) {
            session.userId = resp["user"].value("id", std::string());
        }
        long long expiresIn = resp.value("expires_in", (long long)3600);
        auto now = std::chrono::system_clock::now();
        session.expiresAt = std::chrono::duration_cast<std::chrono::seconds>(
                                now.time_since_epoch()).count() + expiresIn;
        saveSession();
        authenticated = true;
        return true;
    }

    // No access_token → email confirmation required
    outNeedsEmailConfirm = true;
    return true;
}

bool AuthManager::refreshToken(std::string& outError) {
    std::string rt;
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        rt = session.refreshToken;
    }
    if (rt.empty()) {
        outError = "No refresh token";
        return false;
    }

    ofJson body, resp;
    body["refresh_token"] = rt;

    std::string url = std::string(SUPABASE_URL) + "/auth/v1/token?grant_type=refresh_token";

    if (!httpPost(url, "", body.dump(), resp, outError)) {
        return false; // No internet — silent failure, offline mode continues
    }

    if (resp.contains("error") || (resp.contains("msg") && !resp.contains("access_token"))) {
        outError = resp.value("error_description",
                   resp.value("msg", "Token refresh failed"));
        return false;
    }

    std::string at = resp.value("access_token", std::string());
    if (at.empty()) {
        outError = "Invalid refresh response";
        return false;
    }

    std::lock_guard<std::mutex> lock(sessionMutex);
    session.accessToken  = at;
    session.refreshToken = resp.value("refresh_token", session.refreshToken);
    long long expiresIn  = resp.value("expires_in", (long long)3600);
    auto now = std::chrono::system_clock::now();
    session.expiresAt = std::chrono::duration_cast<std::chrono::seconds>(
                            now.time_since_epoch()).count() + expiresIn;
    saveSession();
    return true;
}
