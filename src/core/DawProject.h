#pragma once

// DAWproject import (https://github.com/bitwig/dawproject): the open exchange format Bitwig Studio, Studio One,
// Cubase 14, Cubasis and VST Live write. A .dawproject is a ZIP holding project.xml, metadata.xml and the media the
// project references. This reader turns the documented XML into the model below and then into LYNX's Song: tracks,
// note clips as patterns on the Playlist, audio clips as audio-clip channels, tempo, time signature and markers.
//
// Unlike the Cubase / Cakewalk readers in core/ForeignProject.h this is not a recovery exercise: the format is
// specified (Project.xsd) and the values are plain attributes, so a field either parses or is reported. Audio is
// extracted from the package into a folder the caller chooses (`extractMedia`), so a channel's samplePath points at a
// real file the engine can decode.
//
// Nothing here depends on JUCE, so the whole path is covered by tests/DawProjectTests.inc.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/Song.h"

namespace lynx::dawproject
{
enum class TimeUnit
{
    beats,
    seconds
};

struct Note
{
    double time = 0.0;     // in the containing timeline's time unit
    double duration = 0.0; // in the containing timeline's time unit
    int channel = 0;       // MIDI channel as written by the exporter
    int key = 60;
    double velocity = 0.78;
    double release = 0.5;
};

struct Warp
{
    double time = 0.0;
    double contentTime = 0.0;
};

struct AudioFile
{
    std::string path;          // as written in project.xml
    bool external = false;     // true: a file outside the package (path may be absolute or relative to it)
    bool hasFile = false;
    double duration = 0.0;     // seconds
    int sampleRate = 0;
    int channels = 0;
    std::string algorithm;     // e.g. "stretch"
    std::string resolvedPath;  // set by extractMedia: the file the engine reads
};

enum class ClipKind
{
    notes,
    audio,
    other
};

struct Clip
{
    std::string id;
    std::string name;
    std::string reference; // an alias of another clip: its content is copied
    double time = 0.0;
    double duration = 0.0;
    bool hasDuration = false;
    double playStart = 0.0;
    bool hasPlayStart = false;
    double playStop = 0.0;
    bool hasPlayStop = false;
    double loopStart = 0.0, loopEnd = 0.0;
    bool hasLoop = false;
    double fadeIn = 0.0, fadeOut = 0.0;
    bool hasFadeIn = false, hasFadeOut = false;
    bool enable = true;
    bool hasEnable = false;

    TimeUnit unit = TimeUnit::beats;        // this clip's time / duration
    TimeUnit contentUnit = TimeUnit::beats; // playStart / playStop
    TimeUnit fadeUnit = TimeUnit::beats;

    ClipKind kind = ClipKind::other;
    TimeUnit noteUnit = TimeUnit::beats;
    std::vector<Note> notes;
    std::vector<Warp> warps;
    TimeUnit warpContentUnit = TimeUnit::seconds;
    AudioFile audio;
    std::vector<Clip> clips; // a nested content timeline (Bitwig's audio clips and grouped content)
};

struct Track
{
    std::string id;
    std::string name;
    std::string comment;
    std::string contentTypes; // "audio notes", "tracks", ...
    std::uint32_t colour = 0; // 0xFFRRGGBB; 0 = none
    bool loaded = true;
    bool isMaster = false;
    bool muted = false;
    bool hasMute = false;
    double volume = 1.0; // linear, as exported
    double pan = 0.5;    // normalized 0..1
    int depth = 0;       // 0 = top level; > 0 nested in a group track
    std::vector<Clip> clips;
};

struct Marker
{
    double time = 0.0;
    std::string name;
};

struct Project
{
    std::string application;
    std::string applicationVersion;
    std::string version; // the file format version
    std::string sourcePath;

    double bpm = 120.0;
    int numerator = 0;
    int denominator = 4;
    TimeUnit unit = TimeUnit::beats;
    std::vector<Track> tracks;
    std::vector<Marker> markers;
    std::vector<std::string> findings; // what was imported and what was deliberately not

    int clipCount = 0;
    int noteCount = 0;
    int audioCount = 0;
    int pluginCount = 0;
    bool hasTempoAutomation = false;
    bool hasTimeSignatureAutomation = false;
};

// A .dawproject in memory (the ZIP's project.xml). readFile also remembers the path, which extractMedia needs for the
// package's audio. False (and `error`) when the bytes are not a DAWproject ZIP with a valid project.xml.
bool read (const std::uint8_t* data, std::size_t size, Project& out, std::string& error);
bool readFile (const std::string& path, Project& out, std::string& error);

// Writes the package's audio into `destination` (made with std::filesystem) and fills each AudioFile::resolvedPath.
// External files are resolved next to the package, never copied. A missing entry is reported in the project's findings;
// the rest still imports. False only when the package itself cannot be reopened or the destination cannot be written.
bool extractMedia (Project& project, const std::string& destination, std::string& error);

// The parsed project as tracks, patterns, clips, tempo, markers and audio-clip channels. `mediaDirectory` is handed to
// extractMedia; empty means the audio is used where it already is (or the clip stays silent and is reported).
bool importInto (Song& song, Project& project, const std::string& patternName, const std::string& mediaDirectory,
                 std::string& error);
bool importInto (Song& song, Project& project, const std::string& patternName, std::string& error);
} // namespace lynx::dawproject
