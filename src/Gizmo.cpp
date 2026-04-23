#include "win_byte_fix.h"
#include "Gizmo.h"

// --- GizmoTarget accessors ---
glm::vec3 GizmoTarget::getPosition() const {
    return screen->getPosition();
}
void GizmoTarget::setPosition(const glm::vec3& p) {
    screen->setPosition(p);
}
glm::vec3 GizmoTarget::getRotationEuler() const {
    return screen->getRotationEuler();
}
void GizmoTarget::setRotationEuler(const glm::vec3& e) {
    screen->setRotationEuler(e);
}
glm::vec3 GizmoTarget::getScale() const {
    return screen->getScale();
}
void GizmoTarget::setScale(const glm::vec3& s) {
    screen->setScale(s);
}
float GizmoTarget::getHalfWidth() const {
    return screen->getPlaneWidth() * screen->getScale().x * 0.5f;
}
float GizmoTarget::getHalfHeight() const {
    return screen->getPlaneHeight() * screen->getScale().y * 0.5f;
}

// --- Gizmo ---
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
    int brightness = active ? 255 : 140;
    switch (axis) {
        case Axis::X: return ofColor(brightness, 50, 50);
        case Axis::Y: return ofColor(50, brightness, 50);
        case Axis::Z: return ofColor(50, 50, brightness);
        default: return ofColor(150);
    }
}

float Gizmo::hitTestRing(const ofCamera& cam, const glm::vec2& screenPos,
                          const glm::vec3& center, Axis axis, float radius) const {
    int segments = 48;
    float minDist = 1e9f;
    for (int i = 0; i < segments; i++) {
        float angle = glm::two_pi<float>() * i / segments;
        glm::vec3 p;
        if (axis == Axis::X) p = glm::vec3(0, cos(angle) * radius, sin(angle) * radius);
        else if (axis == Axis::Y) p = glm::vec3(cos(angle) * radius, 0, sin(angle) * radius);
        else p = glm::vec3(cos(angle) * radius, sin(angle) * radius, 0);
        glm::vec3 sp = cam.worldToScreen(center + p);
        float dist = glm::distance(screenPos, glm::vec2(sp.x, sp.y));
        if (dist < minDist) minDist = dist;
    }
    return minDist;
}

void Gizmo::draw(const glm::vec3& pos, const ofCamera& cam) {
    float size = getGizmoSize(pos, cam);

    ofPushStyle();

    if (mode == Mode::Translate) {
        for (int a = 0; a < 3; a++) {
            Axis axis = static_cast<Axis>(a + 1);
            bool active = (activeAxis == axis);
            glm::vec3 dir = getAxisDirection(axis);
            glm::vec3 end = pos + dir * size;

            ofSetLineWidth(active ? 5 : 2);
            ofSetColor(getAxisColor(axis, active));
            ofDrawLine(pos, end);

            ofPushMatrix();
            ofTranslate(end);
            if (axis == Axis::X) ofRotateZDeg(-90);
            else if (axis == Axis::Z) ofRotateXDeg(90);
            ofDrawCone(0, size * 0.05f, 0, size * 0.03f, size * 0.1f);
            ofPopMatrix();
        }
    } else if (mode == Mode::Rotate) {
        int segments = 48;
        for (int a = 0; a < 3; a++) {
            Axis axis = static_cast<Axis>(a + 1);
            bool active = (activeAxis == axis);
            ofSetLineWidth(active ? 5 : 2);
            ofSetColor(getAxisColor(axis, active));

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
        for (int a = 0; a < 3; a++) {
            Axis axis = static_cast<Axis>(a + 1);
            bool active = (activeAxis == axis);
            glm::vec3 dir = getAxisDirection(axis);
            glm::vec3 end = pos + dir * size;

            ofSetLineWidth(active ? 5 : 2);
            ofSetColor(getAxisColor(axis, active));
            ofDrawLine(pos, end);

            float cubeSize = size * 0.06f;
            ofDrawBox(end, cubeSize, cubeSize, cubeSize);
        }
    }

    ofSetLineWidth(1);
    ofPopStyle();
}

bool Gizmo::hitTest(const ofCamera& cam, const glm::vec2& screenPos,
                    const glm::vec3& pos) {
    float size = getGizmoSize(pos, cam);
    float threshold = 20.0f;

    activeAxis = Axis::None;
    float bestDist = threshold;

    if (mode == Mode::Rotate) {
        float ringRadius = size * 0.8f;
        for (int a = 0; a < 3; a++) {
            Axis axis = static_cast<Axis>(a + 1);
            float dist = hitTestRing(cam, screenPos, pos, axis, ringRadius);
            if (dist < bestDist) {
                bestDist = dist;
                activeAxis = axis;
            }
        }
    } else {
        for (int a = 0; a < 3; a++) {
            Axis axis = static_cast<Axis>(a + 1);
            glm::vec3 dir = getAxisDirection(axis);
            glm::vec3 end = pos + dir * size;

            glm::vec3 screenStart = cam.worldToScreen(pos);
            glm::vec3 screenEnd = cam.worldToScreen(end);

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
    }

    return activeAxis != Axis::None;
}

void Gizmo::beginDrag(const glm::vec2& screenPos, const ofCamera& cam,
                      const GizmoTarget& primary,
                      const std::vector<GizmoTarget>& allTargets) {
    dragging = true;
    dragStart = screenPos;

    dragTargets.clear();
    for (auto& t : allTargets) {
        DragStartState state;
        state.target = t;
        state.startPos = t.getPosition();
        state.startRot = t.getRotationEuler();
        state.startScale = t.getScale();
        dragTargets.push_back(state);
    }
}

void Gizmo::updateDrag(const glm::vec2& screenPos, const ofCamera& cam) {
    if (!dragging || activeAxis == Axis::None || dragTargets.empty()) return;

    glm::vec2 delta = screenPos - dragStart;

    const auto& primaryState = dragTargets[0];

    if (mode == Mode::Translate) {
        glm::vec3 pos = primaryState.startPos;
        glm::vec3 dir = getAxisDirection(activeAxis);

        glm::vec3 screenPos3D = cam.worldToScreen(pos);
        glm::vec3 screenAxisEnd = cam.worldToScreen(pos + dir * 100.0f);
        glm::vec2 screenDir = glm::normalize(
            glm::vec2(screenAxisEnd.x - screenPos3D.x, screenAxisEnd.y - screenPos3D.y));

        float projectedDelta = glm::dot(delta, screenDir);
        float worldScale = getGizmoSize(pos, cam) / 80.0f;
        glm::vec3 worldDelta = dir * projectedDelta * worldScale;

        activeSnapLines.clear();

        for (auto& state : dragTargets) {
            glm::vec3 newPos = state.startPos + worldDelta;
            if (snapEnabled) {
                newPos.x = std::round(newPos.x / snapSize) * snapSize;
                newPos.y = std::round(newPos.y / snapSize) * snapSize;
                newPos.z = std::round(newPos.z / snapSize) * snapSize;
            }

            // Edge snap
            if (edgeSnapEnabled && edgeSnapScreens) {
                float hw = state.target.getHalfWidth();
                float hh = state.target.getHalfHeight();

                float myLeft   = newPos.x - hw;
                float myRight  = newPos.x + hw;
                float myTop    = newPos.y + hh;
                float myBottom = newPos.y - hh;

                for (auto& other : *edgeSnapScreens) {
                    // Skip self
                    bool isSelf = false;
                    for (auto& dt : dragTargets) {
                        if (dt.target.screen == other.get()) {
                            isSelf = true; break;
                        }
                    }
                    if (isSelf) continue;

                    glm::vec3 oPos = other->getPosition();
                    float oHW = other->getPlaneWidth() * other->getScale().x * 0.5f;
                    float oHH = other->getPlaneHeight() * other->getScale().y * 0.5f;

                    float oLeft   = oPos.x - oHW;
                    float oRight  = oPos.x + oHW;
                    float oTop    = oPos.y + oHH;
                    float oBottom = oPos.y - oHH;

                    // X-axis edge snapping
                    float xEdges[] = { oLeft, oRight, oPos.x };
                    for (float ox : xEdges) {
                        if (std::abs(myLeft - ox) < edgeSnapThreshold) {
                            newPos.x = ox + hw;
                            activeSnapLines.push_back({
                                glm::vec3(ox, std::min(myBottom, oBottom) - 50, newPos.z),
                                glm::vec3(ox, std::max(myTop, oTop) + 50, newPos.z)
                            });
                        } else if (std::abs(myRight - ox) < edgeSnapThreshold) {
                            newPos.x = ox - hw;
                            activeSnapLines.push_back({
                                glm::vec3(ox, std::min(myBottom, oBottom) - 50, newPos.z),
                                glm::vec3(ox, std::max(myTop, oTop) + 50, newPos.z)
                            });
                        } else if (std::abs(newPos.x - ox) < edgeSnapThreshold) {
                            newPos.x = ox;
                            activeSnapLines.push_back({
                                glm::vec3(ox, std::min(myBottom, oBottom) - 50, newPos.z),
                                glm::vec3(ox, std::max(myTop, oTop) + 50, newPos.z)
                            });
                        }
                    }

                    // Y-axis edge snapping
                    float yEdges[] = { oTop, oBottom, oPos.y };
                    for (float oy : yEdges) {
                        if (std::abs(myTop - oy) < edgeSnapThreshold) {
                            newPos.y = oy - hh;
                            activeSnapLines.push_back({
                                glm::vec3(std::min(myLeft, oLeft) - 50, oy, newPos.z),
                                glm::vec3(std::max(myRight, oRight) + 50, oy, newPos.z)
                            });
                        } else if (std::abs(myBottom - oy) < edgeSnapThreshold) {
                            newPos.y = oy + hh;
                            activeSnapLines.push_back({
                                glm::vec3(std::min(myLeft, oLeft) - 50, oy, newPos.z),
                                glm::vec3(std::max(myRight, oRight) + 50, oy, newPos.z)
                            });
                        } else if (std::abs(newPos.y - oy) < edgeSnapThreshold) {
                            newPos.y = oy;
                            activeSnapLines.push_back({
                                glm::vec3(std::min(myLeft, oLeft) - 50, oy, newPos.z),
                                glm::vec3(std::max(myRight, oRight) + 50, oy, newPos.z)
                            });
                        }
                    }

                    // Z-axis snap (depth alignment, center-to-center)
                    if (std::abs(newPos.z - oPos.z) < edgeSnapThreshold) {
                        newPos.z = oPos.z;
                        activeSnapLines.push_back({
                            glm::vec3(std::min(myLeft, oLeft) - 50, newPos.y, oPos.z),
                            glm::vec3(std::max(myRight, oRight) + 50, newPos.y, oPos.z)
                        });
                    }
                }
            }

            state.target.setPosition(newPos);
        }

    } else if (mode == Mode::Rotate) {
        float degrees = delta.x * 0.5f;

        bool doMirror = mirrorYaw && activeAxis == Axis::Y && dragTargets.size() == 2;

        if (doMirror) {
            auto* stateA = &dragTargets[0];
            auto* stateB = &dragTargets[1];
            bool aIsLeft = (stateA->startPos.x < stateB->startPos.x - 0.1f)
                        || (std::abs(stateA->startPos.x - stateB->startPos.x) <= 0.1f);
            auto* left  = aIsLeft ? stateA : stateB;
            auto* right = aIsLeft ? stateB : stateA;

            glm::vec3 leftRot = left->startRot;
            glm::vec3 rightRot = right->startRot;
            leftRot.y += degrees;
            rightRot.y -= degrees;
            left->target.setRotationEuler(leftRot);
            right->target.setRotationEuler(rightRot);
        } else {
            for (auto& state : dragTargets) {
                glm::vec3 newRot = state.startRot;
                switch (activeAxis) {
                    case Axis::X: newRot.x += degrees; break;
                    case Axis::Y: newRot.y += degrees; break;
                    case Axis::Z: newRot.z += degrees; break;
                    default: break;
                }
                state.target.setRotationEuler(newRot);
            }
        }

    } else if (mode == Mode::Scale) {
        float scaleDelta = delta.x * 0.005f;

        for (auto& state : dragTargets) {
            glm::vec3 newScale = state.startScale;
            switch (activeAxis) {
                case Axis::X: newScale.x = std::max(0.01f, state.startScale.x + scaleDelta); break;
                case Axis::Y: newScale.y = std::max(0.01f, state.startScale.y + scaleDelta); break;
                case Axis::Z: newScale.z = std::max(0.01f, state.startScale.z + scaleDelta); break;
                default: break;
            }
            state.target.setScale(newScale);
        }
    }
}

void Gizmo::endDrag() {
    dragging = false;
    activeAxis = Axis::None;
    dragTargets.clear();
    activeSnapLines.clear();
}

void Gizmo::setEdgeSnapScreens(const std::vector<std::unique_ptr<ScreenObject>>* allScreens) {
    edgeSnapScreens = allScreens;
}

std::string Gizmo::getModeString() const {
    switch (mode) {
        case Mode::Translate: return "Move [W]";
        case Mode::Rotate: return "Rotate [E]";
        case Mode::Scale: return "Scale [R]";
    }
    return "";
}
