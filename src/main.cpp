#include "ofMain.h"
#include "ofApp.h"

int main() {
    ofGLFWWindowSettings settings;
    settings.setSize(1280, 720);
    settings.title = "VirtualStage";
    settings.setGLVersion(3, 2);
    ofCreateWindow(settings);
    ofRunApp(new ofApp());
}
