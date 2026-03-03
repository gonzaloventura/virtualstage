#include "win_byte_fix.h"
#include "StageElement.h"

static std::string typeToString(StageElementType t) {
    switch (t) {
        case StageElementType::Floor: return "floor";
        case StageElementType::Truss: return "truss";
        case StageElementType::Box:   return "box";
    }
    return "box";
}

static StageElementType stringToType(const std::string& s) {
    if (s == "floor") return StageElementType::Floor;
    if (s == "truss") return StageElementType::Truss;
    return StageElementType::Box;
}

StageElement::StageElement(StageElementType t, const std::string& n) : type(t) {
    if (n.empty()) {
        switch (t) {
            case StageElementType::Floor: name = "Floor"; width = 2000; height = 2; depth = 2000; color = ofColor(50, 50, 55); break;
            case StageElementType::Truss: name = "Truss"; width = 30; height = 30; depth = 400; color = ofColor(120, 120, 120); break;
            case StageElementType::Box:   name = "Box";   width = 100; height = 100; depth = 100; color = ofColor(80, 80, 90); break;
        }
    } else {
        name = n;
    }
}

void StageElement::setPosition(const glm::vec3& pos) { node.setPosition(pos); }
void StageElement::setRotationEuler(const glm::vec3& e) {
    node.setOrientation(glm::quat(glm::radians(e)));
}
void StageElement::setScale(const glm::vec3& s) { node.setScale(s); }

glm::vec3 StageElement::getPosition() const { return node.getPosition(); }
glm::vec3 StageElement::getRotationEuler() const {
    return glm::degrees(glm::eulerAngles(node.getOrientationQuat()));
}
glm::vec3 StageElement::getScale() const { return node.getScale(); }

void StageElement::draw(bool selected) {
    ofPushMatrix();
    ofMultMatrix(node.getGlobalTransformMatrix());

    ofSetColor(color);
    switch (type) {
        case StageElementType::Floor: drawFloor(); break;
        case StageElementType::Truss: drawTruss(); break;
        case StageElementType::Box:   drawBox();   break;
    }

    if (selected) {
        ofNoFill();
        ofSetColor(0, 200, 255, 180);
        ofSetLineWidth(2);
        ofDrawBox(0, height / 2, 0, width, height, depth);
        ofFill();
        ofSetLineWidth(1);
    }

    ofPopMatrix();
}

void StageElement::drawFloor() {
    // Flat floor plane with subtle grid lines
    ofFill();
    ofDrawBox(0, -height / 2, 0, width, height, depth);

    // Grid lines on top
    ofPushStyle();
    ofSetColor(color.r + 15, color.g + 15, color.b + 15);
    float step = 100;
    float hw = width / 2, hd = depth / 2;
    for (float x = -hw; x <= hw; x += step) {
        ofDrawLine(x, 0.1f, -hd, x, 0.1f, hd);
    }
    for (float z = -hd; z <= hd; z += step) {
        ofDrawLine(-hw, 0.1f, z, hw, 0.1f, z);
    }
    ofPopStyle();
}

void StageElement::drawTruss() {
    // Simple truss segment as a box with wireframe
    ofFill();
    ofDrawBox(0, height / 2, 0, width, height, depth);
    ofNoFill();
    ofSetColor(color.r + 40, color.g + 40, color.b + 40);
    ofDrawBox(0, height / 2, 0, width, height, depth);
    ofFill();
}

void StageElement::drawBox() {
    ofFill();
    ofDrawBox(0, height / 2, 0, width, height, depth);
}

ofJson StageElement::toJson() const {
    ofJson j;
    j["type"] = typeToString(type);
    j["name"] = name;
    j["width"] = width;
    j["height"] = height;
    j["depth"] = depth;

    glm::vec3 pos = getPosition();
    j["position"] = {pos.x, pos.y, pos.z};

    glm::vec3 rot = getRotationEuler();
    j["rotation"] = {rot.x, rot.y, rot.z};

    glm::vec3 s = getScale();
    j["scale"] = {s.x, s.y, s.z};

    j["color"] = {{"r", (int)color.r}, {"g", (int)color.g}, {"b", (int)color.b}};

    return j;
}

void StageElement::fromJson(const ofJson& j) {
    type = stringToType(j.value("type", "box"));
    name = j.value("name", "Element");
    width = j.value("width", 100.0f);
    height = j.value("height", 100.0f);
    depth = j.value("depth", 100.0f);

    if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
        setPosition(glm::vec3(j["position"][0], j["position"][1], j["position"][2]));
    }
    if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
        setRotationEuler(glm::vec3(j["rotation"][0], j["rotation"][1], j["rotation"][2]));
    }
    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
        setScale(glm::vec3(j["scale"][0], j["scale"][1], j["scale"][2]));
    }
    if (j.contains("color") && j["color"].is_object()) {
        color = ofColor(j["color"].value("r", 80), j["color"].value("g", 80), j["color"].value("b", 80));
    }
}
