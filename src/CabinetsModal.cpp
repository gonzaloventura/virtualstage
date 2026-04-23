#include "win_byte_fix.h"
#include "CabinetsModal.h"
#include <sstream>

// Layout constants
static constexpr float kPanelW      = 500;
static constexpr float kPanelH      = 420;
static constexpr float kHeaderH     = 45;
static constexpr float kFooterH     = 50;
static constexpr float kRowH        = 32;
static constexpr float kIconSize    = 18;

// ─── Public API ──────────────────────────────────────────────────────────────

void CabinetsModal::show(CabinetLibrary* l) {
    lib     = l;
    visible = true;
    scroll  = 0;
}

void CabinetsModal::hide() {
    visible = false;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

bool CabinetsModal::promptEdit(Cabinet& c, bool isNew) {
    // Single dialog: "name, pitchMm, panelWpx, panelHpx"
    std::string preset = isNew
        ? "Absen A3 Pro, 3.9, 128, 128"
        : c.name + ", " + ofToString(c.pitchMm, 2)
              + ", " + ofToString(c.panelWpx)
              + ", " + ofToString(c.panelHpx);

    std::string prompt =
        "Cabinet as: name, pitch (mm), panel W (px), panel H (px)\n"
        "Example:  Absen A3 Pro, 3.9, 128, 128";

    std::string input = ofSystemTextBoxDialog(prompt, preset);
    if (input.empty()) return false;

    auto parts = ofSplitString(input, ",");
    if (parts.size() < 4) {
        ofLogWarning("CabinetsModal") << "Expected 4 comma-separated fields, got " << parts.size();
        return false;
    }

    // Trim whitespace
    for (auto& p : parts) {
        size_t a = p.find_first_not_of(" \t");
        size_t b = p.find_last_not_of(" \t");
        p = (a == std::string::npos) ? "" : p.substr(a, b - a + 1);
    }

    std::string name = parts[0];
    float       pitch = 0;
    int         pw = 0, ph = 0;
    try {
        pitch = std::stof(parts[1]);
        pw    = std::stoi(parts[2]);
        ph    = std::stoi(parts[3]);
    } catch (...) {
        ofLogWarning("CabinetsModal") << "Could not parse numeric fields in: " << input;
        return false;
    }

    if (name.empty() || pitch <= 0 || pw <= 0 || ph <= 0) {
        ofLogWarning("CabinetsModal") << "Invalid values (name empty or non-positive numbers)";
        return false;
    }

    c.name     = name;
    c.pitchMm  = pitch;
    c.panelWpx = pw;
    c.panelHpx = ph;
    return true;
}

// ─── Input handling ──────────────────────────────────────────────────────────

void CabinetsModal::keyPressed(int key) {
    if (!visible) return;
    if (key == OF_KEY_ESC || key == OF_KEY_RETURN) hide();
}

void CabinetsModal::mousePressed(int x, int y) {
    if (!visible || !lib) return;

    float W  = ofGetWidth();
    float H  = ofGetHeight();
    float px = (W - kPanelW) / 2;
    float py = (H - kPanelH) / 2;

    // Close button (top-right)
    float closeX = px + kPanelW - 30;
    float closeY = py + 5;
    if (x >= closeX && x <= closeX + 25 && y >= closeY && y <= closeY + 25) {
        hide(); return;
    }

    // [+] Add button — bottom-left of panel
    float addBtnX = px + 20;
    float addBtnY = py + kPanelH - 38;
    float addBtnW = 140;
    float addBtnH = 26;
    if (x >= addBtnX && x <= addBtnX + addBtnW &&
        y >= addBtnY && y <= addBtnY + addBtnH) {
        Cabinet c;
        if (promptEdit(c, true)) {
            lib->add(c);
            if (onLibraryChanged) onLibraryChanged();
        }
        return;
    }

    // Row clicks: detect edit/delete icon hits
    float listTop    = py + kHeaderH;
    float listBottom = py + kPanelH - kFooterH;

    if (y < listTop || y > listBottom) {
        // Click outside list area but inside panel = stay
        if (x < px || x > px + kPanelW || y < py || y > py + kPanelH) hide();
        return;
    }

    auto cabs = lib->list();
    for (int i = 0; i < (int)cabs.size(); i++) {
        float rowTop = listTop + i * kRowH - scroll;
        if (rowTop + kRowH < listTop || rowTop > listBottom) continue;

        // Edit icon
        float editX = px + kPanelW - 70;
        float editY = rowTop + (kRowH - kIconSize) / 2;
        if (x >= editX && x <= editX + kIconSize &&
            y >= editY && y <= editY + kIconSize) {
            Cabinet c = cabs[i];
            if (promptEdit(c, false)) {
                lib->update(c);
                if (onLibraryChanged) onLibraryChanged();
            }
            return;
        }

        // Delete icon
        float delX = px + kPanelW - 40;
        float delY = editY;
        if (x >= delX && x <= delX + kIconSize &&
            y >= delY && y <= delY + kIconSize) {
            lib->remove(cabs[i].id);
            if (onLibraryChanged) onLibraryChanged();
            return;
        }
    }

    // Click outside the panel bounds = close
    if (x < px || x > px + kPanelW || y < py || y > py + kPanelH) hide();
}

// ─── Drawing ─────────────────────────────────────────────────────────────────

void CabinetsModal::draw() {
    if (!visible || !lib) return;

    float W  = ofGetWidth();
    float H  = ofGetHeight();
    float px = (W - kPanelW) / 2;
    float py = (H - kPanelH) / 2;

    // Dim background
    ofSetColor(0, 0, 0, 180);
    ofDrawRectangle(0, 0, W, H);

    // Shadow + panel + border
    ofSetColor(0, 0, 0, 100);
    ofDrawRectangle(px + 5, py + 5, kPanelW, kPanelH);
    ofSetColor(38, 38, 38);
    ofDrawRectangle(px, py, kPanelW, kPanelH);
    ofNoFill();
    ofSetLineWidth(2);
    ofSetColor(0, 120, 200);
    ofDrawRectangle(px, py, kPanelW, kPanelH);
    ofFill();
    ofSetLineWidth(1);

    // Title
    ofSetColor(0, 180, 255);
    std::string title = "Cabinets (LED panel library)";
    ofDrawBitmapString(title, px + (kPanelW - title.size() * 8) / 2, py + 25);

    // Separator
    ofSetColor(60);
    ofDrawLine(px + 15, py + 40, px + kPanelW - 15, py + 40);

    // Column headers
    ofSetColor(120);
    ofDrawBitmapString("Name",                 px + 20,  py + 62);
    ofDrawBitmapString("Pitch",                px + 220, py + 62);
    ofDrawBitmapString("Panel (px)",           px + 290, py + 62);
    ofDrawBitmapString("Size (m)",             px + 380, py + 62);

    // List clip region (via scissor — avoids drawing over header/footer)
    float listTop    = py + kHeaderH;
    float listBottom = py + kPanelH - kFooterH;
    float listH      = listBottom - listTop;

    auto cabs = lib->list();
    if (cabs.empty()) {
        ofSetColor(90);
        std::string hint = "No cabinets yet — click [+ Add cabinet] below.";
        ofDrawBitmapString(hint, px + (kPanelW - hint.size() * 8) / 2, listTop + listH / 2);
    } else {
        for (int i = 0; i < (int)cabs.size(); i++) {
            const auto& c    = cabs[i];
            float rowTop     = listTop + i * kRowH - scroll;
            if (rowTop + kRowH < listTop || rowTop > listBottom) continue;

            // Alternating row background
            if (i % 2 == 0) {
                ofSetColor(45, 45, 45);
                ofDrawRectangle(px + 10, rowTop, kPanelW - 20, kRowH);
            }

            float textY = rowTop + 20;

            // Name (clipped)
            ofSetColor(230);
            std::string nm = c.name;
            if (nm.size() > 24) nm = nm.substr(0, 22) + "..";
            ofDrawBitmapString(nm, px + 20, textY);

            // Pitch
            ofSetColor(200);
            ofDrawBitmapString(ofToString(c.pitchMm, 2) + " mm", px + 220, textY);

            // Panel resolution
            ofDrawBitmapString(ofToString(c.panelWpx) + " x " + ofToString(c.panelHpx),
                               px + 290, textY);

            // Physical size (meters)
            float wM = c.panelWidthMm()  / 1000.0f;
            float hM = c.panelHeightMm() / 1000.0f;
            ofDrawBitmapString(ofToString(wM, 2) + " x " + ofToString(hM, 2),
                               px + 380, textY);

            // Edit icon
            float editX = px + kPanelW - 70;
            float editY = rowTop + (kRowH - kIconSize) / 2;
            ofNoFill();
            ofSetColor(120, 170, 220);
            ofDrawRectangle(editX, editY, kIconSize, kIconSize);
            ofFill();
            ofSetColor(150, 200, 240);
            ofDrawBitmapString("e", editX + 6, editY + 13);

            // Delete icon
            float delX = px + kPanelW - 40;
            ofNoFill();
            ofSetColor(200, 90, 90);
            ofDrawRectangle(delX, editY, kIconSize, kIconSize);
            ofFill();
            ofSetColor(220, 120, 120);
            ofDrawBitmapString("x", delX + 6, editY + 13);
        }
    }

    // Footer: [+ Add cabinet]
    ofSetColor(60);
    ofDrawLine(px + 15, listBottom, px + kPanelW - 15, listBottom);

    float addBtnX = px + 20;
    float addBtnY = py + kPanelH - 38;
    float addBtnW = 140;
    float addBtnH = 26;
    ofSetColor(0, 140, 100);
    ofDrawRectangle(addBtnX, addBtnY, addBtnW, addBtnH);
    ofSetColor(255);
    ofDrawBitmapString("+ Add cabinet", addBtnX + 18, addBtnY + 18);

    // Close X
    ofSetColor(150);
    ofDrawBitmapString("X", px + kPanelW - 22, py + 21);

    // Footer hint
    ofSetColor(100);
    std::string hint = "ESC to close";
    ofDrawBitmapString(hint, px + kPanelW - hint.size() * 8 - 20, py + kPanelH - 18);

    ofSetColor(255);
}
