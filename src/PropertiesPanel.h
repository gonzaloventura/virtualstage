#pragma once
#include "ofMain.h"
#include "ofxGui.h"
#include "ScreenObject.h"

class PropertiesPanel {
public:
    void setup(float x, float y);
    void setPosition(float x, float y);
    void draw();

    void setTarget(ScreenObject* target);
    void syncFromTarget();
    void syncToTarget();

    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

    // Group visibility (minimize/maximize)
    void setGroupVisible(const std::string& name, bool show);

    // Ambient light (0-100, 100=brightest)
    float getAmbientLight() const { return ambientLight; }

private:
    ofxPanel panel;

    ofParameter<float> ambientLight{"Ambient", 100, 0, 100};
    ofParameterGroup ambientGroup;

    ofParameter<float> posX{"X", 0, -2000, 2000};
    ofParameter<float> posY{"Y", 0, -2000, 2000};
    ofParameter<float> posZ{"Z", 0, -2000, 2000};
    ofParameter<float> rotX{"Pitch", 0, -180, 180};
    ofParameter<float> rotY{"Yaw", 0, -180, 180};
    ofParameter<float> rotZ{"Roll", 0, -180, 180};
    ofParameter<float> scaleX{"Scale X", 1, 0.1, 10};
    ofParameter<float> scaleY{"Scale Y", 1, 0.1, 10};

    ofParameter<float> curvatureParam{"Curvature", 0, -180, 180};

    ofParameter<float> cropX{"Crop X", 0, 0, 1};
    ofParameter<float> cropY{"Crop Y", 0, 0, 1};
    ofParameter<float> cropW{"Crop W", 1, 0, 1};
    ofParameter<float> cropH{"Crop H", 1, 0, 1};

    ofParameterGroup posGroup;
    ofParameterGroup rotGroup;
    ofParameterGroup scaleGroup;
    ofParameterGroup curvatureGroup;
    ofParameterGroup cropGroup;
    ofxLabel nameLabel;
    ofxLabel sourceLabel;

    ScreenObject* target = nullptr;
    bool visible = true;
    bool syncing = false;

    void onParamChanged(float& val);
};
