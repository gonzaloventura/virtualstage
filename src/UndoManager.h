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
    std::vector<ScreenData> screens;
    std::vector<GroupData> groups;
    std::set<int> selectedIndices;
    int primarySelected = -1;
    std::string description;
};

class UndoManager {
public:
    void pushState(Scene& scene, const std::string& desc = "");
    bool undo(Scene& scene);
    bool redo(Scene& scene);

    bool canUndo() const { return currentIndex > 0; }
    bool canRedo() const { return currentIndex < (int)history.size() - 1; }

    // History info for visual display
    int getHistorySize() const { return (int)history.size(); }
    int getCurrentIndex() const { return currentIndex; }
    std::string getDescription(int index) const;
    bool jumpTo(Scene& scene, int index);

    void clear();

private:
    static const int MAX_HISTORY = 50;

    SceneSnapshot captureState(Scene& scene);
    void restoreState(Scene& scene, const SceneSnapshot& snapshot);

    std::vector<SceneSnapshot> history;
    int currentIndex = -1;
};
