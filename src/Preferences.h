#pragma once
#include "ofMain.h"
#include <string>
#include <mutex>

enum class MeasurementUnit { Meters, Centimeters, Feet, Inches };

class Preferences {
public:
    // Load from ~/.virtualstage/preferences.json (call once at startup)
    void loadLocal();

    // Save to ~/.virtualstage/preferences.json (call on every change)
    void saveLocal();

    // Getters/setters
    MeasurementUnit getUnit() const;
    void setUnit(MeasurementUnit u);

    // Unit label for display (e.g., "m", "cm", "ft", "in")
    std::string getUnitSuffix() const;

    // OpenGL units per 1 display unit.
    // Base mapping: 1 OGL unit = 1 centimeter → 100 OGL = 1 meter
    float oglPerDisplayUnit() const;

    // Convert effective OGL dimension to display value
    float oglToDisplay(float oglValue) const;

    // Convert display value to OGL dimension
    float displayToOgl(float displayValue) const;

    // Serialize to/from JSON string (for cloud sync)
    std::string toJsonString() const;
    void fromJsonString(const std::string& jsonStr);

private:
    MeasurementUnit unit = MeasurementUnit::Meters;
    mutable std::mutex mtx;

    std::string getPrefsDir() const;   // ~/.virtualstage/
    std::string getPrefsPath() const;  // ~/.virtualstage/preferences.json
};
