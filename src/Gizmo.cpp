#include "Gizmo.h"

float Gizmo::getGizmoSize(const glm::vec3& pos, const ofCamera& cam) const {
    return glm::distance(pos, cam.getPosition()) * 0.12f;
}

glm::vec3 Gizmo::getAxisDirection(Axis axis) const {
    switch (axis) {
        case Axis::X: return glm::vec3(1, 0, 0);
        case Axis::Y: return glm::vec3(0, 1, 0);
        case Axis::Z: return glm::vec3(0, 0, 1);
        default: return glm::vec3(0);
    }
}

ofColor Gizmo::getAxisColor(Axis axis, bool active) const {
    int brightness = active ? 255 : 180;
    switch (axis) {
        case Axis::X: return ofColor(brightness, 50, 50);
        case Axis::Y: return ofColor(50, brightness, 50);
        case Axis::Z: return ofColor(50, 50, brightness);
        default: return ofColor(150);
    }
}

void Gizmo::draw(const ScreenObject& target, const ofCamera& cam) {
    glm::vec3 pos = target.getPosition();
    float size = getGizmoSize(pos, cam);

    ofPushStyle();
    ofSetLineWidth(3);

    if (mode == Mode::Translate) {
        // Draw axis lines with arrow cones
        for (int a = 0; a < 3; a++) {
            Axis axis = static_cast<Axis>(a + 1);
            glm::vec3 dir = getAxisDirection(axis);
            glm::vec3 end = pos + dir * size;

            ofSetColor(getAxisColor(axis, activeAxis == axis));
            ofDrawLine(pos, end);

            // Arrowhead
            ofPushMatrix();
            ofTranslate(end);
            if (axis == Axis::X) ofRotateZDeg(-90);
            else if (axis == Axis::Z) ofRotateXDeg(90);
            ofDrawCone(0, size * 0.05f, 0, size * 0.03f, size * 0.1f);
            ofPopMatrix();
        }
    } else if (mode == Mode::Rotate) {
        // Draw rotation rings
        int segments = 48;
        for (int a = 0; a < 3; a++) {
            Axis axis = static_cast<Axis>(a + 1);
            ofSetColor(getAxisColor(axis, activeAxis == axis));

            ofPolyline ring;
            for (int i = 0; i <= segments; i++) {
                float angle = glm::two_pi<float>() * i / segments;
                glm::vec3 p;
                float r = size * 0.8f;
                if (axis == Axis::X) p = glm::vec3(0, cos(angle) * r, sin(angle) * r);
                else if (axis == Axis::Y) p = glm::vec3(cos(angle) * r, 0, sin(angle) * r);
                else p = glm::vec3(cos(angle) * r, sin(angle) * r, 0);
                ring.addVertex(pos + p);
            }
            ring.draw();
        }
    } else if (mode == Mode::Scale) {
        // Draw axis lines with cubes at the ends
        for (int a = 0; a < 3; a++) {
            Axis axis = static_cast<Axis>(a + 1);
            glm::vec3 dir = getAxisDirection(axis);
            glm::vec3 end = pos + dir * size;

            ofSetColor(getAxisColor(axis, activeAxis == axis));
            ofDrawLine(pos, end);

            // Scale cube
            float cubeSize = size * 0.06f;
            ofDrawBox(end, cubeSize, cubeSize, cubeSize);
        }
    }

    ofSetLineWidth(1);
    ofPopStyle();
}

bool Gizmo::hitTest(const ofCamera& cam, const glm::vec2& screenPos,
                    const ScreenObject& target) {
    glm::vec3 pos = target.getPosition();
    float size = getGizmoSize(pos, cam);
    float threshold = 20.0f; // pixels

    activeAxis = Axis::None;
    float bestDist = threshold;

    for (int a = 0; a < 3; a++) {
        Axis axis = static_cast<Axis>(a + 1);
        glm::vec3 dir = getAxisDirection(axis);
        glm::vec3 end = pos + dir * size;

        // Project to screen
        glm::vec3 screenStart = cam.worldToScreen(pos);
        glm::vec3 screenEnd = cam.worldToScreen(end);

        // Distance from point to line segment in 2D
        glm::vec2 a2d(screenStart.x, screenStart.y);
        glm::vec2 b2d(screenEnd.x, screenEnd.y);
        glm::vec2 p(screenPos.x, screenPos.y);

        glm::vec2 ab = b2d - a2d;
        float len2 = glm::dot(ab, ab);
        if (len2 < 1.0f) continue;

        float t = glm::clamp(glm::dot(p - a2d, ab) / len2, 0.0f, 1.0f);
        glm::vec2 closest = a2d + ab * t;
        float dist = glm::distance(p, closest);

        if (dist < bestDist) {
            bestDist = dist;
            activeAxis = axis;
        }
    }

    return activeAxis != Axis::None;
}

void Gizmo::beginDrag(const glm::vec2& screenPos, const ofCamera& cam,
                      const ScreenObject& target) {
    dragging = true;
    dragStart = screenPos;
    dragStartPos = target.getPosition();
    dragStartRot = target.getRotationEuler();
    dragStartScale = target.getScale();
}

void Gizmo::updateDrag(const glm::vec2& screenPos, const ofCamera& cam,
                       ScreenObject& target) {
    if (!dragging || activeAxis == Axis::None) return;

    glm::vec2 delta = screenPos - dragStart;

    if (mode == Mode::Translate) {
        // Project the axis direction to screen space to determine which screen
        // direction corresponds to the 3D axis
        glm::vec3 pos = dragStartPos;
        glm::vec3 dir = getAxisDirection(activeAxis);

        glm::vec3 screenPos3D = cam.worldToScreen(pos);
        glm::vec3 screenAxisEnd = cam.worldToScreen(pos + dir * 100.0f);
        glm::vec2 screenDir = glm::normalize(
            glm::vec2(screenAxisEnd.x - screenPos3D.x, screenAxisEnd.y - screenPos3D.y));

        // Project mouse delta onto screen axis direction
        float projectedDelta = glm::dot(delta, screenDir);

        // Scale factor: relate screen pixels to world units
        float worldScale = getGizmoSize(pos, cam) / 80.0f;
        glm::vec3 newPos = dragStartPos + dir * projectedDelta * worldScale;
        target.setPosition(newPos);

    } else if (mode == Mode::Rotate) {
        // Horizontal mouse delta = rotation degrees
        float degrees = delta.x * 0.5f;
        glm::vec3 newRot = dragStartRot;

        switch (activeAxis) {
            case Axis::X: newRot.x += degrees; break;
            case Axis::Y: newRot.y += degrees; break;
            case Axis::Z: newRot.z += degrees; break;
            default: break;
        }
        target.setRotationEuler(newRot);

    } else if (mode == Mode::Scale) {
        // Horizontal mouse delta = scale factor
        float scaleDelta = delta.x * 0.005f;
        glm::vec3 newScale = dragStartScale;

        switch (activeAxis) {
            case Axis::X: newScale.x = std::max(0.1f, dragStartScale.x + scaleDelta); break;
            case Axis::Y: newScale.y = std::max(0.1f, dragStartScale.y + scaleDelta); break;
            case Axis::Z: newScale.z = std::max(0.1f, dragStartScale.z + scaleDelta); break;
            default: break;
        }
        target.setScale(newScale);
    }
}

void Gizmo::endDrag() {
    dragging = false;
    activeAxis = Axis::None;
}

std::string Gizmo::getModeString() const {
    switch (mode) {
        case Mode::Translate: return "Move [W]";
        case Mode::Rotate: return "Rotate [E]";
        case Mode::Scale: return "Scale [R]";
    }
    return "";
}
