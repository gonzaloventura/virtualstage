#pragma once
#include "ofMain.h"
#include "Cabinet.h"
#include <functional>

class CabinetsModal {
public:
    void draw();
    void keyPressed(int key);
    void mousePressed(int x, int y);

    bool isVisible() const { return visible; }
    void show(CabinetLibrary* lib);
    void hide();

    // Fired after any change (add/edit/delete) so the app can persist
    // locally and push to cloud.
    std::function<void()> onLibraryChanged;

private:
    bool            visible = false;
    CabinetLibrary* lib     = nullptr;
    float           scroll  = 0;

    // Prompt the user to enter fields; when editing, preseed with existing values.
    // Returns true if the user entered something valid and the cabinet was
    // added/updated; false if cancelled or invalid.
    bool promptEdit(Cabinet& c, bool isNew);
};
