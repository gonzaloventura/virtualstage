#pragma once
#include "ofMain.h"
#include "ScreenObject.h"
#include <vector>
#include <set>

class Scene;

struct SceneSnapshot {
    struct ScreenData {
        ofJson json;           // full screen state via toJson()
    };
    std::vector<ScreenData> screens;
    std::set<int> selectedIndices;
    int primarySelected = -1;
};

class UndoManager {
public:
    void pushState(Scene& scene);
    bool undo(Scene& scene);
    bool redo(Scene& scene);

    bool canUndo() const { return currentIndex > 0; }
    bool canRedo() const { return currentIndex < (int)history.size() - 1; }

    void clear();

private:
    static const int MAX_HISTORY = 50;

    SceneSnapshot captureState(Scene& scene);
    void restoreState(Scene& scene, const SceneSnapshot& snapshot);

    std::vector<SceneSnapshot> history;
    int currentIndex = -1;
};
