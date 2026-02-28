#pragma once
#include "ofMain.h"
#include <string>
#include <mutex>

// Handles Supabase email/password auth and local session persistence.
// Session is stored in ~/.virtualstage/session.json so the app works
// offline after the first login.
class AuthManager {
public:
    struct Session {
        std::string accessToken;
        std::string refreshToken;
        std::string email;
        std::string userId;
        long long   expiresAt = 0; // Unix timestamp

        bool valid() const { return !accessToken.empty() && !userId.empty(); }
    };

    // Called once at app startup — reads ~/.virtualstage/session.json
    void loadSession();

    // True if a session file exists (offline mode: does NOT require internet)
    bool isAuthenticated() const { return authenticated; }

    // Blocking network calls — run from a background thread
    bool login(const std::string& email, const std::string& password, std::string& outError);
    bool signup(const std::string& email, const std::string& password, std::string& outError,
                bool& outNeedsEmailConfirm);
    bool refreshToken(std::string& outError);

    // Clears session file and resets state
    void logout();

    const Session& getSession() const { return session; }

private:
    Session session;
    bool    authenticated = false;
    std::mutex sessionMutex;

    std::string getVsDir()      const; // ~/.virtualstage/
    std::string getSessionPath()const; // ~/.virtualstage/session.json
    void        ensureVsDir()   const; // mkdir -p ~/.virtualstage

    void saveSession();

    // Writes jsonBody to a temp file, runs curl POST, reads response JSON.
    bool httpPost(const std::string& url,
                  const std::string& extraHeaders, // newline-separated -H strings
                  const std::string& jsonBody,
                  ofJson& outResponse,
                  std::string& outError);
};
