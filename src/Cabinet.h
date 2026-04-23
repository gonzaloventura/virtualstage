#pragma once
#include "ofMain.h"
#include <string>
#include <vector>
#include <mutex>

// A single LED cabinet (panel) spec.
// Physical size is derived from pitch × pixel resolution.
//   1 OGL unit = 1 cm = 10 mm  →  oglPerPx = pitchMm / 10
struct Cabinet {
    std::string id;          // stable id (auto-generated)
    std::string name;        // human label (e.g. "Absen A3 Pro")
    float       pitchMm   = 3.9f;
    int         panelWpx  = 128;
    int         panelHpx  = 128;

    float panelWidthMm()  const { return panelWpx * pitchMm; }
    float panelHeightMm() const { return panelHpx * pitchMm; }
    float oglPerPx()      const { return pitchMm / 10.0f; }            // 1 OGL = 1 cm
    float pxToOgl(float px) const { return px * oglPerPx(); }

    // true if the given pixel dimensions are integer multiples of this cabinet
    // (slice area is a whole number of panels tall AND wide)
    bool fitsSlice(int slicePxW, int slicePxH, int tolerance = 2) const;

    ofJson toJson() const;
    void   fromJson(const ofJson& j);
};

// Manages the user's cabinet library.
// Local storage: ~/.virtualstage/cabinets.json
// Cloud sync:    user_cabinets table (JSONB data column)
class CabinetLibrary {
public:
    // Load/save local JSON file (mirrors Preferences pattern)
    void loadLocal();
    void saveLocal();

    // Cloud sync serialization
    std::string toJsonString() const;
    void        fromJsonString(const std::string& s);

    // Collection access (thread-safe snapshots)
    std::vector<Cabinet> list() const;
    int  size() const;
    bool empty() const;

    // Mutation
    std::string add(const Cabinet& c);          // returns new id (generates if empty)
    bool        update(const Cabinet& c);       // matches by id
    bool        remove(const std::string& id);

    // Lookup
    bool        find(const std::string& id, Cabinet& out) const;

    // Auto-detect: return the cabinet whose panel size evenly divides the given
    // slice pixel dimensions. Returns empty id if no match.
    std::string detectForSlice(int slicePxW, int slicePxH) const;

private:
    mutable std::mutex   mtx;
    std::vector<Cabinet> cabinets;

    std::string getDir() const;   // ~/.virtualstage/
    std::string getPath() const;  // ~/.virtualstage/cabinets.json
    std::string generateId() const;
};
