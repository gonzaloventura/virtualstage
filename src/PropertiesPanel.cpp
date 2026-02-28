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

    sizeGui.setup("Size");
    sizeGui.add(widthParam);
    sizeGui.add(heightParam);

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
    widthParam.addListener(this, &PropertiesPanel::onParamChanged);
    heightParam.addListener(this, &PropertiesPanel::onParamChanged);
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
    if (visScale) drawGroup(sizeGui);
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

void PropertiesPanel::setMultipleTargets(const std::vector<ScreenObject*>& targets) {
    target = nullptr;
    multiTargets = targets;
    multiMode = true;
    multiCount = (int)targets.size();
    nameLabel = "Multiple (" + ofToString(multiCount) + ")";
    sourceLabel = "---";

    // Compute averaged values for display
    syncing = true;
    glm::vec3 avgPos(0), avgRot(0);
    float avgW = 0, avgH = 0;
    float avgCurv = 0;
    for (auto* t : multiTargets) {
        avgPos += t->getPosition();
        avgRot += t->getRotationEuler();
        glm::vec3 s = t->getScale();
        avgW += t->getPlaneWidth() * s.x;
        avgH += t->getPlaneHeight() * s.y;
        avgCurv += t->getCurvature();
    }
    float n = (float)multiCount;
    avgPos /= n;
    avgRot /= n;
    avgW /= n;
    avgH /= n;
    avgCurv /= n;

    posX = avgPos.x; posY = avgPos.y; posZ = avgPos.z;
    rotX = avgRot.x; rotY = avgRot.y; rotZ = avgRot.z;
    if (preferences) {
        widthParam = preferences->oglToDisplay(avgW);
        heightParam = preferences->oglToDisplay(avgH);
    }
    curvatureParam = avgCurv;
    syncing = false;

    captureLastValues();
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
    if (preferences) {
        float effW = target->getPlaneWidth() * s.x;
        float effH = target->getPlaneHeight() * s.y;
        widthParam = preferences->oglToDisplay(effW);
        heightParam = preferences->oglToDisplay(effH);
    }

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
    if (preferences) {
        float baseW = target->getPlaneWidth();
        float baseH = target->getPlaneHeight();
        float newScaleX = (baseW > 0) ? preferences->displayToOgl(widthParam) / baseW : 1.0f;
        float newScaleY = (baseH > 0) ? preferences->displayToOgl(heightParam) / baseH : 1.0f;
        target->setScale(glm::vec3(
            std::max(0.01f, newScaleX),
            std::max(0.01f, newScaleY),
            1.0f));
    }
    target->setCurvature(curvatureParam);
    target->setCropRect(ofRectangle(cropX, cropY, cropW, cropH));
}

void PropertiesPanel::onParamChanged(float& val) {
    if (syncing) return;
    if (onPropertyChanged) onPropertyChanged();
    if (multiMode) {
        syncToMultiTargets();
    } else {
        syncToTarget();
    }
}

void PropertiesPanel::captureLastValues() {
    lastPos = glm::vec3(posX, posY, posZ);
    lastRot = glm::vec3(rotX, rotY, rotZ);
    lastSize = glm::vec2(widthParam, heightParam);
    lastCurvature = curvatureParam;
}

void PropertiesPanel::syncToMultiTargets() {
    if (syncing || multiTargets.empty()) return;

    glm::vec3 deltaPos = glm::vec3(posX, posY, posZ) - lastPos;
    glm::vec3 deltaRot = glm::vec3(rotX, rotY, rotZ) - lastRot;
    glm::vec2 deltaSize = glm::vec2(widthParam, heightParam) - lastSize;
    float deltaCurv = curvatureParam - lastCurvature;

    for (auto* t : multiTargets) {
        if (!t) continue;
        t->setPosition(t->getPosition() + deltaPos);
        t->setRotationEuler(t->getRotationEuler() + deltaRot);
        if (preferences) {
            glm::vec3 s = t->getScale();
            float curEffW = t->getPlaneWidth() * s.x;
            float curEffH = t->getPlaneHeight() * s.y;
            float newEffW = curEffW + preferences->displayToOgl(deltaSize.x);
            float newEffH = curEffH + preferences->displayToOgl(deltaSize.y);
            float newSx = (t->getPlaneWidth() > 0) ? newEffW / t->getPlaneWidth() : s.x;
            float newSy = (t->getPlaneHeight() > 0) ? newEffH / t->getPlaneHeight() : s.y;
            t->setScale(glm::vec3(
                std::max(0.01f, newSx),
                std::max(0.01f, newSy),
                1.0f));
        }
        t->setCurvature(t->getCurvature() + deltaCurv);
    }

    captureLastValues();
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

bool PropertiesPanel::handleRightClick(int x, int y) {
    if (!visible) return false;
    if (!target && !multiMode) return false;

    // Try to edit a parameter if the click falls on its slider control
    auto tryEditParam = [&](ofxGuiGroup& group, ofParameter<float>& param) -> bool {
        for (std::size_t i = 0; i < group.getNumControls(); i++) {
            auto* ctrl = group.getControl(i);
            if (ctrl && ctrl->getName() == param.getName() &&
                ctrl->getShape().inside(x, y)) {
                std::string input = ofSystemTextBoxDialog(
                    param.getName(), ofToString(param.get(), 3));
                if (!input.empty()) {
                    try {
                        float val = std::stof(input);
                        val = ofClamp(val, param.getMin(), param.getMax());
                        param = val;
                    } catch (...) {}
                }
                return true;
            }
        }
        return false;
    };

    if (visAmbient) {
        if (tryEditParam(ambientGui, ambientLight)) return true;
    }
    if (visPos) {
        if (tryEditParam(posGui, posX)) return true;
        if (tryEditParam(posGui, posY)) return true;
        if (tryEditParam(posGui, posZ)) return true;
    }
    if (visRot) {
        if (tryEditParam(rotGui, rotX)) return true;
        if (tryEditParam(rotGui, rotY)) return true;
        if (tryEditParam(rotGui, rotZ)) return true;
    }
    if (visScale) {
        if (tryEditParam(sizeGui, widthParam)) return true;
        if (tryEditParam(sizeGui, heightParam)) return true;
    }
    if (tryEditParam(curvatureGui, curvatureParam)) return true;
    if (visCrop) {
        if (tryEditParam(cropGui, cropX)) return true;
        if (tryEditParam(cropGui, cropY)) return true;
        if (tryEditParam(cropGui, cropW)) return true;
        if (tryEditParam(cropGui, cropH)) return true;
    }

    return false;
}

void PropertiesPanel::refreshUnitLabels() {
    if (!preferences) return;
    syncing = true;

    std::string suffix = " (" + preferences->getUnitSuffix() + ")";
    widthParam.setName("Width" + suffix);
    heightParam.setName("Height" + suffix);

    // Update ranges: min = 1 OGL unit, max = 10000 OGL units (100m)
    float minVal = preferences->oglToDisplay(1.0f);
    float maxVal = preferences->oglToDisplay(10000.0f);
    widthParam.setMin(minVal);
    widthParam.setMax(maxVal);
    heightParam.setMin(minVal);
    heightParam.setMax(maxVal);

    syncing = false;

    // Re-sync displayed values in the new unit
    if (target) syncFromTarget();
}
