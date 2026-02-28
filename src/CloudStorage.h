#pragma once
#include "ofMain.h"
#include "AuthManager.h"
#include <string>
#include <vector>

// Manages project storage in Supabase Postgres via the REST API (PostgREST).
//
// Required SQL (run once in Supabase SQL editor):
// ─────────────────────────────────────────────────────────────────────────────
// CREATE TABLE projects (
//   id         uuid PRIMARY KEY DEFAULT gen_random_uuid(),
//   user_id    uuid REFERENCES auth.users NOT NULL DEFAULT auth.uid(),
//   name       text NOT NULL,
//   data       jsonb NOT NULL,
//   created_at timestamptz DEFAULT now(),
//   updated_at timestamptz DEFAULT now()
// );
// -- Unique constraint for upsert
// ALTER TABLE projects ADD CONSTRAINT projects_user_name_key UNIQUE (user_id, name);
// ALTER TABLE projects ENABLE ROW LEVEL SECURITY;
// CREATE POLICY "Users own rows" ON projects FOR ALL USING (user_id = auth.uid());
// ─────────────────────────────────────────────────────────────────────────────

class CloudStorage {
public:
    struct CloudProject {
        std::string id;
        std::string name;
        std::string updatedAt;
    };

    // List all projects for the logged-in user (blocking network call)
    bool listProjects(const AuthManager::Session& session,
                      std::vector<CloudProject>& outProjects,
                      std::string& outError);

    // Upsert project JSON by name (blocking network call)
    bool saveProject(const AuthManager::Session& session,
                     const ofJson& projectData,
                     const std::string& projectName,
                     std::string& outError);

    // Load a project's data JSON by id (blocking network call)
    bool loadProject(const AuthManager::Session& session,
                     const std::string& projectId,
                     ofJson& outData,
                     std::string& outError);

    // Delete a project by id (blocking network call)
    bool deleteProject(const AuthManager::Session& session,
                       const std::string& projectId,
                       std::string& outError);

    // ── User preferences ────────────────────────────────────────────────────
    // Load all preferences for the logged-in user (blocking)
    bool loadPreferences(const AuthManager::Session& session,
                         std::string& outData,
                         std::string& outError);

    // Upsert preferences JSON (blocking)
    bool savePreferences(const AuthManager::Session& session,
                         const std::string& jsonData,
                         std::string& outError);

private:
    // Generic curl wrapper for Supabase REST (PostgREST)
    // method: GET | POST | PATCH | DELETE
    bool restRequest(const std::string& method,
                     const std::string& endpoint,   // e.g. "/rest/v1/projects?..."
                     const AuthManager::Session& session,
                     const std::string& extraHeaders,
                     const std::string& jsonBody,   // empty for GET/DELETE
                     ofJson& outResponse,
                     std::string& outError);

    std::string getTmpDir() const;
    void ensureTmpDir() const;
};
