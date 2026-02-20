#include "win_byte_fix.h"
#include "ScreenObject.h"

ScreenObject::ScreenObject(const std::string& name, float width, float height)
    : name(name) {
    plane.set(width, height, 2, 2);
    plane.setPosition(0, 0, 0);
}

void ScreenObject::setPosition(const glm::vec3& pos) {
    plane.setPosition(pos);
}

void ScreenObject::setRotationEuler(const glm::vec3& eulerDeg) {
    plane.setOrientation(glm::vec3(eulerDeg.x, eulerDeg.y, eulerDeg.z));
}

void ScreenObject::setScale(const glm::vec3& s) {
    plane.setScale(s);
}

glm::vec3 ScreenObject::getPosition() const {
    return plane.getPosition();
}

glm::vec3 ScreenObject::getRotationEuler() const {
    return plane.getOrientationEulerDeg();
}

glm::vec3 ScreenObject::getScale() const {
    return plane.getScale();
}

// --- Curvature ---

void ScreenObject::setCurvature(float deg) {
    deg = ofClamp(deg, -180, 180);
    if (std::abs(curvature - deg) < 0.001f) return;
    curvature = deg;
    rebuildMesh();
}

float ScreenObject::getCurvature() const {
    return curvature;
}

// --- Crop ---

void ScreenObject::setCropRect(const ofRectangle& r) {
    cropRect = r;
    rebuildMesh();
}

const ofRectangle& ScreenObject::getCropRect() const {
    return cropRect;
}

// --- Polygon Mask ---

void ScreenObject::setMask(const std::vector<glm::vec2>& points) {
    maskPoints = points;
    rebuildPolygonMesh();
}

const std::vector<glm::vec2>& ScreenObject::getMaskPoints() const {
    return maskPoints;
}

bool ScreenObject::hasMask() const {
    return !maskPoints.empty();
}

// --- JSON Serialization ---

ofJson ScreenObject::toJson() const {
    ofJson j;
    j["name"] = name;
    j["width"] = plane.getWidth();
    j["height"] = plane.getHeight();

    auto pos = getPosition();
    j["position"] = {pos.x, pos.y, pos.z};

    auto rot = getRotationEuler();
    j["rotation"] = {rot.x, rot.y, rot.z};

    auto sc = getScale();
    j["scale"] = {sc.x, sc.y, sc.z};

    j["curvature"] = curvature;

    j["crop"] = {
        {"x", cropRect.x},
        {"y", cropRect.y},
        {"w", cropRect.width},
        {"h", cropRect.height}
    };

    if (!sourceName.empty()) {
        j["sourceName"] = sourceName;
    }

    if (!maskPoints.empty()) {
        ofJson maskArr = ofJson::array();
        for (auto& pt : maskPoints) {
            maskArr.push_back({pt.x, pt.y});
        }
        j["mask"] = maskArr;
    }

    return j;
}

void ScreenObject::fromJson(const ofJson& j) {
    if (j.contains("name")) name = j["name"].get<std::string>();

    float w = j.value("width", 320.0f);
    float h = j.value("height", 180.0f);
    plane.set(w, h, 2, 2);

    if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
        setPosition(glm::vec3(j["position"][0], j["position"][1], j["position"][2]));
    }

    if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
        setRotationEuler(glm::vec3(j["rotation"][0], j["rotation"][1], j["rotation"][2]));
    }

    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
        setScale(glm::vec3(j["scale"][0], j["scale"][1], j["scale"][2]));
    }

    setCurvature(j.value("curvature", 0.0f));

    if (j.contains("crop")) {
        auto& c = j["crop"];
        setCropRect(ofRectangle(
            c.value("x", 0.0f),
            c.value("y", 0.0f),
            c.value("w", 1.0f),
            c.value("h", 1.0f)
        ));
    }

    if (j.contains("sourceName")) {
        sourceName = j["sourceName"].get<std::string>();
    }

    if (j.contains("mask") && j["mask"].is_array()) {
        std::vector<glm::vec2> pts;
        for (auto& pt : j["mask"]) {
            if (pt.is_array() && pt.size() >= 2) {
                pts.push_back(glm::vec2(pt[0], pt[1]));
            }
        }
        if (pts.size() >= 3) {
            setMask(pts);
        }
    }
}

void ScreenObject::rebuildPolygonMesh() {
    polygonMesh.clear();
    if (maskPoints.size() < 3) return;

    float w = plane.getWidth();
    float h = plane.getHeight();

    // Use ofPath to tessellate the polygon
    ofPath path;
    path.setFilled(true);
    // maskPoints are normalized 0-1, convert to local coords centered on origin
    path.moveTo((maskPoints[0].x - 0.5f) * w, (0.5f - maskPoints[0].y) * h);
    for (size_t i = 1; i < maskPoints.size(); i++) {
        path.lineTo((maskPoints[i].x - 0.5f) * w, (0.5f - maskPoints[i].y) * h);
    }
    path.close();

    ofMesh tess = path.getTessellation();

    // Build VBO mesh with tex coords
    polygonMesh.setMode(OF_PRIMITIVE_TRIANGLES);
    auto& verts = tess.getVertices();
    auto& indices = tess.getIndices();

    for (auto& v : verts) {
        polygonMesh.addVertex(glm::vec3(v.x, v.y, 0));
        // Compute normalized UV from vertex position
        float u = (v.x / w) + 0.5f;
        float s = 0.5f - (v.y / h);
        float cu = cropRect.x + u * cropRect.width;
        float cv = cropRect.y + s * cropRect.height;
        polygonMesh.addTexCoord(glm::vec2(cu, cv));
        polygonMesh.addNormal(glm::vec3(0, 0, 1));
    }

    for (auto& idx : indices) {
        polygonMesh.addIndex(idx);
    }
}

// --- Mesh rebuild ---

void ScreenObject::rebuildMesh() {
    curvedMesh.clear();
    curvedMesh.setMode(OF_PRIMITIVE_TRIANGLES);

    float w = plane.getWidth();
    float h = plane.getHeight();
    float absCurv = std::abs(curvature);
    float sign = (curvature >= 0) ? 1.0f : -1.0f;
    float totalAngle = absCurv * DEG_TO_RAD;

    int cols = meshColumns;
    int rows = meshRows;

    // Generate vertices
    for (int j = 0; j <= rows; j++) {
        float s = (float)j / rows;  // 0 to 1
        float y = (s - 0.5f) * h;

        for (int i = 0; i <= cols; i++) {
            float t = (float)i / cols;  // 0 to 1

            float x, z;
            if (absCurv > 0.1f) {
                float radius = (w / 2.0f) / sin(totalAngle / 2.0f);
                float angle = (t - 0.5f) * totalAngle;
                x = radius * sin(angle);
                z = sign * radius * (cos(angle) - cos(totalAngle / 2.0f));
            } else {
                x = (t - 0.5f) * w;
                z = 0;
            }

            curvedMesh.addVertex(glm::vec3(x, y, z));

            // Tex coords: placeholder (updated before draw with actual texture size)
            // Store normalized crop coords for now
            float texU = cropRect.x + t * cropRect.width;
            float texV = cropRect.y + s * cropRect.height;
            curvedMesh.addTexCoord(glm::vec2(texU, texV));

            // Normal: pointing outward from arc
            if (absCurv > 0.1f) {
                float radius = (w / 2.0f) / sin(totalAngle / 2.0f);
                float angle = (t - 0.5f) * totalAngle;
                glm::vec3 normal = glm::normalize(glm::vec3(sin(angle), 0, sign * cos(angle)));
                curvedMesh.addNormal(normal);
            } else {
                curvedMesh.addNormal(glm::vec3(0, 0, 1));
            }
        }
    }

    // Generate indices
    for (int j = 0; j < rows; j++) {
        for (int i = 0; i < cols; i++) {
            int topLeft = j * (cols + 1) + i;
            int topRight = topLeft + 1;
            int bottomLeft = (j + 1) * (cols + 1) + i;
            int bottomRight = bottomLeft + 1;

            curvedMesh.addIndex(topLeft);
            curvedMesh.addIndex(bottomLeft);
            curvedMesh.addIndex(topRight);

            curvedMesh.addIndex(topRight);
            curvedMesh.addIndex(bottomLeft);
            curvedMesh.addIndex(bottomRight);
        }
    }
}

// --- Video Source (Syphon / Spout) ---

#ifdef TARGET_OSX
void ScreenObject::connectToSource(const ofxSyphonServerDescription& desc) {
    if (!clientSetup) {
        client.setup();
        clientSetup = true;
    }
    client.set(desc);
    sourceIndex = 0; // mark as connected
    sourceName = desc.appName + " - " + desc.serverName;
    ofLogNotice("ScreenObject") << name << " connected to: " << sourceName;
}
#elif defined(TARGET_WIN32)
void ScreenObject::connectToSource(const std::string& senderName) {
    if (spoutSetup) {
        spoutReceiver.release();
    }
    spoutReceiver.init(senderName);
    spoutSetup = true;
    sourceIndex = 0; // mark as connected
    sourceName = senderName;
    ofLogNotice("ScreenObject") << name << " connected to: " << sourceName;
}

void ScreenObject::updateSpout() {
    if (spoutSetup && hasSource()) {
        spoutReceiver.receive(spoutTexture);
    }
}
#endif

void ScreenObject::disconnectSource() {
#ifdef TARGET_OSX
    if (clientSetup) {
        client.set("", "");
    }
#elif defined(TARGET_WIN32)
    if (spoutSetup) {
        spoutReceiver.release();
        spoutSetup = false;
    }
#endif
    sourceIndex = -1;
    sourceName = "";
}

bool ScreenObject::hasSource() const {
    return sourceIndex >= 0;
}

// --- Drawing ---

// Helper: which mesh to use
// Returns 0=flat plane, 1=curved mesh, 2=polygon mesh
static int meshMode(bool hasMask, float curvature) {
    if (hasMask) return 2;
    if (std::abs(curvature) > 0.1f) return 1;
    return 0;
}

void ScreenObject::draw(bool viewMode) {
    bool textured = false;
    int mode = meshMode(!maskPoints.empty(), curvature);

    // Helper lambda: draw the current mesh (flat/curved/polygon)
    auto drawMesh = [&]() {
        if (mode == 2) {
            ofPushMatrix();
            ofMultMatrix(plane.getGlobalTransformMatrix());
            polygonMesh.draw();
            ofPopMatrix();
        } else if (mode == 1) {
            ofPushMatrix();
            ofMultMatrix(plane.getGlobalTransformMatrix());
            curvedMesh.draw();
            ofPopMatrix();
        } else {
            plane.draw();
        }
    };

    // In View mode, draw solid black base first (like a real LED panel —
    // alpha in the Syphon source will composite against black, not transparent)
    if (viewMode) {
        ofSetColor(0);
        drawMesh();
    }

    // Draw video source texture on top
#ifdef TARGET_OSX
    if (hasSource() && clientSetup) {
        if (client.lockTexture()) {
            // Allow texture to pass depth test at same Z as the black base
            if (viewMode) glDepthFunc(GL_LEQUAL);
            ofTexture& tex = client.getTexture();
            bool flipped = tex.getTextureData().bFlipTexture;

            if (mode == 1) {
                // Curved mesh - update tex coords
                auto& texCoords = curvedMesh.getTexCoords();
                int cols = meshColumns;
                int rows = meshRows;
                for (int j = 0; j <= rows; j++) {
                    float s = (float)j / rows;
                    for (int i = 0; i <= cols; i++) {
                        float t = (float)i / cols;
                        float cu = cropRect.x + t * cropRect.width;
                        float cv = cropRect.y + s * cropRect.height;
                        if (flipped) cv = 1.0f - cv;
                        texCoords[j * (cols + 1) + i] = tex.getCoordFromPercent(cu, cv);
                    }
                }

                tex.bind();
                ofSetColor(255);
                drawMesh();
                tex.unbind();
            } else if (mode == 2) {
                // Polygon mesh - update tex coords from vertex positions
                auto& texCoords = polygonMesh.getTexCoords();
                auto& verts = polygonMesh.getVertices();
                float pw = plane.getWidth();
                float ph = plane.getHeight();
                for (size_t i = 0; i < texCoords.size(); i++) {
                    float u = (verts[i].x / pw) + 0.5f;
                    float v = 0.5f - (verts[i].y / ph);
                    float cu = cropRect.x + u * cropRect.width;
                    float cv = cropRect.y + v * cropRect.height;
                    if (flipped) cv = 1.0f - cv;
                    texCoords[i] = tex.getCoordFromPercent(cu, cv);
                }

                tex.bind();
                ofSetColor(255);
                drawMesh();
                tex.unbind();
            } else {
                // Flat plane
                auto& mesh = plane.getMesh();
                auto& texCoords = mesh.getTexCoords();
                auto& verts = mesh.getVertices();
                float pw = plane.getWidth();
                float ph = plane.getHeight();

                for (size_t i = 0; i < texCoords.size(); i++) {
                    float u = (verts[i].x / pw) + 0.5f;
                    float v = 0.5f - (verts[i].y / ph);
                    float cu = cropRect.x + u * cropRect.width;
                    float cv = cropRect.y + v * cropRect.height;
                    if (flipped) cv = 1.0f - cv;
                    texCoords[i] = tex.getCoordFromPercent(cu, cv);
                }

                tex.bind();
                ofSetColor(255);
                drawMesh();
                tex.unbind();
            }

            if (viewMode) glDepthFunc(GL_LESS); // restore default
            textured = true;
            client.unlockTexture();
        }
    }
#elif defined(TARGET_WIN32)
    if (hasSource() && spoutSetup && spoutTexture.isAllocated()) {
        if (viewMode) glDepthFunc(GL_LEQUAL);
        ofTexture& tex = spoutTexture;
        bool flipped = tex.getTextureData().bFlipTexture;

        if (mode == 1) {
            auto& texCoords = curvedMesh.getTexCoords();
            int cols = meshColumns;
            int rows = meshRows;
            for (int j = 0; j <= rows; j++) {
                float s = (float)j / rows;
                for (int i = 0; i <= cols; i++) {
                    float t = (float)i / cols;
                    float cu = cropRect.x + t * cropRect.width;
                    float cv = cropRect.y + s * cropRect.height;
                    if (flipped) cv = 1.0f - cv;
                    texCoords[j * (cols + 1) + i] = tex.getCoordFromPercent(cu, cv);
                }
            }
            tex.bind();
            ofSetColor(255);
            drawMesh();
            tex.unbind();
        } else if (mode == 2) {
            auto& texCoords = polygonMesh.getTexCoords();
            auto& verts = polygonMesh.getVertices();
            float pw = plane.getWidth();
            float ph = plane.getHeight();
            for (size_t i = 0; i < texCoords.size(); i++) {
                float u = (verts[i].x / pw) + 0.5f;
                float v = 0.5f - (verts[i].y / ph);
                float cu = cropRect.x + u * cropRect.width;
                float cv = cropRect.y + v * cropRect.height;
                if (flipped) cv = 1.0f - cv;
                texCoords[i] = tex.getCoordFromPercent(cu, cv);
            }
            tex.bind();
            ofSetColor(255);
            drawMesh();
            tex.unbind();
        } else {
            auto& mesh = plane.getMesh();
            auto& texCoords = mesh.getTexCoords();
            auto& verts = mesh.getVertices();
            float pw = plane.getWidth();
            float ph = plane.getHeight();
            for (size_t i = 0; i < texCoords.size(); i++) {
                float u = (verts[i].x / pw) + 0.5f;
                float v = 0.5f - (verts[i].y / ph);
                float cu = cropRect.x + u * cropRect.width;
                float cv = cropRect.y + v * cropRect.height;
                if (flipped) cv = 1.0f - cv;
                texCoords[i] = tex.getCoordFromPercent(cu, cv);
            }
            tex.bind();
            ofSetColor(255);
            drawMesh();
            tex.unbind();
        }

        if (viewMode) glDepthFunc(GL_LESS);
        textured = true;
    }
#endif

    // No texture: solid fill (only if black base wasn't already drawn in view mode)
    if (!textured && !viewMode) {
        ofSetColor(80);
        drawMesh();
    }

    // Border outline - only in Designer mode
    if (!viewMode) {
        ofSetColor(60);
        ofNoFill();
        if (mode == 2) {
            ofPushMatrix();
            ofMultMatrix(plane.getGlobalTransformMatrix());
            polygonMesh.drawWireframe();
            ofPopMatrix();
        } else if (mode == 1) {
            ofPushMatrix();
            ofMultMatrix(plane.getGlobalTransformMatrix());
            curvedMesh.drawWireframe();
            ofPopMatrix();
        } else {
            plane.drawWireframe();
        }
        ofFill();
    }
    ofSetColor(255);
}

void ScreenObject::drawSelected() {
    int mode = meshMode(!maskPoints.empty(), curvature);
    ofSetColor(0, 200, 255);
    ofNoFill();
    if (mode == 2) {
        ofPushMatrix();
        ofMultMatrix(plane.getGlobalTransformMatrix());
        polygonMesh.drawWireframe();
        ofPopMatrix();
    } else if (mode == 1) {
        ofPushMatrix();
        ofMultMatrix(plane.getGlobalTransformMatrix());
        curvedMesh.drawWireframe();
        ofPopMatrix();
    } else {
        plane.drawWireframe();
    }
    ofFill();
    ofSetColor(255);
}

bool ScreenObject::drawSourceTexture(const ofRectangle& destRect) {
#ifdef TARGET_OSX
    if (hasSource() && clientSetup) {
        if (client.lockTexture()) {
            ofTexture& tex = client.getTexture();
            ofSetColor(255);
            tex.draw(destRect.x, destRect.y, destRect.width, destRect.height);
            client.unlockTexture();
            return true;
        }
    }
#elif defined(TARGET_WIN32)
    if (hasSource() && spoutSetup && spoutTexture.isAllocated()) {
        ofSetColor(255);
        spoutTexture.draw(destRect.x, destRect.y, destRect.width, destRect.height);
        return true;
    }
#endif
    return false;
}

// --- Picking support ---

glm::vec3 ScreenObject::getWorldNormal() const {
    return glm::normalize(glm::vec3(
        plane.getGlobalTransformMatrix() * glm::vec4(0, 0, 1, 0)));
}

glm::vec3 ScreenObject::getWorldCenter() const {
    return glm::vec3(plane.getGlobalTransformMatrix() * glm::vec4(0, 0, 0, 1));
}

float ScreenObject::getPlaneWidth() const {
    return plane.getWidth();
}

float ScreenObject::getPlaneHeight() const {
    return plane.getHeight();
}
