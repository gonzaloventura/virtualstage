#include "win_byte_fix.h"
#include "ofApp.h"
#include <GLFW/glfw3.h>
// GLFW 3.3 (oF 0.12.0) doesn't have GLFW_RESIZE_ALL_CURSOR; define fallback
#ifndef GLFW_RESIZE_ALL_CURSOR
#define GLFW_RESIZE_ALL_CURSOR 0x00036009
#endif
#include "ofXml.h"
#include <thread>
#ifdef TARGET_OSX
#include <unistd.h>
#endif
#include <mutex>

void ofApp::setup() {
    ofSetEscapeQuitsApp(false);
    ofSetFrameRate(60);
    ofSetVerticalSync(true);

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
    propertiesPanel.onPropertyChanged = [this]() {
        if (!propsDirty) {
            pushUndo();
            propsDirty = true;
            propsDirtyTimer = 0;
        }
    };

    // Cursors
    handCursor = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
    crosshairCursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);

    // Push initial undo state
    undoManager.pushState(scene);

    // ── Auth setup ──────────────────────────────────────────────────────────
    // Wire up the modal submit callback before checking session
    authModal.onSubmit = [this](AuthModal::Tab tab, const std::string& email,
                                const std::string& pwd, const std::string& confirm) {
        handleAuthSubmit(tab, email, pwd, confirm);
    };

    authManager.loadSession();
    if (authManager.isAuthenticated()) {
        // Already logged in — refresh token in background (silent fail = offline OK)
        std::thread([this]() {
            std::string err;
            authManager.refreshToken(err); // ignore error: offline mode is fine
        }).detach();
        // Load cloud preferences in background
        std::thread([this]() {
            std::string err, cloudData;
            if (cloudStorage.loadPreferences(authManager.getSession(), cloudData, err)) {
                if (!cloudData.empty()) {
                    preferences.fromJsonString(cloudData);
                    preferences.saveLocal();
                    prefsNeedRefresh.store(true);
                }
            }
        }).detach();
    } else {
        authModal.show();
        cam.disableMouseInput(); // Block camera while auth modal is up
    }

    // ── Preferences setup ────────────────────────────────────────────────────
    preferences.loadLocal();
    propertiesPanel.setPreferences(&preferences);
    propertiesPanel.refreshUnitLabels();

    settingsModal.onPreferenceChanged = [this]() {
        propertiesPanel.refreshUnitLabels();
        // Sync to cloud in background
        if (authManager.isAuthenticated()) {
            std::string jsonStr = preferences.toJsonString();
            std::thread([this, jsonStr]() {
                std::string err;
                cloudStorage.savePreferences(authManager.getSession(), jsonStr, err);
            }).detach();
        }
    };
}

void ofApp::update() {
    scene.update();
    // Refresh server list periodically
    servers = scene.getAvailableServers();

    // Update background from ambient light slider (0-100 → 0-60)
    bgBrightness = (int)(propertiesPanel.getAmbientLight() * 0.6f);

    // Restrict camera input to the 3D viewport area (excludes sidebar, menu bar, status bar)
    if (appMode == AppMode::Designer && showUI) {
        cam.setControlArea(ofRectangle(serverListWidth, menuBarHeight,
                                        ofGetWidth() - serverListWidth,
                                        ofGetHeight() - menuBarHeight - statusBarHeight));
    } else {
        cam.clearControlArea();
    }

    // Reset properties dirty flag after idle (allows new undo capture)
    if (propsDirty) {
        propsDirtyTimer += ofGetLastFrameTime();
        if (propsDirtyTimer > 0.5f) {
            propsDirty = false;
            propsDirtyTimer = 0;
        }
    }

    // Refresh UI if preferences were updated from cloud
    if (prefsNeedRefresh.exchange(false)) {
        propertiesPanel.refreshUnitLabels();
    }

    // Autosave — works with both local and cloud projects
    if (autosaveEnabled && (!currentProjectPath.empty() || !currentCloudProjectName.empty())) {
        autosaveTimer += ofGetLastFrameTime();
        if (autosaveTimer >= autosaveInterval) {
            autosaveTimer = 0;
            doAutosave();
        }
    }

    // ── Auth result (written by background thread) ──────────────────────────
    {
        std::lock_guard<std::mutex> lock(authResultMutex);
        if (pendingAuthResult.done) {
            pendingAuthResult.done = false;
            if (pendingAuthResult.success) {
                if (pendingAuthResult.needConfirm) {
                    authModal.setLoading(false);
                    authModal.setSuccess("Account created! Please check your email to confirm, then sign in.");
                } else {
                    authModal.hide();
                    cam.enableMouseInput(); // Re-enable camera after successful auth
                    // Load cloud preferences after first login
                    std::thread([this]() {
                        std::string err, cloudData;
                        if (cloudStorage.loadPreferences(authManager.getSession(), cloudData, err)) {
                            if (!cloudData.empty()) {
                                preferences.fromJsonString(cloudData);
                                preferences.saveLocal();
                                prefsNeedRefresh.store(true);
                            }
                        }
                    }).detach();
                }
            } else {
                authModal.setError(pendingAuthResult.error);
            }
        }
    }

    // ── Cloud project load result ────────────────────────────────────────────
    {
        std::lock_guard<std::mutex> lock(cloudProjectResultMutex);
        if (pendingCloudProject.done) {
            pendingCloudProject.done = false;
            if (pendingCloudProject.success) {
                cloudLoadState = CloudLoadState::Hidden;
                // Write to temp file and open as project
                std::string tmpPath = ofFilePath::getUserHomeDir() + "/.virtualstage/cloud_load_tmp.json";
                ofSavePrettyJson(tmpPath, pendingCloudProject.data);
                ofJson camJson;
                if (scene.loadProject(tmpPath, &camJson)) {
                    currentProjectPath = "";
                    currentCloudProjectName = pendingCloudProject.name;
                    autosaveEnabled = true;
                    autosaveTimer = 0;
                    if (!camJson.is_null()) {
                        try {
                            auto pos = camJson["position"];
                            auto tgt = camJson["target"];
                            cam.setPosition(glm::vec3(pos[0], pos[1], pos[2]));
                            cam.setTarget(glm::vec3(tgt[0], tgt[1], tgt[2]));
                            cam.setDistance(camJson.value("distance", 800.0f));
                        } catch (...) {}
                    }
                    undoManager.clear();
                    pushUndo();
                    propertiesPanel.setTarget(nullptr);
                    scene.clearSelection();
                }
                ofFile::removeFile(tmpPath);
            } else {
                cloudLoadState = CloudLoadState::Error;
                cloudLoadError = pendingCloudProject.error;
            }
        }
    }

}

void ofApp::draw() {
    ofBackground(bgBrightness);

    // --- Mapping mode: full-screen 2D editor ---
    if (mappingMode) {
        drawMappingMode();
        drawMenuBar();
        return;
    }

    // --- 3D Scene ---
    ofEnableDepthTest();
    cam.begin();

    if (appMode == AppMode::Designer) {
        scene.drawGrid();
    }

    scene.draw(appMode == AppMode::View);

    // Gizmo for selected object (Designer mode only) — draw on primary selected
    if (appMode == AppMode::Designer && scene.getPrimarySelected() >= 0 && scene.getScreen(scene.getPrimarySelected())) {
        ofDisableDepthTest();
        gizmo.draw(*scene.getScreen(scene.getPrimarySelected()), cam);
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

    // Camera lock button (View mode)
    if (appMode == AppMode::View) {
        drawCameraLock();
    }

    // Context menu (drawn on top of everything except menu bar)
    if (contextMenuOpen) {
        drawContextMenu();
    }

    // Box selection overlay
    if (boxSelecting) {
        ofRectangle r(boxSelectStart.x, boxSelectStart.y,
                      boxSelectEnd.x - boxSelectStart.x,
                      boxSelectEnd.y - boxSelectStart.y);
        r.standardize();
        ofSetColor(80, 160, 255, 50);
        ofFill();
        ofDrawRectangle(r);
        ofNoFill();
        ofSetColor(80, 160, 255, 200);
        ofDrawRectangle(r);
        ofFill();
    }

    // Hide menu bar in View mode for clean look
    if (appMode != AppMode::View) {
        drawMenuBar();
    }
    drawStatusBar();

    // Modal overlays (drawn on top of everything)
    if (showAboutDialog) {
        drawAboutDialog();
    }
    if (showUpdateModal) {
        drawUpdateModal();
    }
    // Cloud load modal
    if (cloudLoadState != CloudLoadState::Hidden) {
        drawCloudLoadModal();
    }
    // Settings modal
    if (settingsModal.isVisible()) {
        settingsModal.draw();
    }
    // Auth modal — topmost, blocks all interaction underneath
    if (authModal.isVisible()) {
        authModal.draw();
    }
}

void ofApp::drawServerList() {
    float panelX = 0;
    float panelY = menuBarHeight;
    float panelH = ofGetHeight() - panelY - statusBarHeight;
    float rowH = 22.0f;
    float xBtnSize = 16.0f; // delete button size

    // Panel background
    ofSetColor(20, 20, 20, 200);
    ofDrawRectangle(panelX, panelY, serverListWidth, panelH);

    // Calculate total content height for scroll
    float contentH = 0;
    contentH += 28; // SCREENS header
    contentH += std::max(scene.getScreenCount(), 1) * rowH;
    contentH += 48; // gap (20) + separator line + gap (8) + SERVERS header (28)
    contentH += std::max((int)servers.size(), 1) * rowH;
    contentH += 10; // bottom padding
    sidebarContentHeight = contentH;

    // Clamp scroll
    float maxScroll = std::max(0.0f, contentH - panelH);
    sidebarScroll = ofClamp(sidebarScroll, 0, maxScroll);

    // Enable scissor to clip content to panel
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)panelX, (int)(ofGetHeight() - panelY - panelH),
              (int)serverListWidth, (int)panelH);

    float curY = panelY - sidebarScroll;

    // --- SCREENS header ---
    ofSetColor(200);
    ofDrawBitmapString("SCREENS  [A]dd", panelX + 10, curY + 18);
    curY += 28;

    // --- Screen rows ---
    float mouseX = ofGetMouseX();
    float mouseY = ofGetMouseY();

    for (int i = 0; i < scene.getScreenCount(); i++) {
        auto* screen = scene.getScreen(i);
        if (!screen) continue;

        float rowTop = curY;
        float rowBot = curY + rowH;
        bool selected = scene.isSelected(i);
        bool hovered = (mouseX >= panelX && mouseX < serverListWidth &&
                        mouseY >= rowTop && mouseY < rowBot &&
                        mouseY >= panelY && mouseY < panelY + panelH);

        // Row background on hover/selected
        if (selected) {
            ofSetColor(0, 120, 200, 60);
            ofDrawRectangle(panelX, rowTop, serverListWidth, rowH);
        } else if (hovered) {
            ofSetColor(255, 255, 255, 20);
            ofDrawRectangle(panelX, rowTop, serverListWidth, rowH);
        }

        // Screen name
        ofSetColor(selected ? ofColor(0, 200, 255) : ofColor(180));
        std::string label = screen->name;
        if (screen->hasSource()) {
            label += " [" + screen->sourceName + "]";
        }
        int maxChars = (int)((serverListWidth - 40) / 8); // 8px per char, leave room for X
        if ((int)label.length() > maxChars) label = label.substr(0, maxChars - 3) + "...";
        ofDrawBitmapString(label, panelX + 10, rowTop + 15);

        // Delete [X] button
        float xBtnX = serverListWidth - xBtnSize - 8;
        float xBtnY = rowTop + (rowH - xBtnSize) / 2;
        bool xHovered = (mouseX >= xBtnX && mouseX <= xBtnX + xBtnSize &&
                         mouseY >= xBtnY && mouseY <= xBtnY + xBtnSize &&
                         mouseY >= panelY && mouseY < panelY + panelH);

        ofSetColor(xHovered ? ofColor(255, 80, 80) : ofColor(100));
        ofNoFill();
        ofDrawRectangle(xBtnX, xBtnY, xBtnSize, xBtnSize);
        ofFill();
        // Draw X
        ofDrawLine(xBtnX + 4, xBtnY + 4, xBtnX + xBtnSize - 4, xBtnY + xBtnSize - 4);
        ofDrawLine(xBtnX + xBtnSize - 4, xBtnY + 4, xBtnX + 4, xBtnY + xBtnSize - 4);

        curY += rowH;
    }

    if (scene.getScreenCount() == 0) {
        ofSetColor(100);
        ofDrawBitmapString("No screens", panelX + 10, curY + 15);
        curY += rowH;
    }

    // --- SERVERS header ---
    curY += 20;
    ofSetColor(60);
    ofDrawLine(panelX + 10, curY, panelX + serverListWidth - 10, curY);
    curY += 8;
    ofSetColor(200);
    ofDrawBitmapString("SERVERS (click to assign)", panelX + 10, curY + 18);
    curY += 28;

    // --- Server rows ---
    for (size_t i = 0; i < servers.size(); i++) {
        float rowTop = curY;
        float rowBot = curY + rowH;

        // Check if assigned to any selected screen
        bool assigned = false;
        for (int si : scene.selectedIndices) {
            auto* sel = scene.getScreen(si);
            if (sel && sel->sourceIndex == (int)i) { assigned = true; break; }
        }

        bool hovered = (mouseX >= panelX && mouseX < serverListWidth &&
                        mouseY >= rowTop && mouseY < rowBot &&
                        mouseY >= panelY && mouseY < panelY + panelH);

        if (assigned) {
            ofSetColor(0, 200, 100, 40);
            ofDrawRectangle(panelX, rowTop, serverListWidth, rowH);
        } else if (hovered) {
            ofSetColor(255, 255, 255, 20);
            ofDrawRectangle(panelX, rowTop, serverListWidth, rowH);
        }

        ofSetColor(assigned ? ofColor(0, 220, 100) : ofColor(180));
        std::string label = ofToString(i + 1) + ". " + servers[i].displayName();
        int maxChars2 = (int)(serverListWidth - 20) / 8;
        if ((int)label.length() > maxChars2) label = label.substr(0, maxChars2 - 3) + "...";
        ofDrawBitmapString(label, panelX + 10, rowTop + 15);

        curY += rowH;
    }

    if (servers.empty()) {
        ofSetColor(100);
#ifdef TARGET_OSX
        ofDrawBitmapString("Waiting for Syphon servers...", panelX + 10, curY + 15);
#elif defined(TARGET_WIN32)
        ofDrawBitmapString("Waiting for Spout servers...", panelX + 10, curY + 15);
#else
        ofDrawBitmapString("Waiting for servers...", panelX + 10, curY + 15);
#endif
        curY += rowH;
    }

    glDisable(GL_SCISSOR_TEST);

    // --- Scrollbar ---
    if (contentH > panelH) {
        float scrollbarH = std::max(20.0f, panelH * (panelH / contentH));
        float scrollTrack = panelH - scrollbarH;
        float scrollPos = (maxScroll > 0) ? (sidebarScroll / maxScroll) * scrollTrack : 0;

        // Track
        ofSetColor(40);
        ofDrawRectangle(serverListWidth - 6, panelY, 6, panelH);

        // Thumb
        ofSetColor(100);
        ofDrawRectangle(serverListWidth - 5, panelY + scrollPos, 4, scrollbarH);
    }

    ofSetColor(255);
}

bool ofApp::handleSidebarClick(int x, int y) {
    if (x < 0 || x >= serverListWidth) return false;

    float panelY = menuBarHeight;
    float panelH = ofGetHeight() - panelY - statusBarHeight;
    if (y < panelY || y >= panelY + panelH) return false;

    float rowH = 22.0f;
    float xBtnSize = 16.0f;

    // Convert click Y to content Y (account for scroll)
    float curY = panelY - sidebarScroll;
    curY += 28; // skip SCREENS header

    // --- Screen rows ---
    for (int i = 0; i < scene.getScreenCount(); i++) {
        float rowTop = curY;
        float rowBot = curY + rowH;

        if (y >= rowTop && y < rowBot) {
            // Check if X delete button was clicked
            float xBtnX = serverListWidth - xBtnSize - 8;
            float xBtnY = rowTop + (rowH - xBtnSize) / 2;
            if (x >= xBtnX && x <= xBtnX + xBtnSize &&
                y >= xBtnY && y <= xBtnY + xBtnSize) {
                pushUndo();
                scene.removeScreen(i);
                updatePropertiesForSelection();
                return true;
            }

            // Click on row — Shift=range, Cmd/Ctrl=toggle, plain=select only
#ifdef TARGET_OSX
            bool multiKey = ofGetKeyPressed(OF_KEY_SUPER);
#else
            bool multiKey = ofGetKeyPressed(OF_KEY_CONTROL);
#endif
            bool shiftKey = ofGetKeyPressed(OF_KEY_SHIFT);

            if (shiftKey && lastClickedSidebarIndex >= 0) {
                scene.selectRange(lastClickedSidebarIndex, i);
            } else if (multiKey) {
                scene.toggleSelected(i);
            } else {
                scene.selectOnly(i);
            }
            lastClickedSidebarIndex = i;
            updatePropertiesForSelection();
            return true;
        }
        curY += rowH;
    }

    if (scene.getScreenCount() == 0) curY += rowH;

    // Gap + separator + SERVERS header
    curY += 20 + 8 + 28;

    // --- Server rows ---
    for (size_t i = 0; i < servers.size(); i++) {
        float rowTop = curY;
        float rowBot = curY + rowH;

        if (y >= rowTop && y < rowBot) {
            // Click on server → assign to all selected screens
            if (scene.getSelectionCount() > 0) {
                pushUndo();
                for (int si : scene.getSelectedIndicesSorted()) {
                    scene.assignSourceToScreen(si, (int)i);
                }
                updatePropertiesForSelection();
            }
            return true;
        }
        curY += rowH;
    }

    return true; // consumed (clicked inside panel)
}

void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
    if (authModal.isVisible() || cloudLoadState != CloudLoadState::Hidden) return;
    if (settingsModal.isVisible()) return;

    // Scroll sidebar when mouse is over it — consume event so camera doesn't zoom
    if (appMode == AppMode::Designer && showUI &&
        x >= 0 && x < serverListWidth &&
        y >= menuBarHeight && y < ofGetHeight() - statusBarHeight) {
        sidebarScroll -= scrollY * 20.0f;
        float panelH = ofGetHeight() - menuBarHeight - statusBarHeight;
        float maxScroll = std::max(0.0f, sidebarContentHeight - panelH);
        sidebarScroll = ofClamp(sidebarScroll, 0, maxScroll);
        return; // don't let camera handle this scroll
    }
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

    float nextX = 100;

    if (appMode == AppMode::View) {
        // View mode: minimal status bar — only FPS + essential hints
        ofSetColor(150);
        std::string fpsStr = "FPS: " + ofToString((int)ofGetFrameRate());
        ofDrawBitmapString(fpsStr, nextX, barY + 20);

        ofSetColor(100);
        std::string hint = "Tab:Designer  F:Full";
        ofDrawBitmapString(hint, ofGetWidth() - hint.length() * 8 - 10, barY + 20);
    } else {
        // Designer mode: full status bar
        if (!currentProjectPath.empty()) {
            ofSetColor(200, 200, 100);
            std::string projName = ofFilePath::getFileName(currentProjectPath);
            ofDrawBitmapString(projName, nextX, barY + 20);
            nextX += projName.length() * 8 + 15;
        }

        ofSetColor(150);
        std::string fpsStr = "FPS: " + ofToString((int)ofGetFrameRate());
        ofDrawBitmapString(fpsStr, nextX, barY + 20);

        nextX += fpsStr.length() * 8 + 15;
        ofSetColor(150);
        std::string srvStr = "Servers: " + ofToString(scene.getServerCount());
        ofDrawBitmapString(srvStr, nextX, barY + 20);

        ofSetColor(100);
        std::string hint;
        if (linkState == LinkState::Confirm) {
            ofSetColor(255, 200, 0);
            hint = "Re-link? L:Yes  Esc:Cancel";
        } else if (linkState == LinkState::ChooseRect) {
            ofSetColor(255, 200, 0);
            hint = "Use rects from Resolume:  I:Input  O:Output  Esc:Cancel";
        } else {
            if (selectMode) {
                ofSetColor(0, 200, 255);
                hint = "SELECT  |  Drag to select  |  S:Exit  W/E/R:Exit";
            } else {
                hint = gizmo.getModeString() +
#ifdef TARGET_OSX
                    "  |  S:Select  A:Add  Del:Remove  L:Link  M:Map  H:UI  Tab:View  Cmd+Z:Undo  Cmd+S/O:Save/Open";
#else
                    "  |  S:Select  A:Add  Del:Remove  L:Link  M:Map  H:UI  Tab:View  Ctrl+Z:Undo  Ctrl+S/O:Save/Open";
#endif
            }
        }
        ofDrawBitmapString(hint, ofGetWidth() - hint.length() * 8 - 10, barY + 20);
    }

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

void ofApp::drawCameraLock() {
    float btnSize = 30;
    float btnX = ofGetWidth() - btnSize - 12;
    float btnY = menuBarHeight + 10;
    float mx = ofGetMouseX(), my = ofGetMouseY();
    bool hover = (mx >= btnX && mx <= btnX + btnSize && my >= btnY && my <= btnY + btnSize);

    // Background
    ofSetColor(cameraLocked ? ofColor(200, 60, 60, 180) : ofColor(60, 60, 60, hover ? 180 : 120));
    ofDrawRectRounded(btnX, btnY, btnSize, btnSize, 4);

    // Lock icon (simple padlock shape)
    float cx = btnX + btnSize / 2;
    float cy = btnY + btnSize / 2;

    if (cameraLocked) {
        // Locked: closed padlock
        ofSetColor(255);
        // Body
        ofDrawRectangle(cx - 7, cy - 1, 14, 10);
        // Shackle (closed arc)
        ofNoFill();
        ofSetLineWidth(2);
        ofDrawLine(cx - 5, cy - 1, cx - 5, cy - 5);
        ofDrawLine(cx + 5, cy - 1, cx + 5, cy - 5);
        ofDrawLine(cx - 5, cy - 5, cx - 2, cy - 9);
        ofDrawLine(cx + 5, cy - 5, cx + 2, cy - 9);
        ofDrawLine(cx - 2, cy - 9, cx + 2, cy - 9);
        ofFill();
        ofSetLineWidth(1);
    } else {
        // Unlocked: open padlock
        ofSetColor(200);
        // Body
        ofDrawRectangle(cx - 7, cy - 1, 14, 10);
        // Shackle (open)
        ofNoFill();
        ofSetLineWidth(2);
        ofDrawLine(cx - 5, cy - 1, cx - 5, cy - 5);
        ofDrawLine(cx - 5, cy - 5, cx - 2, cy - 9);
        ofDrawLine(cx - 2, cy - 9, cx + 2, cy - 9);
        ofDrawLine(cx + 2, cy - 9, cx + 5, cy - 5);
        // Open shackle lifts on right
        ofFill();
        ofSetLineWidth(1);
    }

    ofSetColor(255);
}

void ofApp::refreshServerList() {
    servers = scene.getAvailableServers();
}

// --- Menu Bar ---

// Helper: draw a dropdown menu and return the height
static float drawDropdown(float dropX, float dropY, float dropW,
                          const std::vector<std::tuple<std::string, std::string, bool, bool, bool>>& items) {
    float itemH = 24;
    float dropH = 0;
    for (auto& it : items) dropH += std::get<2>(it) ? 10 : itemH;

    ofSetColor(0, 0, 0, 80);
    ofDrawRectangle(dropX + 3, dropY + 3, dropW, dropH);
    ofSetColor(50, 50, 50);
    ofDrawRectangle(dropX, dropY, dropW, dropH);
    ofSetColor(80);
    ofNoFill();
    ofDrawRectangle(dropX, dropY, dropW, dropH);
    ofFill();

    float iy = dropY;
    for (auto& it : items) {
        if (std::get<2>(it)) { // separator
            ofSetColor(80);
            ofDrawLine(dropX + 8, iy + 5, dropX + dropW - 8, iy + 5);
            iy += 10;
            continue;
        }
        if (ofGetMouseX() >= dropX && ofGetMouseX() <= dropX + dropW &&
            ofGetMouseY() >= iy && ofGetMouseY() < iy + itemH) {
            ofSetColor(0, 120, 200);
            ofDrawRectangle(dropX + 1, iy, dropW - 2, itemH);
        }
        if (std::get<3>(it) && std::get<4>(it)) { // toggle + active
            ofSetColor(100, 220, 100);
            ofDrawBitmapString("*", dropX + 8, iy + 17);
        }
        ofSetColor(220);
        ofDrawBitmapString(std::get<0>(it), dropX + 22, iy + 17);
        if (!std::get<1>(it).empty()) {
            ofSetColor(130);
            float sw = std::get<1>(it).length() * 8;
            ofDrawBitmapString(std::get<1>(it), dropX + dropW - sw - 10, iy + 17);
        }
        iy += itemH;
    }
    return dropH;
}

void ofApp::drawMenuBar() {
    float barW = ofGetWidth();

    // Bar background
    ofSetColor(45, 45, 45);
    ofDrawRectangle(0, 0, barW, menuBarHeight);

    // Menu buttons
    float fileX = 10, fileW = 40;
    float viewX = fileX + fileW + 15, viewW = 42;

    // File button
    bool fileHover = (ofGetMouseX() >= fileX - 5 && ofGetMouseX() <= fileX + fileW + 5 &&
                      ofGetMouseY() >= 0 && ofGetMouseY() <= menuBarHeight);
    if (fileMenuOpen || fileHover) {
        ofSetColor(70, 70, 70);
        ofDrawRectangle(fileX - 5, 0, fileW + 10, menuBarHeight);
    }
    ofSetColor(220);
    ofDrawBitmapString("File", fileX, menuBarHeight - 7);

    // View button
    bool viewHover = (ofGetMouseX() >= viewX - 5 && ofGetMouseX() <= viewX + viewW + 5 &&
                      ofGetMouseY() >= 0 && ofGetMouseY() <= menuBarHeight);
    if (viewMenuOpen || viewHover) {
        ofSetColor(70, 70, 70);
        ofDrawRectangle(viewX - 5, 0, viewW + 10, menuBarHeight);
    }
    ofSetColor(220);
    ofDrawBitmapString("View", viewX, menuBarHeight - 7);

    // Link button
    float linkX = viewX + viewW + 15, linkW = 32;
    bool linkHover = (ofGetMouseX() >= linkX - 5 && ofGetMouseX() <= linkX + linkW + 5 &&
                      ofGetMouseY() >= 0 && ofGetMouseY() <= menuBarHeight);
    if (linkMenuOpen || linkHover) {
        ofSetColor(70, 70, 70);
        ofDrawRectangle(linkX - 5, 0, linkW + 10, menuBarHeight);
    }
    ofSetColor(220);
    ofDrawBitmapString("Link", linkX, menuBarHeight - 7);

    // Help button
    float helpX = linkX + linkW + 15, helpW = 40;
    bool helpHover = (ofGetMouseX() >= helpX - 5 && ofGetMouseX() <= helpX + helpW + 5 &&
                      ofGetMouseY() >= 0 && ofGetMouseY() <= menuBarHeight);
    if (helpMenuOpen || helpHover) {
        ofSetColor(70, 70, 70);
        ofDrawRectangle(helpX - 5, 0, helpW + 10, menuBarHeight);
    }
    ofSetColor(220);
    ofDrawBitmapString("Help", helpX, menuBarHeight - 7);

    // Autosave indicator
    float indX = helpX + helpW + 20;
    if (autosaveEnabled) {
        ofSetColor(100, 200, 100);
        ofDrawBitmapString("[Autosave ON]", indX, menuBarHeight - 7);
    }

    // File dropdown
    // items: {label, shortcut, isSeparator, isToggle, toggleState}
    if (fileMenuOpen) {
        // Logged-in user email for display
        std::string userLabel = authManager.isAuthenticated()
            ? authManager.getSession().email
            : "Not signed in";
        if (userLabel.length() > 28) userLabel = userLabel.substr(0, 25) + "...";

        std::vector<std::tuple<std::string, std::string, bool, bool, bool>> items = {
            {"New Project",       "",             false, false, false},
            {"Open Project",      "Ctrl+O",       false, false, false},
            {"Save Project",      "Ctrl+S",       false, false, false},
            {"Save Project As",   "Ctrl+Shift+S", false, false, false},
            {"Save to Cloud",     "",             false, false, false},
            {"Load from Cloud",   "",             false, false, false},
            {"",                  "",             true,  false, false},
            {"Autosave (15s)",    "",             false, true,  autosaveEnabled},
            {"Preferences...",    "",             false, false, false},
            {"",                  "",             true,  false, false},
            {userLabel,           "",             false, false, false},
            {"Log Out",           "",             false, false, false},
            {"",                  "",             true,  false, false},
            {"Quit",              "",             false, false, false},
        };
        drawDropdown(fileX - 5, menuBarHeight, 240, items);
    }

    // View dropdown
    if (viewMenuOpen) {
        std::vector<std::tuple<std::string, std::string, bool, bool, bool>> items = {
            {"Ambient Light", "", false, true, showAmbientLight},
            {"",              "", true,  false, false},
            {"Position",      "", false, true, showPosition},
            {"Rotation",      "", false, true, showRotation},
            {"Size",          "", false, true, showScale},
            {"Input Mapping", "", false, true, showCrop},
        };
        drawDropdown(viewX - 5, menuBarHeight, 200, items);
    }

    // Link dropdown
    if (linkMenuOpen) {
        std::vector<std::tuple<std::string, std::string, bool, bool, bool>> items = {
            {"Input",  "L L I", false, false, false},
            {"Output", "L L O", false, false, false},
        };
        drawDropdown(linkX - 5, menuBarHeight, 180, items);
    }

    // Help dropdown
    if (helpMenuOpen) {
        std::vector<std::tuple<std::string, std::string, bool, bool, bool>> items = {
            {"Manual",            "", false, false, false},
            {"Check for Updates", "", false, false, false},
            {"",                  "", true,  false, false},
            {"About VirtualStage","", false, false, false},
        };
        drawDropdown(helpX - 5, menuBarHeight, 200, items);
    }

    ofSetColor(255);
}

bool ofApp::handleMenuClick(int x, int y) {
    float fileX = 10, fileW = 40;
    float viewX = fileX + fileW + 15, viewW = 42;
    float itemH = 24;

    float linkX = viewX + viewW + 15, linkW = 32;
    float helpX = linkX + linkW + 15, helpW = 40;

    // Click on "File" button
    if (x >= fileX - 5 && x <= fileX + fileW + 5 && y >= 0 && y <= menuBarHeight) {
        fileMenuOpen = !fileMenuOpen;
        viewMenuOpen = false;
        linkMenuOpen = false;
        helpMenuOpen = false;
        contextMenuOpen = false;
        return true;
    }

    // Click on "View" button
    if (x >= viewX - 5 && x <= viewX + viewW + 5 && y >= 0 && y <= menuBarHeight) {
        viewMenuOpen = !viewMenuOpen;
        fileMenuOpen = false;
        linkMenuOpen = false;
        helpMenuOpen = false;
        contextMenuOpen = false;
        return true;
    }

    // Click on "Link" button
    if (x >= linkX - 5 && x <= linkX + linkW + 5 && y >= 0 && y <= menuBarHeight) {
        linkMenuOpen = !linkMenuOpen;
        fileMenuOpen = false;
        viewMenuOpen = false;
        helpMenuOpen = false;
        contextMenuOpen = false;
        return true;
    }

    // Click on "Help" button
    if (x >= helpX - 5 && x <= helpX + helpW + 5 && y >= 0 && y <= menuBarHeight) {
        helpMenuOpen = !helpMenuOpen;
        fileMenuOpen = false;
        viewMenuOpen = false;
        linkMenuOpen = false;
        contextMenuOpen = false;
        return true;
    }

    // File dropdown clicks
    // Items (14 total): 0=New, 1=Open, 2=Save, 3=SaveAs, 4=SaveCloud, 5=LoadCloud,
    //   6=sep, 7=Autosave, 8=Preferences, 9=sep, 10=UserEmail(disabled), 11=LogOut, 12=sep, 13=Quit
    if (fileMenuOpen) {
        float dropX = fileX - 5, dropW = 240;
        bool isSep[] = {false,false,false,false,false,false,true,false,false,true,false,false,true,false};
        int total = 14;
        float iy = menuBarHeight;

        if (x >= dropX && x <= dropX + dropW) {
            for (int i = 0; i < total; i++) {
                if (isSep[i]) { iy += 10; continue; }
                if (y >= iy && y < iy + itemH) {
                    fileMenuOpen = false;
                    switch (i) {
                        case 0: newProject(); break;
                        case 1: openProject(); break;
                        case 2: saveProject(false); break;
                        case 3: saveProject(true); break;
                        case 4: saveToCloud(); break;
                        case 5: loadFromCloud(); break;
                        case 7: // Autosave toggle
                            if (!autosaveEnabled && currentProjectPath.empty() && currentCloudProjectName.empty()) {
                                // No save destination yet — ask user via text box
                                // Enter a name → cloud; cancel → local save dialog
                                std::string cloudName = ofSystemTextBoxDialog(
                                    "Enter a name to save to Cloud (free)\nor cancel for local save:", "");
                                if (!cloudName.empty()) {
                                    currentCloudProjectName = cloudName;
                                    saveToCloud();
                                } else {
                                    saveProject(false);
                                    if (currentProjectPath.empty()) break;
                                }
                            }
                            autosaveEnabled = !autosaveEnabled;
                            autosaveTimer = 0;
                            break;
                        case 8: // Preferences
                            settingsModal.show(&preferences);
                            break;
                        case 10: break; // User email — display only, no action
                        case 11: // Log Out
                            authManager.logout();
                            authModal.show();
                            cam.disableMouseInput();
                            break;
                        case 13: ofExit(); break;
                    }
                    return true;
                }
                iy += itemH;
            }
        }
        fileMenuOpen = false;
        return true;
    }

    // View dropdown clicks
    if (viewMenuOpen) {
        float dropX = viewX - 5, dropW = 200;
        // items: AmbientLight, sep, Position, Rotation, Scale, InputMapping
        bool isSepV[] = {false, true, false, false, false, false};
        int totalV = 6;
        float iy = menuBarHeight;

        if (x >= dropX && x <= dropX + dropW) {
            for (int i = 0; i < totalV; i++) {
                if (isSepV[i]) { iy += 10; continue; }
                if (y >= iy && y < iy + itemH) {
                    viewMenuOpen = false;
                    switch (i) {
                        case 0: showAmbientLight = !showAmbientLight; break;
                        case 2: showPosition = !showPosition; break;
                        case 3: showRotation = !showRotation; break;
                        case 4: showScale = !showScale; break;
                        case 5: showCrop = !showCrop; break;
                    }
                    propertiesPanel.updateGroupVisibility(
                        showAmbientLight, showPosition, showRotation, showScale, showCrop);
                    return true;
                }
                iy += itemH;
            }
        }
        viewMenuOpen = false;
        return true;
    }

    // Link dropdown clicks
    if (linkMenuOpen) {
        float dropX = linkX - 5, dropW = 180;
        // items: Input, Output
        int totalL = 2;
        float iy = menuBarHeight;

        if (x >= dropX && x <= dropX + dropW) {
            for (int i = 0; i < totalL; i++) {
                if (y >= iy && y < iy + itemH) {
                    linkMenuOpen = false;
                    switch (i) {
                        case 0: loadResolumeXml(true); break;   // Input
                        case 1: loadResolumeXml(false); break;  // Output
                    }
                    return true;
                }
                iy += itemH;
            }
        }
        linkMenuOpen = false;
        return true;
    }

    // Help dropdown clicks
    if (helpMenuOpen) {
        float dropX = helpX - 5, dropW = 200;
        // items: Manual, Check for Updates, sep, About VirtualStage
        bool isSepH[] = {false, false, true, false};
        int totalH = 4;
        float iy = menuBarHeight;

        if (x >= dropX && x <= dropX + dropW) {
            for (int i = 0; i < totalH; i++) {
                if (isSepH[i]) { iy += 10; continue; }
                if (y >= iy && y < iy + itemH) {
                    helpMenuOpen = false;
                    switch (i) {
                        case 0: {
                            std::string manualPath = ofToDataPath("manual.html", true);
#ifdef TARGET_OSX
                            system(("open \"" + manualPath + "\"").c_str());
#elif defined(TARGET_WIN32)
                            ShellExecuteA(NULL, "open", manualPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
                            break;
                        }
                        case 1: checkForUpdates(); break;
                        case 3:
                            showAboutDialog = true;
                            break;
                    }
                    return true;
                }
                iy += itemH;
            }
        }
        helpMenuOpen = false;
        return true;
    }

    return false;
}

// --- Context Menu (right-click on screens) ---

// Context menu item IDs
enum CtxAction { CTX_MAP = -1, CTX_DUPLICATE = -2, CTX_DISCONNECT = -3, CTX_NONE = -99 };

struct CtxItem {
    std::string label;
    int action;       // CTX_* or server index (>=0)
    bool separator;   // draw separator BEFORE this item
    ofColor color;
};

static std::vector<CtxItem> buildContextItems(ScreenObject* screen,
                                               const std::vector<ServerInfo>& servers) {
    std::vector<CtxItem> items;

    // Actions section
    if (screen && screen->hasSource()) {
        items.push_back({"Map",          CTX_MAP,        false, ofColor(0, 200, 255)});
    }
    items.push_back({"Duplicate",    CTX_DUPLICATE,  false, ofColor(220)});
    items.push_back({"Disconnect",   CTX_DISCONNECT, false, ofColor(255, 120, 120)});

    // Servers section
    bool first = true;
    for (int i = 0; i < (int)servers.size(); i++) {
        std::string label = servers[i].displayName();
        if (label.length() > 24) label = label.substr(0, 21) + "...";
        bool assigned = (screen && screen->sourceIndex == i);
        items.push_back({label, i, first, assigned ? ofColor(0, 220, 100) : ofColor(220)});
        first = false;
    }
    if (servers.empty()) {
        items.push_back({"(No servers)", CTX_NONE, true, ofColor(100)});
    }

    return items;
}

void ofApp::drawContextMenu() {
    float itemH = 24;
    float dropW = 230;
    float dropX = contextMenuPos.x;
    float dropY = contextMenuPos.y;

    auto* screen = scene.getScreen(contextScreenIndex);
    auto items = buildContextItems(screen, servers);

    // Calculate total height: header band + items + separators
    float headerH = 26;
    float totalH = headerH;
    for (auto& it : items) {
        if (it.separator) totalH += 10;
        totalH += itemH;
    }

    // Keep on screen
    if (dropX + dropW > ofGetWidth()) dropX = ofGetWidth() - dropW;
    if (dropY + totalH > ofGetHeight() - statusBarHeight) dropY = ofGetHeight() - statusBarHeight - totalH;

    // Shadow + background
    ofSetColor(0, 0, 0, 100);
    ofDrawRectangle(dropX + 3, dropY + 3, dropW, totalH);
    ofSetColor(50, 50, 50);
    ofDrawRectangle(dropX, dropY, dropW, totalH);

    // Header band with screen name
    ofSetColor(35, 35, 35);
    ofDrawRectangle(dropX, dropY, dropW, headerH);
    ofSetColor(255);
    std::string header = screen ? screen->name : "Screen";
    if (header.length() > 26) header = header.substr(0, 23) + "...";
    ofDrawBitmapString(header, dropX + 10, dropY + 18);

    // Border
    ofSetColor(80);
    ofNoFill();
    ofDrawRectangle(dropX, dropY, dropW, totalH);
    ofFill();

    // Items
    float iy = dropY + headerH;
    for (auto& it : items) {
        if (it.separator) {
            ofSetColor(80);
            ofDrawLine(dropX + 8, iy + 5, dropX + dropW - 8, iy + 5);
            iy += 10;
        }

        bool hover = (ofGetMouseX() >= dropX && ofGetMouseX() <= dropX + dropW &&
                       ofGetMouseY() >= iy && ofGetMouseY() < iy + itemH);
        if (hover && it.action != CTX_NONE) {
            ofSetColor(0, 120, 200);
            ofDrawRectangle(dropX + 1, iy, dropW - 2, itemH);
        }

        ofSetColor(it.color);
        ofDrawBitmapString(it.label, dropX + 22, iy + 17);
        iy += itemH;
    }

    ofSetColor(255);
}

bool ofApp::handleContextMenuClick(int x, int y) {
    if (!contextMenuOpen) return false;

    float itemH = 24;
    float dropW = 230;
    float dropX = contextMenuPos.x;
    float dropY = contextMenuPos.y;

    if (dropX + dropW > ofGetWidth()) dropX = ofGetWidth() - dropW;

    auto* screen = scene.getScreen(contextScreenIndex);
    auto items = buildContextItems(screen, servers);

    float headerH = 26;
    float totalH = headerH;
    for (auto& it : items) {
        if (it.separator) totalH += 10;
        totalH += itemH;
    }
    if (dropY + totalH > ofGetHeight() - statusBarHeight) dropY = ofGetHeight() - statusBarHeight - totalH;

    // Check if inside menu area
    if (x >= dropX && x <= dropX + dropW && y >= dropY && y <= dropY + totalH) {
        float iy = dropY + headerH;
        for (auto& it : items) {
            if (it.separator) iy += 10;
            if (y >= iy && y < iy + itemH && it.action != CTX_NONE) {
                contextMenuOpen = false;
                switch (it.action) {
                    case CTX_MAP:
                        // Open mapping mode for this screen
                        scene.selectOnly(contextScreenIndex);
                        updatePropertiesForSelection();
                        mappingMode = true;
                        cam.disableMouseInput();
                        break;
                    case CTX_DUPLICATE: {
                        // Duplicate screen via JSON round-trip
                        if (screen) {
                            pushUndo();
                            ofJson j = screen->toJson();
                            auto dup = std::make_unique<ScreenObject>();
                            dup->fromJson(j);
                            dup->name = screen->name + " Copy";
                            // Offset position slightly
                            glm::vec3 pos = dup->getPosition();
                            pos.x += 50;
                            pos.y -= 50;
                            dup->setPosition(pos);
                            // Reconnect source if any
                            if (!dup->sourceName.empty()) {
                                scene.screens.push_back(std::move(dup));
                                int newIdx = (int)scene.screens.size() - 1;
                                // Try to reconnect by finding source index
                                auto srvs = scene.getAvailableServers();
                                for (int si = 0; si < (int)srvs.size(); si++) {
                                    if (srvs[si].displayName() == scene.screens[newIdx]->sourceName
                                        || srvs[si].serverName == scene.screens[newIdx]->sourceName) {
                                        scene.assignSourceToScreen(newIdx, si);
                                        break;
                                    }
                                }
                                scene.selectOnly(newIdx);
                            } else {
                                scene.screens.push_back(std::move(dup));
                                int newIdx = (int)scene.screens.size() - 1;
                                scene.selectOnly(newIdx);
                            }
                            updatePropertiesForSelection();
                        }
                        break;
                    }
                    case CTX_DISCONNECT:
                        if (screen) {
                            pushUndo();
                            screen->disconnectSource();
                        }
                        updatePropertiesForSelection();
                        break;
                    default:
                        // Assign server (action = server index)
                        if (it.action >= 0) {
                            pushUndo();
                            scene.assignSourceToScreen(contextScreenIndex, it.action);
                            updatePropertiesForSelection();
                        }
                        break;
                }
                return true;
            }
            iy += itemH;
        }
    }

    // Clicked outside
    contextMenuOpen = false;
    return true;
}

// --- New Project ---

void ofApp::newProject() {
    pushUndo();
    // Clear all screens and reset state
    while (scene.getScreenCount() > 0) {
        scene.removeScreen(0);
    }
    scene.clearSelection();
    propertiesPanel.setTarget(nullptr);
    currentProjectPath = "";
    currentCloudProjectName = "";
    autosaveEnabled = false;
    autosaveTimer = 0;

    // Add a default screen
    scene.addScreen("Screen 1");

    // Reset camera
    cam.setDistance(800);
    cam.setTarget(glm::vec3(0, 100, 0));

    // Reset undo for fresh project
    undoManager.clear();
    undoManager.pushState(scene);

    ofLogNotice("ofApp") << "New project created";
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

void ofApp::doAutosave() {
    if (!currentCloudProjectName.empty()) {
        // Cloud autosave — serialize and upload silently
        ofJson camJson;
        auto camPos = cam.getPosition();
        auto camTgt = cam.getTarget().getPosition();
        camJson["position"] = {camPos.x, camPos.y, camPos.z};
        camJson["target"]   = {camTgt.x, camTgt.y, camTgt.z};
        camJson["distance"] = cam.getDistance();

        std::string tmpPath = ofFilePath::getUserHomeDir() + "/.virtualstage/autosave_tmp.json";
        if (scene.saveProject(tmpPath, camJson)) {
            ofBuffer buf = ofBufferFromFile(tmpPath);
            ofFile::removeFile(tmpPath);
            if (buf.size() > 0) {
                try {
                    ofJson data = ofJson::parse(buf.getText());
                    std::string name = currentCloudProjectName;
                    std::thread([this, data, name]() {
                        std::string err;
                        cloudStorage.saveProject(authManager.getSession(), data, name, err);
                    }).detach();
                } catch (...) {}
            }
        }
    } else if (!currentProjectPath.empty()) {
        // Local autosave
        saveProject(false);
    }
}

void ofApp::openProject() {
    auto result = ofSystemLoadDialog("Open VirtualStage Project", false, "");
    if (!result.bSuccess) return;

    ofJson camJson;
    if (scene.loadProject(result.filePath, &camJson)) {
        currentProjectPath = result.filePath;
        currentCloudProjectName = ""; // local project
        autosaveEnabled = true;
        autosaveTimer = 0;

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
        scene.clearSelection();
        propertiesPanel.setTarget(nullptr);
        refreshServerList();

        // Reset undo for loaded project
        undoManager.clear();
        undoManager.pushState(scene);

        ofLogNotice("ofApp") << "Project loaded: " << result.filePath;
    } else {
        ofLogError("ofApp") << "Failed to load project: " << result.filePath;
    }
}

void ofApp::keyPressed(int key) {
    // Auth modal intercepts all keys while visible
    if (authModal.isVisible()) {
        authModal.keyPressed(key);
        return;
    }

    // Settings modal intercepts keys while visible
    if (settingsModal.isVisible()) {
        settingsModal.keyPressed(key);
        return;
    }

    // Cloud load modal: ESC to close
    if (cloudLoadState != CloudLoadState::Hidden) {
        if (key == OF_KEY_ESC) cloudLoadState = CloudLoadState::Hidden;
        return;
    }

    // Close menus on any key press
    if (fileMenuOpen || viewMenuOpen || linkMenuOpen || helpMenuOpen || contextMenuOpen) {
        fileMenuOpen = false;
        viewMenuOpen = false;
        linkMenuOpen = false;
        helpMenuOpen = false;
        contextMenuOpen = false;
        if (key == OF_KEY_ESC) return; // ESC just closes the menu
    }

    // Close About dialog on any key
    if (showAboutDialog) {
        showAboutDialog = false;
        return;
    }

    // Close update modal on Escape (except while downloading)
    if (showUpdateModal && updateState != UpdateState::Downloading) {
        if (key == OF_KEY_ESC || updateState != UpdateState::Checking) {
            showUpdateModal = false;
            updateState = UpdateState::Idle;
            return;
        }
    }

    // --- Mapping mode keys ---
    if (mappingMode) {
        if (key == 'm' || key == 'M' || key == OF_KEY_ESC) {
            mappingMode = false;
            cam.enableMouseInput();
        } else if (key == 's' || key == 'S') {
            mapSnapEnabled = !mapSnapEnabled;
        } else if (key == 'r' || key == 'R') {
            // Reset crop to full
            auto* screen = scene.getScreen(scene.getPrimarySelected());
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
        if (key == 'z' || key == 'Z') {
            if (ofGetKeyPressed(OF_KEY_SHIFT)) {
                if (undoManager.redo(scene)) {
                    updatePropertiesForSelection();
                    ofLogNotice("ofApp") << "Redo";
                }
            } else {
                if (undoManager.undo(scene)) {
                    updatePropertiesForSelection();
                    ofLogNotice("ofApp") << "Undo";
                }
            }
            return;
        }
    }
#else
    {
        // On Windows/GLFW, Ctrl+letter may arrive as a control character
        // (Ctrl+S=19, Ctrl+O=15, Ctrl+Z=26) instead of 's'/'S'. Check both approaches.
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
        if ((ctrlHeld && (key == 'z' || key == 'Z')) || key == 26) {
            if (ofGetKeyPressed(OF_KEY_SHIFT)) {
                if (undoManager.redo(scene)) {
                    updatePropertiesForSelection();
                    ofLogNotice("ofApp") << "Redo";
                }
            } else {
                if (undoManager.undo(scene)) {
                    updatePropertiesForSelection();
                    ofLogNotice("ofApp") << "Undo";
                }
            }
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
            // Respect camera lock state
            if (cameraLocked) {
                cam.disableMouseInput();
            } else {
                cam.enableMouseInput();
            }
        } else {
            // Designer mode: always allow camera
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
            pushUndo();
            int idx = scene.addScreen();
            scene.selectOnly(idx);
            updatePropertiesForSelection();
            break;
        }
        case OF_KEY_DEL: case OF_KEY_BACKSPACE:
            if (scene.getSelectionCount() > 0) {
                pushUndo();
                // Delete all selected in reverse order to keep indices valid
                auto indices = scene.getSelectedIndicesSorted();
                for (int i = (int)indices.size() - 1; i >= 0; i--) {
                    scene.removeScreen(indices[i]);
                }
                scene.clearSelection();
                propertiesPanel.setTarget(nullptr);
            }
            break;

        case 'w':
            gizmo.mode = Gizmo::Mode::Translate;
            if (selectMode) {
                selectMode = false;
                cam.enableMouseInput();
                glfwSetCursor(static_cast<ofAppGLFWWindow*>(ofGetWindowPtr())->getGLFWWindow(), nullptr);
            }
            break;
        case 'e':
            gizmo.mode = Gizmo::Mode::Rotate;
            if (selectMode) {
                selectMode = false;
                cam.enableMouseInput();
                glfwSetCursor(static_cast<ofAppGLFWWindow*>(ofGetWindowPtr())->getGLFWWindow(), nullptr);
            }
            break;
        case 'r':
            gizmo.mode = Gizmo::Mode::Scale;
            if (selectMode) {
                selectMode = false;
                cam.enableMouseInput();
                glfwSetCursor(static_cast<ofAppGLFWWindow*>(ofGetWindowPtr())->getGLFWWindow(), nullptr);
            }
            break;

        case 's': {
            selectMode = !selectMode;
            auto* win = static_cast<ofAppGLFWWindow*>(ofGetWindowPtr())->getGLFWWindow();
            if (selectMode) {
                cam.disableMouseInput();
                glfwSetCursor(win, crosshairCursor);
            } else {
                glfwSetCursor(win, nullptr);
                if (!(appMode == AppMode::View && cameraLocked)) {
                    cam.enableMouseInput();
                }
            }
            break;
        }

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
            if (scene.getPrimarySelected() >= 0 && scene.getScreen(scene.getPrimarySelected())) {
                // Mapping mode works on primary selected only
                scene.selectOnly(scene.getPrimarySelected());
                updatePropertiesForSelection();
                mappingMode = true;
                cam.disableMouseInput();
            }
            break;

        case 'd': case 'D':
            // Disconnect source from all selected screens
            if (scene.getSelectionCount() > 0) {
                pushUndo();
                for (int si : scene.getSelectedIndicesSorted()) {
                    auto* screen = scene.getScreen(si);
                    if (screen) screen->disconnectSource();
                }
                updatePropertiesForSelection();
            }
            break;

        default:
            // 1-9: assign server to all selected screens
            if (key >= '1' && key <= '9') {
                int serverIdx = key - '1';
                if (scene.getSelectionCount() > 0 && serverIdx < (int)servers.size()) {
                    pushUndo();
                    for (int si : scene.getSelectedIndicesSorted()) {
                        scene.assignSourceToScreen(si, serverIdx);
                    }
                    updatePropertiesForSelection();
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
    pushUndo();
    while (scene.getScreenCount() > 0) {
        scene.removeScreen(0);
    }
    scene.clearSelection();
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
    auto* screen = scene.getScreen(scene.getPrimarySelected());
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
    // Auth modal intercepts all clicks while visible
    if (authModal.isVisible()) {
        authModal.mousePressed(x, y);
        return;
    }

    // Settings modal
    if (settingsModal.isVisible()) {
        settingsModal.mousePressed(x, y);
        return;
    }

    // Cloud load modal
    if (cloudLoadState != CloudLoadState::Hidden) {
        handleCloudLoadModalClick(x, y);
        return;
    }

    // Close About dialog on any click
    if (showAboutDialog) {
        showAboutDialog = false;
        return;
    }

    // Update modal click handling
    if (showUpdateModal) {
        if (updateState == UpdateState::Available) {
            startDownloadAndUpdate();
        } else if (updateState != UpdateState::Checking && updateState != UpdateState::Downloading) {
            showUpdateModal = false;
            updateState = UpdateState::Idle;
        }
        return;
    }

    // Right-click: edit parameter values or context menu on screens
    if (button == OF_MOUSE_BUTTON_RIGHT) {
        // Properties panel: right-click to type a value
        if (propertiesPanel.handleRightClick(x, y)) return;

        if (contextMenuOpen) {
            contextMenuOpen = false;
            return;
        }
        if (appMode == AppMode::Designer) {
            int hit = scene.pick(cam, glm::vec2(x, y));
            if (hit >= 0) {
                contextScreenIndex = hit;
                scene.selectOnly(hit);
                updatePropertiesForSelection();
                contextMenuPos = glm::vec2(x, y);
                contextMenuOpen = true;
            }
        }
        return;
    }

    // Middle-click: show hand cursor for panning
    if (button == OF_MOUSE_BUTTON_MIDDLE) {
        middleMouseDown = true;
        auto* win = static_cast<ofAppGLFWWindow*>(ofGetWindowPtr())->getGLFWWindow();
        glfwSetCursor(win, handCursor);
        return;
    }

    if (button != OF_MOUSE_BUTTON_LEFT) return;

    // Context menu click
    if (handleContextMenuClick(x, y)) return;

    // Menu bar always takes priority
    if (handleMenuClick(x, y)) return;

    // Camera lock button (View mode)
    if (appMode == AppMode::View) {
        float btnSize = 30;
        float btnX = ofGetWidth() - btnSize - 12;
        float btnY = menuBarHeight + 10;
        if (x >= btnX && x <= btnX + btnSize && y >= btnY && y <= btnY + btnSize) {
            cameraLocked = !cameraLocked;
            if (cameraLocked) {
                cam.disableMouseInput();
            } else {
                cam.enableMouseInput();
            }
            return;
        }
    }

    // --- Mapping mode mouse ---
    if (mappingMode) {
        auto* screen = scene.getScreen(scene.getPrimarySelected());
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

    // Sidebar click handling (select screen, delete, assign server)
    if (showUI && handleSidebarClick(x, y)) {
        return;
    }

    // Check gizmo hit first (use primary selected for gizmo reference)
    if (scene.getPrimarySelected() >= 0) {
        ScreenObject* primary = scene.getScreen(scene.getPrimarySelected());
        if (primary && gizmo.hitTest(cam, glm::vec2(x, y), *primary)) {
            cam.disableMouseInput();
            gizmoInteracting = true;
            pushUndo();
            // Collect all selected screens as targets
            std::vector<ScreenObject*> targets;
            for (int si : scene.getSelectedIndicesSorted()) {
                auto* s = scene.getScreen(si);
                if (s) targets.push_back(s);
            }
            gizmo.beginDrag(glm::vec2(x, y), cam, *primary, targets);
            return;
        }
    }

    // Pick objects in scene
    int hit = scene.pick(cam, glm::vec2(x, y));
#ifdef TARGET_OSX
    bool multiKey = ofGetKeyPressed(OF_KEY_SUPER);
#else
    bool multiKey = ofGetKeyPressed(OF_KEY_CONTROL);
#endif
    if (hit >= 0) {
        if (multiKey) {
            scene.toggleSelected(hit);
        } else {
            scene.selectOnly(hit);
        }
        updatePropertiesForSelection();
    } else {
        if (!multiKey) {
            if (selectMode) {
                // Select mode: start box selection
                cam.disableMouseInput();
                boxSelecting = true;
                boxSelectStart = boxSelectEnd = glm::vec2(x, y);
                auto* win = static_cast<ofAppGLFWWindow*>(ofGetWindowPtr())->getGLFWWindow();
                glfwSetCursor(win, crosshairCursor);
            } else {
                // Normal mode: clear selection, let camera orbit
                scene.clearSelection();
                updatePropertiesForSelection();
            }
        }
    }
}

void ofApp::mouseDragged(int x, int y, int button) {
    if (authModal.isVisible() || cloudLoadState != CloudLoadState::Hidden) return;

    // --- Mapping mode drag ---
    if (mappingMode && mapDrag != MapDrag::None) {
        auto* screen = scene.getScreen(scene.getPrimarySelected());
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

    // Box selection drag
    if (boxSelecting) {
        boxSelectEnd = glm::vec2(x, y);
        return;
    }

    if (gizmoInteracting) {
        gizmo.updateDrag(glm::vec2(x, y), cam);
        propertiesPanel.syncFromTarget();
    }
}

void ofApp::mouseReleased(int x, int y, int button) {
    if (authModal.isVisible() || cloudLoadState != CloudLoadState::Hidden) return;

    // Restore cursor when middle-click released
    if (button == OF_MOUSE_BUTTON_MIDDLE && middleMouseDown) {
        middleMouseDown = false;
        auto* win = static_cast<ofAppGLFWWindow*>(ofGetWindowPtr())->getGLFWWindow();
        glfwSetCursor(win, nullptr);
    }

    // Box selection release
    if (button == OF_MOUSE_BUTTON_LEFT && boxSelecting) {
        boxSelecting = false;

        ofRectangle r(boxSelectStart.x, boxSelectStart.y,
                      boxSelectEnd.x - boxSelectStart.x,
                      boxSelectEnd.y - boxSelectStart.y);
        r.standardize();

        if (r.getArea() < 25.0f) {
            // Tiny drag = click on empty space → clear selection
            scene.clearSelection();
        } else {
            scene.selectInRect(cam, r);
        }

        // Stay in select mode — keep crosshair and camera disabled
        if (!selectMode) {
            auto* win = static_cast<ofAppGLFWWindow*>(ofGetWindowPtr())->getGLFWWindow();
            glfwSetCursor(win, nullptr);
            if (!(appMode == AppMode::View && cameraLocked)) {
                cam.enableMouseInput();
            }
        }
        updatePropertiesForSelection();
        return;
    }

    if (mappingMode) {
        mapDrag = MapDrag::None;
        return;
    }
    if (gizmoInteracting) {
        gizmo.endDrag();
        gizmoInteracting = false;
    }
    // Only re-enable camera if not locked in View mode
    if (!(appMode == AppMode::View && cameraLocked)) {
        cam.enableMouseInput();
    }
}

void ofApp::windowResized(int w, int h) {
    propertiesPanel.setPosition(w - 240, 10);
}

// --- Helper methods ---

void ofApp::pushUndo() {
    undoManager.pushState(scene);
}

void ofApp::updatePropertiesForSelection() {
    int count = scene.getSelectionCount();
    if (count == 0) {
        propertiesPanel.setTarget(nullptr);
    } else if (count == 1) {
        propertiesPanel.setTarget(scene.getScreen(scene.getPrimarySelected()));
    } else {
        std::vector<ScreenObject*> targets;
        for (int idx : scene.getSelectedIndicesSorted()) {
            auto* s = scene.getScreen(idx);
            if (s) targets.push_back(s);
        }
        propertiesPanel.setMultipleTargets(targets);
    }
}

// --- Update Checking ---

static int compareVersions(const std::string& a, const std::string& b) {
    int a1 = 0, a2 = 0, a3 = 0;
    int b1 = 0, b2 = 0, b3 = 0;
    sscanf(a.c_str(), "%d.%d.%d", &a1, &a2, &a3);
    sscanf(b.c_str(), "%d.%d.%d", &b1, &b2, &b3);
    if (a1 != b1) return a1 - b1;
    if (a2 != b2) return a2 - b2;
    return a3 - b3;
}

void ofApp::checkForUpdates() {
    if (updateState == UpdateState::Checking || updateState == UpdateState::Downloading) return;
    updateState = UpdateState::Checking;
    updateErrorDetail = "";
    showUpdateModal = true;

    std::thread([this]() {
        // Write response to temp file using system-level HTTP tools
        // (ofURLFileLoader local instance doesn't initialize curl properly on Windows)
        std::string tmpPath = ofFilePath::join(ofFilePath::getUserHomeDir(), ".virtualstage_update_check.json");
        int ret = -1;
#ifdef TARGET_OSX
        std::string cmd = "curl -s -H \"User-Agent: VirtualStage/" APP_VERSION "\" "
                          "-H \"Accept: application/vnd.github.v3+json\" "
                          "\"https://api.github.com/repos/gonzaloventura/virtualstage/releases/latest\" "
                          "-o \"" + tmpPath + "\"";
        ret = system(cmd.c_str());
#elif defined(TARGET_WIN32)
        std::string cmd = "powershell -NoProfile -Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
                          "Invoke-RestMethod -Uri 'https://api.github.com/repos/gonzaloventura/virtualstage/releases/latest' "
                          "-Headers @{'User-Agent'='VirtualStage/" APP_VERSION "'} "
                          "| ConvertTo-Json -Depth 10 | Out-File -Encoding utf8 '" + tmpPath + "'\"";
        ret = system(cmd.c_str());
#endif
        if (ret != 0) {
            updateState = UpdateState::Error;
            updateErrorDetail = "Could not reach GitHub";
            return;
        }

        // Read the temp file
        ofFile f(tmpPath);
        if (!f.exists()) {
            updateState = UpdateState::Error;
            updateErrorDetail = "No response received";
            return;
        }
        ofBuffer buf = ofBufferFromFile(tmpPath);
        std::string body = buf.getText();
        ofFile::removeFile(tmpPath);

        if (body.empty()) {
            updateState = UpdateState::Error;
            updateErrorDetail = "Empty response";
            return;
        }

        try {
            ofJson json = ofJson::parse(body);

            std::string tag = json.value("tag_name", "");
            if (tag.empty()) {
                updateState = UpdateState::Error;
                updateErrorDetail = "No release tag found";
                return;
            }
            latestVersion = tag;
            if (latestVersion[0] == 'v' || latestVersion[0] == 'V') {
                latestVersion = latestVersion.substr(1);
            }

            // Find platform-specific asset download URL
            latestDownloadUrl = "";
#ifdef TARGET_OSX
            std::string assetKeyword = "macOS";
#elif defined(TARGET_WIN32)
            std::string assetKeyword = "Windows";
#else
            std::string assetKeyword = "";
#endif
            if (json.contains("assets") && json["assets"].is_array()) {
                for (auto& asset : json["assets"]) {
                    std::string name = asset.value("name", "");
                    if (!assetKeyword.empty() && name.find(assetKeyword) != std::string::npos) {
                        latestDownloadUrl = asset.value("browser_download_url", "");
                        break;
                    }
                }
            }

            // Compare versions (strip -beta etc. for numeric comparison)
            std::string cleanLatest = latestVersion;
            std::string cleanCurrent = APP_VERSION;
            auto stripSuffix = [](std::string& v) {
                auto pos = v.find('-');
                if (pos != std::string::npos) v = v.substr(0, pos);
            };
            stripSuffix(cleanLatest);
            stripSuffix(cleanCurrent);

            if (compareVersions(cleanLatest, cleanCurrent) > 0) {
                updateState = UpdateState::Available;
            } else {
                updateState = UpdateState::UpToDate;
            }
        } catch (const std::exception& e) {
            updateState = UpdateState::Error;
            updateErrorDetail = "Could not parse response";
            ofLogError("Update") << "JSON parse error: " << e.what();
        }
    }).detach();
}

void ofApp::startDownloadAndUpdate() {
    if (latestDownloadUrl.empty()) {
        ofLaunchBrowser("https://github.com/gonzaloventura/virtualstage/releases/latest");
        showUpdateModal = false;
        updateState = UpdateState::Idle;
        return;
    }

    updateState = UpdateState::Downloading;

    std::string updateDir = ofFilePath::getUserHomeDir() + "/.virtualstage_update";
    ofDirectory dir(updateDir);
    if (!dir.exists()) dir.create(true);

#ifdef TARGET_OSX
    std::string zipName = "VirtualStage-macOS.zip";
#elif defined(TARGET_WIN32)
    std::string zipName = "VirtualStage-Windows.zip";
#else
    std::string zipName = "VirtualStage-update.zip";
#endif

    updateZipPath = updateDir + "/" + zipName;
    std::string url = latestDownloadUrl;
    std::string dest = updateZipPath;
    std::thread([this, url, dest]() {
        auto response = ofSaveURLTo(url, dest);
        if (response.status == 200) {
            ofLogNotice("Update") << "Download complete: " << dest;
            launchUpdaterAndExit();
        } else {
            updateState = UpdateState::Error;
            updateErrorDetail = "Download failed (HTTP " + ofToString(response.status) + ")";
            ofLogError("Update") << "Download failed: HTTP " << response.status;
        }
    }).detach();
}

void ofApp::launchUpdaterAndExit() {
#ifdef TARGET_OSX
    std::string exeDir = ofFilePath::getCurrentExeDir();
    std::string macosDir = exeDir;
    std::string contentsDir = ofFilePath::getEnclosingDirectory(macosDir);
    std::string appPath = ofFilePath::getEnclosingDirectory(contentsDir);
    if (!appPath.empty() && appPath.back() == '/') {
        appPath.pop_back();
    }

    std::string scriptPath = ofToDataPath("update_macos.sh", true);
    std::string pid = ofToString(getpid());

    std::string cmd = "bash \"" + scriptPath + "\" \"" + updateZipPath + "\" \"" + appPath + "\" " + pid + " &";
    ofLogNotice("Update") << "Launching updater: " << cmd;
    system(cmd.c_str());
    ofExit();

#elif defined(TARGET_WIN32)
    std::string exeDir = ofFilePath::getCurrentExeDir();
    std::string scriptPath = ofToDataPath("update_windows.bat", true);
    std::string pid = ofToString(GetCurrentProcessId());

    std::string cmd = "start \"\" cmd /c \"" + scriptPath + "\" \"" + updateZipPath + "\" \"" + exeDir + "\" " + pid;
    ofLogNotice("Update") << "Launching updater: " << cmd;
    system(cmd.c_str());
    ofExit();

#else
    ofLaunchBrowser("https://github.com/gonzaloventura/virtualstage/releases/latest");
    showUpdateModal = false;
    updateState = UpdateState::Idle;
#endif
}

// --- Update Modal ---

void ofApp::drawUpdateModal() {
    float w = ofGetWidth();
    float h = ofGetHeight();

    // Dim background
    ofSetColor(0, 0, 0, 160);
    ofDrawRectangle(0, 0, w, h);

    float panelW = 360;
    float panelH = 170;
    float px = (w - panelW) / 2;
    float py = (h - panelH) / 2;

    // Shadow + background
    ofSetColor(0, 0, 0, 80);
    ofDrawRectangle(px + 4, py + 4, panelW, panelH);
    ofSetColor(40, 40, 40);
    ofDrawRectangle(px, py, panelW, panelH);

    // Border color by state
    switch (updateState) {
        case UpdateState::Checking:
        case UpdateState::Downloading:
            ofSetColor(255, 200, 0); break;
        case UpdateState::Available:
            ofSetColor(100, 220, 100); break;
        case UpdateState::Error:
            ofSetColor(255, 80, 80); break;
        default:
            ofSetColor(0, 120, 200); break;
    }
    ofNoFill();
    ofSetLineWidth(2);
    ofDrawRectangle(px, py, panelW, panelH);
    ofFill();
    ofSetLineWidth(1);

    float cy = py + 35;

    if (updateState == UpdateState::Checking) {
        ofSetColor(255, 200, 0);
        ofDrawBitmapString("Checking for updates...", px + panelW / 2 - 92, cy);

        // Animated dots
        int dots = ((int)(ofGetElapsedTimef() * 3)) % 4;
        std::string dotsStr(dots, '.');
        ofDrawBitmapString(dotsStr, px + panelW / 2 + 96, cy);

        ofSetColor(120);
        ofDrawBitmapString("Please wait", px + panelW / 2 - 44, cy + 30);

    } else if (updateState == UpdateState::UpToDate) {
        ofSetColor(0, 200, 255);
        std::string title = "You're up to date!";
        ofDrawBitmapString(title, px + panelW / 2 - (title.length() * 8) / 2, cy);

        ofSetColor(180);
        std::string ver = "Current version: v" APP_VERSION;
        ofDrawBitmapString(ver, px + panelW / 2 - (ver.length() * 8) / 2, cy + 30);

        ofSetColor(100);
        ofDrawBitmapString("Click anywhere to close", px + panelW / 2 - 92, py + panelH - 15);

    } else if (updateState == UpdateState::Available) {
        ofSetColor(100, 220, 100);
        std::string title = "Update available!";
        ofDrawBitmapString(title, px + panelW / 2 - (title.length() * 8) / 2, cy);

        ofSetColor(180);
        std::string from = "Current:  v" APP_VERSION;
        std::string to =   "Latest:   v" + latestVersion;
        ofDrawBitmapString(from, px + 60, cy + 30);
        ofSetColor(100, 220, 100);
        ofDrawBitmapString(to, px + 60, cy + 50);

        ofSetColor(0, 200, 255);
        ofDrawBitmapString("Click to download  |  Esc to close", px + panelW / 2 - 140, py + panelH - 15);

    } else if (updateState == UpdateState::Downloading) {
        ofSetColor(255, 200, 0);
        ofDrawBitmapString("Downloading update...", px + panelW / 2 - 84, cy);

        int dots = ((int)(ofGetElapsedTimef() * 3)) % 4;
        std::string dotsStr(dots, '.');
        ofDrawBitmapString(dotsStr, px + panelW / 2 + 84, cy);

        ofSetColor(120);
        ofDrawBitmapString("Please wait, do not close the app", px + panelW / 2 - 132, cy + 30);

    } else if (updateState == UpdateState::Error) {
        ofSetColor(255, 80, 80);
        std::string title = "Could not check for updates";
        ofDrawBitmapString(title, px + panelW / 2 - (title.length() * 8) / 2, cy);

        if (!updateErrorDetail.empty()) {
            ofSetColor(150);
            ofDrawBitmapString(updateErrorDetail, px + panelW / 2 - (updateErrorDetail.length() * 8) / 2, cy + 30);
        }

        ofSetColor(100);
        ofDrawBitmapString("Click anywhere to close", px + panelW / 2 - 92, py + panelH - 15);
    }

    ofSetColor(255);
}

// --- About Dialog ---

void ofApp::drawAboutDialog() {
    float w = ofGetWidth();
    float h = ofGetHeight();

    // Dim background
    ofSetColor(0, 0, 0, 160);
    ofDrawRectangle(0, 0, w, h);

    // Panel
    float panelW = 340;
    float panelH = 195;
    float px = (w - panelW) / 2;
    float py = (h - panelH) / 2;

    // Shadow
    ofSetColor(0, 0, 0, 80);
    ofDrawRectangle(px + 4, py + 4, panelW, panelH);

    // Background
    ofSetColor(40, 40, 40);
    ofDrawRectangle(px, py, panelW, panelH);

    // Border
    ofSetColor(0, 120, 200);
    ofNoFill();
    ofSetLineWidth(2);
    ofDrawRectangle(px, py, panelW, panelH);
    ofFill();
    ofSetLineWidth(1);

    // Title
    ofSetColor(0, 200, 255);
    ofDrawBitmapString("VirtualStage", px + panelW / 2 - 48, py + 35);

    // Version
    ofSetColor(180);
    std::string verStr = "v" APP_VERSION;
    ofDrawBitmapString(verStr, px + panelW / 2 - (verStr.length() * 8) / 2, py + 55);

    // Separator
    ofSetColor(80);
    ofDrawLine(px + 20, py + 68, px + panelW - 20, py + 68);

    // Description
    ofSetColor(200);
    ofDrawBitmapString("3D virtual screen layout tool", px + 30, py + 90);
    ofDrawBitmapString("for stage design.", px + 30, py + 108);

    ofSetColor(140);
    ofDrawBitmapString("Built by Gonzalo Ventura", px + 30, py + 135);
    ofSetColor(0, 180, 255);
    ofDrawBitmapString("Ventu.dev", px + 30, py + 153);

    // Close hint
    ofSetColor(100);
    ofDrawBitmapString("Click anywhere to close", px + panelW / 2 - 92, py + panelH - 10);

    ofSetColor(255);
}

// ══════════════════════════════════════════════════════════════════════════════
// AUTH
// ══════════════════════════════════════════════════════════════════════════════

void ofApp::handleAuthSubmit(AuthModal::Tab tab,
                              const std::string& email,
                              const std::string& password,
                              const std::string& confirm) {
    // Basic client-side validation
    if (email.empty() || password.empty()) {
        authModal.setError("Email and password are required.");
        return;
    }
    if (tab == AuthModal::Tab::Register && password != confirm) {
        authModal.setError("Passwords do not match.");
        return;
    }
    if (password.size() < 6) {
        authModal.setError("Password must be at least 6 characters.");
        return;
    }

    // Run auth in background thread to avoid freezing the render loop
    std::thread([this, tab, email, password]() {
        std::string err;
        bool success     = false;
        bool needConfirm = false;

        if (tab == AuthModal::Tab::Login) {
            success = authManager.login(email, password, err);
        } else {
            success = authManager.signup(email, password, err, needConfirm);
        }

        std::lock_guard<std::mutex> lock(authResultMutex);
        pendingAuthResult.done        = true;
        pendingAuthResult.success     = success;
        pendingAuthResult.needConfirm = needConfirm;
        pendingAuthResult.error       = err;
    }).detach();
}

// ══════════════════════════════════════════════════════════════════════════════
// CLOUD STORAGE
// ══════════════════════════════════════════════════════════════════════════════

void ofApp::saveToCloud() {
    if (!authManager.isAuthenticated()) {
        authModal.show();
        return;
    }

    // Determine project name
    std::string name;
    if (!currentCloudProjectName.empty()) {
        name = currentCloudProjectName; // already a cloud project
    } else if (!currentProjectPath.empty()) {
        name = ofFilePath::getBaseName(currentProjectPath);
    } else {
        std::string result = ofSystemTextBoxDialog("Cloud project name:", "Untitled");
        if (result.empty()) return;
        name = result;
    }

    // Serialize current project to JSON
    ofJson camJson;
    auto camPos = cam.getPosition();
    auto camTgt = cam.getTarget().getPosition();
    camJson["position"] = {camPos.x, camPos.y, camPos.z};
    camJson["target"]   = {camTgt.x, camTgt.y, camTgt.z};
    camJson["distance"] = cam.getDistance();

    // Write to a temp file, then read back (reuse existing serializer)
    std::string tmpPath = ofFilePath::getUserHomeDir() + "/.virtualstage/cloud_save_tmp.json";
    if (!scene.saveProject(tmpPath, camJson)) {
        ofSystemAlertDialog("Failed to serialize project.");
        return;
    }
    ofBuffer buf = ofBufferFromFile(tmpPath);
    ofFile::removeFile(tmpPath);
    if (buf.size() == 0) {
        ofSystemAlertDialog("Failed to read project for upload.");
        return;
    }
    ofJson projectData;
    try {
        projectData = ofJson::parse(buf.getText());
    } catch (...) {
        ofSystemAlertDialog("Failed to parse project JSON.");
        return;
    }

    currentCloudProjectName = name;

    // Upload in a background thread
    std::thread([this, projectData, name]() {
        std::string err;
        if (!cloudStorage.saveProject(authManager.getSession(), projectData, name, err)) {
            ofLogError("CloudStorage") << "Save failed: " << err;
        }
    }).detach();
}

void ofApp::loadFromCloud() {
    if (!authManager.isAuthenticated()) {
        authModal.show();
        return;
    }

    cloudLoadState = CloudLoadState::Loading;
    cloudProjects.clear();
    cloudLoadError.clear();

    std::thread([this]() {
        std::string err;
        std::vector<CloudStorage::CloudProject> projects;
        if (cloudStorage.listProjects(authManager.getSession(), projects, err)) {
            // Store in member on main thread via flag
            // Direct assignment is safe here since cloudLoadState == Loading
            // and the main thread only reads these when state == Loaded/Error
            cloudProjects  = projects;
            cloudLoadState = CloudLoadState::Loaded;
        } else {
            cloudLoadError = err;
            cloudLoadState = CloudLoadState::Error;
        }
    }).detach();
}

void ofApp::drawCloudLoadModal() {
    float W = ofGetWidth();
    float H = ofGetHeight();

    // Dim background
    ofSetColor(0, 0, 0, 160);
    ofDrawRectangle(0, 0, W, H);

    float panelW = 400;
    float panelH = 350;
    float px     = (W - panelW) / 2;
    float py     = (H - panelH) / 2;

    // Shadow + panel
    ofSetColor(0, 0, 0, 100);
    ofDrawRectangle(px + 5, py + 5, panelW, panelH);
    ofSetColor(38, 38, 38);
    ofDrawRectangle(px, py, panelW, panelH);

    // Border
    ofNoFill();
    ofSetLineWidth(2);
    ofSetColor(0, 120, 200);
    ofDrawRectangle(px, py, panelW, panelH);
    ofFill();
    ofSetLineWidth(1);

    // Title
    ofSetColor(0, 180, 255);
    ofDrawBitmapString("Load from Cloud", px + (panelW - 15 * 8) / 2, py + 28);

    // Separator
    ofSetColor(60);
    ofDrawLine(px + 10, py + 38, px + panelW - 10, py + 38);

    float listY = py + 48;

    if (cloudLoadState == CloudLoadState::Loading) {
        int dots = (int)(ofGetElapsedTimef() * 3) % 4;
        ofSetColor(180);
        ofDrawBitmapString("Loading" + std::string(dots, '.'), px + panelW / 2 - 30, py + panelH / 2);

    } else if (cloudLoadState == CloudLoadState::Error) {
        ofSetColor(255, 80, 80);
        ofDrawBitmapString("Error: " + cloudLoadError, px + 16, listY + 20);

    } else if (cloudLoadState == CloudLoadState::Loaded) {
        if (cloudProjects.empty()) {
            ofSetColor(120);
            ofDrawBitmapString("No saved projects found.", px + panelW / 2 - 96, py + panelH / 2);
        } else {
            // List items
            float itemH = 36;
            float mx    = ofGetMouseX();
            float my    = ofGetMouseY();
            for (int i = 0; i < (int)cloudProjects.size(); i++) {
                float iy = listY + i * itemH;
                if (iy + itemH > py + panelH - 50) break; // clip to panel

                bool hover = (mx >= px + 8 && mx <= px + panelW - 8 &&
                              my >= iy && my < iy + itemH);

                if (hover) {
                    ofSetColor(0, 80, 160);
                    ofDrawRectangle(px + 8, iy, panelW - 16, itemH - 2);
                }

                ofSetColor(hover ? 255 : 210);
                std::string nm = cloudProjects[i].name;
                if (nm.length() > 32) nm = nm.substr(0, 29) + "...";
                ofDrawBitmapString(nm, px + 16, iy + 14);

                // Updated date (trim to date only)
                std::string dt = cloudProjects[i].updatedAt;
                if (dt.length() > 10) dt = dt.substr(0, 10);
                ofSetColor(hover ? 200 : 100);
                ofDrawBitmapString(dt, px + panelW - 16 - dt.length() * 8, iy + 14);

                // Separator
                ofSetColor(50);
                ofDrawLine(px + 8, iy + itemH - 1, px + panelW - 8, iy + itemH - 1);
            }
        }
    }

    // Close hint
    ofSetColor(80);
    ofDrawBitmapString("Press Esc or click outside to close", px + (panelW - 36 * 8) / 2, py + panelH - 12);

    ofSetColor(255);
}

bool ofApp::handleCloudLoadModalClick(int x, int y) {
    float W = ofGetWidth();
    float H = ofGetHeight();
    float panelW = 400;
    float panelH = 350;
    float px     = (W - panelW) / 2;
    float py     = (H - panelH) / 2;

    // Click outside panel = close
    if (x < px || x > px + panelW || y < py || y > py + panelH) {
        cloudLoadState = CloudLoadState::Hidden;
        return true;
    }

    if (cloudLoadState != CloudLoadState::Loaded || cloudProjects.empty()) return true;

    float listY = py + 48;
    float itemH = 36;
    for (int i = 0; i < (int)cloudProjects.size(); i++) {
        float iy = listY + i * itemH;
        if (iy + itemH > py + panelH - 50) break;
        if (x >= px + 8 && x <= px + panelW - 8 &&
            y >= iy && y < iy + itemH) {
            // Load this project
            cloudLoadState = CloudLoadState::Loading;
            std::string projId   = cloudProjects[i].id;
            std::string projName = cloudProjects[i].name;
            std::thread([this, projId, projName]() {
                std::string err;
                ofJson data;
                bool ok = cloudStorage.loadProject(authManager.getSession(), projId, data, err);
                std::lock_guard<std::mutex> lock(cloudProjectResultMutex);
                pendingCloudProject.done    = true;
                pendingCloudProject.success = ok;
                pendingCloudProject.error   = err;
                pendingCloudProject.name    = projName;
                pendingCloudProject.data    = data;
            }).detach();
            return true;
        }
    }
    return true;
}
