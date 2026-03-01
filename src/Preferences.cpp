#include "win_byte_fix.h"
#include "Preferences.h"
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>

// ── Helpers ─────────────────────────────────────────────────────────────────

std::string Preferences::getPrefsDir() const {
    const char* home = getenv("HOME");
#ifdef TARGET_WIN32
    if (!home) home = getenv("USERPROFILE");
#endif
    if (!home) home = "/tmp";
    return std::string(home) + "/.virtualstage";
}

std::string Preferences::getPrefsPath() const {
    return getPrefsDir() + "/preferences.json";
}

static std::string unitToString(MeasurementUnit u) {
    switch (u) {
        case MeasurementUnit::Meters:      return "meters";
        case MeasurementUnit::Centimeters: return "centimeters";
        case MeasurementUnit::Feet:        return "feet";
        case MeasurementUnit::Inches:      return "inches";
    }
    return "meters";
}

static MeasurementUnit stringToUnit(const std::string& s) {
    if (s == "centimeters") return MeasurementUnit::Centimeters;
    if (s == "feet")        return MeasurementUnit::Feet;
    if (s == "inches")      return MeasurementUnit::Inches;
    return MeasurementUnit::Meters;
}

// ── Local I/O ───────────────────────────────────────────────────────────────

void Preferences::loadLocal() {
    std::lock_guard<std::mutex> lock(mtx);
    std::ifstream f(getPrefsPath());
    if (!f.is_open()) return;
    try {
        ofJson j = ofJson::parse(f);
        if (j.contains("measurementUnit") && j["measurementUnit"].is_string()) {
            unit = stringToUnit(j["measurementUnit"].get<std::string>());
        }
    } catch (...) {}
}

void Preferences::saveLocal() {
    std::lock_guard<std::mutex> lock(mtx);
    // Ensure directory exists
    std::string dir = getPrefsDir();
#ifdef TARGET_WIN32
    mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif

    ofJson j;
    j["measurementUnit"] = unitToString(unit);

    std::ofstream f(getPrefsPath());
    if (f.is_open()) {
        f << j.dump(4);
    }
}

// ── Getters / setters ───────────────────────────────────────────────────────

MeasurementUnit Preferences::getUnit() const {
    std::lock_guard<std::mutex> lock(mtx);
    return unit;
}

void Preferences::setUnit(MeasurementUnit u) {
    std::lock_guard<std::mutex> lock(mtx);
    unit = u;
}

std::string Preferences::getUnitSuffix() const {
    std::lock_guard<std::mutex> lock(mtx);
    switch (unit) {
        case MeasurementUnit::Meters:      return "m";
        case MeasurementUnit::Centimeters: return "cm";
        case MeasurementUnit::Feet:        return "ft";
        case MeasurementUnit::Inches:      return "in";
    }
    return "m";
}

// ── Conversion ──────────────────────────────────────────────────────────────
// Base mapping: 1 OGL unit = 1 cm → 100 OGL = 1 m

float Preferences::oglPerDisplayUnit() const {
    std::lock_guard<std::mutex> lock(mtx);
    switch (unit) {
        case MeasurementUnit::Meters:      return 100.0f;
        case MeasurementUnit::Centimeters: return 1.0f;
        case MeasurementUnit::Feet:        return 30.48f;
        case MeasurementUnit::Inches:      return 2.54f;
    }
    return 100.0f;
}

float Preferences::oglToDisplay(float oglValue) const {
    return oglValue / oglPerDisplayUnit();
}

float Preferences::displayToOgl(float displayValue) const {
    return displayValue * oglPerDisplayUnit();
}

// ── Cloud sync serialization ────────────────────────────────────────────────

std::string Preferences::toJsonString() const {
    std::lock_guard<std::mutex> lock(mtx);
    ofJson j;
    j["measurementUnit"] = unitToString(unit);
    return j.dump();
}

void Preferences::fromJsonString(const std::string& jsonStr) {
    std::lock_guard<std::mutex> lock(mtx);
    try {
        ofJson j = ofJson::parse(jsonStr);
        if (j.contains("measurementUnit") && j["measurementUnit"].is_string()) {
            unit = stringToUnit(j["measurementUnit"].get<std::string>());
        }
    } catch (...) {}
}
