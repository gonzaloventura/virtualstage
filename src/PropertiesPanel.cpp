#include "win_byte_fix.h"
#include "PropertiesPanel.h"

void PropertiesPanel::setup(float x, float y) {
    // Main panel: header + labels only
    panel.setup("Properties", "properties.xml", x, y);
    panel.add(nameLabel.setup("Object", "None"));
    panel.add(sourceLabel.setup("Source", "None"));

    // Standalone GUI groups (drawn below panel, visibility controlled)
    ambientGui.setup("Ambient Light");
    ambientGui.add(ambientLight);
    ambientGui.add(ambientReset);

    posGui.setup("Position");
    posGui.add(posX);
    posGui.add(posY);
    posGui.add(posZ);

    rotGui.setup("Rotation");
    rotGui.add(rotX);
    rotGui.add(rotY);
    rotGui.add(rotZ);

    scaleGui.setup("Scale");
    scaleGui.add(scaleX);
    scaleGui.add(scaleY);

    curvatureGui.setup("Curvature");
    curvatureGui.add(curvatureParam);

    cropGui.setup("Input Mapping (M to edit)");
    cropGui.add(cropX);
    cropGui.add(cropY);
    cropGui.add(cropW);
    cropGui.add(cropH);

    // Add listeners
    posX.addListener(this, &PropertiesPanel::onParamChanged);
    posY.addListener(this, &PropertiesPanel::onParamChanged);
    posZ.addListener(this, &PropertiesPanel::onParamChanged);
    rotX.addListener(this, &PropertiesPanel::onParamChanged);
    rotY.addListener(this, &PropertiesPanel::onParamChanged);
    rotZ.addListener(this, &PropertiesPanel::onParamChanged);
    scaleX.addListener(this, &PropertiesPanel::onParamChanged);
    scaleY.addListener(this, &PropertiesPanel::onParamChanged);
    curvatureParam.addListener(this, &PropertiesPanel::onParamChanged);
    cropX.addListener(this, &PropertiesPanel::onParamChanged);
    cropY.addListener(this, &PropertiesPanel::onParamChanged);
    cropW.addListener(this, &PropertiesPanel::onParamChanged);
    cropH.addListener(this, &PropertiesPanel::onParamChanged);
    ambientReset.addListener(this, &PropertiesPanel::onAmbientReset);
}

void PropertiesPanel::setPosition(float x, float y) {
    panel.setPosition(x, y);
}

void PropertiesPanel::draw() {
    if (!visible) return;

    // Draw main panel (header + labels)
    panel.draw();

    // Draw visible groups below the panel
    float x = panel.getPosition().x;
    float w = panel.getWidth();
    float y = panel.getPosition().y + panel.getHeight();

    auto drawGroup = [&](ofxGuiGroup& gui) {
        gui.setPosition(x, y);
        gui.setWidthElements(w);
        gui.draw();
        y += gui.getHeight();
    };

    if (visAmbient) drawGroup(ambientGui);
    if (visPos) drawGroup(posGui);
    if (visRot) drawGroup(rotGui);
    if (visScale) drawGroup(scaleGui);
    drawGroup(curvatureGui); // always visible
    if (visCrop) drawGroup(cropGui);
}

void PropertiesPanel::updateGroupVisibility(bool ambient, bool pos, bool rot, bool scale, bool crop) {
    visAmbient = ambient;
    visPos = pos;
    visRot = rot;
    visScale = scale;
    visCrop = crop;
}

void PropertiesPanel::setTarget(ScreenObject* t) {
    target = t;
    multiMode = false;
    multiCount = 0;
    if (target) {
        nameLabel = target->name;
        sourceLabel = target->hasSource() ? target->sourceName : "None (1-9 to assign)";
        syncFromTarget();
    } else {
        nameLabel = "None";
        sourceLabel = "None";
    }
}

void PropertiesPanel::setMultipleTargets(int count) {
    target = nullptr;
    multiMode = true;
    multiCount = count;
    nameLabel = "Multiple (" + ofToString(count) + ")";
    sourceLabel = "---";
}

void PropertiesPanel::syncFromTarget() {
    if (!target) return;
    syncing = true;

    glm::vec3 pos = target->getPosition();
    posX = pos.x;
    posY = pos.y;
    posZ = pos.z;

    glm::vec3 rot = target->getRotationEuler();
    rotX = rot.x;
    rotY = rot.y;
    rotZ = rot.z;

    glm::vec3 s = target->getScale();
    scaleX = s.x;
    scaleY = s.y;

    curvatureParam = target->getCurvature();

    const ofRectangle& crop = target->getCropRect();
    cropX = crop.x;
    cropY = crop.y;
    cropW = crop.width;
    cropH = crop.height;

    syncing = false;
}

void PropertiesPanel::syncToTarget() {
    if (!target || syncing) return;

    target->setPosition(glm::vec3(posX, posY, posZ));
    target->setRotationEuler(glm::vec3(rotX, rotY, rotZ));
    target->setScale(glm::vec3(scaleX, scaleY, 1.0f));
    target->setCurvature(curvatureParam);
    target->setCropRect(ofRectangle(cropX, cropY, cropW, cropH));
}

void PropertiesPanel::onParamChanged(float& val) {
    if (syncing) return;
    if (onPropertyChanged) onPropertyChanged();
    syncToTarget();
}

void PropertiesPanel::onAmbientReset(bool& val) {
    if (syncing) return;
    if (val) {
        syncing = true;
        ambientLight = 60;
        ambientReset = false;
        syncing = false;
    }
}
