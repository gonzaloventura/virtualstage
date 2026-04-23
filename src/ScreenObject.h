#pragma once
#include "ofMain.h"
#include <string>

#ifdef TARGET_OSX
#include "ofxSyphon.h"
#elif defined(TARGET_WIN32)
#include "ofxSpout.h"
#endif

class ScreenObject {
public:
    ScreenObject(const std::string& name = "Slice", float width = 320.0f, float height = 180.0f);

    std::string name;
    int groupId = -1;  // parent ScreenGroup ID
    ofPlanePrimitive plane;

    // Transform convenience
    void setPosition(const glm::vec3& pos);
    void setRotationEuler(const glm::vec3& eulerDeg);
    void setScale(const glm::vec3& s);
    glm::vec3 getPosition() const;
    glm::vec3 getRotationEuler() const;
    glm::vec3 getScale() const;

    // Curvature
    void setCurvature(float deg);
    float getCurvature() const;
    void setCurveExtent(float pct);   // 0-100%, portion of width that curves
    float getCurveExtent() const;

    // Input mapping (crop) - normalized 0-1
    void setCropRect(const ofRectangle& r);
    const ofRectangle& getCropRect() const;

    // Cabinet (LED panel spec) assignment — empty string = unassigned
    std::string cabinetId;

    // Original pixel dimensions (from Resolume import or similar). Used by the
    // cabinet auto-detect and to recompute physical size when a cabinet is
    // assigned. 0 = unknown.
    int sourcePxWidth  = 0;
    int sourcePxHeight = 0;

    // Per-screen video source (Syphon on macOS, Spout on Windows)
    int sourceIndex = -1;
    std::string sourceName;

#ifdef TARGET_OSX
    void connectToSource(const ofxSyphonServerDescription& desc);
#elif defined(TARGET_WIN32)
    void connectToSource(const std::string& senderName);
    void updateSpout(); // call each frame to receive texture
#endif
    void disconnectSource();
    bool hasSource() const;

    // Polygon mask (normalized 0-1 contour points)
    void setMask(const std::vector<glm::vec2>& points);
    const std::vector<glm::vec2>& getMaskPoints() const;
    bool hasMask() const;

    // JSON serialization
    ofJson toJson() const;
    void fromJson(const ofJson& j);

    // Drawing
    void draw(bool viewMode = false);
    void drawSelected();
    bool drawSourceTexture(const ofRectangle& destRect); // for mapping editor

    // Picking support
    glm::vec3 getWorldNormal() const;
    glm::vec3 getWorldCenter() const;
    float getPlaneWidth() const;
    float getPlaneHeight() const;

private:
    // Curvature
    float curvature = 0;       // degrees of arc (-180 to 180)
    float curveExtent = 100;   // % of width that curves (0-100)
    ofVboMesh curvedMesh;
    int meshColumns = 32;
    int meshRows = 2;
    void rebuildMesh();

    // Polygon mask
    std::vector<glm::vec2> maskPoints; // normalized 0-1 contour
    ofVboMesh polygonMesh;
    void rebuildPolygonMesh();

    // Input mapping (crop)
    ofRectangle cropRect{0, 0, 1, 1};  // normalized region

#ifdef TARGET_OSX
    ofxSyphonClient client;
    bool clientSetup = false;
#elif defined(TARGET_WIN32)
    ofxSpout::Receiver spoutReceiver;
    ofTexture spoutTexture;
    bool spoutSetup = false;
#endif
};
