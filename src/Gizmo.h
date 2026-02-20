#pragma once
#include "ofMain.h"
#include "ScreenObject.h"

class Gizmo {
public:
    enum class Mode { Translate, Rotate, Scale };
    enum class Axis { None, X, Y, Z };

    void draw(const ScreenObject& target, const ofCamera& cam);

    // Returns true if mouse hits a gizmo handle
    bool hitTest(const ofCamera& cam, const glm::vec2& screenPos,
                 const ScreenObject& target);

    void beginDrag(const glm::vec2& screenPos, const ofCamera& cam,
                   const ScreenObject& target);
    void updateDrag(const glm::vec2& screenPos, const ofCamera& cam,
                    ScreenObject& target);
    void endDrag();

    bool isDragging() const { return dragging; }
    Axis getActiveAxis() const { return activeAxis; }

    Mode mode = Mode::Translate;

    std::string getModeString() const;

private:
    float getGizmoSize(const glm::vec3& pos, const ofCamera& cam) const;
    glm::vec3 getAxisDirection(Axis axis) const;
    ofColor getAxisColor(Axis axis, bool active) const;

    Axis activeAxis = Axis::None;
    bool dragging = false;
    glm::vec2 dragStart;
    glm::vec3 dragStartPos;
    glm::vec3 dragStartRot;
    glm::vec3 dragStartScale;
};
