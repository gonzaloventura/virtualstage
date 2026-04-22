#include "win_byte_fix.h"
#include "StageElement.h"

static std::string typeToString(StageElementType t) {
    switch (t) {
        case StageElementType::Floor: return "floor";
        case StageElementType::Truss: return "truss";
        case StageElementType::Layher: return "layher";
    }
    return "layher";
}

static StageElementType stringToType(const std::string& s) {
    if (s == "floor") return StageElementType::Floor;
    if (s == "truss") return StageElementType::Truss;
    if (s == "box" || s == "layher") return StageElementType::Layher; // backward compat
    return StageElementType::Layher;
}

StageElement::StageElement(StageElementType t, const std::string& n) : type(t) {
    if (n.empty()) {
        switch (t) {
            case StageElementType::Floor: name = "Floor"; width = 2000; height = 2; depth = 2000; color = ofColor(50, 50, 55); break;
            case StageElementType::Truss: name = "Truss"; width = 30; height = 30; depth = 400; color = ofColor(120, 120, 120); break;
            case StageElementType::Layher: name = "Layher"; width = 100; height = 200; depth = 100; color = ofColor(80, 80, 90); break;
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

glm::mat4 StageElement::getGlobalTransformMatrix() const {
    return node.getGlobalTransformMatrix();
}

void StageElement::draw(bool selected) {
    ofPushMatrix();
    ofMultMatrix(node.getGlobalTransformMatrix());

    ofSetColor(color);
    switch (type) {
        case StageElementType::Floor: drawFloor(); break;
        case StageElementType::Truss: drawTruss(); break;
        case StageElementType::Layher: drawLayher(); break;
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
    float w = width, h = height, d = depth;
    float hw = w / 2, hh = h / 2, hd = d / 2;
    float railR = std::min(w, h) * 0.06f;

    // 4 corner positions (cross-section centered at origin, extends along Z)
    glm::vec3 corners[4] = {
        {-hw, 0, 0},  // bottom-left
        { hw, 0, 0},  // bottom-right
        { hw, h, 0},  // top-right
        {-hw, h, 0},  // top-left
    };

    // Draw 4 corner rails as cylinders along Z
    for (auto& c : corners) {
        ofPushMatrix();
        ofTranslate(c.x, c.y, 0);
        ofRotateXDeg(90);
        ofDrawCylinder(0, 0, 0, railR, d);
        ofPopMatrix();
    }

    // Cross-bracing on each face
    int bays = std::max(1, (int)(d / std::max(w, h)));
    float bayLen = d / bays;

    ofSetLineWidth(2);
    int faceIdx[4][2] = {{0,1}, {1,2}, {2,3}, {3,0}};
    for (auto& fc : faceIdx) {
        glm::vec3 a = corners[fc[0]];
        glm::vec3 b = corners[fc[1]];
        for (int bay = 0; bay < bays; bay++) {
            float z0 = -hd + bay * bayLen;
            float z1 = z0 + bayLen;
            // Diagonals
            ofDrawLine(a.x, a.y, z0, b.x, b.y, z1);
            ofDrawLine(b.x, b.y, z0, a.x, a.y, z1);
            // Horizontal at bay start
            ofDrawLine(a.x, a.y, z0, b.x, b.y, z0);
        }
        // Horizontal at far end
        ofDrawLine(a.x, a.y, hd, b.x, b.y, hd);
    }
    ofSetLineWidth(1);
}

void StageElement::drawLayher() {
    float w = width, h = height, d = depth;
    float hw = w / 2, hd = d / 2;
    float legR = std::min(w, d) * 0.04f;

    // 4 vertical legs at corners
    glm::vec3 legs[4] = {
        {-hw, h / 2, -hd},
        { hw, h / 2, -hd},
        { hw, h / 2,  hd},
        {-hw, h / 2,  hd},
    };

    for (auto& leg : legs) {
        ofDrawCylinder(leg.x, leg.y, leg.z, legR, h);
    }

    // Horizontal rails at bottom, middle, top
    ofSetLineWidth(2);
    float railLevels[] = {0.0f, h / 2, h};
    for (float ry : railLevels) {
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) % 4;
            ofDrawLine(legs[i].x, ry, legs[i].z,
                       legs[j].x, ry, legs[j].z);
        }
    }

    // Deck/platform on top
    float deckH = std::max(2.0f, h * 0.02f);
    ofDrawBox(0, h + deckH / 2, 0, w, deckH, d);

    // Diagonal bracing on two long faces
    ofSetLineWidth(1.5f);
    ofSetColor(color.r - 10, color.g - 10, color.b - 10);
    ofDrawLine(-hw, 0, -hd, hw, h, -hd);
    ofDrawLine(-hw, 0, hd, hw, h, hd);
    ofSetLineWidth(1);
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
    type = stringToType(j.value("type", "layher"));
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
