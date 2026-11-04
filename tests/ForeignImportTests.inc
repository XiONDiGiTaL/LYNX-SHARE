// --- foreign projects (core/ForeignProject.h): Cubase .cpr / .npr, Cakewalk .cwp / .wrk, MIDI ---

#include "core/ForeignProject.h"

namespace foreigntest
{
using Bytes = std::vector<std::uint8_t>;

void putText (Bytes& v, const char* text)
{
    for (const char* c = text; *c != '\0'; ++c)
        v.push_back ((std::uint8_t) *c);
}

void putBe16 (Bytes& v, std::uint32_t x) { for (int s = 8; s >= 0; s -= 8) v.push_back ((std::uint8_t) (x >> s)); }
void putBe32 (Bytes& v, std::uint32_t x) { for (int s = 24; s >= 0; s -= 8) v.push_back ((std::uint8_t) (x >> s)); }
void putLe32 (Bytes& v, std::uint32_t x) { for (int s = 0; s <= 24; s += 8) v.push_back ((std::uint8_t) (x >> s)); }
void putLe64 (Bytes& v, std::uint64_t x) { for (int s = 0; s <= 56; s += 8) v.push_back ((std::uint8_t) (x >> s)); }

void putVlq (Bytes& v, std::uint32_t value)
{
    std::uint8_t buffer[5] {};
    int n = 0;
    do
    {
        buffer[n++] = (std::uint8_t) (value & 0x7f);
        value >>= 7;
    } while (value != 0);
    for (int i = n - 1; i >= 0; --i)
        v.push_back ((std::uint8_t) (buffer[i] | (i == 0 ? 0x00 : 0x80)));
}

// A one-track standard MIDI file: `notes` notes of a beat each from key 60, named, optionally at `bpm`.
Bytes makeSmf (const char* name, int notes, double bpm)
{
    Bytes track;
    auto meta = [&track] (std::uint8_t type, const Bytes& payload) {
        track.push_back (0);
        track.push_back (0xff);
        track.push_back (type);
        putVlq (track, (std::uint32_t) payload.size());
        track.insert (track.end(), payload.begin(), payload.end());
    };

    if (name != nullptr && *name != '\0')
    {
        Bytes text;
        putText (text, name);
        meta (0x03, text);
    }
    if (bpm > 0.0)
    {
        const auto micro = (std::uint32_t) (60000000.0 / bpm + 0.5);
        meta (0x51, { (std::uint8_t) (micro >> 16), (std::uint8_t) (micro >> 8), (std::uint8_t) micro });
    }
    meta (0x58, { 4, 2 });
    for (int i = 0; i < notes; ++i)
    {
        track.push_back (0);
        track.push_back (0x90);
        track.push_back ((std::uint8_t) (60 + i));
        track.push_back (100);
        putVlq (track, 480);
        track.push_back (0x80);
        track.push_back ((std::uint8_t) (60 + i));
        track.push_back (0);
    }
    meta (0x2f, {});

    Bytes file;
    putText (file, "MThd");
    putBe32 (file, 6);
    putBe16 (file, 1);   // format 1
    putBe16 (file, 1);   // one track
    putBe16 (file, 480); // ticks per quarter
    putText (file, "MTrk");
    putBe32 (file, (std::uint32_t) track.size());
    file.insert (file.end(), track.begin(), track.end());
    return file;
}

// A Cubase-shaped container: RIFF, the eight-character NUNDROOT form, chunks with 8-byte ids and 64-bit lengths.
Bytes makeCpr (const Bytes& midi, const char* version)
{
    auto chunk = [] (const char* id, const Bytes& payload) {
        Bytes c;
        putText (c, id);
        putLe64 (c, payload.size());
        c.insert (c.end(), payload.begin(), payload.end());
        return c;
    };

    Bytes app;
    putText (app, "PAppVersion");
    app.push_back (0);
    putText (app, version);
    app.push_back (0);

    Bytes payload = chunk ("NUNDVERS", app);
    const auto midiChunk = chunk ("NUNDMIDI", midi);
    payload.insert (payload.end(), midiChunk.begin(), midiChunk.end());

    Bytes file;
    putText (file, "RIFF");
    putLe32 (file, (std::uint32_t) payload.size() + 8);
    putText (file, "NUNDROOT");
    putLe64 (file, payload.size());
    file.insert (file.end(), payload.begin(), payload.end());
    return file;
}

// A Cakewalk .wrk-shaped chunk stream: [tag][32-bit length][payload] records from Drumstick's tag set.
Bytes makeWrk()
{
    Bytes file;
    auto record = [&file] (int tag, const Bytes& payload) {
        file.push_back ((std::uint8_t) tag);
        putLe32 (file, (std::uint32_t) payload.size());
        file.insert (file.end(), payload.begin(), payload.end());
    };

    record (10, { 0x06, 0x00 });                       // TIMEBASE_CHUNK
    record (36, {});                                   // NTRACK_CHUNK
    record (24, { 'B', 'a', 's', 's', 0 });             // TRKNAME_CHUNK
    Bytes software;
    putText (software, "Cakewalk 12");
    record (74, software);                              // SOFTVER_CHUNK
    record (255, {});                                   // END_CHUNK
    return file;
}

Bytes ole2 (const Bytes& inner)
{
    const Bytes header { 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
    Bytes file (header);
    file.insert (file.end(), inner.begin(), inner.end());
    return file;
}
} // namespace foreigntest

TEST (foreign_smf_reader)
{
    using namespace lynx::foreign;
    const auto smf = foreigntest::makeSmf ("Drums", 3, 0.0);

    lynx::midiimport::ParsedMidi parsed;
    std::string error;
    CHECK (parseStandardMidi (smf.data(), smf.size(), parsed, &error));
    CHECK (error.empty());
    CHECK (parsed.trackNames.size() == 1 && parsed.trackNames[0] == "Drums");
    CHECK (parsed.numerator == 4 && parsed.denominator == 4);
    CHECK (parsed.bpm == 120.0); // no tempo event: the MIDI default
    CHECK (parsed.events.size() >= 6);
    // three note-ons, each followed by its note-off
    int on = 0, off = 0;
    for (const auto& e : parsed.events)
    {
        on += (e.status & 0xf0) == 0x90 ? 1 : 0;
        off += (e.status & 0xf0) == 0x80 ? 1 : 0;
    }
    CHECK (on == 3 && off == 3);
    // 480 raw ticks per quarter at 960 LYNX ticks per quarter: a beat is 960 ticks
    CHECK (parsed.events[0].tick == 0);
    CHECK (parsed.events[1].tick == lynx::ppq);

    CHECK (! parseStandardMidi (smf.data(), 10, parsed, &error)); // truncated header
    CHECK (! error.empty());
}

TEST (foreign_cubase_project)
{
    using namespace lynx::foreign;
    const auto smf = foreigntest::makeSmf ("Lead", 2, 140.0);
    const auto cpr = foreigntest::makeCpr (smf, "13.0.40");

    CHECK (detect (cpr.data(), cpr.size(), "cpr") == Format::cubase);
    Report report;
    std::string error;
    CHECK (read (cpr.data(), cpr.size(), "cpr", report, error));
    CHECK (error.empty());
    CHECK (report.format == Format::cubase);
    CHECK (report.version == "13.0.40");
    CHECK (report.hasNotes() && report.noteCount() == 2);
    CHECK (report.midi.trackNames.size() == 1 && report.midi.trackNames[0] == "Lead");
    CHECK (std::abs (report.midi.bpm - 140.0) < 0.01);
    CHECK (report.tracks.size() == 1 && report.tracks[0].name == "Lead" && report.tracks[0].notes == 2);
    CHECK (! report.findings.empty());

    // a container with no MIDI still opens as a report, not as a broken project
    auto empty = foreigntest::makeCpr ({}, "12.0.0");
    Report emptyReport;
    CHECK (read (empty.data(), empty.size(), "cpr", emptyReport, error));
    CHECK (! emptyReport.hasNotes());
    CHECK (emptyReport.summary().find ("Cubase") != std::string::npos);

    // and the wrong format is refused
    const std::vector<std::uint8_t> junk { 'P', 'K', '!', '!' };
    Report junkReport;
    CHECK (! read (junk.data(), junk.size(), "cpr", junkReport, error));
    CHECK (! error.empty());
}

TEST (foreign_cubase_audio_beside_the_project)
{
    using namespace lynx::foreign;
    namespace fs = std::filesystem;

    // A Cubase project keeps its audio in the Audio folder beside the .cpr, and its clip positions in the chunk tree
    // this reader cannot decode. The files themselves are the recovery: each one on its own audio track.
    const auto base = fs::temp_directory_path() / "lynx-foreign-audio";
    std::error_code ec;
    fs::remove_all (base, ec);
    fs::create_directories (base / "Audio", ec);

    const auto makeWav = [] (std::uint32_t frames)
    {
        std::vector<std::uint8_t> out;
        const auto put32 = [&out] (std::uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back ((std::uint8_t) (v >> (8 * i))); };
        const auto put16 = [&out] (std::uint16_t v) { out.push_back ((std::uint8_t) (v & 0xff)); out.push_back ((std::uint8_t) (v >> 8)); };
        const auto dataBytes = frames * 4; // 16-bit stereo
        out.insert (out.end(), { 'R', 'I', 'F', 'F' }); put32 (36 + dataBytes); out.insert (out.end(), { 'W', 'A', 'V', 'E' });
        out.insert (out.end(), { 'f', 'm', 't', ' ' }); put32 (16); put16 (1); put16 (2); put32 (44100); put32 (44100 * 4); put16 (4); put16 (16);
        out.insert (out.end(), { 'd', 'a', 't', 'a' }); put32 (dataBytes);
        out.resize (out.size() + dataBytes);
        return out;
    };
    const auto write = [] (const fs::path& path, const std::vector<std::uint8_t>& bytes) {
        std::ofstream out (path, std::ios::binary);
        out.write (reinterpret_cast<const char*> (bytes.data()), (std::streamsize) bytes.size());
    };

    const auto cpr = foreigntest::makeCpr ({}, "14.0.31");
    write (base / "Take.cpr", cpr);
    write (base / "Audio" / "b-voice.wav", makeWav (44100));   // 1 second
    write (base / "Audio" / "a-music.wav", makeWav (88200));   // 2 seconds

    Report report;
    std::string error;
    CHECK (readFile ((base / "Take.cpr").string(), report, error));
    CHECK (error.empty());
    CHECK (report.version == "14.0.31"); // a version, not the next chunk name ("ARCH" used to win)
    CHECK (! report.hasNotes());
    CHECK (report.media.size() == 2);
    CHECK (report.media[0].name == "a-music.wav" && report.media[1].name == "b-voice.wav"); // file-name order
    CHECK (std::abs (report.media[0].seconds - 2.0) < 0.01);
    CHECK (std::abs (report.media[1].seconds - 1.0) < 0.01);
    CHECK (report.hasImportableContent());
    CHECK (report.summary().find ("2 audio files") != std::string::npos);

    lynx::Song song;
    lynx::midiimport::Options options;
    CHECK (importInto (song, report, options, "Imported", error));
    CHECK (error.empty());
    CHECK (song.channels.size() == 2);
    CHECK (song.patterns.size() == 1 && song.patterns[0].notes.empty()); // a song always has a pattern to show
    CHECK (song.channels[0].instrument == lynx::InstrumentKind::audioClip);
    CHECK (song.channels[0].samplePath == (base / "Audio" / "a-music.wav").string());
    CHECK (song.channels[0].name == "a-music");
    CHECK (song.playlistTracks.size() == 2 && song.playlistTracks[0].mode == lynx::PlaylistTrackMode::audio);
    CHECK (song.playlist.size() == 2);
    CHECK (song.playlist[0].kind == lynx::ClipKind::audio && song.playlist[0].channel == 0 && song.playlist[0].track == 0);
    CHECK (song.playlist[0].start == 0);
    const auto expected = (std::int64_t) std::llround (2.0 * song.bpm / 60.0 * (double) lynx::ppq);
    CHECK (song.playlist[0].length == expected);

    // a project with neither notes nor audio still refuses, and says why
    fs::remove_all (base / "Audio", ec);
    Report bare;
    CHECK (readFile ((base / "Take.cpr").string(), bare, error));
    CHECK (bare.media.empty() && ! bare.hasImportableContent());
    lynx::Song none;
    std::string reason;
    CHECK (! importInto (none, bare, options, "Nothing", reason));
    CHECK (reason.find ("no notes") != std::string::npos);

    fs::remove_all (base, ec);
}

TEST (foreign_cakewalk_projects)
{
    using namespace lynx::foreign;
    std::string error;

    // .wrk: the documented chunk records give the track names and the version
    const auto wrk = foreigntest::makeWrk();
    CHECK (detect (wrk.data(), wrk.size(), "wrk") == Format::cakewalkWrk);
    Report wrkReport;
    CHECK (read (wrk.data(), wrk.size(), "wrk", wrkReport, error));
    CHECK (wrkReport.format == Format::cakewalkWrk);
    CHECK (wrkReport.tracks.size() == 1 && wrkReport.tracks[0].name == "Bass");
    CHECK (wrkReport.version == "Cakewalk 12");
    CHECK (! wrkReport.hasNotes()); // the event streams are not decoded yet: reported, never guessed

    // .cwp: an OLE2 container; what can be recovered exactly is the MIDI it embeds
    const auto cwp = foreigntest::ole2 (foreigntest::makeSmf ("Keys", 2, 0.0));
    CHECK (detect (cwp.data(), cwp.size(), "cwp") == Format::cakewalk);
    Report cwpReport;
    CHECK (read (cwp.data(), cwp.size(), "cwp", cwpReport, error));
    CHECK (cwpReport.hasNotes() && cwpReport.noteCount() == 2);
    CHECK (cwpReport.tracks.size() == 1 && cwpReport.tracks[0].name == "Keys");

    // the recovered tracks become channels and a pattern through the MIDI importer
    lynx::Song song;
    lynx::midiimport::Options options;
    CHECK (importInto (song, cwpReport, options, "Imported", error));
    CHECK (error.empty());
    CHECK (song.channels.size() == 1 && song.patterns.size() == 1);
    CHECK (song.patterns[0].notes.size() == 2);
    CHECK (song.patterns[0].name == "Imported");
    CHECK (song.channels[0].name == "Keys");

    // a report with no notes refuses to import, with the reason
    lynx::Song other;
    std::string noNotes;
    CHECK (! importInto (other, wrkReport, options, "Nothing", noNotes));
    CHECK (! noNotes.empty() && other.patterns.empty());
}

TEST (foreign_extensions_and_detection)
{
    using namespace lynx::foreign;
    CHECK (isSupportedExtension ("cpr") && isSupportedExtension (".NPR") && isSupportedExtension ("wrk") && isSupportedExtension ("*.mid"));
    CHECK (! isSupportedExtension ("flp") && ! isSupportedExtension ("lynx") && ! isSupportedExtension (""));
    CHECK (std::string (openFilter()).find ("cpr") != std::string::npos);

    const auto smf = foreigntest::makeSmf ("", 1, 0.0);
    CHECK (detect (smf.data(), smf.size(), "") == Format::midi);          // by signature
    CHECK (detect (smf.data(), smf.size(), "cwp") == Format::midi);       // the signature wins
    const std::vector<std::uint8_t> riff { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'N', 'U', 'N', 'D', 'R', 'O', 'O', 'T' };
    CHECK (detect (riff.data(), riff.size(), "npr") == Format::cubase);
    CHECK (detect (riff.data(), riff.size(), "") == Format::cubase);
    const std::vector<std::uint8_t> nothing { 1, 2, 3, 4 };
    CHECK (detect (nothing.data(), nothing.size(), "flp") == Format::unknown);
    Report report;
    std::string error;
    CHECK (! read (nothing.data(), nothing.size(), "flp", report, error));
    CHECK (! error.empty());
}
