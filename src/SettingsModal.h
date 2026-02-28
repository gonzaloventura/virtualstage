#pragma once
#include "ofMain.h"
#include "Preferences.h"
#include <string>
#include <functional>

class SettingsModal {
public:
    void draw();
    void keyPressed(int key);
    void mousePressed(int x, int y);

    bool isVisible() const { return visible; }
    void show(Preferences* prefs);
    void hide();

    // Callback when a preference changes (for cloud sync + UI refresh)
    std::function<void()> onPreferenceChanged;

private:
    bool visible = false;
    Preferences* prefs = nullptr;
    int selectedUnitIndex = 0; // 0=Meters, 1=Centimeters, 2=Feet, 3=Inches
};
