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
    struct GroupData {
        int id;
        std::string name;
        int sourceIndex;
        std::string sourceName;
    };
    struct ElementData {
        ofJson json;           // full stage element state via toJson()
    };
    std::vector<ScreenData> screens;
    std::vector<GroupData> groups;
    std::vector<ElementData> elements;
    std::set<int> selectedIndices;
    int primarySelected = -1;
    int selectedStageElement = -1;
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
