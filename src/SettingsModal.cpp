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
    float panelH = 340;
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

    // --- View Mode checkboxes ---
    float viewSectionY = radioStartY + unitCount * radioH + 10;
    viewSectionY += 10;
    float cbY1 = viewSectionY + 10;
    float cbY2 = cbY1 + 28;
    float cbHitH = 24;

    if (prefs) {
        if (x >= radioX && x <= px + panelW - 30 &&
            y >= cbY1 && y <= cbY1 + cbHitH) {
            prefs->setHideStatusBarInView(!prefs->getHideStatusBarInView());
            prefs->saveLocal();
            if (onPreferenceChanged) onPreferenceChanged();
            return;
        }
        if (x >= radioX && x <= px + panelW - 30 &&
            y >= cbY2 && y <= cbY2 + cbHitH) {
            prefs->setHideTitleBarInView(!prefs->getHideTitleBarInView());
            prefs->saveLocal();
            if (onPreferenceChanged) onPreferenceChanged();
            return;
        }
    }

    // --- Updates section ---
    float updateSectionY = cbY2 + 28 + 10;
    updateSectionY += 10;
    float cbY3 = updateSectionY + 10;
    if (prefs) {
        if (x >= radioX && x <= px + panelW - 30 &&
            y >= cbY3 && y <= cbY3 + cbHitH) {
            prefs->setCheckForUpdatesOnStart(!prefs->getCheckForUpdatesOnStart());
            prefs->saveLocal();
            if (onPreferenceChanged) onPreferenceChanged();
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

static void drawCheckbox(float x, float y, float size, bool checked, const char* label) {
    ofNoFill();
    ofSetColor(checked ? ofColor(0, 150, 255) : ofColor(100));
    ofSetLineWidth(2);
    ofDrawRectangle(x, y + 4, size, size);
    ofFill();
    ofSetLineWidth(1);
    if (checked) {
        ofSetColor(0, 150, 255);
        ofDrawRectangle(x + 3, y + 7, size - 6, size - 6);
    }
    ofSetColor(180);
    ofDrawBitmapString(label, x + size + 10, y + 15);
}

void SettingsModal::draw() {
    if (!visible) return;

    float W = ofGetWidth();
    float H = ofGetHeight();
    float panelW = 360;
    float panelH = 340;
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
    float cbSize = 14;

    for (int i = 0; i < unitCount; i++) {
        drawRadio(radioX, radioStartY + i * radioH, radioH, i == selectedUnitIndex, unitLabels[i]);
    }

    // --- View Mode section ---
    float viewSectionY = radioStartY + unitCount * radioH + 10;

    ofSetColor(60);
    ofDrawLine(px + 15, viewSectionY, px + panelW - 15, viewSectionY);

    viewSectionY += 10;
    ofSetColor(180);
    ofDrawBitmapString("View Mode", px + 30, viewSectionY + 5);

    float cbY1 = viewSectionY + 10;
    float cbY2 = cbY1 + 28;

    if (prefs) {
        drawCheckbox(radioX, cbY1, cbSize, prefs->getHideStatusBarInView(), "Hide status bar");
        drawCheckbox(radioX, cbY2, cbSize, prefs->getHideTitleBarInView(), "Hide window title bar");
    }

    // --- Updates section ---
    float updateSectionY = cbY2 + 28 + 10;

    ofSetColor(60);
    ofDrawLine(px + 15, updateSectionY, px + panelW - 15, updateSectionY);

    updateSectionY += 10;
    ofSetColor(180);
    ofDrawBitmapString("Updates", px + 30, updateSectionY + 5);

    float cbY3 = updateSectionY + 10;
    if (prefs) {
        drawCheckbox(radioX, cbY3, cbSize, prefs->getCheckForUpdatesOnStart(), "Check for updates on startup");
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
