#pragma once

// Opening projects made in other DAWs, at the level LYNX can honour: the tracks and their notes.
//
// What each container actually holds, and why this reader is deliberately careful:
//   .cpr / .npr (Cubase, Nuendo): a RIFF container whose form type is the eight-character "NUNDROOT"
//       (Steinberg used the whole chunk name where RIFF normally has four characters). The chunk ids are plain
//       ASCII ("PAppVersion", "Plugin UID" are literal tokens), the tree's length fields are not documented, so
//       this reader validates a 64- or 32-bit length walk and otherwise falls back to a byte scan. Cubase keeps its
//       events in that proprietary tree; anything it embeds as a standard MIDI file is recovered exactly, and the
//       rest is reported instead of guessed. Audio always lives beside the .cpr, never inside it.
//   .cwp / .cwb (Cakewalk, SONAR, BandLab): no public specification exists (the format is an OLE2 container in
//       practice). Only embedded standard MIDI is recovered; notes are otherwise reported as not readable, which is
//       the honest result rather than a silent half-import.
//   .wrk (older Cakewalk): a tag/length chunk stream whose record types are documented (Drumstick's WrkChunkType).
//       The walk is validated against the known tag set and the file length, so a file is either read structurally or
//       reported; the event streams (tags 2 / 45) are not decoded yet.
//   .mid / .midi: handed to the same reader as File > Import, for one code path.
//   .dawproject (Bitwig, Studio One, Cubase 14, Cubasis, VST Live): the open DAWproject ZIP. Unlike the containers
//       above it is documented, so it brings the arrangement itself: tracks, note clips, audio clips, tempo, time
//       signature and markers, through core/DawProject.h.
//
// Nothing here depends on JUCE, so the whole thing is covered by the core tests (tests/ForeignImportTests.inc and
// tests/DawProjectTests.inc), and the recovered notes are imported by the MIDI importer (core/MidiImport.h), which
// already knows how to turn them into channels and a pattern.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/DawProject.h"
#include "core/MidiImport.h"
#include "core/Song.h"

namespace lynx::foreign
{
enum class Format
{
    unknown,
    cubase,      // .cpr / .npr
    cakewalk,    // .cwp / .cwb / .cwz (OLE2 container or a zip)
    cakewalkWrk, // .wrk (tag/length chunks)
    dawproject,  // .dawproject (the documented open exchange ZIP)
    midi         // .mid / .midi
};

// Short name for the UI ("Cubase", "Cakewalk", "Cakewalk (.wrk)", "MIDI").
const char* formatName (Format format) noexcept;

struct Track
{
    std::string name;
    int midiChannel = 0; // the first MIDI channel found on it (0-based)
    int notes = 0;
    bool operator== (const Track&) const = default;
};

struct Report
{
    Format format = Format::unknown;
    std::string application; // "Cubase", "Cakewalk", "MIDI"
    std::string version;     // what the container says, when it says it
    std::vector<Track> tracks;
    std::vector<std::string> plugins;  // Cubase plugin tokens found in the container (not converted)
    std::vector<std::string> findings; // what was recovered and what was not; shown to the user
    midiimport::ParsedMidi midi;
    dawproject::Project dawproject; // Format::dawproject: the full arrangement

    int noteCount() const noexcept;
    bool hasNotes() const noexcept { return noteCount() > 0; }
    bool hasImportableContent() const noexcept { return hasNotes() || (format == Format::dawproject && ! dawproject.tracks.empty()); }
    std::string summary() const; // one line: format, tracks, notes, then one line per finding
};

// The extension list an open/import dialog offers (juce filter syntax: "cpr;npr;...").
const char* openFilter() noexcept;
bool isSupportedExtension (const std::string& extension) noexcept;

// The container of these bytes, from their signature first and the extension second.
Format detect (const std::uint8_t* data, std::size_t size, const std::string& extension) noexcept;

bool read (const std::uint8_t* data, std::size_t size, const std::string& extension, Report& out, std::string& error);
bool readFile (const std::string& path, Report& out, std::string& error);

// The recovered tracks as channels and a new pattern. False (and `error`) when the report has no notes. For a
// DAWproject the whole arrangement is used instead (patterns, Playlist clips and audio-clip channels), and the findings
// the import itself produces (extracted or missing audio, disabled tracks, clips it left out) are written back to the
// report, so `report.findings` and `report.summary()` describe the import and not only the read.
bool importInto (Song& song, Report& report, const midiimport::Options& options, const std::string& patternName, std::string& error);

// The same, with a folder for the audio a DAWproject embeds (`extractMedia`). `mediaDirectory` is ignored by the
// other formats, whose audio never lives inside the project file; empty leaves a DAWproject's audio where it already is.
bool importInto (Song& song, Report& report, const midiimport::Options& options, const std::string& patternName,
                 const std::string& mediaDirectory, std::string& error);

// --- the pieces, exposed for tests -------------------------------------------------------------------------------
// A standard MIDI file (format 0 / 1 / 2) into the MIDI importer's own model. False when there is no valid MThd.
bool parseStandardMidi (const std::uint8_t* data, std::size_t size, midiimport::ParsedMidi& out, std::string* error = nullptr);
bool readCubase (const std::uint8_t* data, std::size_t size, Report& out, std::string& error);
bool readCakewalkWrk (const std::uint8_t* data, std::size_t size, Report& out, std::string& error);
bool readCakewalk (const std::uint8_t* data, std::size_t size, Report& out, std::string& error);
} // namespace lynx::foreign
