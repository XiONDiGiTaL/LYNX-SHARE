# Opening projects from other DAWs

`core/ForeignProject.h` reads the projects Cubase, Nuendo, Cakewalk, SONAR and BandLab write, at the level LYNX can
honour: tracks and the notes on them. It is platform-neutral (no JUCE), so it is covered by the core tests
(`tests/ForeignImportTests.inc`) exactly as it ships.

Opening one goes through `MainComponent::openFile` -> `MainComponent::importForeignProject`, which puts the recovered
tracks through the MIDI importer (`core/MidiImport.h`) as channels and a new pattern. Nothing about the existing
`.lynx` / `.flp` / `.mid` paths changed.

## What each container holds

| Format | Container | What this reader recovers exactly | What it reports instead of guessing |
| --- | --- | --- | --- |
| `.cpr`, `.npr` (Cubase, Nuendo) | RIFF with the eight-character form type `NUNDROOT` (Steinberg uses the whole chunk name where RIFF has four characters) | the app version token (`PAppVersion`), the chunk tree when its length width validates, plugin `Plugin UID` tokens, and every embedded standard MIDI file | the project's own note chunks; audio, which Cubase always keeps as files in a folder beside the project |
| `.cwp`, `.cwb` (Cakewalk, SONAR, BandLab) | OLE2 compound file (no public stream specification exists) | embedded standard MIDI | everything else; there is no published layout to read |
| `.cwz` | a zip package (Cakewalk's own backup format) | the fact that it is a package | the user is told to unpack it and open the `.cwp` |
| `.wrk` (older Cakewalk) | tag/length chunk stream; the record types are documented (Drumstick's `WrkChunkType`) | the chunk walk itself (validated against the known tag set and the file length), track names (tag 24), the software version (tag 74) | the note events in its stream records (tags 2 / 45), which are not decoded yet |
| `.mid`, `.midi` | standard MIDI file | everything the MIDI importer already handles | – |

The design rule is that a container is either read in a form that validates, or reported. A `.cpr` whose chunk walk
does not reconcile is not half-parsed, and a `.wrk` whose records do not cover the file is not decoded from a guess.

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
* **More MIDI lanes**: `midiimport::Options` already supports per-channel grouping, controller import and MIDI Out
  channels; `importForeignProject` currently asks for one channel per track.

## Tests

```sh
./scripts/build-local.sh          # the project's own core suite, which now includes the foreign reader
```

The tests build their own fixtures: a synthetic SMF, a `NUNDROOT` container, a `.wrk` record stream and an OLE2 file
with embedded MIDI. They check the notes, the tempo, the track names, the version token, the refusals (wrong format,
truncated file) and the import into a `Song`.
