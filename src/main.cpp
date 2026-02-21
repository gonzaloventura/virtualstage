#include "win_byte_fix.h"
#include "ofMain.h"
#include "ofApp.h"

// Force dedicated GPU on laptops with hybrid graphics (NVIDIA Optimus / AMD Switchable)
#ifdef TARGET_WIN32
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main() {
    ofGLFWWindowSettings settings;
    settings.setSize(1280, 720);
    settings.title = "VirtualStage Beta";
    settings.setGLVersion(3, 2);
    ofCreateWindow(settings);
    ofRunApp(new ofApp());
}
