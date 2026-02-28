#include "win_byte_fix.h"
#include "AuthModal.h"

// ─── Public API ──────────────────────────────────────────────────────────────

void AuthModal::show() {
    visible         = true;
    activeTab       = Tab::Login;
    activeField     = 0;
    emailInput.clear();
    passwordInput.clear();
    confirmInput.clear();
    loading.store(false);
    loginSuccessFlag.store(false);
    registerConfirmFlag.store(false);
    std::lock_guard<std::mutex> lock(msgMutex);
    errorMessage.clear();
    successMessage.clear();
}

void AuthModal::hide() {
    visible = false;
    loading.store(false);
}

void AuthModal::setError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(msgMutex);
    errorMessage   = msg;
    successMessage.clear();
    loading.store(false);
}

void AuthModal::setSuccess(const std::string& msg) {
    std::lock_guard<std::mutex> lock(msgMutex);
    successMessage = msg;
    errorMessage.clear();
}

void AuthModal::clearMessages() {
    std::lock_guard<std::mutex> lock(msgMutex);
    errorMessage.clear();
    successMessage.clear();
}

bool AuthModal::consumeLoginSuccess() {
    return loginSuccessFlag.exchange(false);
}

bool AuthModal::consumeRegisterConfirm() {
    return registerConfirmFlag.exchange(false);
}

std::string& AuthModal::fieldAt(int i) {
    if (i == 0) return emailInput;
    if (i == 1) return passwordInput;
    return confirmInput;
}

// ─── Input handling ───────────────────────────────────────────────────────────

void AuthModal::keyPressed(int key) {
    if (!visible || loading.load()) return;

    if (key == OF_KEY_TAB) {
        activeField = (activeField + 1) % fieldCount();
        return;
    }

    if (key == OF_KEY_RETURN) {
        // Submit on Enter
        if (!emailInput.empty() && !passwordInput.empty()) {
            if (activeTab == Tab::Register && confirmInput.empty()) {
                activeField = 2;
                return;
            }
            // Validate confirm password
            if (activeTab == Tab::Register && passwordInput != confirmInput) {
                setError("Passwords do not match");
                return;
            }
            if (onSubmit) {
                loading.store(true);
                clearMessages();
                onSubmit(activeTab, emailInput, passwordInput, confirmInput);
            }
        }
        return;
    }

    if (key == OF_KEY_BACKSPACE) {
        auto& f = fieldAt(activeField);
        if (!f.empty()) f.pop_back();
        return;
    }

    // Printable ASCII characters
    if (key >= 32 && key <= 126) {
        fieldAt(activeField) += (char)key;
        return;
    }
}

void AuthModal::mousePressed(int x, int y) {
    if (!visible) return;

    float w = ofGetWidth();
    float h = ofGetHeight();
    bool isRegister = (activeTab == Tab::Register);
    float panelW = 420;
    float panelH = isRegister ? 330 : 280;
    float px     = (w - panelW) / 2;
    float py     = (h - panelH) / 2;

    // Tab clicks (top of panel)
    float tabY  = py + 40;
    float tabH  = 30;
    float tabW  = panelW / 2;

    if (y >= tabY && y < tabY + tabH) {
        if (x >= px && x < px + tabW) {
            activeTab   = Tab::Login;
            activeField = 0;
            clearMessages();
        } else if (x >= px + tabW && x < px + panelW) {
            activeTab   = Tab::Register;
            activeField = 0;
            clearMessages();
        }
        return;
    }

    if (loading.load()) return;

    // Field clicks — compute field positions (same as draw)
    float fieldX = px + 30;
    float fieldW = panelW - 60;
    float fieldH = 28;
    float startY = tabY + tabH + 20;
    float fieldGap = 60;

    for (int i = 0; i < fieldCount(); i++) {
        float fy = startY + i * fieldGap;
        if (x >= fieldX && x <= fieldX + fieldW &&
            y >= fy && y <= fy + fieldH) {
            activeField = i;
            return;
        }
    }

    // Submit button click
    float btnY = startY + fieldCount() * fieldGap + 10;
    float btnW = panelW - 60;
    float btnH = 32;
    if (x >= fieldX && x <= fieldX + btnW &&
        y >= btnY && y <= btnY + btnH) {
        if (!emailInput.empty() && !passwordInput.empty()) {
            if (activeTab == Tab::Register && passwordInput != confirmInput) {
                setError("Passwords do not match");
                return;
            }
            if (onSubmit) {
                loading.store(true);
                clearMessages();
                onSubmit(activeTab, emailInput, passwordInput, confirmInput);
            }
        }
        return;
    }

}

// ─── Drawing helpers ─────────────────────────────────────────────────────────

void AuthModal::drawInputField(float x, float y, float w, float h,
                                const std::string& label, const std::string& value,
                                bool active, bool isPassword) {
    // Label
    ofSetColor(160);
    ofDrawBitmapString(label, x, y - 5);

    // Background
    ofSetColor(28, 28, 28);
    ofDrawRectangle(x, y, w, h);

    // Border — blue if active, gray otherwise
    ofNoFill();
    ofSetLineWidth(active ? 2 : 1);
    ofSetColor(active ? ofColor(0, 120, 220) : ofColor(80));
    ofDrawRectangle(x, y, w, h);
    ofFill();
    ofSetLineWidth(1);

    // Text (masked for password)
    std::string display;
    if (isPassword) {
        display = std::string(value.size(), '*');
    } else {
        display = value;
    }
    // Truncate to fit
    int maxChars = (int)((w - 12) / 8);
    if ((int)display.size() > maxChars) {
        display = display.substr(display.size() - maxChars);
    }

    ofSetColor(220);
    ofDrawBitmapString(display, x + 8, y + h - 8);

    // Blinking cursor
    if (active) {
        float blink = fmod(ofGetElapsedTimef(), 1.0f);
        if (blink < 0.5f) {
            float curX = x + 8 + (int)display.size() * 8;
            ofSetColor(0, 120, 220);
            ofDrawLine(curX, y + 6, curX, y + h - 6);
        }
    }
}

void AuthModal::drawButton(float x, float y, float w, float h,
                            const std::string& label, bool enabled, bool primary) {
    if (enabled && primary) {
        ofSetColor(0, 110, 210);
    } else if (enabled) {
        ofSetColor(60, 60, 60);
    } else {
        ofSetColor(50, 50, 50);
    }
    ofDrawRectangle(x, y, w, h);

    ofNoFill();
    ofSetColor(enabled ? ofColor(0, 150, 255) : ofColor(80));
    ofDrawRectangle(x, y, w, h);
    ofFill();

    ofSetColor(enabled ? ofColor(255) : ofColor(120));
    float sw = label.size() * 8;
    ofDrawBitmapString(label, x + (w - sw) / 2, y + h / 2 + 4);
}

void AuthModal::drawTab(float x, float y, float w, float h,
                         const std::string& label, bool active) {
    ofSetColor(active ? 45 : 30, active ? 45 : 30, active ? 45 : 30);
    ofDrawRectangle(x, y, w, h);

    if (active) {
        ofSetColor(0, 120, 220);
        ofDrawRectangle(x, y + h - 2, w, 2);
    }

    ofSetColor(active ? 220 : 120);
    float sw = label.size() * 8;
    ofDrawBitmapString(label, x + (w - sw) / 2, y + h / 2 + 4);
}

// ─── Main draw ────────────────────────────────────────────────────────────────

void AuthModal::draw() {
    if (!visible) return;

    float W = ofGetWidth();
    float H = ofGetHeight();
    bool isReg = (activeTab == Tab::Register);

    float panelW = 420;
    float panelH = isReg ? 330 : 280;
    float px     = (W - panelW) / 2;
    float py     = (H - panelH) / 2;

    // ── Dim background ──
    ofSetColor(0, 0, 0, 180);
    ofDrawRectangle(0, 0, W, H);

    // ── Panel shadow ──
    ofSetColor(0, 0, 0, 100);
    ofDrawRectangle(px + 5, py + 5, panelW, panelH);

    // ── Panel background ──
    ofSetColor(38, 38, 38);
    ofDrawRectangle(px, py, panelW, panelH);

    // ── Panel border ──
    ofNoFill();
    ofSetLineWidth(2);
    ofSetColor(0, 120, 200);
    ofDrawRectangle(px, py, panelW, panelH);
    ofFill();
    ofSetLineWidth(1);

    // ── App name header ──
    ofSetColor(0, 180, 255);
    std::string title = "VirtualStage";
    ofDrawBitmapString(title, px + (panelW - title.size() * 8) / 2, py + 25);

    // ── Tabs ──
    float tabY = py + 40;
    float tabH = 30;
    float tabW = panelW / 2;
    drawTab(px,         tabY, tabW, tabH, "Login",    activeTab == Tab::Login);
    drawTab(px + tabW,  tabY, tabW, tabH, "Register", activeTab == Tab::Register);

    // Separator under tabs
    ofSetColor(60);
    ofDrawLine(px, tabY + tabH, px + panelW, tabY + tabH);

    // ── Fields ──
    float fieldX   = px + 30;
    float fieldW   = panelW - 60;
    float fieldH   = 28;
    float startY   = tabY + tabH + 32;
    float fieldGap = 60;

    bool isLoad = loading.load();

    drawInputField(fieldX, startY,               fieldW, fieldH, "Email",            emailInput,    activeField == 0 && !isLoad, false);
    drawInputField(fieldX, startY + fieldGap,     fieldW, fieldH, "Password",         passwordInput, activeField == 1 && !isLoad, true);
    if (isReg) {
        drawInputField(fieldX, startY + 2*fieldGap, fieldW, fieldH, "Confirm Password", confirmInput,  activeField == 2 && !isLoad, true);
    }

    // ── Submit button ──
    float btnY = startY + fieldCount() * fieldGap + 10;
    float btnW = panelW - 60;
    float btnH = 32;
    std::string btnLabel;

    if (isLoad) {
        int dots = (int)(ofGetElapsedTimef() * 3) % 4;
        btnLabel = std::string(isReg ? "Creating account" : "Signing in") + std::string(dots, '.');
        drawButton(fieldX, btnY, btnW, btnH, btnLabel, false);
    } else {
        btnLabel = isReg ? "Create Account" : "Sign In";
        bool canSubmit = !emailInput.empty() && !passwordInput.empty() &&
                         (!isReg || !confirmInput.empty());
        drawButton(fieldX, btnY, btnW, btnH, btnLabel, canSubmit);
    }

    // ── Error / success message ──
    {
        std::lock_guard<std::mutex> lock(msgMutex);
        if (!errorMessage.empty()) {
            ofSetColor(255, 80, 80);
            // Word-wrap at ~55 chars
            std::string msg = errorMessage;
            int lineMax = (int)(btnW / 8);
            int lines = 0;
            while (!msg.empty() && lines < 3) {
                std::string chunk = msg.substr(0, lineMax);
                ofDrawBitmapString(chunk, fieldX, btnY + btnH + 16 + lines * 14);
                msg = msg.size() > (size_t)lineMax ? msg.substr(lineMax) : "";
                lines++;
            }
        } else if (!successMessage.empty()) {
            ofSetColor(100, 220, 100);
            ofDrawBitmapString(successMessage, fieldX, btnY + btnH + 16);
        }
    }

    ofSetColor(255);
}
