#include "win_byte_fix.h"
#include "Scene.h"

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
        ? "Screen " + ofToString(nextScreenId)
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

#ifdef TARGET_OSX
void Scene::onServerAnnounced(ofxSyphonServerDirectoryEventArgs& args) {
    for (const auto& s : args.servers) {
        ofLogNotice("Scene") << "Server announced: " << s.appName << " - " << s.serverName;
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
        if (onServerListChanged) onServerListChanged();
    }
}
#endif

// --- Project Save/Load ---

bool Scene::saveProject(const std::string& path, const ofJson& cameraJson) const {
    ofJson root;
    root["version"] = 1;

    if (!cameraJson.is_null()) {
        root["camera"] = cameraJson;
    }

    ofJson screensArr = ofJson::array();
    for (auto& screen : screens) {
        screensArr.push_back(screen->toJson());
    }
    root["screens"] = screensArr;

    return ofSavePrettyJson(path, root);
}

bool Scene::loadProject(const std::string& path, ofJson* outCameraJson) {
    ofJson root = ofLoadJson(path);
    if (root.is_null() || !root.contains("screens")) {
        ofLogError("Scene") << "Failed to load project or missing 'screens': " << path;
        return false;
    }

    // Clear existing screens
    screens.clear();
    clearSelection();
    nextScreenId = 1;

    // Load camera if present
    if (outCameraJson && root.contains("camera")) {
        *outCameraJson = root["camera"];
    }

    // Load screens
    for (auto& sj : root["screens"]) {
        auto screen = std::make_unique<ScreenObject>();
        screen->fromJson(sj);
        screens.push_back(std::move(screen));
        nextScreenId++;
    }

    reconnectSources();

    ofLogNotice("Scene") << "Loaded project: " << screens.size() << " screens from " << path;
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
