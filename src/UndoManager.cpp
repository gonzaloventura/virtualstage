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

    // Rebuild screens from snapshot
    for (auto& sd : snapshot.screens) {
        auto screen = std::make_unique<ScreenObject>();
        screen->fromJson(sd.json);
        scene.screens.push_back(std::move(screen));
    }

    // Restore selection
    scene.selectedIndices = snapshot.selectedIndices;
    scene.primarySelected = snapshot.primarySelected;

    // Reconnect sources by name
    scene.reconnectSources();
}

void UndoManager::pushState(Scene& scene) {
    SceneSnapshot snap = captureState(scene);

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
