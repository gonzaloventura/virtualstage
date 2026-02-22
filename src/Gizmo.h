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

    std::string getModeString() const;

private:
    float getGizmoSize(const glm::vec3& pos, const ofCamera& cam) const;
    glm::vec3 getAxisDirection(Axis axis) const;
    ofColor getAxisColor(Axis axis, bool active) const;
    float hitTestRing(const ofCamera& cam, const glm::vec2& screenPos,
                      const glm::vec3& center, Axis axis, float radius) const;

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
