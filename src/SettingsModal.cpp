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

// ─── Public API ──────────────────────────────────────────────────────────────

void SettingsModal::show(Preferences* p) {
    prefs = p;
    visible = true;
    if (prefs) {
        MeasurementUnit u = prefs->getUnit();
        for (int i = 0; i < unitCount; i++) {
            if (unitValues[i] == u) { selectedUnitIndex = i; break; }
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

    if (key == OF_KEY_UP) {
        selectedUnitIndex = std::max(0, selectedUnitIndex - 1);
    } else if (key == OF_KEY_DOWN) {
        selectedUnitIndex = std::min(unitCount - 1, selectedUnitIndex + 1);
    } else if (key == OF_KEY_RETURN) {
        if (prefs) {
            prefs->setUnit(unitValues[selectedUnitIndex]);
            prefs->saveLocal();
            if (onPreferenceChanged) onPreferenceChanged();
        }
        hide();
        return;
    }

    // Apply selection immediately on arrow navigation
    if (prefs && (key == OF_KEY_UP || key == OF_KEY_DOWN)) {
        prefs->setUnit(unitValues[selectedUnitIndex]);
        prefs->saveLocal();
        if (onPreferenceChanged) onPreferenceChanged();
    }
}

void SettingsModal::mousePressed(int x, int y) {
    if (!visible) return;

    float W = ofGetWidth();
    float H = ofGetHeight();
    float panelW = 340;
    float panelH = 260;
    float px = (W - panelW) / 2;
    float py = (H - panelH) / 2;

    // Close button (top-right corner)
    float closeX = px + panelW - 30;
    float closeY = py + 5;
    if (x >= closeX && x <= closeX + 25 && y >= closeY && y <= closeY + 25) {
        hide();
        return;
    }

    // Radio button rows
    float radioX = px + 30;
    float radioStartY = py + 80;
    float radioH = 30;

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

    // Click outside panel = close
    if (x < px || x > px + panelW || y < py || y > py + panelH) {
        hide();
        return;
    }
}

// ─── Drawing ─────────────────────────────────────────────────────────────────

void SettingsModal::draw() {
    if (!visible) return;

    float W = ofGetWidth();
    float H = ofGetHeight();
    float panelW = 340;
    float panelH = 260;
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

    // Section label
    ofSetColor(180);
    ofDrawBitmapString("Measurement Unit", px + 30, py + 65);

    // Radio buttons
    float radioX = px + 30;
    float radioStartY = py + 80;
    float radioH = 30;

    for (int i = 0; i < unitCount; i++) {
        float ry = radioStartY + i * radioH;
        float circleX = radioX + 8;
        float circleY = ry + radioH / 2;
        float radius = 7;

        // Outer circle
        ofNoFill();
        ofSetColor(i == selectedUnitIndex ? ofColor(0, 150, 255) : ofColor(100));
        ofSetLineWidth(2);
        ofDrawCircle(circleX, circleY, radius);
        ofFill();
        ofSetLineWidth(1);

        // Inner dot (selected)
        if (i == selectedUnitIndex) {
            ofSetColor(0, 150, 255);
            ofDrawCircle(circleX, circleY, 4);
        }

        // Label
        ofSetColor(i == selectedUnitIndex ? ofColor(255) : ofColor(180));
        ofDrawBitmapString(unitLabels[i], radioX + 24, circleY + 4);
    }

    // Close button (X) — top-right
    float closeX = px + panelW - 30;
    float closeY = py + 8;
    ofSetColor(150);
    ofDrawBitmapString("X", closeX + 8, closeY + 13);

    // Hint at bottom
    ofSetColor(100);
    ofDrawBitmapString("ESC to close", px + (panelW - 12 * 8) / 2, py + panelH - 15);

    ofSetColor(255);
}
