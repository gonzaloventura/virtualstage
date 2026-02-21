#include "win_byte_fix.h"
#include "PropertiesPanel.h"

void PropertiesPanel::setup(float x, float y) {
    ambientGroup.setName("Ambient Light");
    ambientGroup.add(ambientLight);
    ambientGroup.add(ambientReset);

    posGroup.setName("Position");
    posGroup.add(posX);
    posGroup.add(posY);
    posGroup.add(posZ);

    rotGroup.setName("Rotation");
    rotGroup.add(rotX);
    rotGroup.add(rotY);
    rotGroup.add(rotZ);

    scaleGroup.setName("Scale");
    scaleGroup.add(scaleX);
    scaleGroup.add(scaleY);

    curvatureGroup.setName("Curvature");
    curvatureGroup.add(curvatureParam);

    cropGroup.setName("Input Mapping (M to edit)");
    cropGroup.add(cropX);
    cropGroup.add(cropY);
    cropGroup.add(cropW);
    cropGroup.add(cropH);

    panel.setup("Properties", "properties.xml", x, y);
    rebuildPanel();

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

void PropertiesPanel::rebuildPanel() {
    float x = panel.getPosition().x;
    float y = panel.getPosition().y;
    panel.clear();
    panel.add(nameLabel.setup("Object", target ? target->name : "None"));
    panel.add(sourceLabel.setup("Source",
        (target && target->hasSource()) ? target->sourceName : "None (1-9 to assign)"));
    if (visAmbient) panel.add(ambientGroup);
    if (visPos) panel.add(posGroup);
    if (visRot) panel.add(rotGroup);
    if (visScale) panel.add(scaleGroup);
    panel.add(curvatureGroup);
    if (visCrop) panel.add(cropGroup);
    panel.setPosition(x, y);
}

void PropertiesPanel::updateGroupVisibility(bool ambient, bool pos, bool rot, bool scale, bool crop) {
    visAmbient = ambient;
    visPos = pos;
    visRot = rot;
    visScale = scale;
    visCrop = crop;
    rebuildPanel();
}

void PropertiesPanel::setPosition(float x, float y) {
    panel.setPosition(x, y);
}

void PropertiesPanel::draw() {
    if (!visible) return;
    panel.draw();
}

void PropertiesPanel::setTarget(ScreenObject* t) {
    target = t;
    if (target) {
        nameLabel = target->name;
        sourceLabel = target->hasSource() ? target->sourceName : "None (1-9 to assign)";
        syncFromTarget();
    } else {
        nameLabel = "None";
        sourceLabel = "None";
    }
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
