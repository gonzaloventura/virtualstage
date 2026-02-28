#pragma once
#include "ofMain.h"
#include <string>
#include <functional>
#include <mutex>
#include <atomic>

// Login / Register modal drawn on top of the entire app.
// Uses a custom text-input implementation (OF has no built-in widget).
// Thread-safe: setError/setSuccess can be called from background threads;
// draw() / keyPressed() / mousePressed() run on the main thread.
class AuthModal {
public:
    enum class Tab { Login, Register };

    void draw();
    void keyPressed(int key);
    void mousePressed(int x, int y);

    bool isVisible() const { return visible; }
    void show();
    void hide();

    // Called from main thread after background auth succeeds/fails
    void setLoading(bool v) { loading.store(v); }
    void setError(const std::string& msg);
    void setSuccess(const std::string& msg);
    void clearMessages();

    // Check from main thread: background auth finished successfully
    bool consumeLoginSuccess();   // returns true once, then resets
    bool consumeRegisterConfirm();// returns true once (email confirm needed)

    // Callbacks — set by ofApp before show(); called on main thread
    // (tab, email, password, confirmPassword)
    std::function<void(Tab, const std::string&, const std::string&, const std::string&)> onSubmit;

private:
    bool visible = false;
    Tab  activeTab   = Tab::Login;
    int  activeField = 0; // 0=email, 1=password, 2=confirm

    std::string emailInput;
    std::string passwordInput;
    std::string confirmInput;

    std::atomic<bool> loading{false};

    std::mutex   msgMutex;
    std::string  errorMessage;
    std::string  successMessage;
    std::atomic<bool> loginSuccessFlag{false};
    std::atomic<bool> registerConfirmFlag{false};

    // Drawing helpers
    void drawInputField(float x, float y, float w, float h,
                        const std::string& label, const std::string& value,
                        bool active, bool isPassword);
    void drawButton(float x, float y, float w, float h,
                    const std::string& label, bool enabled, bool primary = true);
    void drawTab(float x, float y, float w, float h,
                 const std::string& label, bool active);

    // Returns field count for current tab (2 = login, 3 = register)
    int fieldCount() const { return activeTab == Tab::Login ? 2 : 3; }
    std::string& fieldAt(int i);
};
