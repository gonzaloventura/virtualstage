#include "win_byte_fix.h"
#include "CloudStorage.h"
#include "SupabaseConfig.h"
#include <fstream>
#include <cstdlib>

#ifdef TARGET_OSX
#include <sys/stat.h>
#endif

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::string getHomeDir() {
    const char* h = getenv("HOME");
#ifdef TARGET_WIN32
    if (!h) h = getenv("USERPROFILE");
#endif
    return h ? std::string(h) : std::string("/tmp");
}

std::string CloudStorage::getTmpDir() const {
    return getHomeDir() + "/.virtualstage";
}

void CloudStorage::ensureTmpDir() const {
    std::string d = getTmpDir();
#ifdef TARGET_OSX
    mkdir(d.c_str(), 0700);
#elif defined(TARGET_WIN32)
    system(("mkdir \"" + d + "\" 2>nul").c_str());
#endif
}

// ─── REST request ─────────────────────────────────────────────────────────────

bool CloudStorage::restRequest(const std::string& method,
                                const std::string& endpoint,
                                const AuthManager::Session& session,
                                const std::string& extraHeaders,
                                const std::string& jsonBody,
                                ofJson& outResponse,
                                std::string& outError) {
    ensureTmpDir();
    std::string dir      = getTmpDir();
    std::string bodyFile = dir + "/cloud_body.json";
    std::string respFile = dir + "/cloud_resp.json";

    std::string url = std::string(SUPABASE_URL) + endpoint;

    bool hasBody = !jsonBody.empty();
    if (hasBody) {
        std::ofstream bf(bodyFile);
        if (!bf.is_open()) { outError = "Could not write temp file"; return false; }
        bf << jsonBody;
    }

#ifdef TARGET_OSX
    std::string cmd = "curl -s -X " + method + " "
        "-H \"apikey: " + std::string(SUPABASE_ANON_KEY) + "\" "
        "-H \"Authorization: Bearer " + session.accessToken + "\" ";

    if (hasBody) {
        cmd += "-H \"Content-Type: application/json\" ";
    }
    if (!extraHeaders.empty()) {
        cmd += extraHeaders + " ";
    }
    if (hasBody) {
        cmd += "-d @\"" + bodyFile + "\" ";
    }
    cmd += "\"" + url + "\" -o \"" + respFile + "\" 2>/dev/null";

#elif defined(TARGET_WIN32)
    std::string cmd = "powershell -Command \""
        "$h = @{"
        "'apikey'='" + std::string(SUPABASE_ANON_KEY) + "';"
        "'Authorization'='Bearer " + session.accessToken + "'";
    if (hasBody) cmd += ";"
        "'Content-Type'='application/json'";
    if (!extraHeaders.empty()) cmd += ";" + extraHeaders;
    cmd += "};";
    if (hasBody) {
        cmd += "Invoke-RestMethod -Method " + method +
               " -Uri '" + url + "' -Headers $h -InFile '" + bodyFile + "'"
               " -OutFile '" + respFile + "'\"";
    } else {
        cmd += "Invoke-RestMethod -Method " + method +
               " -Uri '" + url + "' -Headers $h"
               " -OutFile '" + respFile + "'\"";
    }
#endif

    system(cmd.c_str());

    if (hasBody) std::remove(bodyFile.c_str());

    std::ifstream rf(respFile);
    if (!rf.is_open()) {
        // Empty response is OK for DELETE (204 No Content)
        outResponse = ofJson::array();
        return true;
    }
    try {
        outResponse = ofJson::parse(rf);
    } catch (...) {
        outResponse = ofJson::array();
        rf.close();
        std::remove(respFile.c_str());
        return true; // empty / non-JSON response treated as success
    }
    rf.close();
    std::remove(respFile.c_str());

    // Check for PostgREST error object
    if (outResponse.is_object() && outResponse.contains("message")) {
        outError = outResponse.value("message", "Request failed");
        return false;
    }
    if (outResponse.is_object() && outResponse.contains("error")) {
        outError = outResponse.value("error", "Request failed");
        return false;
    }
    if (outResponse.is_object() && outResponse.contains("msg")) {
        outError = outResponse.value("msg", "Request failed");
        return false;
    }

    return true;
}

// ─── CRUD operations ──────────────────────────────────────────────────────────

bool CloudStorage::listProjects(const AuthManager::Session& session,
                                 std::vector<CloudProject>& outProjects,
                                 std::string& outError) {
    ofJson resp;
    std::string endpoint = "/rest/v1/projects?select=id,name,updated_at"
                           "&order=updated_at.desc";

    if (!restRequest("GET", endpoint, session, "", "", resp, outError)) {
        return false;
    }

    outProjects.clear();
    if (!resp.is_array()) return true;

    for (auto& item : resp) {
        CloudProject p;
        p.id        = item.value("id",         std::string());
        p.name      = item.value("name",        std::string());
        p.updatedAt = item.value("updated_at",  std::string());
        if (!p.id.empty()) outProjects.push_back(p);
    }
    return true;
}

bool CloudStorage::saveProject(const AuthManager::Session& session,
                                const ofJson& projectData,
                                const std::string& projectName,
                                std::string& outError) {
    ofJson body;
    body["name"] = projectName;
    body["data"] = projectData;

    // Upsert on (user_id, name) unique constraint
    std::string endpoint = "/rest/v1/projects?on_conflict=user_id,name";
    std::string extraH   = "-H \"Prefer: resolution=merge-duplicates\"";

    ofJson resp;
    return restRequest("POST", endpoint, session, extraH, body.dump(), resp, outError);
}

bool CloudStorage::loadProject(const AuthManager::Session& session,
                                const std::string& projectId,
                                ofJson& outData,
                                std::string& outError) {
    std::string endpoint = "/rest/v1/projects?id=eq." + projectId + "&select=data";

    ofJson resp;
    if (!restRequest("GET", endpoint, session, "", "", resp, outError)) {
        return false;
    }

    if (!resp.is_array() || resp.empty()) {
        outError = "Project not found";
        return false;
    }

    outData = resp[0].value("data", ofJson());
    return true;
}

bool CloudStorage::deleteProject(const AuthManager::Session& session,
                                  const std::string& projectId,
                                  std::string& outError) {
    std::string endpoint = "/rest/v1/projects?id=eq." + projectId;
    ofJson resp;
    return restRequest("DELETE", endpoint, session, "", "", resp, outError);
}

// ─── User preferences ───────────────────────────────────────────────────────

bool CloudStorage::loadPreferences(const AuthManager::Session& session,
                                    std::string& outData,
                                    std::string& outError) {
    std::string endpoint = "/rest/v1/user_preferences?select=data";
    ofJson resp;
    if (!restRequest("GET", endpoint, session, "", "", resp, outError)) {
        return false;
    }

    outData.clear();
    if (!resp.is_array() || resp.empty()) return true; // no prefs yet

    // Extract the JSONB data field as a string
    if (resp[0].contains("data") && resp[0]["data"].is_object()) {
        outData = resp[0]["data"].dump();
    }
    return true;
}

bool CloudStorage::savePreferences(const AuthManager::Session& session,
                                    const std::string& jsonData,
                                    std::string& outError) {
    ofJson body;
    body["data"] = ofJson::parse(jsonData);

    std::string endpoint = "/rest/v1/user_preferences?on_conflict=user_id";
    std::string extraH   = "-H \"Prefer: resolution=merge-duplicates\"";

    ofJson resp;
    return restRequest("POST", endpoint, session, extraH, body.dump(), resp, outError);
}
