#include "win_byte_fix.h"
#include "Scene.h"
#include <algorithm>
#include <cctype>

// Case-insensitive substring check
static bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return false;
    std::string h = haystack, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

void Scene::setup() {
    light.setDirectional();
    light.setOrientation(glm::vec3(-45, -45, 0));
    light.setDiffuseColor(ofFloatColor(0.9f, 0.9f, 0.9f));
    light.setAmbientColor(ofFloatColor(0.4f, 0.4f, 0.4f));

#ifdef TARGET_OSX
    ofAddListener(directory.events.serverAnnounced, this, &Scene::onServerAnnounced);
    ofAddListener(directory.events.serverRetired, this, &Scene::onServerRetired);
    directory.setup();
#endif
}

void Scene::update() {
#ifdef TARGET_WIN32
    // Receive Spout textures each frame
    for (auto& screen : screens) {
        screen->updateSpout();
    }
    // Poll for new/removed senders periodically
    spoutPollTimer += ofGetLastFrameTime();
    if (spoutPollTimer >= 1.0f) {
        spoutPollTimer = 0;
        pollSpoutSenders();
    }
#endif
}

void Scene::draw(bool viewMode) {
    ofEnableLighting();
    light.enable();

    for (auto& screen : screens) {
        screen->draw(viewMode);
    }

    // Draw selection highlight for all selected screens
    for (int idx : selectedIndices) {
        if (idx >= 0 && idx < (int)screens.size()) {
            screens[idx]->drawSelected();
        }
    }

    // Draw stage elements
    for (int i = 0; i < (int)stageElements.size(); i++) {
        stageElements[i]->draw(i == selectedStageElement);
    }

    light.disable();
    ofDisableLighting();
}

void Scene::drawGrid(float size, float step) {
    ofPushStyle();
    float halfSize = size * 0.5f;

    // Grid lines
    ofSetColor(50);
    for (float i = -halfSize; i <= halfSize; i += step) {
        ofDrawLine(i, 0, -halfSize, i, 0, halfSize);
        ofDrawLine(-halfSize, 0, i, halfSize, 0, i);
    }

    // Axis lines
    ofSetLineWidth(2);
    ofSetColor(180, 50, 50); // X = red
    ofDrawLine(-halfSize, 0, 0, halfSize, 0, 0);
    ofSetColor(50, 180, 50); // Y = green
    ofDrawLine(0, 0, 0, 0, halfSize, 0);
    ofSetColor(50, 50, 180); // Z = blue
    ofDrawLine(0, 0, -halfSize, 0, 0, halfSize);
    ofSetLineWidth(1);

    ofPopStyle();
}

int Scene::addScreen(const std::string& name) {
    std::string screenName = name.empty()
        ? "Slice " + ofToString(nextScreenId)
        : name;
    nextScreenId++;

    auto screen = std::make_unique<ScreenObject>(screenName);
    // Offset each new screen so they don't overlap
    float offset = (screens.size()) * 350.0f;
    screen->setPosition(glm::vec3(offset, 150, 0));

    screens.push_back(std::move(screen));
    return (int)screens.size() - 1;
}

void Scene::removeScreen(int index) {
    if (index >= 0 && index < (int)screens.size()) {
        screens.erase(screens.begin() + index);

        // Rebuild selectedIndices: remove the deleted index, shift down indices above it
        std::set<int> newSelection;
        for (int idx : selectedIndices) {
            if (idx < index) {
                newSelection.insert(idx);
            } else if (idx > index) {
                newSelection.insert(idx - 1);
            }
            // idx == index is removed
        }
        selectedIndices = newSelection;

        // Update primarySelected
        if (primarySelected == index) {
            primarySelected = selectedIndices.empty() ? -1 : *selectedIndices.begin();
        } else if (primarySelected > index) {
            primarySelected--;
        }
    }
}

int Scene::getScreenCount() const {
    return (int)screens.size();
}

ScreenObject* Scene::getScreen(int index) {
    if (index >= 0 && index < (int)screens.size()) {
        return screens[index].get();
    }
    return nullptr;
}

std::vector<ServerInfo> Scene::getAvailableServers() const {
    std::vector<ServerInfo> servers;
#ifdef TARGET_OSX
    const auto& list = directory.getServerList();
    for (const auto& desc : list) {
        servers.push_back({desc.serverName, desc.appName});
    }
#elif defined(TARGET_WIN32)
    for (const auto& name : spoutSenders) {
        servers.push_back({name, ""});
    }
#endif
    return servers;
}

int Scene::getServerCount() const {
#ifdef TARGET_OSX
    return (int)directory.getServerList().size();
#elif defined(TARGET_WIN32)
    return (int)spoutSenders.size();
#else
    return 0;
#endif
}

void Scene::assignSourceToScreen(int screenIndex, int serverIndex) {
    ScreenObject* screen = getScreen(screenIndex);
    if (!screen) return;

#ifdef TARGET_OSX
    if (!directory.isValidIndex(serverIndex)) {
        screen->disconnectSource();
        return;
    }
    const auto& desc = directory.getDescription(serverIndex);
    screen->sourceIndex = serverIndex;
    screen->connectToSource(desc);
#elif defined(TARGET_WIN32)
    if (serverIndex < 0 || serverIndex >= (int)spoutSenders.size()) {
        screen->disconnectSource();
        return;
    }
    screen->sourceIndex = serverIndex;
    screen->connectToSource(spoutSenders[serverIndex]);
#endif
}

// --- Screen Group Management ---

int Scene::addGroup(const std::string& name) {
    ScreenGroup g;
    g.id = nextGroupId++;
    g.name = name.empty() ? ("Screen " + ofToString(g.id)) : name;
    groups.push_back(g);
    return g.id;
}

void Scene::removeGroup(int groupId) {
    // Remove all slices belonging to this group (reverse order for stable indices)
    for (int i = (int)screens.size() - 1; i >= 0; i--) {
        if (screens[i]->groupId == groupId) {
            removeScreen(i);
        }
    }
    groups.erase(std::remove_if(groups.begin(), groups.end(),
        [groupId](const ScreenGroup& g) { return g.id == groupId; }), groups.end());
}

ScreenGroup* Scene::getGroup(int groupId) {
    for (auto& g : groups) {
        if (g.id == groupId) return &g;
    }
    return nullptr;
}

int Scene::getGroupCount() const {
    return (int)groups.size();
}

std::vector<int> Scene::getSliceIndicesForGroup(int groupId) const {
    std::vector<int> result;
    for (int i = 0; i < (int)screens.size(); i++) {
        if (screens[i]->groupId == groupId) result.push_back(i);
    }
    return result;
}

int Scene::addSliceToGroup(int groupId, const std::string& name) {
    int idx = addScreen(name);
    screens[idx]->groupId = groupId;

    // Auto-connect to group source if one is assigned
    ScreenGroup* g = getGroup(groupId);
    if (g && g->sourceIndex >= 0) {
        assignSourceToScreen(idx, g->sourceIndex);
    }
    return idx;
}

void Scene::assignSourceToGroup(int groupId, int serverIndex) {
    ScreenGroup* g = getGroup(groupId);
    if (!g) return;

    g->sourceIndex = serverIndex;

    // Build display name
#ifdef TARGET_OSX
    if (directory.isValidIndex(serverIndex)) {
        const auto& desc = directory.getDescription(serverIndex);
        g->sourceName = desc.appName + " - " + desc.serverName;
    }
#elif defined(TARGET_WIN32)
    if (serverIndex >= 0 && serverIndex < (int)spoutSenders.size()) {
        g->sourceName = spoutSenders[serverIndex];
    }
#endif

    // Connect all child slices
    for (int i = 0; i < (int)screens.size(); i++) {
        if (screens[i]->groupId == groupId) {
            assignSourceToScreen(i, serverIndex);
        }
    }
}

void Scene::disconnectGroup(int groupId) {
    ScreenGroup* g = getGroup(groupId);
    if (!g) return;
    g->sourceIndex = -1;
    g->sourceName = "";
    for (auto& screen : screens) {
        if (screen->groupId == groupId) {
            screen->disconnectSource();
        }
    }
}

#ifdef TARGET_OSX
void Scene::onServerAnnounced(ofxSyphonServerDirectoryEventArgs& args) {
    for (const auto& s : args.servers) {
        ofLogNotice("Scene") << "Server announced: " << s.appName << " - " << s.serverName;
    }

    // Auto-link: match new servers to unconnected groups by name
    const auto& serverList = directory.getServerList();
    for (auto& g : groups) {
        if (g.sourceIndex >= 0) continue; // already connected
        for (int i = 0; i < (int)serverList.size(); i++) {
            std::string displayName = serverList[i].appName + " - " + serverList[i].serverName;
            if (containsIgnoreCase(displayName, g.name)) {
                assignSourceToGroup(g.id, i);
                ofLogNotice("Scene") << "Auto-linked '" << g.name << "' to '" << displayName << "'";
                break;
            }
        }
    }

    if (onServerListChanged) onServerListChanged();
}

void Scene::onServerRetired(ofxSyphonServerDirectoryEventArgs& args) {
    for (const auto& s : args.servers) {
        ofLogNotice("Scene") << "Server retired: " << s.appName << " - " << s.serverName;
    }
    // Check if any connected screens lost their server
    for (auto& screen : screens) {
        if (screen->hasSource() && screen->sourceIndex >= 0) {
            if (!directory.isValidIndex(screen->sourceIndex)) {
                screen->disconnectSource();
            }
        }
    }
    if (onServerListChanged) onServerListChanged();
}
#elif defined(TARGET_WIN32)
void Scene::pollSpoutSenders() {
    SpoutReceiver tempReceiver;
    int count = tempReceiver.GetSenderCount();
    std::vector<std::string> current;
    current.reserve(count);
    for (int i = 0; i < count; i++) {
        char name[256];
        if (tempReceiver.GetSender(i, name, 256)) {
            current.push_back(std::string(name));
        }
    }
    tempReceiver.ReleaseReceiver();

    if (current != spoutSenders) {
        spoutSenders = current;
        // Check if any connected screens lost their sender
        for (auto& screen : screens) {
            if (!screen->hasSource()) continue;
            bool found = false;
            for (auto& s : spoutSenders) {
                if (s == screen->sourceName) { found = true; break; }
            }
            if (!found) {
                screen->disconnectSource();
            }
        }

        // Auto-link: match new senders to unconnected groups by name
        for (auto& g : groups) {
            if (g.sourceIndex >= 0) continue; // already connected
            for (int i = 0; i < (int)spoutSenders.size(); i++) {
                if (containsIgnoreCase(spoutSenders[i], g.name)) {
                    assignSourceToGroup(g.id, i);
                    ofLogNotice("Scene") << "Auto-linked '" << g.name << "' to '" << spoutSenders[i] << "'";
                    break;
                }
            }
        }

        if (onServerListChanged) onServerListChanged();
    }
}
#endif

// --- Stage Element Management ---

int Scene::addStageElement(StageElementType type, const std::string& name) {
    auto elem = std::make_unique<StageElement>(type, name);
    stageElements.push_back(std::move(elem));
    return (int)stageElements.size() - 1;
}

void Scene::removeStageElement(int index) {
    if (index >= 0 && index < (int)stageElements.size()) {
        stageElements.erase(stageElements.begin() + index);
        if (selectedStageElement == index) selectedStageElement = -1;
        else if (selectedStageElement > index) selectedStageElement--;
    }
}

StageElement* Scene::getStageElement(int index) {
    if (index >= 0 && index < (int)stageElements.size()) {
        return stageElements[index].get();
    }
    return nullptr;
}

int Scene::getStageElementCount() const {
    return (int)stageElements.size();
}

// --- Project Save/Load ---

bool Scene::saveProject(const std::string& path, const ofJson& cameraJson) const {
    ofJson root;
    root["version"] = 2;

    if (!cameraJson.is_null()) {
        root["camera"] = cameraJson;
    }

    // Serialize groups with their child slices
    ofJson groupsArr = ofJson::array();
    for (const auto& g : groups) {
        ofJson gj;
        gj["id"] = g.id;
        gj["name"] = g.name;
        if (!g.sourceName.empty()) {
            gj["sourceName"] = g.sourceName;
        }

        ofJson slicesArr = ofJson::array();
        for (const auto& screen : screens) {
            if (screen->groupId == g.id) {
                slicesArr.push_back(screen->toJson());
            }
        }
        gj["slices"] = slicesArr;
        groupsArr.push_back(gj);
    }
    root["groups"] = groupsArr;

    // Serialize stage elements
    if (!stageElements.empty()) {
        ofJson elemArr = ofJson::array();
        for (const auto& e : stageElements) {
            elemArr.push_back(e->toJson());
        }
        root["stageElements"] = elemArr;
    }

    return ofSavePrettyJson(path, root);
}

bool Scene::loadProject(const std::string& path, ofJson* outCameraJson) {
    ofJson root = ofLoadJson(path);
    if (root.is_null()) {
        ofLogError("Scene") << "Failed to load project: " << path;
        return false;
    }

    // Clear existing state
    screens.clear();
    groups.clear();
    stageElements.clear();
    selectedStageElement = -1;
    clearSelection();
    nextScreenId = 1;
    nextGroupId = 1;

    // Load camera if present
    if (outCameraJson && root.contains("camera")) {
        *outCameraJson = root["camera"];
    }

    int version = root.value("version", 1);

    if (version >= 2 && root.contains("groups")) {
        // v2 format: groups with nested slices
        for (auto& gj : root["groups"]) {
            ScreenGroup g;
            g.id = gj.value("id", nextGroupId);
            g.name = gj.value("name", "Screen " + ofToString(g.id));
            g.sourceName = gj.value("sourceName", "");
            if (g.id >= nextGroupId) nextGroupId = g.id + 1;

            if (gj.contains("slices")) {
                for (auto& sj : gj["slices"]) {
                    auto screen = std::make_unique<ScreenObject>();
                    screen->fromJson(sj);
                    screen->groupId = g.id;
                    screens.push_back(std::move(screen));
                    nextScreenId++;
                }
            }
            groups.push_back(g);
        }
    } else if (root.contains("screens")) {
        // v1 legacy format: flat array — auto-wrap in default group
        int defaultGroupId = addGroup("Screen 1");
        for (auto& sj : root["screens"]) {
            auto screen = std::make_unique<ScreenObject>();
            screen->fromJson(sj);
            screen->groupId = defaultGroupId;
            screens.push_back(std::move(screen));
            nextScreenId++;
        }
    } else {
        ofLogError("Scene") << "No 'groups' or 'screens' found in: " << path;
        return false;
    }

    // Load stage elements if present
    if (root.contains("stageElements") && root["stageElements"].is_array()) {
        for (auto& ej : root["stageElements"]) {
            auto elem = std::make_unique<StageElement>();
            elem->fromJson(ej);
            stageElements.push_back(std::move(elem));
        }
    }

    reconnectSources();

    ofLogNotice("Scene") << "Loaded project: " << groups.size() << " screens, "
                         << screens.size() << " slices, "
                         << stageElements.size() << " stage elements from " << path;
    return true;
}

bool Scene::exportPreset(const std::string& path) const {
    ofJson root;
    root["type"] = "vstpreset";
    root["version"] = 1;

    ofJson groupsArr = ofJson::array();
    for (const auto& g : groups) {
        ofJson gj;
        gj["id"] = g.id;
        gj["name"] = g.name;
        // No source info in presets

        ofJson slicesArr = ofJson::array();
        for (const auto& screen : screens) {
            if (screen->groupId == g.id) {
                ofJson sj = screen->toJson();
                // Strip source info
                sj.erase("sourceName");
                sj.erase("sourceIndex");
                slicesArr.push_back(sj);
            }
        }
        gj["slices"] = slicesArr;
        groupsArr.push_back(gj);
    }
    root["groups"] = groupsArr;

    if (!stageElements.empty()) {
        ofJson elemArr = ofJson::array();
        for (const auto& e : stageElements) {
            elemArr.push_back(e->toJson());
        }
        root["stageElements"] = elemArr;
    }

    return ofSavePrettyJson(path, root);
}

bool Scene::importPreset(const std::string& path) {
    ofJson root = ofLoadJson(path);
    if (root.is_null() || root.value("type", "") != "vstpreset") {
        ofLogError("Scene") << "Not a valid preset file: " << path;
        return false;
    }

    // Clear existing state
    screens.clear();
    groups.clear();
    stageElements.clear();
    selectedStageElement = -1;
    clearSelection();
    nextScreenId = 1;
    nextGroupId = 1;

    if (root.contains("groups")) {
        for (auto& gj : root["groups"]) {
            ScreenGroup g;
            g.id = gj.value("id", nextGroupId);
            g.name = gj.value("name", "Screen " + ofToString(g.id));
            if (g.id >= nextGroupId) nextGroupId = g.id + 1;

            if (gj.contains("slices")) {
                for (auto& sj : gj["slices"]) {
                    auto screen = std::make_unique<ScreenObject>();
                    screen->fromJson(sj);
                    screen->groupId = g.id;
                    screen->disconnectSource();
                    screens.push_back(std::move(screen));
                    nextScreenId++;
                }
            }
            groups.push_back(g);
        }
    }

    if (root.contains("stageElements") && root["stageElements"].is_array()) {
        for (auto& ej : root["stageElements"]) {
            auto elem = std::make_unique<StageElement>();
            elem->fromJson(ej);
            stageElements.push_back(std::move(elem));
        }
    }

    ofLogNotice("Scene") << "Imported preset: " << groups.size() << " screens, "
                         << screens.size() << " slices from " << path;
    return true;
}

void Scene::reconnectSources() {
#ifdef TARGET_OSX
    const auto& serverList = directory.getServerList();
    for (auto& screen : screens) {
        if (screen->sourceName.empty()) continue;
        for (int i = 0; i < (int)serverList.size(); i++) {
            std::string displayName = serverList[i].appName + " - " + serverList[i].serverName;
            if (displayName == screen->sourceName) {
                screen->connectToSource(serverList[i]);
                screen->sourceIndex = i;
                ofLogNotice("Scene") << "Reconnected '" << screen->name << "' to: " << displayName;
                break;
            }
        }
    }
#elif defined(TARGET_WIN32)
    pollSpoutSenders(); // refresh sender list
    for (auto& screen : screens) {
        if (screen->sourceName.empty()) continue;
        for (int i = 0; i < (int)spoutSenders.size(); i++) {
            if (spoutSenders[i] == screen->sourceName) {
                screen->connectToSource(spoutSenders[i]);
                screen->sourceIndex = i;
                ofLogNotice("Scene") << "Reconnected '" << screen->name << "' to: " << spoutSenders[i];
                break;
            }
        }
    }
#endif

    // Sync group-level source info from reconnected child slices
    for (auto& g : groups) {
        if (!g.sourceName.empty() && g.sourceIndex < 0) {
            // Try to find the server index matching this group's saved sourceName
            auto servers = getAvailableServers();
            for (int i = 0; i < (int)servers.size(); i++) {
                if (servers[i].displayName() == g.sourceName) {
                    g.sourceIndex = i;
                    break;
                }
            }
        }
    }
}

int Scene::pick(const ofCamera& cam, const glm::vec2& screenPos) {
    // Build ray from camera through screen point
    glm::vec3 nearPoint = cam.screenToWorld(glm::vec3(screenPos.x, screenPos.y, 0.0f));
    glm::vec3 farPoint = cam.screenToWorld(glm::vec3(screenPos.x, screenPos.y, 1.0f));
    glm::vec3 rayDir = glm::normalize(farPoint - nearPoint);
    glm::vec3 rayOrigin = nearPoint;

    int closestIndex = -1;
    float closestT = std::numeric_limits<float>::max();

    for (int i = 0; i < (int)screens.size(); i++) {
        float t;
        if (rayIntersectsScreen(rayOrigin, rayDir, *screens[i], t)) {
            if (t < closestT) {
                closestT = t;
                closestIndex = i;
            }
        }
    }

    return closestIndex;
}

bool Scene::rayIntersectsScreen(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                const ScreenObject& screen, float& t) {
    glm::vec3 normal = screen.getWorldNormal();
    glm::vec3 center = screen.getWorldCenter();

    float denom = glm::dot(normal, rayDir);
    if (std::abs(denom) < 1e-6f) return false;

    t = glm::dot(center - rayOrigin, normal) / denom;
    if (t < 0) return false;

    glm::vec3 hitPoint = rayOrigin + rayDir * t;

    // Transform to local space
    glm::mat4 invTransform = glm::inverse(screen.plane.getGlobalTransformMatrix());
    glm::vec3 localHit = glm::vec3(invTransform * glm::vec4(hitPoint, 1.0f));

    return (std::abs(localHit.x) <= screen.getPlaneWidth() * 0.5f &&
            std::abs(localHit.y) <= screen.getPlaneHeight() * 0.5f);
}

// --- Multi-selection helpers ---

void Scene::selectOnly(int index) {
    selectedIndices.clear();
    if (index >= 0 && index < (int)screens.size()) {
        selectedIndices.insert(index);
        primarySelected = index;
    } else {
        primarySelected = -1;
    }
}

void Scene::toggleSelected(int index) {
    if (index < 0 || index >= (int)screens.size()) return;
    if (selectedIndices.count(index)) {
        selectedIndices.erase(index);
        if (primarySelected == index) {
            primarySelected = selectedIndices.empty() ? -1 : *selectedIndices.begin();
        }
    } else {
        selectedIndices.insert(index);
        primarySelected = index;
    }
}

void Scene::clearSelection() {
    selectedIndices.clear();
    primarySelected = -1;
}

void Scene::selectRange(int from, int to) {
    int lo = std::min(from, to);
    int hi = std::max(from, to);
    selectedIndices.clear();
    for (int i = lo; i <= hi && i < (int)screens.size(); i++) {
        if (i >= 0) selectedIndices.insert(i);
    }
    primarySelected = to;
}

void Scene::selectInRect(const ofCamera& cam, const ofRectangle& screenRect) {
    clearSelection();
    for (int i = 0; i < (int)screens.size(); i++) {
        glm::vec3 sp = cam.worldToScreen(screens[i]->getPosition());
        if (screenRect.inside(sp.x, sp.y)) {
            selectedIndices.insert(i);
            if (primarySelected < 0) primarySelected = i;
        }
    }
}

bool Scene::isSelected(int index) const {
    return selectedIndices.count(index) > 0;
}

int Scene::getPrimarySelected() const {
    return primarySelected;
}

int Scene::getSelectionCount() const {
    return (int)selectedIndices.size();
}

std::vector<int> Scene::getSelectedIndicesSorted() const {
    return std::vector<int>(selectedIndices.begin(), selectedIndices.end());
}
