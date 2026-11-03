// --- DAWproject (core/DawProject.h): the open ZIP + project.xml exchange format ---

#include "core/DawProject.h"

#include <filesystem>
#include <fstream>
#include <cmath>

namespace dawprojecttest
{
using Bytes = std::vector<std::uint8_t>;

void putLe16 (Bytes& v, std::uint32_t x)
{
    v.push_back ((std::uint8_t) (x & 0xff));
    v.push_back ((std::uint8_t) ((x >> 8) & 0xff));
}

void putLe32 (Bytes& v, std::uint32_t x)
{
    for (int bit = 0; bit <= 24; bit += 8)
        v.push_back ((std::uint8_t) (x >> bit));
}

void putText (Bytes& v, const std::string& text)
{
    v.insert (v.end(), text.begin(), text.end());
}

// A raw DEFLATE stream made of one stored block (RFC 1951): what ZIP method 8 carries for incompressible data, and
// enough to exercise the reader's inflate path without a compressor.
Bytes rawDeflateStored (const Bytes& data)
{
    Bytes out;
    out.push_back (0x01); // BFINAL = 1, BTYPE = 00 (stored)
    putLe16 (out, (std::uint32_t) data.size());
    putLe16 (out, (std::uint32_t) (~data.size() & 0xffff));
    out.insert (out.end(), data.begin(), data.end());
    return out;
}

struct ZipInput
{
    std::string name;
    Bytes data;
    bool deflate = false;
};

// A minimal ZIP (local headers, central directory, EOCD). The CRCs are left at zero: this reader does not verify them,
// and the directory's sizes and offsets are what it walks.
Bytes makeZip (const std::vector<ZipInput>& entries)
{
    Bytes out, central;
    for (const auto& entry : entries)
    {
        const auto payload = entry.deflate ? rawDeflateStored (entry.data) : entry.data;
        const auto offset = (std::uint32_t) out.size();

        putLe32 (out, 0x04034b50u);
        putLe16 (out, 20);                   // version needed
        putLe16 (out, 0);                    // flags
        putLe16 (out, entry.deflate ? 8 : 0); // method
        putLe16 (out, 0);                    // modification time
        putLe16 (out, 0);                    // modification date
        putLe32 (out, 0);                    // CRC-32 (not checked)
        putLe32 (out, (std::uint32_t) payload.size());
        putLe32 (out, (std::uint32_t) entry.data.size());
        putLe16 (out, (std::uint32_t) entry.name.size());
        putLe16 (out, 0);                    // extra field
        putText (out, entry.name);
        out.insert (out.end(), payload.begin(), payload.end());

        putLe32 (central, 0x02014b50u);
        putLe16 (central, 20);                   // version made by
        putLe16 (central, 20);                   // version needed
        putLe16 (central, 0);                    // flags
        putLe16 (central, entry.deflate ? 8 : 0); // method
        putLe16 (central, 0);                    // time
        putLe16 (central, 0);                    // date
        putLe32 (central, 0);                    // CRC-32
        putLe32 (central, (std::uint32_t) payload.size());
        putLe32 (central, (std::uint32_t) entry.data.size());
        putLe16 (central, (std::uint32_t) entry.name.size());
        putLe16 (central, 0);                    // extra
        putLe16 (central, 0);                    // comment
        putLe16 (central, 0);                    // disk
        putLe16 (central, 0);                    // internal attributes
        putLe32 (central, 0);                    // external attributes
        putLe32 (central, offset);
        putText (central, entry.name);
    }

    const auto centralOffset = (std::uint32_t) out.size();
    out.insert (out.end(), central.begin(), central.end());
    putLe32 (out, 0x06054b50u);
    putLe16 (out, 0);
    putLe16 (out, 0);
    putLe16 (out, (std::uint32_t) entries.size());
    putLe16 (out, (std::uint32_t) entries.size());
    putLe32 (out, (std::uint32_t) central.size());
    putLe32 (out, centralOffset);
    putLe16 (out, 0); // comment length
    return out;
}

Bytes fromText (const std::string& text)
{
    return Bytes (text.begin(), text.end());
}

const char* bitwigProjectXml()
{
    // The Bitwig Studio 5.0 example from the DAWproject README with fixed values: an instrument track with one note
    // clip, an audio track whose clip is a nested timeline of one audio event, and a master track.
    return R"xml(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Project version="1.0">
  <Application name="Bitwig Studio" version="5.0"/>
  <Transport>
    <Tempo value="149.000000" unit="bpm" id="id0" name="Tempo"/>
    <TimeSignature numerator="4" denominator="4" id="id1"/>
  </Transport>
  <Structure>
    <Track id="id2" name="Bass" contentType="notes" loaded="true" color="#a2eabf">
      <Channel audioChannels="2" role="regular" id="id3">
        <Devices>
          <ClapPlugin deviceID="org.example.synth" deviceName="Surge XT" id="id7" name="Surge XT">
            <Parameters/>
            <Enabled value="true" id="id8" name="On/Off"/>
          </ClapPlugin>
        </Devices>
        <Mute value="false" id="id6" name="Mute"/>
        <Pan value="0.500000" id="id5" name="Pan"/>
        <Volume value="0.659140" id="id4" name="Volume"/>
      </Channel>
    </Track>
    <Track id="id9" name="Drumloop" contentType="audio" loaded="true" color="#b53bba">
      <Channel audioChannels="2" role="regular" id="id10">
        <Mute value="false" id="id13" name="Mute"/>
        <Pan value="0.500000" id="id12" name="Pan"/>
        <Volume value="0.177125" id="id11" name="Volume"/>
      </Channel>
    </Track>
    <Track id="id14" name="Master" contentType="audio notes" loaded="true">
      <Channel audioChannels="2" role="master" id="id15">
        <Mute value="false" id="id18" name="Mute"/>
        <Volume value="1.000000" id="id16" name="Volume"/>
      </Channel>
    </Track>
  </Structure>
  <Arrangement id="id19">
    <Markers id="mid1">
      <Marker time="4.0" name="Chorus" id="mid2"/>
    </Markers>
    <Lanes timeUnit="beats" id="id20">
      <Lanes track="id2" id="id21">
        <Clips id="id22">
          <Clip time="0.0" duration="8.0" playStart="0.0" id="id23">
            <Notes id="id24">
              <Note time="0.000000" duration="0.250000" channel="0" key="65" vel="0.787402" rel="0.787402"/>
              <Note time="1.000000" duration="0.250000" channel="0" key="70" vel="0.500000" rel="0.250000"/>
              <Note time="4.000000" duration="2.500000" channel="0" key="53" vel="1.000000" rel="0.500000"/>
            </Notes>
          </Clip>
        </Clips>
      </Lanes>
      <Lanes track="id9" id="id25">
        <Clips id="id26">
          <Clip time="8.0" duration="8.0" playStart="0.0" loopStart="0.0" loopEnd="8.0" fadeTimeUnit="beats"
                fadeInTime="0.5" fadeOutTime="0.25" name="Drumfunk3 170bpm" id="id27">
            <Clips id="id28">
              <Clip time="0.0" duration="8.00003433227539" contentTimeUnit="beats" playStart="0.0"
                    fadeTimeUnit="beats" fadeInTime="0.0" fadeOutTime="0.0" id="id29">
                <Warps contentTimeUnit="seconds" timeUnit="beats" id="id31">
                  <Audio algorithm="stretch" channels="2" duration="2.823541666666667" sampleRate="48000" id="id30">
                    <File path="audio/Drumfunk3 170bpm.wav"/>
                  </Audio>
                  <Warp time="0.0" contentTime="0.0"/>
                  <Warp time="8.00003433227539" contentTime="2.823541666666667"/>
                </Warps>
              </Clip>
            </Clips>
          </Clip>
        </Clips>
      </Lanes>
      <Lanes track="id14" id="id32">
        <Clips id="id33"/>
      </Lanes>
    </Lanes>
  </Arrangement>
  <Scenes/>
</Project>)xml";
}

Bytes makeBitwigProject()
{
    return makeZip ({ { "project.xml", fromText (bitwigProjectXml()) },
                      { "metadata.xml", fromText ("<MetaData/>") },
                      { "audio/Drumfunk3 170bpm.wav", { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E' } } });
}

Bytes makeSecondsProject()
{
    // A seconds-based arrangement (Studio One style) with a deflated project.xml and a 3/8 signature.
    const std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="Studio One" version="6.5"/>
  <Transport>
    <Tempo value="120.0" id="t"/>
    <TimeSignature numerator="3" denominator="8" id="s"/>
  </Transport>
  <Structure>
    <Track id="t1" name="Keys" contentType="notes">
      <Channel role="regular" id="c1"/>
    </Track>
  </Structure>
  <Arrangement>
    <Lanes timeUnit="seconds" id="l0">
      <Lanes track="t1" id="l1">
        <Clips id="c">
          <Clip time="2.0" duration="1.5" playStart="0.5" contentTimeUnit="seconds" id="clip1">
            <Notes timeUnit="beats" id="n">
              <Note time="0.25" duration="0.5" channel="0" key="60" vel="0.9" rel="0.4"/>
            </Notes>
          </Clip>
        </Clips>
      </Lanes>
    </Lanes>
  </Arrangement>
</Project>)xml";
    return makeZip ({ { "project.xml", fromText (xml), true } });
}

std::filesystem::path makeTempFolder (const char* tag)
{
    std::error_code ec;
    auto folder = std::filesystem::temp_directory_path (ec) / (std::string ("lynx_dawproject_") + tag);
    std::filesystem::remove_all (folder, ec);
    std::filesystem::create_directories (folder, ec);
    return folder;
}

bool writeBytes (const std::filesystem::path& path, const Bytes& bytes)
{
    std::error_code ec;
    std::filesystem::create_directories (path.parent_path(), ec);
    std::ofstream out (path, std::ios::binary | std::ios::trunc);
    if (! out)
        return false;
    out.write (reinterpret_cast<const char*> (bytes.data()), (std::streamsize) bytes.size());
    return (bool) out;
}

bool contains (const std::vector<std::string>& lines, const char* text)
{
    for (const auto& line : lines)
        if (line.find (text) != std::string::npos)
            return true;
    return false;
}
} // namespace dawprojecttest

TEST (dawproject_reads_the_arrangement)
{
    using namespace lynx;
    using namespace dawprojecttest;
    const auto zip = makeBitwigProject();

    CHECK (foreign::detect (zip.data(), zip.size(), "dawproject") == foreign::Format::dawproject);
    // the extension is not needed: a ZIP carrying project.xml is recognised by content
    CHECK (foreign::detect (zip.data(), zip.size(), "") == foreign::Format::dawproject);

    foreign::Report report;
    std::string error;
    CHECK (foreign::read (zip.data(), zip.size(), "dawproject", report, error));
    CHECK (error.empty());
    CHECK (report.format == foreign::Format::dawproject);
    CHECK (foreign::formatName (report.format) == std::string ("DAWproject"));
    CHECK (report.version == "1.0");
    CHECK (report.application == "Bitwig Studio");
    CHECK (report.hasImportableContent());

    const auto& project = report.dawproject;
    CHECK (std::abs (project.bpm - 149.0) < 1e-9);
    CHECK (project.numerator == 4 && project.denominator == 4);
    CHECK (project.unit == dawproject::TimeUnit::beats);
    CHECK (project.tracks.size() == 2); // the master is the mixer master, not a Playlist track
    CHECK (project.tracks[0].name == "Bass" && project.tracks[1].name == "Drumloop");
    CHECK (project.tracks[0].colour == 0xffa2eabfu && project.tracks[1].colour == 0xffb53bbau);

    CHECK (project.noteCount == 3);
    CHECK (project.clipCount == 2);
    CHECK (project.audioCount == 1);
    CHECK (report.noteCount() == 3);
    CHECK (report.tracks.size() == 2 && report.tracks[0].notes == 3 && report.tracks[1].notes == 0);

    CHECK (project.pluginCount == 1);
    CHECK (contains (project.findings, "written by Bitwig Studio 5.0"));
    CHECK (contains (project.findings, "plugins named in the project: 1"));
    CHECK (contains (project.findings, "track levels, pan, automation, devices and mixer routing"));

    const auto& notes = project.tracks[0].clips[0];
    CHECK (notes.kind == dawproject::ClipKind::notes);
    CHECK (notes.notes.size() == 3);
    CHECK (std::abs (notes.notes[0].time) < 1e-9 && notes.notes[0].key == 65);
    CHECK (std::abs (notes.notes[0].velocity - 0.787402) < 1e-4);
    CHECK (std::abs (notes.notes[2].duration - 2.5) < 1e-9);

    const auto& audio = project.tracks[1].clips[0];
    CHECK (audio.kind == dawproject::ClipKind::audio);
    CHECK (audio.name == "Drumfunk3 170bpm");
    CHECK (audio.audio.path == "audio/Drumfunk3 170bpm.wav");
    CHECK (! audio.audio.external && audio.audio.hasFile);
    CHECK (audio.audio.channels == 2 && audio.audio.sampleRate == 48000);
    CHECK (audio.audio.algorithm == "stretch");
    CHECK (audio.hasFadeIn && std::abs (audio.fadeIn - 0.5) < 1e-9);
    CHECK (audio.hasFadeOut && std::abs (audio.fadeOut - 0.25) < 1e-9);

    CHECK (project.markers.size() == 1 && project.markers[0].name == "Chorus");
    CHECK (std::abs (project.markers[0].time - 4.0) < 1e-9);

    CHECK (report.summary().find ("DAWproject 1.0: 2 tracks, 3 notes, 2 clips (1 audio)") != std::string::npos);
}

TEST (dawproject_imports_tracks_clips_and_audio)
{
    using namespace lynx;
    using namespace dawprojecttest;
    const auto folder = makeTempFolder ("import");
    const auto path = folder / "Song.dawproject";
    CHECK (writeBytes (path, makeBitwigProject()));

    foreign::Report report;
    std::string error;
    CHECK (foreign::readFile (path.string(), report, error));
    CHECK (report.dawproject.sourcePath == path.string());

    const auto media = folder / "media";
    Song song;
    midiimport::Options options;
    CHECK (foreign::importInto (song, report, options, "Imported", media.string(), error));
    CHECK (error.empty());

    // tempo and signature
    CHECK (std::abs (song.bpm - 149.0) < 1e-9);
    CHECK (song.beatsPerBar == 4);

    // one Playlist track per non-master track, with the grouping and mute flags
    CHECK (song.playlistTracks.size() == 2);
    CHECK (song.playlistTracks[0].name == "Bass" && ! song.playlistTracks[0].grouped);
    CHECK (song.playlistTracks[1].name == "Drumloop" && ! song.playlistTracks[1].grouped);

    // the note clip: a pattern on the Playlist with its notes in ticks (960 per beat)
    CHECK (song.patterns.size() == 1);
    const auto& pattern = song.patterns[0];
    CHECK (pattern.name == "Bass");
    CHECK (pattern.notes.size() == 3);
    CHECK (pattern.notes[0].start == 0 && pattern.notes[0].length == 240 && pattern.notes[0].key == 65);
    CHECK (pattern.notes[1].start == ppq && pattern.notes[1].key == 70);
    CHECK (pattern.notes[2].start == 4 * ppq && pattern.notes[2].length == 2400 && pattern.notes[2].key == 53);
    CHECK (std::abs (pattern.notes[0].velocity - 0.787402f) < 1e-4f);
    CHECK (song.currentPattern == 0);

    // two clips: the pattern at beat 0, the audio at beat 8 (both 8 beats long)
    CHECK (song.playlist.size() == 2);
    CHECK (song.playlist[0].kind == lynx::ClipKind::pattern && song.playlist[0].pattern == 0);
    CHECK (song.playlist[0].start == 0 && song.playlist[0].length == 8 * ppq && song.playlist[0].offset == 0);
    CHECK (song.playlist[1].kind == lynx::ClipKind::audio);
    CHECK (song.playlist[1].start == 8 * ppq && song.playlist[1].length == 8 * ppq && song.playlist[1].offset == 0);
    CHECK (song.playlist[1].fadeIn == ppq / 2 && song.playlist[1].fadeOut == ppq / 4);

    // the audio file was extracted out of the ZIP and the channel points at it
    CHECK (song.channels.size() == 2);
    CHECK (song.channels[0].name == "Bass" && song.channels[0].instrument == InstrumentKind::synth);
    CHECK (song.channels[1].name == "Drumloop" && song.channels[1].instrument == InstrumentKind::audioClip);
    CHECK (! song.channels[1].samplePath.empty());
    CHECK (std::filesystem::exists (song.channels[1].samplePath));
    CHECK (std::filesystem::exists (media / "audio" / "Drumfunk3 170bpm.wav"));
    CHECK (song.playlist[1].channel == 1);

    CHECK (song.playlistTracks[0].mode == PlaylistTrackMode::instrument && song.playlistTracks[0].channel == 0);
    CHECK (song.playlistTracks[1].mode == PlaylistTrackMode::audio);

    // markers and song mode
    CHECK (song.markers().size() == 1);
    CHECK (song.markers()[0].tick == 4 * ppq && song.markers()[0].name == "Chorus");
    CHECK (song.mode == PlayMode::song);

    std::filesystem::remove_all (folder);
}

TEST (dawproject_seconds_deflate_and_signature)
{
    using namespace lynx;
    using namespace dawprojecttest;
    const auto zip = makeSecondsProject();
    CHECK (foreign::detect (zip.data(), zip.size(), "dawproject") == foreign::Format::dawproject);

    foreign::Report report;
    std::string error;
    CHECK (foreign::read (zip.data(), zip.size(), "dawproject", report, error));
    CHECK (report.application == "Studio One");
    CHECK (std::abs (report.dawproject.bpm - 120.0) < 1e-9);
    CHECK (report.dawproject.numerator == 3 && report.dawproject.denominator == 8);
    CHECK (report.dawproject.unit == dawproject::TimeUnit::seconds);
    CHECK (report.noteCount() == 1);

    Song song;
    midiimport::Options options;
    CHECK (foreign::importInto (song, report, options, "Seconds", error));
    CHECK (song.beatsPerBar == 3);
    CHECK (contains (report.findings, "denominator (8) is not imported"));

    // 120 BPM: one second is two beats, so 2 s = 3840 ticks, 1.5 s = 2880, 0.5 s = 960
    CHECK (song.playlist.size() == 1);
    CHECK (song.playlist[0].start == 2 * 2 * ppq);
    CHECK (song.playlist[0].length == 3 * ppq);
    CHECK (song.playlist[0].offset == ppq);
    CHECK (song.patterns.size() == 1 && song.patterns[0].notes.size() == 1);
    CHECK (song.patterns[0].notes[0].start == ppq / 4); // 0.25 beat
    CHECK (song.patterns[0].notes[0].length == ppq / 2);
}

TEST (dawproject_groups_references_and_disabled_tracks)
{
    using namespace lynx;
    using namespace dawprojecttest;
    const std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Transport><Tempo value="100.0"/><TimeSignature numerator="4" denominator="4"/></Transport>
  <Structure>
    <Track id="g" name="Group">
      <Channel role="regular" id="cg"><Mute value="true"/></Channel>
      <Track id="c" name="Child" loaded="false">
        <Channel role="regular" id="cc"/>
      </Track>
    </Track>
  </Structure>
  <Arrangement>
    <Lanes timeUnit="beats">
      <Lanes track="g">
        <Clips>
          <Clip id="a" time="0.0" duration="4.0">
            <Notes>
              <Note time="0.0" duration="1.0" key="60"/>
              <Note time="1.0" duration="1.0" key="62"/>
            </Notes>
          </Clip>
          <Clip reference="a" time="4.0" duration="4.0"/>
        </Clips>
      </Lanes>
    </Lanes>
  </Arrangement>
</Project>)xml";
    const auto zip = makeZip ({ { "project.xml", fromText (xml) } });

    foreign::Report report;
    std::string error;
    CHECK (foreign::read (zip.data(), zip.size(), "dawproject", report, error));
    CHECK (report.dawproject.tracks.size() == 2);
    CHECK (report.dawproject.tracks[0].name == "Group" && report.dawproject.tracks[0].depth == 0);
    CHECK (report.dawproject.tracks[1].name == "Child" && report.dawproject.tracks[1].depth == 1);
    CHECK (report.dawproject.tracks[0].muted && report.dawproject.tracks[0].hasMute);
    CHECK (! report.dawproject.tracks[1].loaded);
    // a reference alias copies the clip's notes: both clips carry them
    CHECK (report.dawproject.noteCount == 4);
    CHECK (report.dawproject.clipCount == 2);

    Song song;
    midiimport::Options options;
    CHECK (foreign::importInto (song, report, options, "Grouped", error));
    CHECK (song.playlistTracks.size() == 2);
    CHECK (song.playlistTracks[0].grouped == false);
    CHECK (song.playlistTracks[1].grouped == true);
    CHECK (song.playlistTracks[0].muted && song.playlistTracks[1].muted); // muted, and disabled -> muted
    CHECK (contains (report.findings, "a disabled track was imported muted: Child"));

    CHECK (song.patterns.size() == 2);
    CHECK (song.patterns[0].name == "Group" && song.patterns[1].name == "Group 2");
    CHECK (song.patterns[0].notes.size() == 2 && song.patterns[1].notes.size() == 2);
    CHECK (song.playlist.size() == 2);
    CHECK (song.playlist[0].start == 0 && song.playlist[1].start == 4 * ppq);
    CHECK (song.channels.size() == 1); // both patterns belong to the same track/channel
}

TEST (dawproject_reports_missing_audio)
{
    using namespace lynx;
    using namespace dawprojecttest;
    const std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Transport><Tempo value="120.0"/></Transport>
  <Structure>
    <Track id="t" name="Audio"><Channel role="regular" id="c"/></Track>
  </Structure>
  <Arrangement>
    <Lanes timeUnit="beats">
      <Lanes track="t">
        <Clips>
          <Clip time="0.0" duration="4.0">
            <Clips><Clip time="0.0" duration="4.0" contentTimeUnit="beats" playStart="0.0">
              <Warps contentTimeUnit="seconds"><Audio duration="1.0">
                <File path="audio/missing.wav"/>
              </Audio></Warps>
            </Clip></Clips>
          </Clip>
          <Clip time="8.0" duration="2.0">
            <Clips><Clip time="0.0" duration="2.0" contentTimeUnit="beats" playStart="0.0">
              <Warps contentTimeUnit="seconds"><Audio duration="1.0">
                <File path="C:/elsewhere/external.wav" external="true"/>
              </Audio></Warps>
            </Clip></Clips>
          </Clip>
        </Clips>
      </Lanes>
    </Lanes>
  </Arrangement>
</Project>)xml";
    const auto folder = makeTempFolder ("missing");
    const auto path = folder / "Missing.dawproject";
    CHECK (writeBytes (path, makeZip ({ { "project.xml", fromText (xml) } })));

    foreign::Report report;
    std::string error;
    CHECK (foreign::readFile (path.string(), report, error));

    const auto media = folder / "media";
    Song song;
    midiimport::Options options;
    CHECK (foreign::importInto (song, report, options, "Missing", media.string(), error));

    // both clips still import, silent, and the report says why
    CHECK (song.playlist.size() == 2);
    CHECK (song.channels.size() == 2);
    CHECK (song.channels[0].samplePath.empty() && song.channels[1].samplePath.empty());
    CHECK (contains (report.findings, "audio file missing from the package: audio/missing.wav"));
    CHECK (contains (report.findings, "audio file not found: C:/elsewhere/external.wav"));
    CHECK (contains (report.findings, "audio clip has no extracted file: audio/missing.wav"));

    std::filesystem::remove_all (folder);
}

TEST (dawproject_refuses_what_it_cannot_read)
{
    using namespace lynx;
    using namespace dawprojecttest;
    std::string error;

    // the extension alone is enough to route a file here, but the container still has to be a ZIP
    const dawprojecttest::Bytes notZip = fromText ("this is not a ZIP file at all");
    foreign::Report report;
    CHECK (foreign::detect (notZip.data(), notZip.size(), "dawproject") == foreign::Format::dawproject);
    CHECK (! foreign::read (notZip.data(), notZip.size(), "dawproject", report, error));
    CHECK (error.find ("not a DAWproject") != std::string::npos);

    CHECK (! foreign::read (nullptr, 0, "dawproject", report, error));
    CHECK (error.find ("the file is empty") != std::string::npos);

    // a ZIP is not a DAWproject without project.xml
    const auto metadataOnly = makeZip ({ { "metadata.xml", fromText ("<MetaData/>") } });
    CHECK (! foreign::read (metadataOnly.data(), metadataOnly.size(), "dawproject", report, error));
    CHECK (error.find ("no project.xml") != std::string::npos);

    // malformed XML
    const auto broken = makeZip ({ { "project.xml", fromText ("<Project><Track></Project>") } });
    CHECK (! foreign::read (broken.data(), broken.size(), "dawproject", report, error));
    CHECK (error.find ("project.xml:") != std::string::npos);

    // well-formed XML that is not a DAWproject
    const auto otherRoot = makeZip ({ { "project.xml", fromText ("<Foo/>") } });
    CHECK (! foreign::read (otherRoot.data(), otherRoot.size(), "dawproject", report, error));
    CHECK (error.find ("no Project root") != std::string::npos);

    // an empty project parses, but has nothing to import
    const auto empty = makeZip ({ { "project.xml", fromText ("<Project version=\"1.0\"/>") } });
    CHECK (foreign::read (empty.data(), empty.size(), "dawproject", report, error));
    CHECK (! report.hasImportableContent());
    Song song;
    midiimport::Options options;
    CHECK (! foreign::importInto (song, report, options, "Empty", error));
    CHECK (error.find ("no tracks, clips or notes") != std::string::npos);

    // the extension is advertised and accepted
    CHECK (foreign::isSupportedExtension ("dawproject"));
    CHECK (foreign::isSupportedExtension (".DAWPROJECT"));
    CHECK (! foreign::isSupportedExtension ("dawproj-not"));
    CHECK (std::string (foreign::openFilter()).find ("dawproject") != std::string::npos);
}
