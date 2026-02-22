#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ofAppGLFWWindow.h"
#include <GLFW/glfw3.h>
#include "Scene.h"
#include "Gizmo.h"
#include "PropertiesPanel.h"
#include "UndoManager.h"
#include "AppVersion.h"

enum class AppMode { Designer, View };
enum class UpdateState { Idle, Checking, Available, UpToDate, Error, Downloading };

class ofApp : public ofBaseApp {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void mousePressed(int x, int y, int button) override;
    void mouseDragged(int x, int y, int button) override;
    void mouseReleased(int x, int y, int button) override;
    void windowResized(int w, int h) override;

private:
    // Mode
    AppMode appMode = AppMode::Designer;

    // 3D
    ofEasyCam cam;
    Scene scene;
    Gizmo gizmo;

    // GUI
    PropertiesPanel propertiesPanel;
    bool showUI = true;

    // Server list cache
    std::vector<ServerInfo> servers;
    void drawServerList();
    void drawStatusBar();
    void drawToolbar();
    void refreshServerList();
    bool handleSidebarClick(int x, int y); // returns true if consumed
    void mouseScrolled(int x, int y, float scrollX, float scrollY) override;

    // Sidebar scroll
    float sidebarScroll = 0;
    float sidebarContentHeight = 0;

    // Menu bar
    float menuBarHeight = 25.0f;
    bool fileMenuOpen = false;
    bool viewMenuOpen = false;
    bool linkMenuOpen = false;
    bool helpMenuOpen = false;
    void drawMenuBar();
    bool handleMenuClick(int x, int y); // returns true if click was consumed

    // Background brightness (driven by ambient light slider, default=lightest)
    int bgBrightness = 60;

    // View menu toggle states
    bool showAmbientLight = false;
    bool showPosition = true;
    bool showRotation = true;
    bool showScale = true;
    bool showCrop = true;

    // Camera lock (View mode)
    bool cameraLocked = false;
    void drawCameraLock();

    // Right-click context menu
    bool contextMenuOpen = false;
    int contextScreenIndex = -1;
    glm::vec2 contextMenuPos;
    void drawContextMenu();
    bool handleContextMenuClick(int x, int y);

    // Layout
    float statusBarHeight = 30.0f;
    float serverListWidth = 250.0f;

    // Interaction state
    bool gizmoInteracting = false;

    // Cursor feedback for middle-click panning
    GLFWcursor* handCursor = nullptr;
    GLFWcursor* crosshairCursor = nullptr;
    bool middleMouseDown = false;

    // Select mode (S key toggle) — enables box selection with left-drag
    bool selectMode = false;

    // Box selection (left-drag on empty 3D space while in select mode)
    bool boxSelecting = false;
    glm::vec2 boxSelectStart;
    glm::vec2 boxSelectEnd;

    // Sidebar shift-click range selection
    int lastClickedSidebarIndex = -1;

    // Undo/redo
    UndoManager undoManager;
    void pushUndo();

    // Properties panel undo support
    bool propsDirty = false;
    float propsDirtyTimer = 0;

    // Helper: update properties panel based on current selection
    void updatePropertiesForSelection();

    // Project save/load
    std::string currentProjectPath;
    void saveProject(bool saveAs = false);
    void openProject();
    void newProject();

    // Autosave
    bool autosaveEnabled = false;
    float autosaveInterval = 15.0f;
    float autosaveTimer = 0.0f;

    // Resolume XML import
    enum class LinkState { None, Confirm, ChooseRect };
    LinkState linkState = LinkState::None;
    void loadResolumeXml(bool useInputRect);

    // Input mapping 2D editor
    bool mappingMode = false;
    enum class MapDrag { None, Move, TL, TR, BL, BR, Left, Right, Top, Bottom };
    MapDrag mapDrag = MapDrag::None;
    glm::vec2 mapDragStart;
    ofRectangle mapDragStartCrop;
    float mapSnapGrid = 0.05f; // snap increment (5%)
    bool mapSnapEnabled = true;
    float snapValue(float v) const;
    ofRectangle getMapPreviewArea() const;
    void drawMappingMode();

    // Update checking
    UpdateState updateState = UpdateState::Idle;
    std::string latestVersion;
    std::string latestDownloadUrl;
    std::string updateErrorDetail;
    bool showUpdateModal = false;
    std::string updateZipPath;
    void checkForUpdates();
    void startDownloadAndUpdate();
    void launchUpdaterAndExit();
    void drawUpdateModal();

    // About dialog
    bool showAboutDialog = false;
    void drawAboutDialog();
};
