#include "win_byte_fix.h"
#include "ofApp.h"
#include <GLFW/glfw3.h>
#include "ofXml.h"

void ofApp::setup() {
    ofSetFrameRate(60);
    ofSetVerticalSync(true);
    ofBackground(30);

    // Camera
    cam.setDistance(800);
    cam.setTarget(glm::vec3(0, 100, 0));
    cam.setNearClip(1.0f);
    cam.setFarClip(10000.0f);

    // Scene (sets up shared server directory)
    scene.setup();
    scene.addScreen("Screen 1");
    scene.onServerListChanged = [this]() { refreshServerList(); };

    // Properties panel (right side)
    propertiesPanel.setup(ofGetWidth() - 240, 10);
}

void ofApp::update() {
    scene.update();
    // Refresh server list periodically
    servers = scene.getAvailableServers();
}

void ofApp::draw() {
    // --- Mapping mode: full-screen 2D editor ---
    if (mappingMode) {
        drawMappingMode();
        return;
    }

    // --- 3D Scene ---
    ofEnableDepthTest();
    cam.begin();

    if (appMode == AppMode::Designer) {
        scene.drawGrid();
    }

    scene.draw(appMode == AppMode::View);

    // Gizmo for selected object (Designer mode only)
    if (appMode == AppMode::Designer && scene.selectedIndex >= 0 && scene.getScreen(scene.selectedIndex)) {
        ofDisableDepthTest();
        gizmo.draw(*scene.getScreen(scene.selectedIndex), cam);
        ofEnableDepthTest();
    }

    cam.end();

    // --- 2D Overlay ---
    ofDisableDepthTest();

    if (appMode == AppMode::Designer && showUI) {
        drawServerList();
        propertiesPanel.draw();
        drawToolbar();
    }

    drawStatusBar();
}

void ofApp::drawServerList() {
    float panelX = 0;
    float panelY = 10;
    float panelH = ofGetHeight() - panelY - statusBarHeight;

    ofSetColor(20, 20, 20, 200);
    ofDrawRectangle(panelX, panelY, serverListWidth, panelH);

    // Screens section first
    ofSetColor(200);
    ofDrawBitmapString("SCREENS [A]dd [Del]ete", panelX + 10, panelY + 20);
    float objY = panelY + 38;

    for (int i = 0; i < scene.getScreenCount(); i++) {
        auto* screen = scene.getScreen(i);
        if (!screen) continue;

        bool selected = (scene.selectedIndex == i);
        ofSetColor(selected ? ofColor(0, 200, 255) : ofColor(150));

        std::string label = screen->name;
        if (screen->hasSource()) {
            label += " [" + screen->sourceName + "]";
        }
        if (label.length() > 30) label = label.substr(0, 27) + "...";
        ofDrawBitmapString(label, panelX + 10, objY + i * 18);
    }

    // Servers section
    float srvY = objY + std::max(scene.getScreenCount(), 1) * 18 + 20;
    ofSetColor(200);
    ofDrawBitmapString("SERVERS (1-9 assign to sel.)", panelX + 10, srvY);
    srvY += 18;

    for (size_t i = 0; i < servers.size(); i++) {
        if (srvY > ofGetHeight() - statusBarHeight) break;

        // Highlight if any selected screen uses this server
        bool assigned = false;
        if (scene.selectedIndex >= 0) {
            auto* sel = scene.getScreen(scene.selectedIndex);
            if (sel && sel->sourceIndex == (int)i) assigned = true;
        }

        ofSetColor(assigned ? ofColor(0, 200, 100) : ofColor(180));

        std::string label = ofToString(i + 1) + ". " + servers[i].displayName();
        if (label.length() > 28) label = label.substr(0, 25) + "...";
        ofDrawBitmapString(label, panelX + 10, srvY + i * 22);
    }

    if (servers.empty()) {
        ofSetColor(100);
        ofDrawBitmapString("Waiting for servers...", panelX + 10, srvY);
    }

    ofSetColor(255);
}

void ofApp::drawStatusBar() {
    float barY = ofGetHeight() - statusBarHeight;

    ofSetColor(15, 15, 15);
    ofDrawRectangle(0, barY, ofGetWidth(), statusBarHeight);

    // Mode indicator
    if (appMode == AppMode::Designer) {
        ofSetColor(0, 200, 255);
        ofDrawBitmapString("DESIGNER", 10, barY + 20);
    } else {
        ofSetColor(100, 200, 100);
        ofDrawBitmapString("VIEW", 10, barY + 20);
    }

    // Project name
    float nextX = 100;
    if (!currentProjectPath.empty()) {
        ofSetColor(200, 200, 100);
        std::string projName = ofFilePath::getFileName(currentProjectPath);
        ofDrawBitmapString(projName, nextX, barY + 20);
        nextX += projName.length() * 8 + 15;
    }

    // FPS
    ofSetColor(150);
    std::string fpsStr = "FPS: " + ofToString((int)ofGetFrameRate());
    ofDrawBitmapString(fpsStr, nextX, barY + 20);

    // Server count
    nextX += fpsStr.length() * 8 + 15;
    ofSetColor(150);
    std::string srvStr = "Servers: " + ofToString(scene.getServerCount());
    ofDrawBitmapString(srvStr, nextX, barY + 20);

    // Hints
    ofSetColor(100);
    std::string hint;
    if (linkState == LinkState::Confirm) {
        ofSetColor(255, 200, 0);
        hint = "Re-link? L:Yes  Esc:Cancel";
    } else if (linkState == LinkState::ChooseRect) {
        ofSetColor(255, 200, 0);
        hint = "Use rects from Resolume:  I:Input  O:Output  Esc:Cancel";
    } else if (appMode == AppMode::Designer) {
        hint = gizmo.getModeString() +
#ifdef TARGET_OSX
            "  |  A:Add  Del:Remove  L:Link  M:Map  H:UI  Tab:View  F:Full  Cmd+S/O:Save/Open";
#else
            "  |  A:Add  Del:Remove  L:Link  M:Map  H:UI  Tab:View  F:Full  Ctrl+S/O:Save/Open";
#endif
    } else {
        hint = "1:Front  2:Top  3:3/4  0:Level  Tab:Designer  F:Full";
    }
    ofDrawBitmapString(hint, ofGetWidth() - hint.length() * 8 - 10, barY + 20);

    ofSetColor(255);
}

void ofApp::drawToolbar() {
    float y = ofGetHeight() - statusBarHeight - 35;
    float cx = ofGetWidth() / 2.0f;

    ofSetColor(30, 30, 30, 220);
    ofDrawRectangle(cx - 120, y, 240, 25);

    auto drawBtn = [&](const std::string& label, Gizmo::Mode m, float x) {
        ofSetColor(gizmo.mode == m ? ofColor(0, 200, 255) : ofColor(120));
        ofDrawBitmapString(label, x, y + 17);
    };

    drawBtn("[W] Move", Gizmo::Mode::Translate, cx - 110);
    drawBtn("[E] Rotate", Gizmo::Mode::Rotate, cx - 35);
    drawBtn("[R] Scale", Gizmo::Mode::Scale, cx + 50);
    ofSetColor(255);
}

void ofApp::refreshServerList() {
    servers = scene.getAvailableServers();
}

// --- Project Save/Load ---

void ofApp::saveProject(bool saveAs) {
    std::string path = currentProjectPath;

    if (saveAs || path.empty()) {
        auto result = ofSystemSaveDialog("project.json", "Save VirtualStage Project");
        if (!result.bSuccess) return;
        path = result.filePath;
        // Ensure .json extension
        if (path.size() < 5 || path.substr(path.size() - 5) != ".json") {
            path += ".json";
        }
    }

    // Build camera state
    ofJson camJson;
    auto camPos = cam.getPosition();
    auto camTarget = cam.getTarget().getPosition();
    camJson["position"] = {camPos.x, camPos.y, camPos.z};
    camJson["target"] = {camTarget.x, camTarget.y, camTarget.z};
    camJson["distance"] = cam.getDistance();

    if (scene.saveProject(path, camJson)) {
        currentProjectPath = path;
        ofLogNotice("ofApp") << "Project saved: " << path;
    } else {
        ofLogError("ofApp") << "Failed to save project: " << path;
    }
}

void ofApp::openProject() {
    auto result = ofSystemLoadDialog("Open VirtualStage Project", false, "");
    if (!result.bSuccess) return;

    ofJson camJson;
    if (scene.loadProject(result.filePath, &camJson)) {
        currentProjectPath = result.filePath;

        // Restore camera
        if (camJson.contains("position") && camJson["position"].is_array() && camJson["position"].size() >= 3) {
            cam.setPosition(camJson["position"][0], camJson["position"][1], camJson["position"][2]);
        }
        if (camJson.contains("target") && camJson["target"].is_array() && camJson["target"].size() >= 3) {
            glm::vec3 target(camJson["target"][0], camJson["target"][1], camJson["target"][2]);
            cam.setTarget(target);
        }
        if (camJson.contains("distance")) {
            cam.setDistance(camJson["distance"].get<float>());
        }

        // Reset UI state
        scene.selectedIndex = -1;
        propertiesPanel.setTarget(nullptr);
        refreshServerList();

        ofLogNotice("ofApp") << "Project loaded: " << result.filePath;
    } else {
        ofLogError("ofApp") << "Failed to load project: " << result.filePath;
    }
}

void ofApp::keyPressed(int key) {
    // --- Mapping mode keys ---
    if (mappingMode) {
        if (key == 'm' || key == 'M' || key == OF_KEY_ESC) {
            mappingMode = false;
            cam.enableMouseInput();
        } else if (key == 's' || key == 'S') {
            mapSnapEnabled = !mapSnapEnabled;
        } else if (key == 'r' || key == 'R') {
            // Reset crop to full
            auto* screen = scene.getScreen(scene.selectedIndex);
            if (screen) {
                screen->setCropRect(ofRectangle(0, 0, 1, 1));
                propertiesPanel.syncFromTarget();
            }
        }
        return;
    }

    // Cmd+S / Cmd+Shift+S / Cmd+O (project save/load) — Ctrl on Windows
#ifdef TARGET_OSX
    if (ofGetKeyPressed(OF_KEY_SUPER)) {
        if (key == 's' || key == 'S') {
            saveProject(ofGetKeyPressed(OF_KEY_SHIFT));
            return;
        }
        if (key == 'o' || key == 'O') {
            openProject();
            return;
        }
    }
#else
    {
        // On Windows/GLFW, Ctrl+letter may arrive as a control character
        // (Ctrl+S=19, Ctrl+O=15) instead of 's'/'S'. Check both approaches.
        auto* win = dynamic_cast<ofAppGLFWWindow*>(ofGetWindowPtr());
        GLFWwindow* gw = win ? win->getGLFWWindow() : nullptr;
        bool ctrlHeld = ofGetKeyPressed(OF_KEY_CONTROL) ||
            (gw && (glfwGetKey(gw, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                    glfwGetKey(gw, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS));

        if ((ctrlHeld && (key == 's' || key == 'S')) || key == 19) {
            saveProject(ofGetKeyPressed(OF_KEY_SHIFT));
            return;
        }
        if ((ctrlHeld && (key == 'o' || key == 'O')) || key == 15) {
            openProject();
            return;
        }
    }
#endif

    // Tab toggles mode
    if (key == OF_KEY_TAB) {
        appMode = (appMode == AppMode::Designer) ? AppMode::View : AppMode::Designer;
        if (appMode == AppMode::View) {
            // Deactivate gizmo interaction
            gizmo.endDrag();
            gizmoInteracting = false;
            cam.enableMouseInput();
        }
        // Always-on-top in View mode
        auto* win = dynamic_cast<ofAppGLFWWindow*>(ofGetWindowPtr());
        if (win) {
            glfwSetWindowAttrib(win->getGLFWWindow(), GLFW_FLOATING,
                                appMode == AppMode::View ? GLFW_TRUE : GLFW_FALSE);
        }
        return;
    }

    // Camera presets (both modes)
    switch (key) {
        case OF_KEY_F1:
            cam.setPosition(0, 150, 800);
            cam.setTarget(glm::vec3(0, 150, 0));
            return;
        case OF_KEY_F2:
            cam.setPosition(0, 800, 0.01f);
            cam.setTarget(glm::vec3(0, 0, 0));
            return;
        case OF_KEY_F3:
            cam.setPosition(500, 400, 500);
            cam.setTarget(glm::vec3(0, 100, 0));
            return;
        case 'f': case 'F':
            ofToggleFullscreen();
            return;
    }

    // --- View mode keys ---
    if (appMode == AppMode::View) {
        switch (key) {
            case '1': // Front view
                cam.setPosition(0, 150, 800);
                cam.setTarget(glm::vec3(0, 150, 0));
                return;
            case '2': // Top view
                cam.setPosition(0, 800, 0.01f);
                cam.setTarget(glm::vec3(0, 0, 0));
                return;
            case '3': // 3/4 perspective
                cam.setPosition(500, 400, 500);
                cam.setTarget(glm::vec3(0, 100, 0));
                return;
            case '0': { // Level camera (fix tilt, keep position)
                glm::vec3 pos = cam.getPosition();
                glm::vec3 target = cam.getTarget().getPosition();
                cam.lookAt(target, glm::vec3(0, 1, 0));
                cam.setPosition(pos);
                return;
            }
        }
        return;
    }

    // --- Link state machine ---
    if (linkState == LinkState::Confirm) {
        if (key == 'l' || key == 'L') {
            ofLogNotice("Link") << "Confirmed → choose rect";
            linkState = LinkState::ChooseRect;
        } else {
            ofLogNotice("Link") << "Cancelled";
            linkState = LinkState::None;
        }
        return;
    }
    if (linkState == LinkState::ChooseRect) {
        if (key == 'i' || key == 'I') {
            ofLogNotice("Link") << "Using InputRect";
            linkState = LinkState::None;
            loadResolumeXml(true);
        } else if (key == 'o' || key == 'O') {
            ofLogNotice("Link") << "Using OutputRect";
            linkState = LinkState::None;
            loadResolumeXml(false);
        } else {
            ofLogNotice("Link") << "Cancelled";
            linkState = LinkState::None;
        }
        return;
    }

    // --- Designer-only keys ---
    switch (key) {
        case 'a': case 'A': {
            int idx = scene.addScreen();
            scene.selectedIndex = idx;
            propertiesPanel.setTarget(scene.getScreen(idx));
            break;
        }
        case OF_KEY_DEL: case OF_KEY_BACKSPACE:
            if (scene.selectedIndex >= 0) {
                scene.removeScreen(scene.selectedIndex);
                scene.selectedIndex = -1;
                propertiesPanel.setTarget(nullptr);
            }
            break;

        case 'w': gizmo.mode = Gizmo::Mode::Translate; break;
        case 'e': gizmo.mode = Gizmo::Mode::Rotate; break;
        case 'r': gizmo.mode = Gizmo::Mode::Scale; break;

        case 'h': case 'H':
            showUI = !showUI;
            propertiesPanel.setVisible(showUI);
            break;

        case 'l': case 'L':
            if (scene.getScreenCount() > 0) {
                ofLogNotice("Link") << "Screens exist → confirm?";
                linkState = LinkState::Confirm;
            } else {
                ofLogNotice("Link") << "No screens → choose rect";
                linkState = LinkState::ChooseRect;
            }
            break;

        case 'm': case 'M':
            if (scene.selectedIndex >= 0 && scene.getScreen(scene.selectedIndex)) {
                mappingMode = true;
                cam.disableMouseInput();
            }
            break;

        case 'd': case 'D':
            // Disconnect source from selected screen
            if (scene.selectedIndex >= 0) {
                auto* screen = scene.getScreen(scene.selectedIndex);
                if (screen) {
                    screen->disconnectSource();
                    propertiesPanel.setTarget(screen);
                }
            }
            break;

        default:
            // 1-9: assign server to selected screen
            if (key >= '1' && key <= '9') {
                int serverIdx = key - '1';
                if (scene.selectedIndex >= 0 && serverIdx < (int)servers.size()) {
                    scene.assignSourceToScreen(scene.selectedIndex, serverIdx);
                    propertiesPanel.setTarget(scene.getScreen(scene.selectedIndex));
                }
            }
            break;
    }
}

// --- Resolume XML Import ---

void ofApp::loadResolumeXml(bool useInputRect) {
    // Auto-find: most recently modified .xml in Resolume Advanced Output presets
    std::string presetsDir = ofFilePath::getUserHomeDir() +
        "/Documents/Resolume Arena/Presets/Advanced Output";
    std::string xmlPath;

    ofDirectory dir(presetsDir);
    if (dir.exists()) {
        dir.allowExt("xml");
        dir.listDir();

        // Find newest file by comparing file_time directly (no clock conversion)
        std::filesystem::file_time_type newestTime;
        bool found = false;
        for (size_t i = 0; i < dir.size(); i++) {
            auto fpath = std::filesystem::path(dir.getPath(i));
            try {
                auto mod = std::filesystem::last_write_time(fpath);
                if (!found || mod > newestTime) {
                    newestTime = mod;
                    xmlPath = dir.getPath(i);
                    found = true;
                }
            } catch (...) {}
        }

        if (found) {
            ofLogNotice("ofApp") << "Auto-found: " << ofFilePath::getFileName(xmlPath);
        }
    }

    // Fallback to file dialog
    if (xmlPath.empty()) {
        ofLogWarning("ofApp") << "No Resolume presets found, opening file dialog";
        auto result = ofSystemLoadDialog("Load Resolume Advanced Output XML");
        if (!result.bSuccess) return;
        xmlPath = result.filePath;
    }

    ofLogNotice("ofApp") << "Loading: " << xmlPath;

    ofXml xml;
    if (!xml.load(xmlPath)) {
        ofLogError("ofApp") << "Failed to load XML";
        return;
    }

    // Navigate: XmlState > ScreenSetup > screens
    auto xmlState = xml.getChild("XmlState");
    if (xmlState.getName().empty()) {
        ofLogError("ofApp") << "No <XmlState> root element";
        return;
    }

    auto screenSetup = xmlState.getChild("ScreenSetup");
    if (screenSetup.getName().empty()) {
        ofLogError("ofApp") << "No <ScreenSetup> found";
        return;
    }

    // Get composition texture size
    float compW = 1920, compH = 1080;
    auto compSize = screenSetup.getChild("CurrentCompositionTextureSize");
    if (!compSize.getName().empty()) {
        auto wAttr = compSize.getAttribute("width");
        auto hAttr = compSize.getAttribute("height");
        if (wAttr.getName() != "") compW = wAttr.getFloatValue();
        if (hAttr.getName() != "") compH = hAttr.getFloatValue();
    }
    ofLogNotice("ofApp") << "Composition: " << compW << "x" << compH;

    auto screensNode = screenSetup.getChild("screens");
    if (screensNode.getName().empty()) {
        ofLogError("ofApp") << "No <screens> element";
        return;
    }

    // Collect slice data from all Screens (skip DmxScreen)
    struct SliceData {
        std::string name;
        float rx, ry, rw, rh;   // chosen rect in pixels (bounding box)
        std::vector<glm::vec2> contourPoints; // normalized 0-1 polygon contour (empty = rect)
    };
    std::vector<SliceData> parsed;

    // Helper: parse a <Slice> or <Polygon> layer node
    auto parseLayer = [&](const ofXml& layerNode, bool isPolygon) {
        SliceData sd;

        // Get name from Params
        sd.name = "";
        for (auto& paramsGroup : layerNode.getChildren("Params")) {
            for (auto& p : paramsGroup.getChildren()) {
                if (p.getAttribute("name").getValue() == "Name") {
                    sd.name = p.getAttribute("value").getValue();
                    break;
                }
            }
            if (!sd.name.empty()) break;
        }
        if (sd.name.empty() || sd.name == "Layer") {
            sd.name = (isPolygon ? "Polygon " : "Slice ") + ofToString(parsed.size() + 1);
        }

        // Get bounding rect (Input or Output)
        std::string rectTag = useInputRect ? "InputRect" : "OutputRect";
        auto rectNode = layerNode.getChild(rectTag);
        if (rectNode.getName().empty()) {
            ofLogWarning("ofApp") << "  " << sd.name << ": no <" << rectTag << ">";
            return;
        }

        float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
        int vCount = 0;
        for (auto& v : rectNode.getChildren("v")) {
            float fx = v.getAttribute("x").getFloatValue();
            float fy = v.getAttribute("y").getFloatValue();
            minX = std::min(minX, fx);
            minY = std::min(minY, fy);
            maxX = std::max(maxX, fx);
            maxY = std::max(maxY, fy);
            vCount++;
        }

        if (vCount < 2) {
            ofLogWarning("ofApp") << "  " << sd.name << ": not enough rect vertices";
            return;
        }

        sd.rx = minX; sd.ry = minY;
        sd.rw = maxX - minX; sd.rh = maxY - minY;

        if (sd.rw <= 0 || sd.rh <= 0) {
            ofLogWarning("ofApp") << "  " << sd.name << ": zero size";
            return;
        }

        // For Polygons: parse contour points
        if (isPolygon) {
            std::string contourTag = useInputRect ? "InputContour" : "OutputContour";
            auto contourNode = layerNode.getChild(contourTag);
            if (!contourNode.getName().empty()) {
                auto pointsNode = contourNode.getChild("points");
                if (!pointsNode.getName().empty()) {
                    for (auto& v : pointsNode.getChildren("v")) {
                        float fx = v.getAttribute("x").getFloatValue();
                        float fy = v.getAttribute("y").getFloatValue();
                        // Normalize to 0-1 relative to bounding box
                        float nx = (sd.rw > 0) ? (fx - sd.rx) / sd.rw : 0.5f;
                        float ny = (sd.rh > 0) ? (fy - sd.ry) / sd.rh : 0.5f;
                        sd.contourPoints.push_back(glm::vec2(nx, ny));
                    }
                }
            }
            if (sd.contourPoints.size() < 3) {
                ofLogWarning("ofApp") << "  " << sd.name << ": polygon has < 3 contour points, treating as rect";
                sd.contourPoints.clear();
            }
        }

        parsed.push_back(sd);
        ofLogNotice("ofApp") << "  + " << sd.name
            << (isPolygon ? " (polygon)" : "")
            << "  " << sd.rw << "x" << sd.rh
            << "  @ " << sd.rx << "," << sd.ry;
    };

    for (auto& screenNode : screensNode.getChildren()) {
        std::string tag = screenNode.getName();
        if (tag != "Screen") {
            ofLogNotice("ofApp") << "Skipping <" << tag << ">";
            continue;
        }

        // Get screen name for context
        std::string screenName = tag;
        auto screenParams = screenNode.getChild("Params");
        if (!screenParams.getName().empty()) {
            for (auto& p : screenParams.getChildren()) {
                if (p.getAttribute("name").getValue() == "Name") {
                    screenName = p.getAttribute("value").getValue();
                    break;
                }
            }
        }
        ofLogNotice("ofApp") << "Screen: " << screenName;

        auto layers = screenNode.getChild("layers");
        if (layers.getName().empty()) {
            ofLogWarning("ofApp") << "  No <layers> found";
            continue;
        }

        for (auto& layerNode : layers.getChildren()) {
            std::string layerTag = layerNode.getName();
            if (layerTag == "Slice") {
                parseLayer(layerNode, false);
            } else if (layerTag == "Polygon") {
                parseLayer(layerNode, true);
            }
        }
    }

    if (parsed.empty()) {
        ofLogWarning("ofApp") << "No slices found in XML!";
        return;
    }

    // Clear existing screens
    while (scene.getScreenCount() > 0) {
        scene.removeScreen(0);
    }
    scene.selectedIndex = -1;
    propertiesPanel.setTarget(nullptr);

    // Compute total bounding box for layout and crop
    float totalMinX = 1e9f, totalMinY = 1e9f, totalMaxX = -1e9f, totalMaxY = -1e9f;
    for (auto& sd : parsed) {
        totalMinX = std::min(totalMinX, sd.rx);
        totalMinY = std::min(totalMinY, sd.ry);
        totalMaxX = std::max(totalMaxX, sd.rx + sd.rw);
        totalMaxY = std::max(totalMaxY, sd.ry + sd.rh);
    }
    float totalW = totalMaxX - totalMinX;
    float totalH = totalMaxY - totalMinY;

    // Scale: fit largest dimension to ~600 3D units
    float maxDim = std::max(totalW, totalH);
    float scaleFactor = (maxDim > 0) ? 600.0f / maxDim : 1.0f;

    for (auto& sd : parsed) {
        float w3d = sd.rw * scaleFactor;
        float h3d = sd.rh * scaleFactor;

        // Position: remap to 3D space, flip Y, center around X=0
        float cx = (sd.rx + sd.rw * 0.5f - totalMinX) * scaleFactor;
        float cy = (totalMaxY - (sd.ry + sd.rh * 0.5f)) * scaleFactor;
        cx -= totalW * scaleFactor * 0.5f;

        int idx = scene.addScreen(sd.name);
        auto* screen = scene.getScreen(idx);
        if (screen) {
            screen->plane.set(w3d, h3d, 2, 2);
            screen->setPosition(glm::vec3(cx, cy, 0));

            // Crop: slice region relative to total bounding box
            float cropX = (sd.rx - totalMinX) / totalW;
            float cropY = (sd.ry - totalMinY) / totalH;
            float cropW = sd.rw / totalW;
            float cropH = sd.rh / totalH;
            screen->setCropRect(ofRectangle(cropX, cropY, cropW, cropH));

            // Apply polygon mask if available
            if (!sd.contourPoints.empty()) {
                screen->setMask(sd.contourPoints);
            }
        }
    }

    std::string presetName = ofFilePath::getBaseName(xmlPath);
    ofLogNotice("ofApp") << "OK: " << parsed.size() << " slices from \"" << presetName
        << "\" (" << (useInputRect ? "Input" : "Output") << "Rect)";
}

// --- Input Mapping 2D Editor ---

float ofApp::snapValue(float v) const {
    if (!mapSnapEnabled) return v;
    return std::round(v / mapSnapGrid) * mapSnapGrid;
}

ofRectangle ofApp::getMapPreviewArea() const {
    float margin = 50;
    float barH = statusBarHeight + 50;
    float pw = ofGetWidth() - margin * 2;
    float ph = ofGetHeight() - margin - barH;
    return ofRectangle(margin, margin, pw, ph);
}

void ofApp::drawMappingMode() {
    auto* screen = scene.getScreen(scene.selectedIndex);
    if (!screen) { mappingMode = false; return; }

    ofRectangle preview = getMapPreviewArea();
    const ofRectangle& crop = screen->getCropRect();

    // Background
    ofSetColor(15);
    ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

    // Draw the actual Syphon texture if available
    ofSetColor(30);
    ofDrawRectangle(preview);
    if (!screen->drawSourceTexture(preview)) {
        // No texture: draw checkerboard
        float gridStep = 40;
        ofSetColor(40);
        for (float gx = preview.x; gx < preview.x + preview.width; gx += gridStep) {
            for (float gy = preview.y; gy < preview.y + preview.height; gy += gridStep) {
                int ix = (int)((gx - preview.x) / gridStep);
                int iy = (int)((gy - preview.y) / gridStep);
                if ((ix + iy) % 2 == 0) {
                    ofDrawRectangle(gx, gy,
                        std::min(gridStep, preview.x + preview.width - gx),
                        std::min(gridStep, preview.y + preview.height - gy));
                }
            }
        }
    }

    // Snap grid lines
    ofSetColor(255, 255, 255, 25);
    for (float g = mapSnapGrid; g < 1.0f; g += mapSnapGrid) {
        float gx = preview.x + g * preview.width;
        float gy = preview.y + g * preview.height;
        ofDrawLine(gx, preview.y, gx, preview.y + preview.height);
        ofDrawLine(preview.x, gy, preview.x + preview.width, gy);
    }
    // Major lines at 25% 50% 75%
    ofSetColor(255, 255, 255, 50);
    for (float g : {0.25f, 0.5f, 0.75f}) {
        float gx = preview.x + g * preview.width;
        float gy = preview.y + g * preview.height;
        ofDrawLine(gx, preview.y, gx, preview.y + preview.height);
        ofDrawLine(preview.x, gy, preview.x + preview.width, gy);
    }

    // Crop rectangle in screen space
    float cx = preview.x + crop.x * preview.width;
    float cy = preview.y + crop.y * preview.height;
    float cw = crop.width * preview.width;
    float ch = crop.height * preview.height;

    // Dim area outside crop
    ofSetColor(0, 0, 0, 140);
    // Top
    ofDrawRectangle(preview.x, preview.y, preview.width, cy - preview.y);
    // Bottom
    ofDrawRectangle(preview.x, cy + ch, preview.width, preview.y + preview.height - cy - ch);
    // Left
    ofDrawRectangle(preview.x, cy, cx - preview.x, ch);
    // Right
    ofDrawRectangle(cx + cw, cy, preview.x + preview.width - cx - cw, ch);

    // Crop border
    ofSetColor(255, 200, 0);
    ofNoFill();
    ofSetLineWidth(2);
    ofDrawRectangle(cx, cy, cw, ch);
    ofFill();
    ofSetLineWidth(1);

    // Corner handles
    float hs = 5;
    ofSetColor(255, 200, 0);
    ofDrawRectangle(cx - hs, cy - hs, hs * 2, hs * 2);
    ofDrawRectangle(cx + cw - hs, cy - hs, hs * 2, hs * 2);
    ofDrawRectangle(cx - hs, cy + ch - hs, hs * 2, hs * 2);
    ofDrawRectangle(cx + cw - hs, cy + ch - hs, hs * 2, hs * 2);
    // Edge midpoint handles
    ofDrawRectangle(cx + cw / 2 - hs, cy - hs, hs * 2, hs * 2);
    ofDrawRectangle(cx + cw / 2 - hs, cy + ch - hs, hs * 2, hs * 2);
    ofDrawRectangle(cx - hs, cy + ch / 2 - hs, hs * 2, hs * 2);
    ofDrawRectangle(cx + cw - hs, cy + ch / 2 - hs, hs * 2, hs * 2);

    // Crosshair
    ofSetColor(255, 200, 0, 80);
    ofDrawLine(cx + cw / 2, cy, cx + cw / 2, cy + ch);
    ofDrawLine(cx, cy + ch / 2, cx + cw, cy + ch / 2);

    // Preview outline
    ofSetColor(80);
    ofNoFill();
    ofDrawRectangle(preview);
    ofFill();

    // Title
    ofSetColor(255);
    ofDrawBitmapString(screen->name + " - Input Mapping"
        + (screen->hasSource() ? ("  [" + screen->sourceName + "]") : "  [No source]"),
        preview.x, preview.y - 15);

    // Values
    ofSetColor(200);
    std::string info = "X:" + ofToString(crop.x, 3)
        + "  Y:" + ofToString(crop.y, 3)
        + "  W:" + ofToString(crop.width, 3)
        + "  H:" + ofToString(crop.height, 3);
    ofDrawBitmapString(info, preview.x, preview.y + preview.height + 20);

    // Help
    ofSetColor(100);
    std::string help = "Drag:Move  Corners/Edges:Resize  S:Snap("
        + std::string(mapSnapEnabled ? "ON" : "OFF")
        + ")  M/Esc:Close";
    ofDrawBitmapString(help, preview.x, preview.y + preview.height + 40);

    ofSetColor(255);
    drawStatusBar();
}

void ofApp::mousePressed(int x, int y, int button) {
    if (button != OF_MOUSE_BUTTON_LEFT) return;

    // --- Mapping mode mouse ---
    if (mappingMode) {
        auto* screen = scene.getScreen(scene.selectedIndex);
        if (!screen) return;

        ofRectangle preview = getMapPreviewArea();
        const ofRectangle& crop = screen->getCropRect();

        float cx = preview.x + crop.x * preview.width;
        float cy = preview.y + crop.y * preview.height;
        float cw = crop.width * preview.width;
        float ch = crop.height * preview.height;
        float hs = 10; // hit zone for handles

        mapDragStart = glm::vec2(x, y);
        mapDragStartCrop = crop;
        mapDrag = MapDrag::None;

        // Check corners first
        if (glm::distance(glm::vec2(x, y), glm::vec2(cx, cy)) < hs)
            mapDrag = MapDrag::TL;
        else if (glm::distance(glm::vec2(x, y), glm::vec2(cx + cw, cy)) < hs)
            mapDrag = MapDrag::TR;
        else if (glm::distance(glm::vec2(x, y), glm::vec2(cx, cy + ch)) < hs)
            mapDrag = MapDrag::BL;
        else if (glm::distance(glm::vec2(x, y), glm::vec2(cx + cw, cy + ch)) < hs)
            mapDrag = MapDrag::BR;
        // Edges
        else if (std::abs(x - cx) < hs && y > cy && y < cy + ch)
            mapDrag = MapDrag::Left;
        else if (std::abs(x - (cx + cw)) < hs && y > cy && y < cy + ch)
            mapDrag = MapDrag::Right;
        else if (std::abs(y - cy) < hs && x > cx && x < cx + cw)
            mapDrag = MapDrag::Top;
        else if (std::abs(y - (cy + ch)) < hs && x > cx && x < cx + cw)
            mapDrag = MapDrag::Bottom;
        // Inside = move
        else if (x > cx && x < cx + cw && y > cy && y < cy + ch)
            mapDrag = MapDrag::Move;

        return;
    }

    // View mode: no interaction
    if (appMode == AppMode::View) return;

    // Check if click is on server list area
    if (showUI && x < serverListWidth) {
        return;
    }

    // Check gizmo hit first
    if (scene.selectedIndex >= 0) {
        ScreenObject* selected = scene.getScreen(scene.selectedIndex);
        if (selected && gizmo.hitTest(cam, glm::vec2(x, y), *selected)) {
            cam.disableMouseInput();
            gizmoInteracting = true;
            gizmo.beginDrag(glm::vec2(x, y), cam, *selected);
            return;
        }
    }

    // Pick objects in scene
    int hit = scene.pick(cam, glm::vec2(x, y));
    if (hit >= 0) {
        scene.selectedIndex = hit;
        propertiesPanel.setTarget(scene.getScreen(hit));
    } else {
        scene.selectedIndex = -1;
        propertiesPanel.setTarget(nullptr);
    }
}

void ofApp::mouseDragged(int x, int y, int button) {
    // --- Mapping mode drag ---
    if (mappingMode && mapDrag != MapDrag::None) {
        auto* screen = scene.getScreen(scene.selectedIndex);
        if (!screen) return;

        ofRectangle preview = getMapPreviewArea();
        float dx = (x - mapDragStart.x) / preview.width;
        float dy = (y - mapDragStart.y) / preview.height;

        // Work with absolute edge positions from the original crop
        float oL = mapDragStartCrop.x;                         // original left
        float oT = mapDragStartCrop.y;                         // original top
        float oR = mapDragStartCrop.x + mapDragStartCrop.width;  // original right
        float oB = mapDragStartCrop.y + mapDragStartCrop.height; // original bottom

        float nL = oL, nT = oT, nR = oR, nB = oB; // new edges

        switch (mapDrag) {
            case MapDrag::Move:
                nL = snapValue(oL + dx); nR = nL + (oR - oL);
                nT = snapValue(oT + dy); nB = nT + (oB - oT);
                break;
            case MapDrag::TL:
                nL = snapValue(oL + dx); nT = snapValue(oT + dy);
                break;
            case MapDrag::TR:
                nR = snapValue(oR + dx); nT = snapValue(oT + dy);
                break;
            case MapDrag::BL:
                nL = snapValue(oL + dx); nB = snapValue(oB + dy);
                break;
            case MapDrag::BR:
                nR = snapValue(oR + dx); nB = snapValue(oB + dy);
                break;
            case MapDrag::Left:
                nL = snapValue(oL + dx);
                break;
            case MapDrag::Right:
                nR = snapValue(oR + dx);
                break;
            case MapDrag::Top:
                nT = snapValue(oT + dy);
                break;
            case MapDrag::Bottom:
                nB = snapValue(oB + dy);
                break;
            default: break;
        }

        // Ensure minimum size
        if (nR - nL < 0.01f) nR = nL + 0.01f;
        if (nB - nT < 0.01f) nB = nT + 0.01f;

        screen->setCropRect(ofRectangle(nL, nT, nR - nL, nB - nT));
        propertiesPanel.syncFromTarget();
        return;
    }

    if (appMode != AppMode::Designer) return;

    if (gizmoInteracting && scene.selectedIndex >= 0) {
        ScreenObject* selected = scene.getScreen(scene.selectedIndex);
        if (selected) {
            gizmo.updateDrag(glm::vec2(x, y), cam, *selected);
            propertiesPanel.syncFromTarget();
        }
    }
}

void ofApp::mouseReleased(int x, int y, int button) {
    if (mappingMode) {
        mapDrag = MapDrag::None;
        return;
    }
    if (gizmoInteracting) {
        gizmo.endDrag();
        gizmoInteracting = false;
    }
    cam.enableMouseInput();
}

void ofApp::windowResized(int w, int h) {
    propertiesPanel.setPosition(w - 240, 10);
}
