#pragma once
#include "ofMain.h"
#include "ofxGui.h"
#include "ScreenObject.h"
#include "Preferences.h"
#include <functional>

class PropertiesPanel {
public:
    void setup(float x, float y);
    void setPosition(float x, float y);
    void draw();

    void setTarget(ScreenObject* target);
    void setMultipleTargets(const std::vector<ScreenObject*>& targets);
    void syncFromTarget();
    void syncToTarget();

    // Callback invoked when a property value changes (for undo capture)
    std::function<void()> onPropertyChanged;

    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

    // Group visibility (completely show/hide groups from panel)
    void updateGroupVisibility(bool ambient, bool pos, bool rot, bool scale, bool crop);

    // Preferences (unit conversion for width/height display)
    void setPreferences(Preferences* p) { preferences = p; }
    void refreshUnitLabels();

    // Ambient light (0-100, default 60)
    float getAmbientLight() const { return ambientLight; }

    // Right-click on a slider to type a value. Returns true if handled.
    bool handleRightClick(int x, int y);

private:
    ofxPanel panel; // header + labels only

    // Parameters
    ofParameter<float> ambientLight{"Ambient", 60, 0, 100};
    ofParameter<bool> ambientReset{"Reset to 60", false};

    ofParameter<float> posX{"X", 0, -2000, 2000};
    ofParameter<float> posY{"Y", 0, -2000, 2000};
    ofParameter<float> posZ{"Z", 0, -2000, 2000};
    ofParameter<float> rotX{"Pitch", 0, -180, 180};
    ofParameter<float> rotY{"Yaw", 0, -180, 180};
    ofParameter<float> rotZ{"Roll", 0, -180, 180};
    ofParameter<float> widthParam{"Width (m)", 3.2, 0.01, 100};
    ofParameter<float> heightParam{"Height (m)", 1.8, 0.01, 100};

    ofParameter<float> curvatureParam{"Curvature", 0, -180, 180};

    ofParameter<float> cropX{"Crop X", 0, 0, 1};
    ofParameter<float> cropY{"Crop Y", 0, 0, 1};
    ofParameter<float> cropW{"Crop W", 1, 0, 1};
    ofParameter<float> cropH{"Crop H", 1, 0, 1};

    // Standalone GUI groups (drawn manually, not inside panel)
    ofxGuiGroup ambientGui;
    ofxGuiGroup posGui;
    ofxGuiGroup rotGui;
    ofxGuiGroup sizeGui;
    ofxGuiGroup curvatureGui;
    ofxGuiGroup cropGui;

    ofxLabel nameLabel;
    ofxLabel sourceLabel;

    Preferences* preferences = nullptr;
    ScreenObject* target = nullptr;
    std::vector<ScreenObject*> multiTargets;
    bool visible = true;
    bool syncing = false;
    bool multiMode = false;
    int multiCount = 0;

    // Delta tracking for multi-target editing
    glm::vec3 lastPos;
    glm::vec3 lastRot;
    glm::vec2 lastSize;
    float lastCurvature = 0;

    // Group visibility flags
    bool visAmbient = false;
    bool visPos = true;
    bool visRot = true;
    bool visScale = true;
    bool visCrop = true;

    void onParamChanged(float& val);
    void onAmbientReset(bool& val);
    void captureLastValues();
    void syncToMultiTargets();
};
