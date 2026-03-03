#include "win_byte_fix.h"
#include "SettingsModal.h"

static const char* unitLabels[] = { "Meters (m)", "Centimeters (cm)", "Feet (ft)", "Inches (in)" };
static const MeasurementUnit unitValues[] = {
    MeasurementUnit::Meters,
    MeasurementUnit::Centimeters,
    MeasurementUnit::Feet,
    MeasurementUnit::Inches
};
static const int unitCount = 4;

static const char* bgModeLabels[] = { "Solid Color", "Gradient", "Image" };
static const BackgroundMode bgModeValues[] = {
    BackgroundMode::Solid,
    BackgroundMode::Gradient,
    BackgroundMode::Image
};
static const int bgModeCount = 3;

// ─── Public API ──────────────────────────────────────────────────────────────

void SettingsModal::show(Preferences* p) {
    prefs = p;
    visible = true;
    if (prefs) {
        MeasurementUnit u = prefs->getUnit();
        for (int i = 0; i < unitCount; i++) {
            if (unitValues[i] == u) { selectedUnitIndex = i; break; }
        }
        BackgroundMode bm = prefs->getBgMode();
        for (int i = 0; i < bgModeCount; i++) {
            if (bgModeValues[i] == bm) { selectedBgMode = i; break; }
        }
    }
}

void SettingsModal::hide() {
    visible = false;
}

// ─── Input handling ──────────────────────────────────────────────────────────

void SettingsModal::keyPressed(int key) {
    if (!visible) return;

    if (key == OF_KEY_ESC) {
        hide();
        return;
    }

    if (key == OF_KEY_RETURN) {
        hide();
        return;
    }
}

void SettingsModal::mousePressed(int x, int y) {
    if (!visible) return;

    float W = ofGetWidth();
    float H = ofGetHeight();
    float panelW = 360;
    float panelH = 420;
    float px = (W - panelW) / 2;
    float py = (H - panelH) / 2;

    // Close button (top-right corner)
    float closeX = px + panelW - 30;
    float closeY = py + 5;
    if (x >= closeX && x <= closeX + 25 && y >= closeY && y <= closeY + 25) {
        hide();
        return;
    }

    // --- Measurement Unit radio buttons ---
    float radioX = px + 30;
    float radioStartY = py + 80;
    float radioH = 28;

    for (int i = 0; i < unitCount; i++) {
        float ry = radioStartY + i * radioH;
        if (x >= radioX && x <= px + panelW - 30 &&
            y >= ry && y <= ry + radioH) {
            selectedUnitIndex = i;
            if (prefs) {
                prefs->setUnit(unitValues[selectedUnitIndex]);
                prefs->saveLocal();
                if (onPreferenceChanged) onPreferenceChanged();
            }
            return;
        }
    }

    // --- Background Mode radio buttons ---
    float bgSectionY = radioStartY + unitCount * radioH + 40;
    float bgRadioStartY = bgSectionY + 20;

    for (int i = 0; i < bgModeCount; i++) {
        float ry = bgRadioStartY + i * radioH;
        if (x >= radioX && x <= px + panelW - 30 &&
            y >= ry && y <= ry + radioH) {
            selectedBgMode = i;
            if (prefs) {
                prefs->setBgMode(bgModeValues[selectedBgMode]);

                // If Image mode, prompt for file
                if (bgModeValues[selectedBgMode] == BackgroundMode::Image) {
                    ofFileDialogResult result = ofSystemLoadDialog("Load Background Image", false);
                    if (result.bSuccess) {
                        prefs->setBgImagePath(result.getPath());
                    }
                }

                prefs->saveLocal();
                if (onPreferenceChanged) onPreferenceChanged();
            }
            return;
        }
    }

    // --- Color edit buttons (Solid, Gradient top, Gradient bottom) ---
    float colorBtnsY = bgRadioStartY + bgModeCount * radioH + 15;
    float btnW = 120;
    float btnH = 24;

    if (prefs) {
        BackgroundMode mode = prefs->getBgMode();
        if (mode == BackgroundMode::Solid) {
            // "Edit Color" button
            if (x >= radioX && x <= radioX + btnW && y >= colorBtnsY && y <= colorBtnsY + btnH) {
                ofColor c = prefs->getBgColor();
                std::string input = ofSystemTextBoxDialog("Solid Color (R,G,B)",
                    ofToString(c.r) + "," + ofToString(c.g) + "," + ofToString(c.b));
                if (!input.empty()) {
                    auto parts = ofSplitString(input, ",");
                    if (parts.size() >= 3) {
                        prefs->setBgColor(ofColor(ofToInt(parts[0]), ofToInt(parts[1]), ofToInt(parts[2])));
                        prefs->saveLocal();
                        if (onPreferenceChanged) onPreferenceChanged();
                    }
                }
                return;
            }
        } else if (mode == BackgroundMode::Gradient) {
            // "Top Color" button
            if (x >= radioX && x <= radioX + btnW && y >= colorBtnsY && y <= colorBtnsY + btnH) {
                ofColor c = prefs->getBgGradientTop();
                std::string input = ofSystemTextBoxDialog("Gradient Top (R,G,B)",
                    ofToString(c.r) + "," + ofToString(c.g) + "," + ofToString(c.b));
                if (!input.empty()) {
                    auto parts = ofSplitString(input, ",");
                    if (parts.size() >= 3) {
                        prefs->setBgGradientTop(ofColor(ofToInt(parts[0]), ofToInt(parts[1]), ofToInt(parts[2])));
                        prefs->saveLocal();
                        if (onPreferenceChanged) onPreferenceChanged();
                    }
                }
                return;
            }
            // "Bottom Color" button
            float btn2Y = colorBtnsY + btnH + 8;
            if (x >= radioX && x <= radioX + btnW && y >= btn2Y && y <= btn2Y + btnH) {
                ofColor c = prefs->getBgGradientBottom();
                std::string input = ofSystemTextBoxDialog("Gradient Bottom (R,G,B)",
                    ofToString(c.r) + "," + ofToString(c.g) + "," + ofToString(c.b));
                if (!input.empty()) {
                    auto parts = ofSplitString(input, ",");
                    if (parts.size() >= 3) {
                        prefs->setBgGradientBottom(ofColor(ofToInt(parts[0]), ofToInt(parts[1]), ofToInt(parts[2])));
                        prefs->saveLocal();
                        if (onPreferenceChanged) onPreferenceChanged();
                    }
                }
                return;
            }
        }
    }

    // Click outside panel = close
    if (x < px || x > px + panelW || y < py || y > py + panelH) {
        hide();
        return;
    }
}

// ─── Drawing ─────────────────────────────────────────────────────────────────

static void drawRadio(float x, float y, float radioH, bool selected, const char* label) {
    float circleX = x + 8;
    float circleY = y + radioH / 2;
    float radius = 7;

    ofNoFill();
    ofSetColor(selected ? ofColor(0, 150, 255) : ofColor(100));
    ofSetLineWidth(2);
    ofDrawCircle(circleX, circleY, radius);
    ofFill();
    ofSetLineWidth(1);

    if (selected) {
        ofSetColor(0, 150, 255);
        ofDrawCircle(circleX, circleY, 4);
    }

    ofSetColor(selected ? ofColor(255) : ofColor(180));
    ofDrawBitmapString(label, x + 24, circleY + 4);
}

void SettingsModal::draw() {
    if (!visible) return;

    float W = ofGetWidth();
    float H = ofGetHeight();
    float panelW = 360;
    float panelH = 420;
    float px = (W - panelW) / 2;
    float py = (H - panelH) / 2;

    // Dim background
    ofSetColor(0, 0, 0, 180);
    ofDrawRectangle(0, 0, W, H);

    // Shadow
    ofSetColor(0, 0, 0, 100);
    ofDrawRectangle(px + 5, py + 5, panelW, panelH);

    // Panel background
    ofSetColor(38, 38, 38);
    ofDrawRectangle(px, py, panelW, panelH);

    // Panel border
    ofNoFill();
    ofSetLineWidth(2);
    ofSetColor(0, 120, 200);
    ofDrawRectangle(px, py, panelW, panelH);
    ofFill();
    ofSetLineWidth(1);

    // Title
    ofSetColor(0, 180, 255);
    std::string title = "Settings";
    ofDrawBitmapString(title, px + (panelW - title.size() * 8) / 2, py + 25);

    // Separator
    ofSetColor(60);
    ofDrawLine(px + 15, py + 40, px + panelW - 15, py + 40);

    // --- Measurement Unit section ---
    ofSetColor(180);
    ofDrawBitmapString("Measurement Unit", px + 30, py + 65);

    float radioX = px + 30;
    float radioStartY = py + 80;
    float radioH = 28;

    for (int i = 0; i < unitCount; i++) {
        drawRadio(radioX, radioStartY + i * radioH, radioH, i == selectedUnitIndex, unitLabels[i]);
    }

    // --- Background section ---
    float bgSectionY = radioStartY + unitCount * radioH + 10;

    ofSetColor(60);
    ofDrawLine(px + 15, bgSectionY, px + panelW - 15, bgSectionY);

    bgSectionY += 10;
    ofSetColor(180);
    ofDrawBitmapString("Background", px + 30, bgSectionY + 15);
    float bgRadioStartY = bgSectionY + 25;

    for (int i = 0; i < bgModeCount; i++) {
        drawRadio(radioX, bgRadioStartY + i * radioH, radioH, i == selectedBgMode, bgModeLabels[i]);
    }

    // Color edit hints/buttons
    float colorBtnsY = bgRadioStartY + bgModeCount * radioH + 10;
    float btnW = 120;
    float btnH = 24;

    if (prefs) {
        BackgroundMode mode = prefs->getBgMode();
        if (mode == BackgroundMode::Solid) {
            ofColor c = prefs->getBgColor();
            // Color swatch
            ofSetColor(c);
            ofDrawRectangle(radioX, colorBtnsY, 20, btnH);
            // Button
            ofSetColor(80);
            ofDrawRectangle(radioX + 25, colorBtnsY, btnW, btnH);
            ofSetColor(200);
            ofDrawBitmapString("Edit Color", radioX + 30, colorBtnsY + 16);
        } else if (mode == BackgroundMode::Gradient) {
            ofColor ct = prefs->getBgGradientTop();
            ofColor cb = prefs->getBgGradientBottom();
            // Top
            ofSetColor(ct);
            ofDrawRectangle(radioX, colorBtnsY, 20, btnH);
            ofSetColor(80);
            ofDrawRectangle(radioX + 25, colorBtnsY, btnW, btnH);
            ofSetColor(200);
            ofDrawBitmapString("Top Color", radioX + 30, colorBtnsY + 16);
            // Bottom
            float btn2Y = colorBtnsY + btnH + 8;
            ofSetColor(cb);
            ofDrawRectangle(radioX, btn2Y, 20, btnH);
            ofSetColor(80);
            ofDrawRectangle(radioX + 25, btn2Y, btnW, btnH);
            ofSetColor(200);
            ofDrawBitmapString("Bottom Color", radioX + 30, btn2Y + 16);
        } else if (mode == BackgroundMode::Image) {
            std::string path = prefs->getBgImagePath();
            ofSetColor(160);
            if (path.empty()) {
                ofDrawBitmapString("No image loaded", radioX, colorBtnsY + 16);
            } else {
                std::string fname = ofFilePath::getFileName(path);
                ofDrawBitmapString(fname, radioX, colorBtnsY + 16);
            }
        }
    }

    // Close button (X) — top-right
    float closeX = px + panelW - 30;
    float closeY2 = py + 8;
    ofSetColor(150);
    ofDrawBitmapString("X", closeX + 8, closeY2 + 13);

    // Hint at bottom
    ofSetColor(100);
    ofDrawBitmapString("ESC to close", px + (panelW - 12 * 8) / 2, py + panelH - 15);

    ofSetColor(255);
}
