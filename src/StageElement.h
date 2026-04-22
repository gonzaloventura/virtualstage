#pragma once
#include "ofMain.h"
#include <string>

enum class StageElementType { Floor, Truss, Layher };

class StageElement {
public:
    StageElement(StageElementType type = StageElementType::Layher, const std::string& name = "");

    std::string name;
    StageElementType type;

    // Transform
    void setPosition(const glm::vec3& pos);
    void setRotationEuler(const glm::vec3& eulerDeg);
    void setScale(const glm::vec3& s);
    glm::vec3 getPosition() const;
    glm::vec3 getRotationEuler() const;
    glm::vec3 getScale() const;

    // Dimensions (type-specific defaults)
    float width = 100;
    float height = 5;
    float depth = 100;

    // Color
    ofColor color{80, 80, 80};

    // Drawing
    void draw(bool selected = false);

    // Transform matrix (for 3D picking)
    glm::mat4 getGlobalTransformMatrix() const;

    // Serialization
    ofJson toJson() const;
    void fromJson(const ofJson& j);

private:
    ofNode node;
    void drawFloor();
    void drawTruss();
    void drawLayher();
};
