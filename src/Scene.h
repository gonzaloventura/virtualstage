#pragma once
#include "ofMain.h"
#include "ScreenObject.h"
#include <vector>
#include <set>
#include <memory>
#include <functional>

#ifdef TARGET_OSX
#include "ofxSyphon.h"
#elif defined(TARGET_WIN32)
#include "ofxSpout.h"
#include "SpoutReceiver.h"
#endif

struct ServerInfo {
    std::string serverName;
    std::string appName;

    std::string displayName() const {
        if (serverName.empty() && appName.empty()) return "(unknown)";
        if (serverName.empty()) return appName;
        if (appName.empty()) return serverName;
        return appName + " - " + serverName;
    }
};

class Scene {
public:
    void setup();
    void draw(bool viewMode = false);
    void drawGrid(float size = 1000.0f, float step = 50.0f);

    // Object management
    int addScreen(const std::string& name = "");
    void removeScreen(int index);
    int getScreenCount() const;
    ScreenObject* getScreen(int index);

    // Picking: returns index of hit object or -1
    int pick(const ofCamera& cam, const glm::vec2& screenPos);

    // Server directory
    std::vector<ServerInfo> getAvailableServers() const;
    int getServerCount() const;

#ifdef TARGET_OSX
    ofxSyphonServerDirectory& getDirectory() { return directory; }
#endif

    // Assign source to a screen by server index
    void assignSourceToScreen(int screenIndex, int serverIndex);

    // Update (for Spout receiver polling)
    void update();

    // Project save/load
    bool saveProject(const std::string& path, const ofJson& cameraJson = ofJson()) const;
    bool loadProject(const std::string& path, ofJson* outCameraJson = nullptr);

    // Reconnect all screens to their sources by name (used after undo/redo/load)
    void reconnectSources();

    // Multi-selection
    std::set<int> selectedIndices;
    int primarySelected = -1;

    void selectOnly(int index);
    void toggleSelected(int index);
    void clearSelection();
    void selectRange(int from, int to);
    void selectInRect(const ofCamera& cam, const ofRectangle& screenRect);
    bool isSelected(int index) const;
    int getPrimarySelected() const;
    int getSelectionCount() const;
    std::vector<int> getSelectedIndicesSorted() const;

    // Callback when server list changes
    std::function<void()> onServerListChanged;

    std::vector<std::unique_ptr<ScreenObject>> screens;

private:
    ofLight light;
    int nextScreenId = 1;

#ifdef TARGET_OSX
    ofxSyphonServerDirectory directory;
    void onServerAnnounced(ofxSyphonServerDirectoryEventArgs& args);
    void onServerRetired(ofxSyphonServerDirectoryEventArgs& args);
#elif defined(TARGET_WIN32)
    std::vector<std::string> spoutSenders; // cached sender list
    float spoutPollTimer = 0;
    void pollSpoutSenders();
#endif

    bool rayIntersectsScreen(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                             const ScreenObject& screen, float& t);
};
