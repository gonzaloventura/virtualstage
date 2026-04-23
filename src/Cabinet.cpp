#include "win_byte_fix.h"
#include "Cabinet.h"
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>
#include <chrono>
#include <atomic>

// ── Cabinet ────────────────────────────────────────────────────────────────

bool Cabinet::fitsSlice(int slicePxW, int slicePxH, int tolerance) const {
    if (panelWpx <= 0 || panelHpx <= 0) return false;
    int remW = slicePxW % panelWpx;
    int remH = slicePxH % panelHpx;
    // Accept exact or off-by-tolerance (Resolume sometimes stores pixel rects
    // that are one off due to bounding-box math)
    if (remW > panelWpx / 2) remW = panelWpx - remW;
    if (remH > panelHpx / 2) remH = panelHpx - remH;
    return remW <= tolerance && remH <= tolerance
        && slicePxW >= panelWpx && slicePxH >= panelHpx;
}

ofJson Cabinet::toJson() const {
    ofJson j;
    j["id"]       = id;
    j["name"]     = name;
    j["pitchMm"]  = pitchMm;
    j["panelWpx"] = panelWpx;
    j["panelHpx"] = panelHpx;
    return j;
}

void Cabinet::fromJson(const ofJson& j) {
    id       = j.value("id", std::string(""));
    name     = j.value("name", std::string(""));
    pitchMm  = j.value("pitchMm", 3.9f);
    panelWpx = j.value("panelWpx", 128);
    panelHpx = j.value("panelHpx", 128);
}

// ── CabinetLibrary: paths ──────────────────────────────────────────────────

std::string CabinetLibrary::getDir() const {
    const char* home = getenv("HOME");
#ifdef TARGET_WIN32
    if (!home) home = getenv("USERPROFILE");
#endif
    if (!home) home = "/tmp";
    return std::string(home) + "/.virtualstage";
}

std::string CabinetLibrary::getPath() const {
    return getDir() + "/cabinets.json";
}

std::string CabinetLibrary::generateId() const {
    static std::atomic<uint64_t> counter{0};
    uint64_t t = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
    uint64_t n = counter.fetch_add(1);
    return "cab_" + std::to_string(t) + "_" + std::to_string(n);
}

// ── CabinetLibrary: local I/O ──────────────────────────────────────────────

void CabinetLibrary::loadLocal() {
    std::lock_guard<std::mutex> lock(mtx);
    std::ifstream f(getPath());
    if (!f.is_open()) return;
    try {
        ofJson j = ofJson::parse(f);
        cabinets.clear();
        if (j.contains("cabinets") && j["cabinets"].is_array()) {
            for (auto& cj : j["cabinets"]) {
                Cabinet c;
                c.fromJson(cj);
                if (c.id.empty()) c.id = generateId();
                cabinets.push_back(c);
            }
        }
    } catch (...) {}
}

void CabinetLibrary::saveLocal() {
    std::lock_guard<std::mutex> lock(mtx);
    std::string dir = getDir();
#ifdef TARGET_WIN32
    mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif

    ofJson j;
    ofJson arr = ofJson::array();
    for (const auto& c : cabinets) arr.push_back(c.toJson());
    j["cabinets"] = arr;

    std::ofstream f(getPath());
    if (f.is_open()) {
        f << j.dump(4);
    }
}

// ── Cloud sync serialization ───────────────────────────────────────────────

std::string CabinetLibrary::toJsonString() const {
    std::lock_guard<std::mutex> lock(mtx);
    ofJson j;
    ofJson arr = ofJson::array();
    for (const auto& c : cabinets) arr.push_back(c.toJson());
    j["cabinets"] = arr;
    return j.dump();
}

void CabinetLibrary::fromJsonString(const std::string& s) {
    std::lock_guard<std::mutex> lock(mtx);
    try {
        ofJson j = ofJson::parse(s);
        cabinets.clear();
        if (j.contains("cabinets") && j["cabinets"].is_array()) {
            for (auto& cj : j["cabinets"]) {
                Cabinet c;
                c.fromJson(cj);
                if (c.id.empty()) c.id = generateId();
                cabinets.push_back(c);
            }
        }
    } catch (...) {}
}

// ── Access / mutation ──────────────────────────────────────────────────────

std::vector<Cabinet> CabinetLibrary::list() const {
    std::lock_guard<std::mutex> lock(mtx);
    return cabinets;
}

int CabinetLibrary::size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return (int)cabinets.size();
}

bool CabinetLibrary::empty() const {
    std::lock_guard<std::mutex> lock(mtx);
    return cabinets.empty();
}

std::string CabinetLibrary::add(const Cabinet& c) {
    std::lock_guard<std::mutex> lock(mtx);
    Cabinet copy = c;
    if (copy.id.empty()) copy.id = generateId();
    cabinets.push_back(copy);
    return copy.id;
}

bool CabinetLibrary::update(const Cabinet& c) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& existing : cabinets) {
        if (existing.id == c.id) {
            existing = c;
            return true;
        }
    }
    return false;
}

bool CabinetLibrary::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto it = cabinets.begin(); it != cabinets.end(); ++it) {
        if (it->id == id) {
            cabinets.erase(it);
            return true;
        }
    }
    return false;
}

bool CabinetLibrary::find(const std::string& id, Cabinet& out) const {
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& c : cabinets) {
        if (c.id == id) { out = c; return true; }
    }
    return false;
}

std::string CabinetLibrary::detectForSlice(int slicePxW, int slicePxH) const {
    std::lock_guard<std::mutex> lock(mtx);
    // Prefer the cabinet that fits evenly AND uses the largest panel size
    // (fewer, larger panels = more likely the intended one)
    const Cabinet* best = nullptr;
    long bestArea = 0;
    for (const auto& c : cabinets) {
        if (!c.fitsSlice(slicePxW, slicePxH)) continue;
        long area = (long)c.panelWpx * (long)c.panelHpx;
        if (area > bestArea) { bestArea = area; best = &c; }
    }
    return best ? best->id : std::string("");
}
