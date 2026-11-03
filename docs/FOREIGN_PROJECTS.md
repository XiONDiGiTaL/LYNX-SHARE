# Opening projects from other DAWs

`core/ForeignProject.h` reads the projects Cubase, Nuendo, Cakewalk, SONAR, BandLab, Bitwig Studio, Studio One,
Cubasis and VST Live write, at the level LYNX can honour. It is platform-neutral (no JUCE), so it is covered by the core
tests (`tests/ForeignImportTests.inc`, `tests/DawProjectTests.inc`) exactly as it ships.

Opening one goes through `MainComponent::openFile` -> `MainComponent::importForeignProject`. MIDI files and the
recovered Cubase / Cakewalk notes go through the MIDI importer (`core/MidiImport.h`) as channels and a new pattern; a
DAWproject brings its own arrangement and is mapped by `core/DawProject.h`. Nothing about the existing `.lynx` /
`.flp` / `.mid` paths changed.

## What each container holds

| Format | Container | What this reader recovers exactly | What it reports instead of guessing |
| --- | --- | --- | --- |
| `.dawproject` (Bitwig Studio, Studio One, Cubase 14, Cubasis, VST Live) | ZIP with `project.xml`, `metadata.xml` and the media | tracks (names, colours, mute, grouping depth), note clips as patterns on the Playlist, audio clips as audio-clip channels with their files extracted from the ZIP, tempo, time signature, markers, fades and clip references | plugins (named, never loaded), automation, track levels / pan, mixer routing, tempo and time-signature automation, clip-launcher scenes |
| `.cpr`, `.npr` (Cubase, Nuendo) | RIFF with the eight-character form type `NUNDROOT` (Steinberg uses the whole chunk name where RIFF has four characters) | the app version token (`PAppVersion`), the chunk tree when its length width validates, plugin `Plugin UID` tokens, and every embedded standard MIDI file | the project's own note chunks; audio, which Cubase always keeps as files in a folder beside the project |
| `.cwp`, `.cwb` (Cakewalk, SONAR, BandLab) | OLE2 compound file (no public stream specification exists) | embedded standard MIDI | everything else; there is no published layout to read |
| `.cwz` | a zip package (Cakewalk's own backup format) | the fact that it is a package | the user is told to unpack it and open the `.cwp` |
| `.wrk` (older Cakewalk) | tag/length chunk stream; the record types are documented (Drumstick's `WrkChunkType`) | the chunk walk itself (validated against the known tag set and the file length), track names (tag 24), the software version (tag 74) | the note events in its stream records (tags 2 / 45), which are not decoded yet |
| `.mid`, `.midi` | standard MIDI file | everything the MIDI importer already handles | – |

The design rule is that a container is either read in a form that validates, or reported. A `.cpr` whose chunk walk
does not reconcile is not half-parsed, a `.wrk` whose records do not cover the file is not decoded from a guess, and a
`.dawproject` whose `project.xml` does not parse is refused with the XML error.

## DAWproject

DAWproject is the only one of these formats with a published specification
([bitwig/dawproject](https://github.com/bitwig/dawproject)), so the reader is a parser rather than a recovery
exercise. `core/DawProject.cpp` opens the ZIP itself (stored and DEFLATE entries, including ZIP64 sizes), parses the
XML subset `project.xml` uses, and then maps the arrangement:

* **Tracks** — `Structure/Track` elements become Playlist tracks. A `Channel role="master"` is LYNX's mixer master, not
  a Playlist track. A nested Track becomes a grouped Playlist track. `Mute` and `loaded="false"` (a disabled track)
  both arrive muted, the latter with a finding.
* **Note clips** — each `Clip` with `Notes` becomes a `Pattern` (notes converted from beats or seconds at 960 ticks per
  quarter), placed on the Playlist at the clip's `time` with its `playStart` as the clip offset and `duration` as the
  clip length. A clip that only carries a `reference` copies the referenced clip's content.
* **Audio clips** — Bitwig nests the audio event inside the arrangement clip (`Clips/Clip/Warps/Audio/File`), which the
  reader flattens. Each unique file becomes one `InstrumentKind::audioClip` channel; the file is extracted from the ZIP
  into the folder the importer is given (`ProjectPackage::defaultExtractRoot()` in the UI), so its `samplePath` stays
  valid after the import. `external="true"` files are resolved next to the package and never copied.
* **Transport and markers** — the start tempo and time signature, and every `Arrangement/Markers/Marker`.
* **Fades** — `fadeInTime` / `fadeOutTime` on audio clips, in ticks.

Time units are per element (`timeUnit`, `contentTimeUnit`, `fadeTimeUnit`); a seconds-based timeline is converted with
the project tempo. The reader reports what it deliberately leaves out rather than approximating it: plugin instances
(third-party plugins are black boxes), automation and tempo / signature curves, track volume and pan, mixer routing and
clip-launcher scenes.

## Why embedded MIDI matters

Cubase and Cakewalk both store their events in proprietary structures, but both let the user export a standard MIDI
file, and many project files also embed MIDI parts as ordinary `MThd` blobs. `recoverEmbeddedMidi` scans for those,
parses each one with the SMF reader below, and merges them into one model. That is what makes an import of a real
Cubase project produce notes today; when nothing is embedded, the dialog says so and points at
*File > Export > MIDI File* instead of importing an empty song.

## The MIDI reader

`parseStandardMidi` is a self-contained SMF reader (formats 0 / 1 / 2): variable-length delta times, running status,
note on/off, controllers, program changes, pitch bend, tempo (FF 51), time signature (FF 58) and track names (FF 03).
It converts raw ticks to LYNX ticks at 960 per quarter note, and handles SMPTE time divisions at the 120 bpm
reference. Sysex is skipped; it carries no notes.

## Extending it

* **`.wrk` notes**: the event streams are MIDI-like. Drumstick's `QWrk` (`library/drumstick/qwrk.cpp`) and `wrk2mid`
  are the reference for the record layout; the walk in `readCakewalkWrk` already hands you each record's payload.
* **`.cpr` names**: the chunk tree is walked in `walkCubase`; adding a record name to `CubaseChunk` handling is enough
  to surface more of the tree.
* **DAWproject plugins**: `ProjectParser::collectDevices` already counts them; a device could map to a LYNX substitute
  the way `core/PluginSubstitutes.h` does for missing FL plugins.
* **DAWproject automation**: `Arrangement/TempoAutomation`, `TimeSignatureAutomation` and `Points` clips are detected
  and reported today; mapping them would use `Song::automations`.
* **More MIDI lanes**: `midiimport::Options` already supports per-channel grouping, controller import and MIDI Out
  channels; `importForeignProject` currently asks for one channel per track.

## Tests

```sh
./scripts/build-local.sh          # the project's own core suite, which now includes the foreign and DAWproject readers
```

The tests build their own fixtures: a synthetic SMF, a `NUNDROOT` container, a `.wrk` record stream, an OLE2 file with
embedded MIDI, and DAWproject ZIPs written from scratch (stored and DEFLATE entries). They check the notes, tempo,
track names, version token, the DAWproject tempo / time signature / tracks / clips / audio extraction / markers, the
import into a `Song`, the refusals (wrong container, missing `project.xml`, malformed XML) and the missing-audio
findings.
