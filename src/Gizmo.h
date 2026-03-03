#pragma once
#include "ofMain.h"
#include "ScreenObject.h"
#include <vector>

class Gizmo {
public:
    enum class Mode { Translate, Rotate, Scale };
    enum class Axis { None, X, Y, Z };

    void draw(const ScreenObject& target, const ofCamera& cam);

    // Returns true if mouse hits a gizmo handle
    bool hitTest(const ofCamera& cam, const glm::vec2& screenPos,
                 const ScreenObject& target);

    void beginDrag(const glm::vec2& screenPos, const ofCamera& cam,
                   const ScreenObject& primary,
                   const std::vector<ScreenObject*>& allTargets);
    void updateDrag(const glm::vec2& screenPos, const ofCamera& cam);
    void endDrag();

    bool isDragging() const { return dragging; }
    Axis getActiveAxis() const { return activeAxis; }

    Mode mode = Mode::Translate;
    bool mirrorYaw = false; // when true + 2 targets, Y-rotation is mirrored

    // Snap to grid
    bool snapEnabled = false;
    float snapSize = 50.0f;

    // Snap to edges
    bool edgeSnapEnabled = true;
    float edgeSnapThreshold = 15.0f; // world units
    // Set all screens for edge-snap reference (call before drag)
    void setEdgeSnapScreens(const std::vector<std::unique_ptr<ScreenObject>>* allScreens);
    // Active snap lines for visual feedback (populated during updateDrag)
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
        ScreenObject* target;
        glm::vec3 startPos;
        glm::vec3 startRot;
        glm::vec3 startScale;
    };

    std::vector<DragStartState> dragTargets;
};
