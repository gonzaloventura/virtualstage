#pragma once
#include "ofMain.h"
#include "ScreenObject.h"
#include "StageElement.h"
#include <vector>

// Lightweight wrapper so Gizmo can manipulate either type
struct GizmoTarget {
    enum Type { Screen, StageElem };
    Type type;
    union { ScreenObject* screen; StageElement* element; };

    GizmoTarget() : type(Screen), screen(nullptr) {}
    GizmoTarget(ScreenObject* s) : type(Screen), screen(s) {}
    GizmoTarget(StageElement* e) : type(StageElem), element(e) {}

    glm::vec3 getPosition() const;
    void setPosition(const glm::vec3& p);
    glm::vec3 getRotationEuler() const;
    void setRotationEuler(const glm::vec3& e);
    glm::vec3 getScale() const;
    void setScale(const glm::vec3& s);
    float getHalfWidth() const;
    float getHalfHeight() const;
};

class Gizmo {
public:
    enum class Mode { Translate, Rotate, Scale };
    enum class Axis { None, X, Y, Z };

    void draw(const glm::vec3& targetPos, const ofCamera& cam);

    bool hitTest(const ofCamera& cam, const glm::vec2& screenPos,
                 const glm::vec3& targetPos);

    void beginDrag(const glm::vec2& screenPos, const ofCamera& cam,
                   const GizmoTarget& primary,
                   const std::vector<GizmoTarget>& allTargets);
    void updateDrag(const glm::vec2& screenPos, const ofCamera& cam);
    void endDrag();

    bool isDragging() const { return dragging; }
    Axis getActiveAxis() const { return activeAxis; }

    Mode mode = Mode::Translate;
    bool mirrorYaw = false;

    // Snap to grid
    bool snapEnabled = false;
    float snapSize = 50.0f;

    // Snap to edges
    bool edgeSnapEnabled = true;
    float edgeSnapThreshold = 15.0f;
    void setEdgeSnapScreens(const std::vector<std::unique_ptr<ScreenObject>>* allScreens);
    struct SnapLine { glm::vec3 a, b; };
    std::vector<SnapLine> activeSnapLines;

    std::string getModeString() const;

private:
    float getGizmoSize(const glm::vec3& pos, const ofCamera& cam) const;
    glm::vec3 getAxisDirection(Axis axis) const;
    ofColor getAxisColor(Axis axis, bool active) const;
    float hitTestRing(const ofCamera& cam, const glm::vec2& screenPos,
                      const glm::vec3& center, Axis axis, float radius) const;

    const std::vector<std::unique_ptr<ScreenObject>>* edgeSnapScreens = nullptr;

    Axis activeAxis = Axis::None;
    bool dragging = false;
    glm::vec2 dragStart;

    struct DragStartState {
        GizmoTarget target;
        glm::vec3 startPos;
        glm::vec3 startRot;
        glm::vec3 startScale;
    };

    std::vector<DragStartState> dragTargets;
};
