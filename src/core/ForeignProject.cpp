#include "core/ForeignProject.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace lynx::foreign
{
namespace
{
constexpr std::size_t npos = std::string::npos;

std::uint16_t be16 (const std::uint8_t* p) noexcept
{
    return (std::uint16_t) (((std::uint16_t) p[0] << 8) | (std::uint16_t) p[1]);
}

std::uint32_t be32 (const std::uint8_t* p) noexcept
{
    return ((std::uint32_t) p[0] << 24) | ((std::uint32_t) p[1] << 16) | ((std::uint32_t) p[2] << 8) | (std::uint32_t) p[3];
}

std::uint32_t le32 (const std::uint8_t* p) noexcept
{
    return (std::uint32_t) p[0] | ((std::uint32_t) p[1] << 8) | ((std::uint32_t) p[2] << 16) | ((std::uint32_t) p[3] << 24);
}

std::uint64_t le64 (const std::uint8_t* p) noexcept
{
    return (std::uint64_t) le32 (p) | ((std::uint64_t) le32 (p + 4) << 32);
}

bool printable (std::uint8_t c) noexcept
{
    return c >= 0x20 && c < 0x7f;
}

std::size_t findBytes (const std::uint8_t* d, std::size_t n, const char* needle, std::size_t length, std::size_t from = 0)
{
    if (length == 0 || n < length)
        return npos;
    for (std::size_t i = from; i + length <= n; ++i)
        if (std::memcmp (d + i, needle, length) == 0)
            return i;
    return npos;
}

// A printable ASCII run, with the NUL padding the project formats put in front of a text token skipped.
std::string printableRun (const std::uint8_t* d, std::size_t end, std::size_t at, std::size_t maxLength)
{
    while (at < end && d[at] == 0)
        ++at;
    std::string out;
    while (at < end && out.size() < maxLength && printable (d[at]))
        out += (char) d[at++];
    while (! out.empty() && out.back() == ' ')
        out.pop_back();
    return out;
}

void addUnique (std::vector<std::string>& list, const std::string& value)
{
    if (value.empty())
        return;
    if (std::find (list.begin(), list.end(), value) == list.end())
        list.push_back (value);
}

// --- the audio that lives beside a project ----------------------------------------------------------------------
std::string lowerAscii (std::string s)
{
    for (auto& c : s)
        c = (char) std::tolower ((unsigned char) c);
    return s;
}

std::string fileNameOf (const std::string& path)
{
    const auto cut = path.find_last_of ("/\\");
    return cut == npos ? path : path.substr (cut + 1);
}

std::string stemOf (const std::string& path)
{
    auto name = fileNameOf (path);
    if (const auto dot = name.find_last_of ('.'); dot != npos && dot > 0)
        name.erase (dot);
    return name;
}

std::string extensionOf (const std::string& path)
{
    const auto name = fileNameOf (path);
    const auto dot = name.find_last_of ('.');
    return dot == npos ? std::string() : lowerAscii (name.substr (dot + 1));
}

bool isAudioFile (const std::string& path)
{
    const auto ext = extensionOf (path);
    return ext == "wav" || ext == "wave" || ext == "mp3" || ext == "mp2" || ext == "aif" || ext == "aiff"
        || ext == "flac" || ext == "m4a" || ext == "ogg" || ext == "caf" || ext == "w64";
}

std::uint32_t be32At (const std::vector<std::uint8_t>& d, std::size_t at)
{
    return at + 4 <= d.size() ? ((std::uint32_t) d[at] << 24) | ((std::uint32_t) d[at + 1] << 16)
                                   | ((std::uint32_t) d[at + 2] << 8) | (std::uint32_t) d[at + 3]
                             : 0;
}

// The length of a WAV / AIFF file, in seconds (0 when the header is not one this reads). Only the headers are parsed:
// the samples are never touched, so a 70 MB take costs a 64 KB read.
double audioSeconds (const std::string& path)
{
    std::ifstream in (path, std::ios::binary);
    if (! in)
        return 0.0;
    std::vector<std::uint8_t> head (65536);
    in.read (reinterpret_cast<char*> (head.data()), (std::streamsize) head.size());
    head.resize ((std::size_t) in.gcount());
    if (head.size() < 12)
        return 0.0;

    if (std::memcmp (head.data(), "RIFF", 4) == 0 && std::memcmp (head.data() + 8, "WAVE", 4) == 0)
    {
        std::uint32_t rate = 0, byteRate = 0;
        for (std::size_t at = 12; at + 8 <= head.size();)
        {
            const auto id = std::string (reinterpret_cast<const char*> (head.data() + at), 4);
            const auto length = (std::size_t) le32 (head.data() + at + 4);
            const auto body = at + 8;
            if (id == "fmt " && length >= 16 && body + 16 <= head.size())
            {
                rate = le32 (head.data() + body + 4);
                byteRate = le32 (head.data() + body + 8);
            }
            else if (id == "data")
            {
                if (rate > 0 && byteRate > 0)
                    return (double) length / (double) byteRate;
                return 0.0;
            }
            if (length == 0 || length > head.size())
                break; // truncated header (a chunk larger than what was read): nothing more to learn here
            at = body + length + (length & 1);
        }
        return 0.0;
    }

    // AIFF / AIFC: COMM carries the frame count and the (80-bit) sample rate.
    if (std::memcmp (head.data(), "FORM", 4) == 0
        && (std::memcmp (head.data() + 8, "AIFF", 4) == 0 || std::memcmp (head.data() + 8, "AIFC", 4) == 0))
    {
        for (std::size_t at = 12; at + 8 <= head.size();)
        {
            const auto id = std::string (reinterpret_cast<const char*> (head.data() + at), 4);
            const auto length = (std::size_t) be32At (head, at + 4);
            const auto body = at + 8;
            if (id == "COMM" && length >= 18 && body + 18 <= head.size())
            {
                const auto frames = (std::uint32_t) be32At (head, body + 2);
                const auto exp = (int) (((std::uint16_t) head[body + 8] << 8) | head[body + 9]) - 16383;
                const auto mantissa = (std::uint64_t) be32At (head, body + 10) | ((std::uint64_t) head[body + 14] << 32);
                if (mantissa == 0)
                    return 0.0;
                const auto rate = std::ldexp ((double) mantissa, exp - 63);
                return rate > 0.0 ? (double) frames / rate : 0.0;
            }
            if (length == 0 || length > head.size())
                break;
            at = body + length + (length & 1);
        }
    }
    return 0.0;
}

// A variable-length quantity (MIDI delta times and meta lengths).
bool readVlq (const std::uint8_t* d, std::size_t end, std::size_t& i, std::uint32_t& value) noexcept
{
    value = 0;
    for (int k = 0; k < 4; ++k)
    {
        if (i >= end)
            return false;
        const auto b = d[i++];
        value = (value << 7) | (std::uint32_t) (b & 0x7f);
        if ((b & 0x80) == 0)
            return true;
    }
    return false;
}

void mergeInto (midiimport::ParsedMidi& into, const midiimport::ParsedMidi& from, int trackOffset, bool takeTiming)
{
    for (auto e : from.events)
    {
        e.track += trackOffset;
        into.events.push_back (e);
    }
    for (const auto& name : from.trackNames)
        into.trackNames.push_back (name);
    if (takeTiming)
    {
        into.bpm = from.bpm;
        into.numerator = from.numerator;
        into.denominator = from.denominator;
    }
}

// Every standard MIDI file embedded in the container: the one thing a proprietary project file always stores in a
// form we can read exactly. Returns how many were recovered.
int recoverEmbeddedMidi (const std::uint8_t* d, std::size_t n, midiimport::ParsedMidi& out)
{
    int blobs = 0;
    for (std::size_t at = 0; at + 14 <= n; ++at)
    {
        if (d[at] != 'M' || std::memcmp (d + at, "MThd", 4) != 0)
            continue;
        midiimport::ParsedMidi parsed;
        if (! parseStandardMidi (d + at, n - at, parsed, nullptr))
            continue;
        mergeInto (out, parsed, (int) out.trackNames.size(), blobs == 0);
        ++blobs;
        at += 3;
    }
    return blobs;
}

void fillTracks (Report& out)
{
    out.tracks.clear();
    for (std::size_t t = 0; t < out.midi.trackNames.size(); ++t)
    {
        Track track;
        track.name = out.midi.trackNames[t];
        bool channelSeen = false;
        for (const auto& e : out.midi.events)
        {
            if (e.track != (int) t)
                continue;
            if (! channelSeen && (e.status & 0xf0) != 0xf0)
            {
                track.midiChannel = e.status & 0x0f;
                channelSeen = true;
            }
            if ((e.status & 0xf0) == 0x90 && e.data2 > 0)
                ++track.notes;
        }
        if (track.name.empty())
            track.name = "Track " + std::to_string (t + 1);
        out.tracks.push_back (std::move (track));
    }
}

// --- Cubase: the eight-character chunk tree ------------------------------------
struct CubaseChunk
{
    std::string id;
    std::size_t payload = 0;
    std::size_t length = 0;
};

bool printableId (const std::uint8_t* p, std::size_t n) noexcept
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (p[i] == 0)
            return count >= 4;
        if (! printable (p[i]))
            return false;
        ++count;
    }
    return count >= 4;
}

// Walks [begin, end) as consecutive [8-byte id][length]["payload"] chunks with `width`-byte little-endian lengths.
// False when the walk does not cover the region, which is what tells a 64-bit length field from a 32-bit one.
bool walkCubase (const std::uint8_t* d, std::size_t begin, std::size_t end, int width, std::vector<CubaseChunk>& out)
{
    const auto header = (std::size_t) width + 8;
    std::size_t i = begin;
    while (i + header <= end)
    {
        if (! printableId (d + i, 8))
            return false;
        const auto length = width == 8 ? le64 (d + i + 8) : (std::uint64_t) le32 (d + i + 8);
        if (length > (std::uint64_t) (end - i - header))
            return false;
        CubaseChunk chunk;
        for (std::size_t k = 0; k < 8 && d[i + k] != 0; ++k)
            chunk.id += (char) d[i + k];
        while (! chunk.id.empty() && chunk.id.back() == ' ')
            chunk.id.pop_back();
        chunk.payload = i + header;
        chunk.length = (std::size_t) length;
        out.push_back (std::move (chunk));
        i += header + (std::size_t) length;
    }
    return i + 3 >= end; // a few bytes of padding are fine; anything more means this width is wrong
}

double ticksPerQuarterOf (std::uint16_t division) noexcept
{
    if ((division & 0x8000) == 0)
        return (double) division;
    // SMPTE: frames per second in the high byte (as a negative number) and ticks per frame in the low byte
    const auto fps = 256 - ((division >> 8) & 0xff);
    const auto ticksPerFrame = division & 0xff;
    if (fps <= 0 || ticksPerFrame == 0 || division == 0)
        return 0.0;
    return (double) fps * (double) ticksPerFrame * 0.5; // at the 120 bpm reference
}
} // namespace

const char* formatName (Format format) noexcept
{
    switch (format)
    {
        case Format::cubase: return "Cubase";
        case Format::cakewalk: return "Cakewalk";
        case Format::cakewalkWrk: return "Cakewalk (.wrk)";
        case Format::dawproject: return "DAWproject";
        case Format::midi: return "MIDI";
        case Format::unknown: break;
    }
    return "Unknown";
}

const char* openFilter() noexcept
{
    return "cpr;npr;cwp;cwb;cwz;wrk;dawproject;mid;midi";
}

bool isSupportedExtension (const std::string& extension) noexcept
{
    std::string ext;
    for (auto c : extension)
        if (c != '.' && c != '*')
            ext += (char) std::tolower ((unsigned char) c);
    for (const auto* candidate : { "cpr", "npr", "cwp", "cwb", "cwz", "wrk", "dawproject", "mid", "midi" })
        if (ext == candidate)
            return true;
    return false;
}

Format detect (const std::uint8_t* data, std::size_t size, const std::string& extension) noexcept
{
    std::string ext;
    for (auto c : extension)
        if (c != '.' && c != '*')
            ext += (char) std::tolower ((unsigned char) c);

    if (size >= 4 && std::memcmp (data, "PK\x03\x04", 4) == 0)
    {
        // A DAWproject is a ZIP; the extension is the reliable discriminator, with project.xml as the fallback for a
        // file saved without one.
        if (ext == "dawproject" || findBytes (data, std::min<std::size_t> (size, 4096), "project.xml", 11) != npos)
            return Format::dawproject;
    }
    if (size >= 14 && std::memcmp (data, "MThd", 4) == 0)
        return Format::midi;
    constexpr std::uint8_t ole2[8] { 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
    if (size >= 8 && std::memcmp (data, ole2, 8) == 0)
        return Format::cakewalk;
    if (size >= 4 && (std::memcmp (data, "RIFF", 4) == 0 || std::memcmp (data, "RIF2", 4) == 0))
    {
        if (findBytes (data, std::min<std::size_t> (size, 512), "NUNDROOT", 8) != npos || ext == "cpr" || ext == "npr")
            return Format::cubase;
    }
    if (ext == "wrk")
        return Format::cakewalkWrk;
    if (ext == "cpr" || ext == "npr")
        return Format::cubase;
    if (ext == "cwp" || ext == "cwb" || ext == "cwz")
        return Format::cakewalk;
    if (ext == "dawproject")
        return Format::dawproject;
    if (ext == "mid" || ext == "midi")
        return Format::midi;
    return Format::unknown;
}

int Report::noteCount() const noexcept
{
    if (format == Format::dawproject)
        return dawproject.noteCount;
    int count = 0;
    for (const auto& e : midi.events)
        if ((e.status & 0xf0) == 0x90 && e.data2 > 0)
            ++count;
    return count;
}

std::string Report::summary() const
{
    std::ostringstream out;
    out << formatName (format);
    if (! version.empty())
        out << " " << version;
    out << ": " << tracks.size() << (tracks.size() == 1 ? " track" : " tracks") << ", " << noteCount()
        << (noteCount() == 1 ? " note" : " notes");
    if (! media.empty())
        out << ", " << media.size() << (media.size() == 1 ? " audio file" : " audio files") << " beside the project";
    if (format == Format::dawproject && dawproject.clipCount > 0)
        out << ", " << dawproject.clipCount << (dawproject.clipCount == 1 ? " clip" : " clips") << " ("
            << dawproject.audioCount << " audio)";
    for (const auto& f : findings)
        out << "\n- " << f;
    return out.str();
}

bool parseStandardMidi (const std::uint8_t* data, std::size_t size, midiimport::ParsedMidi& out, std::string* error)
{
    auto fail = [error] (const std::string& text) {
        if (error != nullptr)
            *error = text;
        return false;
    };

    out = {};
    if (size < 14 || data == nullptr || std::memcmp (data, "MThd", 4) != 0)
        return fail ("no MThd header");

    const auto headerLength = be32 (data + 4);
    if (headerLength < 6 || (std::size_t) 8 + headerLength > size)
        return fail ("the header is truncated");

    const auto format = be16 (data + 8);
    const auto trackCount = (int) be16 (data + 10);
    const auto division = be16 (data + 12);
    if (format > 2)
        return fail ("unsupported MIDI format");
    const auto ticksPerQuarter = ticksPerQuarterOf (division);
    if (ticksPerQuarter <= 0.0)
        return fail ("the time division is not usable");

    out.trackNames.assign ((std::size_t) std::max (0, trackCount), {});
    bool tempoSeen = false;
    std::size_t i = 8 + (std::size_t) headerLength;

    for (int t = 0; t < trackCount; ++t)
    {
        while (i + 8 <= size && std::memcmp (data + i, "MTrk", 4) != 0) // skip chunks this reader does not know
            i += 8 + (std::size_t) be32 (data + i + 4);
        if (i + 8 > size)
            return fail ("an MTrk chunk is missing");
        const auto length = (std::size_t) be32 (data + i + 4);
        const auto end = i + 8 + length;
        if (end > size)
            return fail ("an MTrk chunk is truncated");

        std::size_t j = i + 8;
        std::int64_t tick = 0;
        std::uint8_t running = 0;
        while (j < end)
        {
            std::uint32_t delta = 0;
            if (! readVlq (data, end, j, delta))
                return fail ("a delta time is malformed");
            tick += (std::int64_t) delta;
            if (j >= end)
                break;

            std::uint8_t status = data[j];
            if (status >= 0x80)
            {
                ++j;
                if (status < 0xf0)
                    running = status;
            }
            else if (running != 0)
                status = running;
            else
                return fail ("an event has no status byte");

            if (status == 0xff) // meta
            {
                if (j >= end)
                    return fail ("a meta event is truncated");
                const auto type = data[j++];
                std::uint32_t metaLength = 0;
                if (! readVlq (data, end, j, metaLength) || j + metaLength > end)
                    return fail ("a meta event is truncated");
                if (type == 0x51 && metaLength >= 3 && ! tempoSeen)
                {
                    const auto micro = ((std::uint32_t) data[j] << 16) | ((std::uint32_t) data[j + 1] << 8) | (std::uint32_t) data[j + 2];
                    if (micro > 0)
                    {
                        out.bpm = 60000000.0 / (double) micro;
                        tempoSeen = true;
                    }
                }
                else if (type == 0x58 && metaLength >= 2)
                {
                    if (out.numerator == 0 && data[j] > 0)
                    {
                        out.numerator = (int) data[j];
                        out.denominator = 1 << std::min<int> (7, (int) data[j + 1]);
                    }
                }
                else if (type == 0x03 && t < (int) out.trackNames.size() && out.trackNames[(std::size_t) t].empty())
                {
                    std::string name;
                    for (std::uint32_t k = 0; k < metaLength; ++k)
                        if (printable (data[j + k]))
                            name += (char) data[j + k];
                    out.trackNames[(std::size_t) t] = name;
                }
                j += metaLength;
                continue;
            }
            if (status == 0xf0 || status == 0xf7) // sysex: skipped, it carries no notes
            {
                std::uint32_t sysexLength = 0;
                if (! readVlq (data, end, j, sysexLength) || j + sysexLength > end)
                    return fail ("a sysex event is truncated");
                j += sysexLength;
                continue;
            }

            const auto type = status & 0xf0;
            if (type < 0x80)
                return fail ("an event has an unusable status");
            const auto dataBytes = (type == 0xc0 || type == 0xd0) ? 1 : 2;
            if (j + (std::size_t) dataBytes > end)
                return fail ("an event is truncated");

            midiimport::RawEvent e;
            e.track = t;
            e.tick = (std::int64_t) std::llround ((double) tick * (double) ppq / ticksPerQuarter);
            e.status = status;
            e.data1 = data[j];
            e.data2 = dataBytes == 2 ? data[j + 1] : 0;
            j += (std::size_t) dataBytes;
            out.events.push_back (e);
        }
        i = end;
    }

    if (out.events.empty())
        return fail ("the MIDI file has no events");
    return true;
}

bool readCubase (const std::uint8_t* data, std::size_t size, Report& out, std::string& error)
{
    out = {};
    out.format = Format::cubase;
    out.application = "Cubase";

    // Cubase 14 writes its 64-bit RIFF variant, "RIF2"; older versions wrote "RIFF". Both carry NUNDROOT.
    const bool riff2 = std::memcmp (data, "RIF2", 4) == 0;
    if (size < 16 || (std::memcmp (data, "RIFF", 4) != 0 && ! riff2))
    {
        error = "not a Cubase project: the file does not start with RIFF";
        return false;
    }

    const auto root = findBytes (data, std::min<std::size_t> (size, 512), "NUNDROOT", 8);
    if (root == npos)
    {
        error = "not a Cubase project: no NUNDROOT chunk";
        return false;
    }

    // The version token is the one part of the tree whose meaning is public ("PAppVersion" then the text). Only a run
    // that starts with a digit is a version: the next chunk name follows it, which used to be reported as "ARCH".
    const auto versionAt = findBytes (data, size, "PAppVersion", 11);
    if (versionAt != npos)
    {
        auto text = printableRun (data, size, versionAt + 11, 32);
        const auto firstDigit = text.find_first_of ("0123456789");
        text = firstDigit == npos ? std::string() : text.substr (firstDigit);
        if (! text.empty() && text.find ('.') != npos)
            out.version = text;
    }

    for (int width : { 8, 4 })
    {
        std::vector<CubaseChunk> chunks;
        if (walkCubase (data, root + 8, size, width, chunks) && ! chunks.empty())
        {
            out.findings.push_back ("chunk tree: " + std::to_string (chunks.size()) + " chunks, " + std::to_string (width * 8) + "-bit lengths");
            break;
        }
    }

    // Plugin tokens: reported, never converted (there is nothing to load them with).
    for (std::size_t at = findBytes (data, size, "Plugin UID", 10); at != npos; at = findBytes (data, size, "Plugin UID", 10, at + 10))
        addUnique (out.plugins, printableRun (data, size, at + 10, 48));
    if (! out.plugins.empty())
        out.findings.push_back ("plugins named in the project: " + std::to_string (out.plugins.size()) + " (not converted)");

    const auto blobs = recoverEmbeddedMidi (data, size, out.midi);
    if (blobs > 0)
    {
        fillTracks (out);
        out.findings.push_back ("MIDI: " + std::to_string (blobs) + " embedded MIDI file(s) recovered exactly");
        out.findings.push_back ("audio clips are not imported: Cubase keeps them in a folder beside the project");
    }
    else
    {
        out.findings.push_back ("this project stores its events in Cubase's own chunk tree, where this reader found no MIDI");
        out.findings.push_back ("the audio, if the project has any, is in its Audio folder: those files are recovered below");
    }
    return true;
}

int findAudioBeside (const std::string& projectPath, Report& out)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path project (projectPath);
    const auto folder = project.parent_path();
    if (folder.empty() || ! fs::is_directory (folder, ec))
        return 0;

    // Cubase records into <project>/Audio; when there is no such folder the media sometimes sits beside the .cpr.
    // The first folder that holds audio wins, so a project's takes are never listed twice.
    const fs::path candidates[] { folder / "Audio", folder / "audio", folder };
    for (const auto& candidate : candidates)
    {
        std::vector<fs::path> found;
        for (fs::directory_iterator it (candidate, fs::directory_options::skip_permission_denied, ec), end;
             ! ec && it != end; it.increment (ec))
        {
            std::error_code fileError;
            const auto& path = it->path();
            if (it->is_regular_file (fileError) && isAudioFile (path.string()))
                found.push_back (path);
        }
        if (found.empty())
            continue;

        std::sort (found.begin(), found.end(), [] (const fs::path& a, const fs::path& b) {
            return lowerAscii (a.filename().string()) < lowerAscii (b.filename().string());
        });
        for (const auto& path : found)
        {
            MediaFile media;
            media.path = path.string();
            media.name = path.filename().string();
            media.seconds = audioSeconds (media.path);
            out.media.push_back (std::move (media));
        }
        return (int) found.size();
    }
    return 0;
}

bool readCakewalkWrk (const std::uint8_t* data, std::size_t size, Report& out, std::string& error)
{
    out = {};
    out.format = Format::cakewalkWrk;
    out.application = "Cakewalk";
    error.clear();

    // Drumstick's WrkChunkType: the record types a .wrk may hold (old and new revisions of the same records).
    static constexpr int knownTags[] { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 18, 19, 20, 21, 22, 23, 24, 26, 27, 30, 36, 44, 45, 49, 74, 255 };
    auto known = [] (int tag) {
        for (auto t : knownTags)
            if (t == tag)
                return true;
        return false;
    };

    struct Record
    {
        int tag;
        std::size_t payload;
        std::size_t length;
    };
    std::vector<Record> records;
    std::size_t i = 0;
    bool valid = size >= 6;
    while (valid && i + 5 <= size)
    {
        const auto tag = (int) data[i];
        const auto length = (std::size_t) le32 (data + i + 1);
        if (! known (tag) || length > size - i - 5)
        {
            valid = false;
            break;
        }
        records.push_back ({ tag, i + 5, length });
        i += 5 + length;
        if (tag == 255)
            break;
    }
    valid = valid && ! records.empty() && i + 3 >= size;

    if (! valid)
    {
        out.findings.push_back ("this .wrk does not follow the chunk layout the reader validates, so nothing was guessed");
        out.findings.push_back ("in Cakewalk / BandLab choose File > Export > MIDI, then File > Import > MIDI file here");
        return true;
    }

    out.findings.push_back ("chunk stream: " + std::to_string (records.size()) + " records validated");
    for (const auto& record : records)
    {
        const auto end = record.payload + record.length;
        if (record.tag == 24) // TRKNAME_CHUNK
        {
            Track track;
            track.name = printableRun (data, end, record.payload, 64);
            if (! track.name.empty())
                out.tracks.push_back (std::move (track));
        }
        else if (record.tag == 74) // SOFTVER_CHUNK
            out.version = printableRun (data, end, record.payload, 40);
    }

    const auto blobs = recoverEmbeddedMidi (data, size, out.midi);
    if (blobs > 0)
    {
        fillTracks (out);
        out.findings.push_back ("MIDI: " + std::to_string (blobs) + " embedded MIDI file(s) recovered exactly");
    }
    else
    {
        out.findings.push_back ("the note events of a .wrk live in its stream records (tags 2 / 45), which this reader does not decode yet");
        out.findings.push_back ("in Cakewalk / BandLab choose File > Export > MIDI, then File > Import > MIDI file here");
    }
    return true;
}

bool readCakewalk (const std::uint8_t* data, std::size_t size, Report& out, std::string& error)
{
    out = {};
    out.format = Format::cakewalk;
    out.application = "Cakewalk";
    error.clear();

    if (size >= 4 && data[0] == 'P' && data[1] == 'K' && data[2] == 3 && data[3] == 4)
    {
        out.findings.push_back ("this is a .cwz package (a zip): unpack it and open the .cwp inside");
        return true;
    }

    constexpr std::uint8_t ole2[8] { 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
    if (size >= 8 && std::memcmp (data, ole2, 8) == 0)
        out.findings.push_back ("OLE2 compound file: only the MIDI it embeds is recovered, its own stream is not decoded");

    const auto blobs = recoverEmbeddedMidi (data, size, out.midi);
    if (blobs > 0)
    {
        fillTracks (out);
        out.findings.push_back ("MIDI: " + std::to_string (blobs) + " embedded MIDI file(s) recovered exactly");
        out.findings.push_back ("audio clips are not imported: Cakewalk keeps them as files beside the project");
    }
    else
    {
        out.findings.push_back ("no embedded MIDI was found; Cakewalk / BandLab keeps its events in a proprietary stream");
        out.findings.push_back ("in Cakewalk / BandLab choose File > Export > MIDI, then File > Import > MIDI file here");
    }
    return true;
}

bool read (const std::uint8_t* data, std::size_t size, const std::string& extension, Report& out, std::string& error)
{
    error.clear();
    if (data == nullptr || size == 0)
    {
        error = "the file is empty";
        return false;
    }

    switch (detect (data, size, extension))
    {
        case Format::cubase: return readCubase (data, size, out, error);
        case Format::cakewalk: return readCakewalk (data, size, out, error);
        case Format::cakewalkWrk: return readCakewalkWrk (data, size, out, error);
        case Format::dawproject:
        {
            out = {};
            out.format = Format::dawproject;
            std::string projectError;
            if (! dawproject::read (data, size, out.dawproject, projectError))
            {
                error = "DAWproject: " + projectError;
                return false;
            }
            out.application = out.dawproject.application;
            out.version = out.dawproject.version;
            out.findings = out.dawproject.findings;
            for (std::size_t i = 0; i < out.dawproject.tracks.size(); ++i)
            {
                const auto& source = out.dawproject.tracks[i];
                Track track;
                track.name = source.name.empty() ? "Track " + std::to_string (i + 1) : source.name;
                for (const auto& clip : source.clips)
                    if (clip.kind == dawproject::ClipKind::notes)
                        track.notes += (int) clip.notes.size();
                out.tracks.push_back (std::move (track));
            }
            return true;
        }
        case Format::midi:
        {
            out = {};
            out.format = Format::midi;
            out.application = "MIDI";
            std::string midiError;
            if (! parseStandardMidi (data, size, out.midi, &midiError))
            {
                error = "MIDI: " + midiError;
                return false;
            }
            fillTracks (out);
            return true;
        }
        case Format::unknown: break;
    }

    error = "not a project this reader knows (Cubase .cpr/.npr, Cakewalk .cwp/.cwb/.wrk, DAWproject, MIDI)";
    return false;
}

bool readFile (const std::string& path, Report& out, std::string& error)
{
    std::ifstream in (path, std::ios::binary);
    if (! in)
    {
        error = "could not read " + path;
        return false;
    }
    const std::vector<std::uint8_t> bytes { std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>() };
    if (bytes.empty())
    {
        error = "the file is empty";
        return false;
    }
    std::string extension;
    if (const auto dot = path.find_last_of ('.'); dot != std::string::npos)
        extension = path.substr (dot + 1);
    if (! read (bytes.data(), bytes.size(), extension, out, error))
        return false;
    out.sourcePath = path;
    if (out.format == Format::dawproject)
        out.dawproject.sourcePath = path;

    // Cubase, Nuendo and SONAR keep their audio beside the project, never inside it. When the container gave no notes,
    // the recordings are what is left to recover: the files themselves, on their own audio tracks (their positions live
    // in the same unreadable tree, so the arrangement of the clips is reported rather than invented).
    const bool besideTheProject = out.format == Format::cubase || out.format == Format::cakewalk || out.format == Format::cakewalkWrk;
    if (besideTheProject && ! out.hasNotes())
    {
        const auto files = findAudioBeside (path, out);
        if (files > 0)
        {
            std::size_t measured = 0;
            for (const auto& media : out.media)
                measured += media.seconds > 0.0 ? 1 : 0;
            out.findings.push_back ("audio: " + std::to_string (files) + (files == 1 ? " file" : " files")
                                    + " from the project's audio folder (every file in it, old takes included),"
                                    " each on its own audio track from bar 1");
            if (measured < out.media.size())
                out.findings.push_back (std::to_string (out.media.size() - measured)
                                        + " of them have no readable length (only WAV and AIFF headers are read), so their clips are estimated");
            out.findings.push_back ("Cubase does not store where the clips sit in a form this reader can read: the audio arrives whole, in file-name order");
        }
        else
            out.findings.push_back ("no audio files were found beside the project (Cubase records into its Audio folder)");
    }
    return true;
}

// A project whose only recoverable content is its audio: one audio channel and one Playlist track per file, each clip
// from bar 1. The channel holds the file (instrument = audioClip, samplePath), exactly as a DAWproject's audio clip
// does, so the takes play and can be dragged where they belong.
bool importMediaAsAudio (Song& song, Report& report, std::string& error)
{
    if (report.media.empty())
    {
        error = "no audio to import";
        return false;
    }

    // A song always has a pattern to show (the Channel Rack and the Piano Roll both land on one); an audio-only
    // project gets an empty one to hold that place rather than a song with none.
    if (song.patterns.empty())
    {
        Pattern pattern;
        pattern.name = "Imported";
        song.patterns.push_back (std::move (pattern));
    }

    const auto bpm = song.bpm > 0.0 ? song.bpm : 130.0;
    const auto ticksOfSeconds = [bpm] (double seconds)
    {
        return std::max<std::int64_t> (1, (std::int64_t) std::llround (seconds * bpm / 60.0 * (double) ppq));
    };
    auto uniqueName = [&song] (const std::string& wanted)
    {
        auto name = wanted;
        for (int n = 2;; ++n)
        {
            bool used = false;
            for (const auto& c : song.channels)
                used = used || c.name == name;
            if (! used)
                return name;
            name = wanted + " (" + std::to_string (n) + ")";
        }
    };

    for (const auto& media : report.media)
    {
        ChannelDef channel;
        channel.name = uniqueName (stemOf (media.name));
        channel.instrument = InstrumentKind::audioClip;
        channel.samplePath = media.path;
        channel.volume = 1.0f;
        song.channels.push_back (std::move (channel));
        const auto channelIndex = (int) song.channels.size() - 1;

        PlaylistTrack track;
        track.name = song.channels[(std::size_t) channelIndex].name;
        track.mode = PlaylistTrackMode::audio;
        song.playlistTracks.push_back (std::move (track));
        const auto trackIndex = (int) song.playlistTracks.size() - 1;

        Clip placed;
        placed.kind = ClipKind::audio;
        placed.channel = channelIndex;
        placed.track = trackIndex;
        placed.start = 0;
        placed.offset = 0;
        // A length this reader cannot measure is estimated from the file size (16-bit 44.1 kHz stereo is the widest
        // common case, so the guess errs long: a tail of silence, never a cut take).
        std::error_code sizeError;
        const auto bytes = std::filesystem::file_size (media.path, sizeError);
        if (media.seconds > 0.0)
            placed.length = ticksOfSeconds (media.seconds);
        else if (! sizeError && bytes > 0)
            placed.length = ticksOfSeconds ((double) bytes / 176400.0);
        else
            placed.length = ticksOfSeconds (8.0);
        song.playlist.push_back (placed);
    }

    report.findings.push_back ("imported " + std::to_string (report.media.size()) + " audio file(s) on their own tracks, all starting at bar 1");
    return true;
}

bool importInto (Song& song, Report& report, const midiimport::Options& options, const std::string& patternName, std::string& error)
{
    return importInto (song, report, options, patternName, std::string(), error);
}

bool importInto (Song& song, Report& report, const midiimport::Options& options, const std::string& patternName,
                 const std::string& mediaDirectory, std::string& error)
{
    if (report.format == Format::dawproject)
    {
        const auto imported = dawproject::importInto (song, report.dawproject, patternName, mediaDirectory, error);
        report.findings = report.dawproject.findings; // extraction and mapping findings reach the report
        return imported;
    }
    if (! report.hasNotes())
    {
        if (report.media.empty())
        {
            error = "no notes could be recovered from this project:\n" + report.summary();
            return false;
        }
        return importMediaAsAudio (song, report, error);
    }
    return midiimport::importInto (song, report.midi, options, patternName, error);
}
} // namespace lynx::foreign
