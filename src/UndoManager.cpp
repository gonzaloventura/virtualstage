#include "win_byte_fix.h"
#include "UndoManager.h"
#include "Scene.h"

SceneSnapshot UndoManager::captureState(Scene& scene) {
    SceneSnapshot snap;
    for (auto& screen : scene.screens) {
        SceneSnapshot::ScreenData sd;
        sd.json = screen->toJson();
        snap.screens.push_back(sd);
    }
    for (auto& g : scene.groups) {
        snap.groups.push_back({g.id, g.name, g.sourceIndex, g.sourceName});
    }
    snap.selectedIndices = scene.selectedIndices;
    snap.primarySelected = scene.primarySelected;
    return snap;
}

void UndoManager::restoreState(Scene& scene, const SceneSnapshot& snapshot) {
    // Disconnect all existing screens
    for (auto& screen : scene.screens) {
        screen->disconnectSource();
    }
    scene.screens.clear();
    scene.groups.clear();

    // Rebuild screens from snapshot
    for (auto& sd : snapshot.screens) {
        auto screen = std::make_unique<ScreenObject>();
        screen->fromJson(sd.json);
        scene.screens.push_back(std::move(screen));
    }

    // Rebuild groups from snapshot
    int maxGroupId = 0;
    for (auto& gd : snapshot.groups) {
        ScreenGroup g;
        g.id = gd.id;
        g.name = gd.name;
        g.sourceIndex = gd.sourceIndex;
        g.sourceName = gd.sourceName;
        scene.groups.push_back(g);
        if (g.id > maxGroupId) maxGroupId = g.id;
    }
    scene.nextGroupId = maxGroupId + 1;

    // Restore selection
    scene.selectedIndices = snapshot.selectedIndices;
    scene.primarySelected = snapshot.primarySelected;

    // Reconnect sources by name
    scene.reconnectSources();
}

void UndoManager::pushState(Scene& scene, const std::string& desc) {
    SceneSnapshot snap = captureState(scene);
    snap.description = desc;

    // Truncate any redo history
    if (currentIndex + 1 < (int)history.size()) {
        history.erase(history.begin() + currentIndex + 1, history.end());
    }

    history.push_back(snap);
    currentIndex = (int)history.size() - 1;

    // Cap at MAX_HISTORY
    if ((int)history.size() > MAX_HISTORY) {
        history.erase(history.begin());
        currentIndex--;
    }
}

std::string UndoManager::getDescription(int index) const {
    if (index >= 0 && index < (int)history.size()) {
        return history[index].description;
    }
    return "";
}

bool UndoManager::jumpTo(Scene& scene, int index) {
    if (index < 0 || index >= (int)history.size()) return false;
    currentIndex = index;
    restoreState(scene, history[currentIndex]);
    return true;
}

bool UndoManager::undo(Scene& scene) {
    if (!canUndo()) return false;
    currentIndex--;
    restoreState(scene, history[currentIndex]);
    return true;
}

bool UndoManager::redo(Scene& scene) {
    if (!canRedo()) return false;
    currentIndex++;
    restoreState(scene, history[currentIndex]);
    return true;
}

void UndoManager::clear() {
    history.clear();
    currentIndex = -1;
}
