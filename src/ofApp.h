#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ofAppGLFWWindow.h"
#include "Scene.h"
#include "Gizmo.h"
#include "PropertiesPanel.h"

enum class AppMode { Designer, View };

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

    // Layout
    float statusBarHeight = 30.0f;
    float serverListWidth = 250.0f;

    // Interaction state
    bool gizmoInteracting = false;

    // Project save/load
    std::string currentProjectPath;
    void saveProject(bool saveAs = false);
    void openProject();

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
};
