#include "core/DawProject.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>

#include "core/Inflate.h"

namespace lynx::dawproject
{
namespace
{
constexpr std::size_t npos = std::string::npos;
namespace fs = std::filesystem;

std::string trim (const std::string& text)
{
    std::size_t begin = 0, end = text.size();
    while (begin < end && std::isspace ((unsigned char) text[begin]) != 0)
        ++begin;
    while (end > begin && std::isspace ((unsigned char) text[end - 1]) != 0)
        --end;
    return text.substr (begin, end - begin);
}

std::string lower (std::string text)
{
    for (auto& c : text)
        c = (char) std::tolower ((unsigned char) c);
    return text;
}

bool endsWith (const std::string& text, const std::string& suffix)
{
    return text.size() >= suffix.size() && text.compare (text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// --- XML ---------------------------------------------------------------------------------------------------------
// A small pull parser for the subset project.xml uses: elements, attributes, self-closing tags, comments, CDATA,
// processing instructions and the five predefined entities. It deliberately has no DTD or namespace handling, because
// DAWproject does not use them.
struct XmlNode
{
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<XmlNode> children;

    const std::string* attribute (const char* key) const
    {
        for (const auto& [name, value] : attributes)
            if (name == key)
                return &value;
        return nullptr;
    }

    std::string attributeOr (const char* key, const std::string& fallback = {}) const
    {
        if (const auto* value = attribute (key))
            return *value;
        return fallback;
    }

    const XmlNode* child (const char* name) const
    {
        for (const auto& c : children)
            if (c.name == name)
                return &c;
        return nullptr;
    }
};

void appendUtf8 (std::string& out, std::uint32_t code)
{
    if (code <= 0x7f)
        out += (char) code;
    else if (code <= 0x7ff)
    {
        out += (char) (0xc0 | (code >> 6));
        out += (char) (0x80 | (code & 0x3f));
    }
    else if (code <= 0xffff)
    {
        out += (char) (0xe0 | (code >> 12));
        out += (char) (0x80 | ((code >> 6) & 0x3f));
        out += (char) (0x80 | (code & 0x3f));
    }
    else
    {
        out += (char) (0xf0 | (code >> 18));
        out += (char) (0x80 | ((code >> 12) & 0x3f));
        out += (char) (0x80 | ((code >> 6) & 0x3f));
        out += (char) (0x80 | (code & 0x3f));
    }
}

std::string decodeEntities (const std::string& text)
{
    std::string out;
    out.reserve (text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != '&')
        {
            out += text[i];
            continue;
        }
        const auto semicolon = text.find (';', i + 1);
        if (semicolon == npos || semicolon - i > 10)
        {
            out += text[i];
            continue;
        }
        const auto entity = text.substr (i + 1, semicolon - i - 1);
        if (entity == "amp")
            out += '&';
        else if (entity == "lt")
            out += '<';
        else if (entity == "gt")
            out += '>';
        else if (entity == "quot")
            out += '"';
        else if (entity == "apos")
            out += '\'';
        else if (! entity.empty() && entity[0] == '#')
        {
            const bool hex = entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X');
            const auto digits = entity.substr (hex ? 2 : 1);
            char* end = nullptr;
            const auto code = std::strtoul (digits.c_str(), &end, hex ? 16 : 10);
            if (end != nullptr && *end == '\0')
                appendUtf8 (out, (std::uint32_t) code);
            else
                out += text[i];
        }
        else
        {
            out += text[i];
            continue;
        }
        i = semicolon;
    }
    return out;
}

class XmlParser
{
public:
    XmlParser (const std::vector<std::uint8_t>& bytes) : data (bytes) {}

    bool parse (XmlNode& root, std::string& error)
    {
        if (data.size() >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf)
            at = 3;

        while (at < data.size())
        {
            skipSpace();
            if (at >= data.size())
                break;
            if (! startsWith ("<"))
            {
                error = "text outside the root element";
                return false;
            }
            if (startsWith ("<?"))
            {
                if (! skipUntil ("?>"))
                    return fail (error, "an XML declaration is truncated");
                continue;
            }
            if (startsWith ("<!--"))
            {
                if (! skipUntil ("-->"))
                    return fail (error, "a comment is truncated");
                continue;
            }
            if (startsWith ("<!"))
            {
                if (! skipUntil (">"))
                    return fail (error, "a declaration is truncated");
                continue;
            }
            return parseElement (root, error);
        }
        error = "the XML is empty";
        return false;
    }

private:
    const std::vector<std::uint8_t>& data;
    std::size_t at = 0;

    static bool fail (std::string& error, const char* text)
    {
        error = text;
        return false;
    }

    bool startsWith (const char* text) const
    {
        const auto length = std::strlen (text);
        return at + length <= data.size() && std::memcmp (data.data() + at, text, length) == 0;
    }

    void skipSpace()
    {
        while (at < data.size() && std::isspace ((unsigned char) data[at]) != 0)
            ++at;
    }

    bool skipUntil (const char* terminator)
    {
        const auto length = std::strlen (terminator);
        while (at + length <= data.size())
        {
            if (std::memcmp (data.data() + at, terminator, length) == 0)
            {
                at += length;
                return true;
            }
            ++at;
        }
        return false;
    }

    bool readName (std::string& name)
    {
        const auto begin = at;
        while (at < data.size())
        {
            const auto c = (char) data[at];
            if (std::isspace ((unsigned char) c) != 0 || c == '>' || c == '/' || c == '=')
                break;
            ++at;
        }
        name.assign (reinterpret_cast<const char*> (data.data() + begin), at - begin);
        return ! name.empty();
    }

    bool parseElement (XmlNode& node, std::string& error)
    {
        if (! startsWith ("<") || startsWith ("</"))
            return fail (error, "an element does not start");
        ++at; // '<'
        if (! readName (node.name))
            return fail (error, "an element has no name");

        bool selfClosing = false;
        while (at < data.size())
        {
            skipSpace();
            if (startsWith ("/>"))
            {
                at += 2;
                selfClosing = true;
                break;
            }
            if (startsWith (">"))
            {
                ++at;
                break;
            }
            std::string key;
            if (! readName (key))
                return fail (error, "an attribute has no name");
            skipSpace();
            if (! startsWith ("="))
                return fail (error, "an attribute has no value");
            ++at;
            skipSpace();
            if (at >= data.size() || (data[at] != '"' && data[at] != '\''))
                return fail (error, "an attribute value is not quoted");
            const auto quote = (char) data[at++];
            const auto begin = at;
            while (at < data.size() && (char) data[at] != quote)
                ++at;
            if (at >= data.size())
                return fail (error, "an attribute value is truncated");
            node.attributes.emplace_back (key, decodeEntities (std::string (reinterpret_cast<const char*> (data.data() + begin), at - begin)));
            ++at; // quote
        }

        if (selfClosing)
            return true;

        while (at < data.size())
        {
            skipSpace();
            if (at >= data.size())
                return fail (error, "the XML ends inside an element");
            if (startsWith ("</"))
            {
                at += 2;
                std::string closing;
                if (! readName (closing))
                    return fail (error, "a closing tag has no name");
                skipSpace();
                if (! startsWith (">"))
                    return fail (error, "a closing tag is malformed");
                ++at;
                if (closing != node.name)
                    return fail (error, "a closing tag does not match its element");
                return true;
            }
            if (startsWith ("<!--"))
            {
                if (! skipUntil ("-->"))
                    return fail (error, "a comment is truncated");
                continue;
            }
            if (startsWith ("<?"))
            {
                if (! skipUntil ("?>"))
                    return fail (error, "a processing instruction is truncated");
                continue;
            }
            if (startsWith ("<![CDATA["))
            {
                if (! skipUntil ("]]>"))
                    return fail (error, "a CDATA section is truncated");
                continue;
            }
            if (startsWith ("<!"))
            {
                if (! skipUntil (">"))
                    return fail (error, "a declaration is truncated");
                continue;
            }
            if (! startsWith ("<"))
            {
                // Character data: DAWproject keeps every value in attributes, but skip it without failing.
                while (at < data.size() && data[at] != '<')
                    ++at;
                continue;
            }
            XmlNode child;
            if (! parseElement (child, error))
                return false;
            node.children.push_back (std::move (child));
        }
        return fail (error, "the XML ends inside an element");
    }
};

double toDouble (const std::string& text, double fallback = 0.0)
{
    const auto value = trim (text);
    if (value.empty())
        return fallback;
    char* end = nullptr;
    const auto number = std::strtod (value.c_str(), &end);
    if (end == nullptr || *end != '\0' || ! std::isfinite (number))
        return fallback;
    return number;
}

int toInt (const std::string& text, int fallback = 0)
{
    const auto value = trim (text);
    if (value.empty())
        return fallback;
    char* end = nullptr;
    const auto number = std::strtol (value.c_str(), &end, 10);
    if (end == nullptr || *end != '\0')
        return fallback;
    return (int) number;
}

bool toBool (const std::string& text, bool fallback)
{
    const auto value = lower (trim (text));
    if (value == "true" || value == "1")
        return true;
    if (value == "false" || value == "0")
        return false;
    return fallback;
}

TimeUnit timeUnitOf (const XmlNode& node, const char* key, TimeUnit fallback)
{
    const auto* value = node.attribute (key);
    if (value == nullptr)
        return fallback;
    const auto unit = lower (trim (*value));
    if (unit == "seconds")
        return TimeUnit::seconds;
    if (unit == "beats")
        return TimeUnit::beats;
    return fallback;
}

TimeUnit timeUnitOf (const XmlNode& node, TimeUnit fallback)
{
    return timeUnitOf (node, "timeUnit", fallback);
}

std::uint32_t parseColour (const std::string& text)
{
    auto value = trim (text);
    if (! value.empty() && value[0] == '#')
        value.erase (value.begin());
    if (value.size() != 6 && value.size() != 8)
        return 0;
    char* end = nullptr;
    const auto number = std::strtoul (value.c_str(), &end, 16);
    if (end == nullptr || *end != '\0')
        return 0;
    if (value.size() == 6)
        return 0xff000000u | (std::uint32_t) number;
    return (std::uint32_t) number;
}

// --- ZIP ---------------------------------------------------------------------------------------------------------
std::uint16_t le16 (const std::uint8_t* p) noexcept
{
    return (std::uint16_t) ((std::uint16_t) p[0] | ((std::uint16_t) p[1] << 8));
}

std::uint32_t le32 (const std::uint8_t* p) noexcept
{
    return (std::uint32_t) p[0] | ((std::uint32_t) p[1] << 8) | ((std::uint32_t) p[2] << 16) | ((std::uint32_t) p[3] << 24);
}

std::uint64_t le64 (const std::uint8_t* p) noexcept
{
    return (std::uint64_t) le32 (p) | ((std::uint64_t) le32 (p + 4) << 32);
}

struct ZipEntry
{
    std::string name;
    std::uint16_t method = 0;
    std::uint64_t compressed = 0;
    std::uint64_t uncompressed = 0;
    std::uint64_t localOffset = 0;
};

struct ZipArchive
{
    std::vector<std::uint8_t> data;
    std::vector<ZipEntry> entries;

    bool open (const std::uint8_t* bytes, std::size_t size, std::string& error)
    {
        data.assign (bytes, bytes + size);
        if (size < 22)
            return fail (error, "not a ZIP file (too small)");

        std::size_t eocd = npos;
        const auto lowest = size > 65557 ? size - 65557 : 0;
        for (auto i = size - 22;; --i)
        {
            if (le32 (data.data() + i) == 0x06054b50u)
            {
                eocd = i;
                break;
            }
            if (i == lowest)
                break;
        }
        if (eocd == npos)
            return fail (error, "not a ZIP file (no end of central directory)");

        std::uint64_t count = le16 (data.data() + eocd + 10);
        std::uint64_t centralSize = le32 (data.data() + eocd + 12);
        std::uint64_t centralOffset = le32 (data.data() + eocd + 16);
        std::uint64_t centralEnd = centralOffset + centralSize;

        // ZIP64: the locator sits immediately before the classic end record.
        if ((count == 0xffffu || centralOffset == 0xffffffffu || centralSize == 0xffffffffu) && eocd >= 20
            && le32 (data.data() + eocd - 20) == 0x07064b50u)
        {
            const auto zip64 = (std::size_t) le64 (data.data() + eocd - 20 + 8);
            if (zip64 + 56 <= size && le32 (data.data() + zip64) == 0x06064b50u)
            {
                count = le64 (data.data() + zip64 + 32);
                centralSize = le64 (data.data() + zip64 + 40);
                centralOffset = le64 (data.data() + zip64 + 48);
                centralEnd = centralOffset + centralSize;
            }
        }
        if (centralOffset > size || centralEnd > size || count > 100000)
            return fail (error, "not a ZIP file (bad central directory)");

        std::size_t at = (std::size_t) centralOffset;
        for (std::uint64_t index = 0; index < count; ++index)
        {
            if (at + 46 > size || le32 (data.data() + at) != 0x02014b50u)
                return fail (error, "not a ZIP file (bad entry)");
            ZipEntry entry;
            entry.method = le16 (data.data() + at + 10);
            entry.compressed = le32 (data.data() + at + 20);
            entry.uncompressed = le32 (data.data() + at + 24);
            const auto nameLength = le16 (data.data() + at + 28);
            const auto extraLength = le16 (data.data() + at + 30);
            const auto commentLength = le16 (data.data() + at + 32);
            entry.localOffset = le32 (data.data() + at + 42);
            if (at + 46 + nameLength > size)
                return fail (error, "not a ZIP file (bad entry name)");
            entry.name.assign (reinterpret_cast<const char*> (data.data() + at + 46), nameLength);

            // ZIP64 extra field: values appear in this order and replace the 0xFFFFFFFF placeholders.
            std::size_t extra = at + 46 + nameLength;
            const auto extraEnd = extra + extraLength;
            while (extra + 4 <= extraEnd && extra + 4 <= size)
            {
                const auto id = le16 (data.data() + extra);
                const auto length = le16 (data.data() + extra + 2);
                if (extra + 4 + length > extraEnd || extra + 4 + length > size)
                    break;
                if (id == 0x0001)
                {
                    std::size_t p = extra + 4;
                    const auto end = p + length;
                    if (entry.uncompressed == 0xffffffffu && p + 8 <= end)
                    {
                        entry.uncompressed = le64 (data.data() + p);
                        p += 8;
                    }
                    if (entry.compressed == 0xffffffffu && p + 8 <= end)
                    {
                        entry.compressed = le64 (data.data() + p);
                        p += 8;
                    }
                    if (entry.localOffset == 0xffffffffu && p + 8 <= end)
                        entry.localOffset = le64 (data.data() + p);
                }
                extra += 4 + length;
            }

            entries.push_back (std::move (entry));
            at += 46 + nameLength + extraLength + commentLength;
        }
        return true;
    }

    const ZipEntry* find (const std::string& name) const
    {
        for (const auto& entry : entries)
            if (entry.name == name)
                return &entry;
        return nullptr;
    }

    bool readEntry (const ZipEntry& entry, std::vector<std::uint8_t>& out, std::string& error) const
    {
        if (entry.localOffset + 30 > data.size() || le32 (data.data() + entry.localOffset) != 0x04034b50u)
            return fail (error, "a ZIP entry has no local header");
        const auto nameLength = le16 (data.data() + entry.localOffset + 26);
        const auto extraLength = le16 (data.data() + entry.localOffset + 28);
        const auto begin = entry.localOffset + 30 + nameLength + extraLength;
        if (begin > data.size() || entry.compressed > data.size() - begin)
            return fail (error, "a ZIP entry is truncated");
        const auto end = begin + (std::size_t) entry.compressed;

        if (entry.method == 0)
        {
            out.assign (data.begin() + (std::ptrdiff_t) begin, data.begin() + (std::ptrdiff_t) end);
            return true;
        }
        if (entry.method != 8)
            return fail (error, "a ZIP entry uses an unsupported compression method");

        const auto maxSize = entry.uncompressed > 0 ? (std::size_t) std::min<std::uint64_t> (entry.uncompressed, 1ull << 30)
                                                    : (std::size_t) (64ull << 20);
        auto inflated = lynx::inflateZlib (data.data() + begin, (std::size_t) entry.compressed, true, maxSize);
        if (! inflated)
            return fail (error, "a ZIP entry could not be decompressed");
        out = std::move (*inflated);
        return true;
    }

private:
    static bool fail (std::string& error, const char* text)
    {
        error = text;
        return false;
    }
};

// --- project.xml -------------------------------------------------------------------------------------------------
void collectClips (const XmlNode& node, std::vector<const XmlNode*>& out)
{
    if (node.name == "Clip")
        out.push_back (&node);
    for (const auto& child : node.children)
        collectClips (child, out);
}

void collectDevices (const XmlNode& node, int& plugins)
{
    static constexpr const char* pluginNames[] { "Vst2Plugin", "Vst3Plugin", "ClapPlugin", "AuPlugin", "BuiltinDevice",
                                                 "Equalizer", "Compressor", "NoiseGate", "Limiter" };
    for (const auto* name : pluginNames)
        if (node.name == name)
        {
            ++plugins;
            break;
        }
    for (const auto& child : node.children)
        collectDevices (child, plugins);
}

class ProjectParser
{
public:
    ProjectParser (Project& project) : out (project) {}

    bool parse (const XmlNode& root, std::string& error)
    {
        if (root.name != "Project")
        {
            error = "the XML is not a DAWproject (no Project root)";
            return false;
        }
        out.version = root.attributeOr ("version");

        for (const auto& child : root.children)
        {
            if (child.name == "Application")
            {
                out.application = child.attributeOr ("name");
                out.applicationVersion = child.attributeOr ("version");
            }
            else if (child.name == "Transport")
                parseTransport (child);
            else if (child.name == "Structure")
            {
                for (const auto& item : child.children)
                {
                    if (item.name == "Track")
                        parseTrack (item, 0);
                    else if (item.name == "Channel")
                        parseBareChannel (item);
                }
            }
            else if (child.name == "Arrangement")
                parseArrangement (child);
            else if (child.name == "Scenes" && ! child.children.empty())
                out.findings.push_back ("clip-launcher scenes are not imported (the arrangement is)");
        }

        resolveReferences();
        countProject();
        return true;
    }

private:
    Project& out;
    std::map<std::string, Clip> clipsById;

    static const std::string* attr (const XmlNode& node, const char* key)
    {
        return node.attribute (key);
    }

    void parseTransport (const XmlNode& transport)
    {
        if (const auto* tempo = transport.child ("Tempo"))
        {
            if (const auto* value = tempo->attribute ("value"))
            {
                const auto bpm = toDouble (*value, out.bpm);
                if (bpm > 0.0)
                    out.bpm = bpm;
            }
        }
        if (const auto* signature = transport.child ("TimeSignature"))
        {
            out.numerator = toInt (signature->attributeOr ("numerator"), 0);
            out.denominator = toInt (signature->attributeOr ("denominator"), 4);
        }
    }

    void parseBareChannel (const XmlNode& channel)
    {
        Track track;
        track.id = channel.attributeOr ("id");
        track.name = channel.attributeOr ("name");
        track.isMaster = channel.attributeOr ("role") == "master";
        if (! track.isMaster)
            out.tracks.push_back (std::move (track));
    }

    void parseTrack (const XmlNode& node, int depth)
    {
        Track track;
        track.id = node.attributeOr ("id");
        track.name = node.attributeOr ("name");
        track.comment = node.attributeOr ("comment");
        track.contentTypes = node.attributeOr ("contentType");
        track.colour = parseColour (node.attributeOr ("color"));
        track.loaded = toBool (node.attributeOr ("loaded"), true);
        track.depth = depth;

        if (const auto* channel = node.child ("Channel"))
        {
            track.isMaster = channel->attributeOr ("role") == "master";
            if (const auto* mute = channel->child ("Mute"))
            {
                if (const auto* value = mute->attribute ("value"))
                {
                    track.muted = toBool (*value, false);
                    track.hasMute = true;
                }
            }
            if (const auto* volume = channel->child ("Volume"))
                track.volume = toDouble (volume->attributeOr ("value"), 1.0);
            if (const auto* pan = channel->child ("Pan"))
                track.pan = toDouble (pan->attributeOr ("value"), 0.5);
            collectDevices (*channel, out.pluginCount);
        }

        if (! track.isMaster)
            out.tracks.push_back (std::move (track));

        for (const auto& child : node.children)
            if (child.name == "Track")
                parseTrack (child, depth + 1);
    }

    Track* trackFor (const std::string& id)
    {
        if (! id.empty())
            for (auto& track : out.tracks)
                if (track.id == id)
                    return &track;
        return nullptr;
    }

    void parseArrangement (const XmlNode& arrangement)
    {
        for (const auto& child : arrangement.children)
        {
            if (child.name == "Markers")
            {
                for (const auto& markerNode : child.children)
                    if (markerNode.name == "Marker")
                    {
                        Marker marker;
                        marker.time = toDouble (markerNode.attributeOr ("time"), 0.0);
                        marker.name = markerNode.attributeOr ("name");
                        out.markers.push_back (std::move (marker));
                    }
            }
            else if (child.name == "TempoAutomation")
                out.hasTempoAutomation = true;
            else if (child.name == "TimeSignatureAutomation")
                out.hasTimeSignatureAutomation = true;
            else if (child.name == "Lanes")
            {
                // the arrangement's own time unit: what its clips' time / duration and the markers are counted in
                out.unit = timeUnitOf (child, out.unit);
                parseLanes (child, out.unit, -1);
            }
        }
    }

    void assignClip (int& track, Clip clip)
    {
        if (track == -2)
            return; // the lane belongs to a track this import leaves out
        if (track < 0)
        {
            if (out.tracks.empty())
            {
                Track fallback;
                fallback.name = "Arrangement";
                out.tracks.push_back (std::move (fallback));
            }
            track = 0;
        }
        out.tracks[(std::size_t) track].clips.push_back (std::move (clip));
    }

    void parseLanes (const XmlNode& node, TimeUnit inheritedUnit, int inheritedTrack)
    {
        const auto unit = timeUnitOf (node, inheritedUnit);
        int track = inheritedTrack;
        if (const auto* id = attr (node, "track"))
        {
            if (auto* found = trackFor (*id))
                track = (int) (found - out.tracks.data());
            else if (! id->empty())
                track = -2; // the lane belongs to a track this import leaves out (the master, say)
        }

        // The schema lets a lane hold its content directly (Notes / Audio / Warps), without a Clip wrapper. Read such a
        // lane as one clip at the start of the timeline.
        bool directContent = false;
        for (const auto& child : node.children)
            if (child.name == "Notes" || child.name == "Audio" || child.name == "Warps")
                directContent = true;
        if (directContent && node.child ("Clips") == nullptr && node.child ("Lanes") == nullptr)
        {
            auto clip = parseClip (node, unit);
            if (clip.kind == ClipKind::notes || clip.kind == ClipKind::audio)
                assignClip (track, std::move (clip));
        }

        for (const auto& child : node.children)
        {
            if (directContent && (child.name == "Notes" || child.name == "Audio" || child.name == "Warps"))
                continue;
            if (child.name == "Lanes" || child.name == "Timeline")
                parseLanes (child, unit, track);
            else if (child.name == "Clips")
            {
                for (const auto& clipNode : child.children)
                    if (clipNode.name == "Clip")
                        assignClip (track, parseClip (clipNode, unit));
            }
            else if (child.name == "Clip")
                assignClip (track, parseClip (child, unit));
            else if (child.name == "ClipSlot" && ! child.children.empty())
                out.findings.push_back ("clip-launcher slots are not imported (only the arrangement is)");
        }
    }

    Clip parseClip (const XmlNode& node, TimeUnit inheritedUnit)
    {
        Clip clip;
        clip.id = node.attributeOr ("id");
        clip.name = node.attributeOr ("name");
        clip.reference = node.attributeOr ("reference");
        clip.time = toDouble (node.attributeOr ("time"), 0.0);
        if (node.attribute ("duration") != nullptr)
        {
            clip.duration = toDouble (*node.attribute ("duration"), 0.0);
            clip.hasDuration = true;
        }
        if (node.attribute ("playStart") != nullptr)
        {
            clip.playStart = toDouble (*node.attribute ("playStart"), 0.0);
            clip.hasPlayStart = true;
        }
        if (node.attribute ("playStop") != nullptr)
        {
            clip.playStop = toDouble (*node.attribute ("playStop"), 0.0);
            clip.hasPlayStop = true;
        }
        if (node.attribute ("loopStart") != nullptr && node.attribute ("loopEnd") != nullptr)
        {
            clip.loopStart = toDouble (*node.attribute ("loopStart"), 0.0);
            clip.loopEnd = toDouble (*node.attribute ("loopEnd"), 0.0);
            clip.hasLoop = true;
        }
        if (node.attribute ("fadeInTime") != nullptr)
        {
            clip.fadeIn = toDouble (*node.attribute ("fadeInTime"), 0.0);
            clip.hasFadeIn = true;
        }
        if (node.attribute ("fadeOutTime") != nullptr)
        {
            clip.fadeOut = toDouble (*node.attribute ("fadeOutTime"), 0.0);
            clip.hasFadeOut = true;
        }
        if (node.attribute ("enable") != nullptr)
        {
            clip.enable = toBool (*node.attribute ("enable"), true);
            clip.hasEnable = true;
        }
        clip.unit = inheritedUnit;
        clip.contentUnit = timeUnitOf (node, "contentTimeUnit", clip.unit);
        clip.fadeUnit = timeUnitOf (node, "fadeTimeUnit", clip.unit);

        for (const auto& child : node.children)
        {
            if (child.name == "Notes")
            {
                clip.noteUnit = timeUnitOf (child, clip.contentUnit);
                for (const auto& noteNode : child.children)
                    if (noteNode.name == "Note")
                        clip.notes.push_back (parseNote (noteNode));
                if (! clip.notes.empty())
                    clip.kind = ClipKind::notes;
            }
            else if (child.name == "Clips")
            {
                for (const auto& inner : child.children)
                    if (inner.name == "Clip")
                    {
                        auto nested = parseClip (inner, clip.contentUnit);
                        if (nested.hasPlayStart && ! clip.hasPlayStart)
                        {
                            clip.playStart = nested.playStart;
                            clip.hasPlayStart = true;
                            clip.contentUnit = nested.contentUnit;
                        }
                        if (! nested.notes.empty() && clip.kind == ClipKind::other)
                        {
                            clip.notes = nested.notes;
                            clip.noteUnit = nested.noteUnit;
                            clip.kind = ClipKind::notes;
                        }
                        if (nested.audio.hasFile)
                        {
                            clip.audio = nested.audio;
                            clip.kind = ClipKind::audio;
                        }
                        if (! nested.warps.empty())
                        {
                            clip.warps = nested.warps;
                            clip.warpContentUnit = nested.warpContentUnit;
                        }
                        clip.clips.push_back (std::move (nested));
                    }
            }
            else if (child.name == "Audio")
            {
                clip.audio = parseAudio (child);
                if (clip.audio.hasFile)
                    clip.kind = ClipKind::audio;
            }
            else if (child.name == "Warps")
            {
                clip.warpContentUnit = timeUnitOf (child, TimeUnit::seconds);
                for (const auto& warpNode : child.children)
                {
                    if (warpNode.name == "Warp")
                        clip.warps.push_back ({ toDouble (warpNode.attributeOr ("time"), 0.0),
                                                toDouble (warpNode.attributeOr ("contentTime"), 0.0) });
                    else if (warpNode.name == "Audio")
                    {
                        clip.audio = parseAudio (warpNode);
                        if (clip.audio.hasFile)
                            clip.kind = ClipKind::audio;
                    }
                }
            }
            else if (child.name == "Points" && clip.kind == ClipKind::other)
                clip.kind = ClipKind::other; // automation clips stay reported, not guessed
        }

        if (! clip.id.empty())
            clipsById[clip.id] = clip;
        return clip;
    }

    static Note parseNote (const XmlNode& node)
    {
        Note note;
        note.time = toDouble (node.attributeOr ("time"), 0.0);
        note.duration = toDouble (node.attributeOr ("duration"), 0.0);
        note.channel = toInt (node.attributeOr ("channel"), 0);
        note.key = toInt (node.attributeOr ("key"), 60);
        note.velocity = toDouble (node.attributeOr ("vel"), 0.78);
        note.release = toDouble (node.attributeOr ("rel"), 0.5);
        return note;
    }

    static AudioFile parseAudio (const XmlNode& node)
    {
        AudioFile audio;
        audio.duration = toDouble (node.attributeOr ("duration"), 0.0);
        audio.sampleRate = toInt (node.attributeOr ("sampleRate"), 0);
        audio.channels = toInt (node.attributeOr ("channels"), 0);
        audio.algorithm = node.attributeOr ("algorithm");
        if (const auto* file = node.child ("File"))
        {
            audio.path = file->attributeOr ("path");
            audio.external = toBool (file->attributeOr ("external"), false);
            audio.hasFile = ! audio.path.empty();
        }
        return audio;
    }

    void resolveReferences()
    {
        if (clipsById.empty())
            return;
        for (auto& track : out.tracks)
            for (auto& clip : track.clips)
                resolveClip (clip, 0);
    }

    void resolveClip (Clip& clip, int depth)
    {
        if (depth > 8)
            return;
        if (! clip.reference.empty() && clip.notes.empty() && ! clip.audio.hasFile)
        {
            const auto found = clipsById.find (clip.reference);
            if (found != clipsById.end())
            {
                clip.kind = found->second.kind;
                clip.notes = found->second.notes;
                clip.noteUnit = found->second.noteUnit;
                clip.audio = found->second.audio;
                clip.warps = found->second.warps;
                clip.warpContentUnit = found->second.warpContentUnit;
                clip.clips = found->second.clips;
                if (clip.name.empty())
                    clip.name = found->second.name;
            }
        }
        for (auto& nested : clip.clips)
            resolveClip (nested, depth + 1);
    }

    void countProject()
    {
        for (auto& track : out.tracks)
            for (const auto& clip : track.clips)
                countClip (clip);
        if (! out.findings.empty())
            out.findings.erase (std::remove_if (out.findings.begin(), out.findings.end(),
                                                [] (const std::string& s) { return s.empty(); }),
                                out.findings.end());
    }

    void countClip (const Clip& clip)
    {
        if (clip.kind == ClipKind::notes)
        {
            ++out.clipCount;
            out.noteCount += (int) clip.notes.size();
        }
        else if (clip.kind == ClipKind::audio)
        {
            ++out.clipCount;
            ++out.audioCount;
        }
    }
};

// --- model -> Song -----------------------------------------------------------------------------------------------
std::int64_t ticksOf (double value, TimeUnit unit, double bpm)
{
    if (! std::isfinite (value) || value <= 0.0)
        return 0;
    const auto beats = unit == TimeUnit::seconds ? value * bpm / 60.0 : value;
    return (std::int64_t) std::llround (beats * (double) ppq);
}

std::string uniqueName (const std::string& wanted, const std::vector<Pattern>& patterns)
{
    if (wanted.empty())
        return "Imported";
    const auto exists = [&patterns] (const std::string& name) {
        for (const auto& p : patterns)
            if (p.name == name)
                return true;
        return false;
    };
    if (! exists (wanted))
        return wanted;
    for (int i = 2;; ++i)
    {
        const auto candidate = wanted + " " + std::to_string (i);
        if (! exists (candidate))
            return candidate;
    }
}

std::string fileNameOf (const std::string& path)
{
    const auto slash = path.find_last_of ("/\\");
    const auto name = slash == npos ? path : path.substr (slash + 1);
    return name.empty() ? path : name;
}

std::string stemOf (const std::string& path)
{
    auto name = fileNameOf (path);
    if (const auto dot = name.find_last_of ('.'); dot != npos && dot > 0)
        name.erase (dot);
    return name;
}

bool safeEntryPath (const std::string& path)
{
    if (path.empty() || path[0] == '/' || path[0] == '\\')
        return false;
    if (path.size() > 1 && path[1] == ':')
        return false;
    // Split on both separators: a ZIP always uses '/', but an exporter may write '\', and '..' must not slip past the
    // check on a platform whose path parser does not treat one of them as a separator.
    std::size_t begin = 0;
    while (begin <= path.size())
    {
        const auto end = path.find_first_of ("/\\", begin);
        const auto part = path.substr (begin, (end == npos ? path.size() : end) - begin);
        if (part == ".." || part == ".")
            return false;
        if (end == npos)
            break;
        begin = end + 1;
    }
    return true;
}

struct MediaSlot
{
    AudioFile* audio = nullptr;
};

void collectMedia (Clip& clip, std::vector<MediaSlot>& out)
{
    // A parent clip copies the audio of its nested event timeline (Bitwig exports audio clips that way), so stop at
    // the first level that has a file: the same AudioFile must not be extracted twice.
    if (clip.audio.hasFile)
    {
        out.push_back ({ &clip.audio });
        return;
    }
    for (auto& nested : clip.clips)
        collectMedia (nested, out);
}

bool readWholeFile (const std::string& path, std::vector<std::uint8_t>& bytes, std::string& error)
{
    std::ifstream in (path, std::ios::binary);
    if (! in)
    {
        error = "could not read " + path;
        return false;
    }
    bytes.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
    return true;
}

bool writeWholeFile (const fs::path& path, const std::vector<std::uint8_t>& bytes, std::string& error)
{
    std::error_code ec;
    if (path.has_parent_path())
        fs::create_directories (path.parent_path(), ec);
    if (ec)
    {
        error = "could not create " + path.parent_path().string();
        return false;
    }
    std::ofstream out (path, std::ios::binary | std::ios::trunc);
    if (! out)
    {
        error = "could not write " + path.string();
        return false;
    }
    out.write (reinterpret_cast<const char*> (bytes.data()), (std::streamsize) bytes.size());
    if (! out)
    {
        error = "could not finish writing " + path.string();
        return false;
    }
    return true;
}
} // namespace

bool read (const std::uint8_t* data, std::size_t size, Project& out, std::string& error)
{
    out = {};
    error.clear();
    if (data == nullptr || size == 0)
    {
        error = "the file is empty";
        return false;
    }

    ZipArchive zip;
    if (! zip.open (data, size, error))
    {
        error = "not a DAWproject: " + error;
        return false;
    }

    const ZipEntry* projectEntry = zip.find ("project.xml");
    if (projectEntry == nullptr)
        for (const auto& entry : zip.entries)
            if (endsWith (entry.name, "/project.xml"))
            {
                projectEntry = &entry;
                break;
            }
    if (projectEntry == nullptr)
    {
        error = "not a DAWproject: the ZIP has no project.xml";
        return false;
    }

    std::vector<std::uint8_t> xml;
    if (! zip.readEntry (*projectEntry, xml, error))
        return false;

    XmlNode root;
    XmlParser parser (xml);
    if (! parser.parse (root, error))
    {
        error = "project.xml: " + error;
        return false;
    }

    ProjectParser projectParser (out);
    if (! projectParser.parse (root, error))
        return false;

    if (! out.application.empty())
        out.findings.push_back ("written by " + out.application + (out.applicationVersion.empty() ? "" : " " + out.applicationVersion));
    if (out.pluginCount > 0)
        out.findings.push_back ("plugins named in the project: " + std::to_string (out.pluginCount) + " (not converted)");
    if (out.hasTempoAutomation)
        out.findings.push_back ("tempo automation is not imported: the tempo at the start is used");
    if (out.hasTimeSignatureAutomation)
        out.findings.push_back ("time signature changes are not imported: the first signature is used");
    out.findings.push_back ("track levels, pan, automation, devices and mixer routing are not imported");
    return true;
}

bool readFile (const std::string& path, Project& out, std::string& error)
{
    std::vector<std::uint8_t> bytes;
    if (! readWholeFile (path, bytes, error))
        return false;
    if (! read (bytes.data(), bytes.size(), out, error))
        return false;
    out.sourcePath = path;
    return true;
}

bool extractMedia (Project& project, const std::string& destination, std::string& error)
{
    error.clear();
    std::vector<MediaSlot> media;
    for (auto& track : project.tracks)
        for (auto& clip : track.clips)
            collectMedia (clip, media);
    if (media.empty())
        return true;

    std::vector<std::uint8_t> zipBytes;
    ZipArchive zip;
    bool zipOpen = false;
    const fs::path packageDir = project.sourcePath.empty() ? fs::path() : fs::path (project.sourcePath).parent_path();
    const fs::path destinationDir = destination.empty() ? fs::path() : fs::path (destination);

    for (auto& slot : media)
    {
        auto& audio = *slot.audio;
        if (audio.resolvedPath.empty())
        {
            if (audio.external)
            {
                const fs::path written (audio.path);
                const auto candidate = written.is_absolute() ? written : packageDir / written;
                std::error_code ec;
                if (fs::exists (candidate, ec))
                    audio.resolvedPath = candidate.lexically_normal().string();
                else
                    project.findings.push_back ("audio file not found: " + audio.path);
                continue;
            }

            if (! zipOpen)
            {
                if (project.sourcePath.empty() || ! readWholeFile (project.sourcePath, zipBytes, error))
                {
                    error = "the DAWproject path is unknown, so its audio cannot be extracted";
                    return false;
                }
                if (! zip.open (zipBytes.data(), zipBytes.size(), error))
                    return false;
                zipOpen = true;
            }
            if (destinationDir.empty())
            {
                error = "no folder to extract the DAWproject audio into";
                return false;
            }

            auto entryName = audio.path;
            while (entryName.size() >= 2 && entryName[0] == '.'
                   && (entryName[1] == '/' || entryName[1] == '\\'))
                entryName.erase (0, 2);
            if (safeEntryPath (entryName))
            {
                const auto* entry = zip.find (entryName);
                if (entry == nullptr)
                    for (const auto& candidate : zip.entries)
                        if (lower (candidate.name) == lower (entryName))
                        {
                            entry = &candidate;
                            break;
                        }
                if (entry != nullptr)
                {
                    std::vector<std::uint8_t> bytes;
                    if (zip.readEntry (*entry, bytes, error))
                    {
                        const auto target = destinationDir / fs::path (entryName);
                        if (writeWholeFile (target, bytes, error))
                        {
                            std::error_code ec;
                            const auto absolute = fs::absolute (target, ec);
                            audio.resolvedPath = (ec ? target : absolute).lexically_normal().string();
                            continue;
                        }
                    }
                }
            }
            project.findings.push_back ("audio file missing from the package: " + audio.path);
        }
    }
    return true;
}

bool importInto (Song& song, Project& project, const std::string& patternName, const std::string& mediaDirectory,
                 std::string& error)
{
    error.clear();
    if (! mediaDirectory.empty())
    {
        if (! extractMedia (project, mediaDirectory, error))
            return false;
    }

    if (project.bpm > 0.0)
        song.bpm = project.bpm;
    if (project.numerator > 0)
        song.beatsPerBar = std::clamp (project.numerator, 1, 16);
    if (project.denominator != 4)
        project.findings.push_back ("the time signature's denominator (" + std::to_string (project.denominator)
                                    + ") is not imported: LYNX counts bars in quarter notes");

    // One Playlist track per non-master DAWproject track; the master becomes LYNX's mixer master.
    std::vector<int> playlistOfTrack (project.tracks.size(), -1);
    for (std::size_t i = 0; i < project.tracks.size(); ++i)
    {
        const auto& track = project.tracks[i];
        PlaylistTrack playlistTrack;
        playlistTrack.name = track.name.empty() ? "Track " + std::to_string (i + 1) : track.name;
        playlistTrack.muted = (track.hasMute && track.muted) || ! track.loaded;
        if (! track.loaded)
            project.findings.push_back ("a disabled track was imported muted: "
                                        + (track.name.empty() ? "Track " + std::to_string (i + 1) : track.name));
        playlistTrack.colour = track.colour;
        playlistTrack.grouped = track.depth > 0;
        song.playlistTracks.push_back (std::move (playlistTrack));
        playlistOfTrack[i] = (int) song.playlistTracks.size() - 1;
    }

    std::map<int, int> notesChannelOfTrack;
    std::map<std::string, int> audioChannelOfFile;
    int firstPattern = -1;

    const auto audioChannelFor = [&] (const Track& track, const AudioFile& audio) {
        const auto key = audio.resolvedPath.empty() ? audio.path : audio.resolvedPath;
        if (const auto found = audioChannelOfFile.find (key); found != audioChannelOfFile.end())
            return found->second;

        ChannelDef channel;
        channel.name = track.name.empty() ? stemOf (audio.path) : track.name;
        if (audioChannelOfFile.size() > 0)
        {
            bool used = false;
            for (const auto& c : song.channels)
                used = used || c.name == channel.name;
            if (used)
                channel.name += " (" + stemOf (audio.path) + ")";
        }
        channel.instrument = InstrumentKind::audioClip;
        channel.samplePath = audio.resolvedPath;
        channel.volume = 1.0f;
        channel.colour = track.colour;
        song.channels.push_back (std::move (channel));
        const auto index = (int) song.channels.size() - 1;
        audioChannelOfFile[key] = index;
        return index;
    };

    const auto notesChannelFor = [&] (const Track& track, std::size_t trackIndex) {
        if (const auto found = notesChannelOfTrack.find ((int) trackIndex); found != notesChannelOfTrack.end())
            return found->second;

        ChannelDef channel;
        channel.name = track.name.empty() ? "Track " + std::to_string (trackIndex + 1) : track.name;
        channel.instrument = InstrumentKind::synth;
        channel.volume = 0.6f;
        channel.colour = track.colour;
        song.channels.push_back (std::move (channel));
        const auto index = (int) song.channels.size() - 1;
        notesChannelOfTrack[(int) trackIndex] = index;
        return index;
    };

    for (std::size_t trackIndex = 0; trackIndex < project.tracks.size(); ++trackIndex)
    {
        auto& track = project.tracks[trackIndex];
        const auto playlistTrack = playlistOfTrack[trackIndex];
        bool hasNotes = false, hasAudio = false;

        for (auto& clip : track.clips)
        {
            if (clip.kind == ClipKind::notes && ! clip.notes.empty())
            {
                hasNotes = true;
                const auto channel = notesChannelFor (track, trackIndex);
                Pattern pattern;
                pattern.name = uniqueName (clip.name.empty() ? (track.name.empty() ? patternName : track.name) : clip.name,
                                           song.patterns);
                std::int64_t lastNote = 0;
                for (const auto& source : clip.notes)
                {
                    lynx::Note note;
                    note.start = ticksOf (source.time, clip.noteUnit, project.bpm);
                    note.length = std::max<std::int64_t> (1, ticksOf (source.duration, clip.noteUnit, project.bpm));
                    note.channel = channel;
                    note.key = std::clamp (source.key, 0, 127);
                    note.velocity = (float) std::clamp (source.velocity, 0.0, 1.0);
                    note.release = (float) std::clamp (source.release, 0.0, 1.0);
                    lastNote = std::max (lastNote, note.start + note.length);
                    pattern.notes.push_back (note);
                }
                pattern.sortNotes();
                song.patterns.push_back (std::move (pattern));
                const auto patternIndex = (int) song.patterns.size() - 1;
                if (firstPattern < 0)
                    firstPattern = patternIndex;

                // (lynx::Clip / lynx::ClipKind: the DAWproject structs of those names are in scope here)
                lynx::Clip placed;
                placed.kind = lynx::ClipKind::pattern;
                placed.pattern = patternIndex;
                placed.track = playlistTrack;
                placed.start = ticksOf (clip.time, project.unit, project.bpm);
                placed.offset = clip.hasPlayStart ? ticksOf (clip.playStart, clip.contentUnit, project.bpm) : 0;
                placed.length = clip.hasDuration ? ticksOf (clip.duration, project.unit, project.bpm) : std::max<std::int64_t> (1, lastNote - placed.offset);
                if (placed.length <= 0)
                    placed.length = std::max<std::int64_t> (1, lastNote - placed.offset);
                placed.muted = clip.hasEnable && ! clip.enable;
                song.playlist.push_back (placed);
            }
            else if (clip.kind == ClipKind::audio && clip.audio.hasFile)
            {
                hasAudio = true;
                const auto channel = audioChannelFor (track, clip.audio);
                if (clip.audio.resolvedPath.empty())
                    project.findings.push_back ("audio clip has no extracted file: " + clip.audio.path);

                lynx::Clip placed;
                placed.kind = lynx::ClipKind::audio;
                placed.channel = channel;
                placed.track = playlistTrack;
                placed.start = ticksOf (clip.time, project.unit, project.bpm);
                placed.offset = clip.hasPlayStart ? ticksOf (clip.playStart, clip.contentUnit, project.bpm) : 0;
                placed.length = clip.hasDuration ? ticksOf (clip.duration, project.unit, project.bpm)
                                                 : std::max<std::int64_t> (1, ticksOf (clip.audio.duration, TimeUnit::seconds, project.bpm) - placed.offset);
                if (placed.length <= 0)
                    placed.length = std::max<std::int64_t> (1, ticksOf (clip.audio.duration, TimeUnit::seconds, project.bpm));
                placed.fadeIn = clip.hasFadeIn ? ticksOf (clip.fadeIn, clip.fadeUnit, project.bpm) : 0;
                placed.fadeOut = clip.hasFadeOut ? ticksOf (clip.fadeOut, clip.fadeUnit, project.bpm) : 0;
                placed.muted = clip.hasEnable && ! clip.enable;
                song.playlist.push_back (placed);
            }
            else if (clip.kind == ClipKind::other)
                project.findings.push_back ("an automation or unsupported clip was left out: "
                                            + (clip.name.empty() ? std::string ("unnamed") : clip.name));
        }

        if (hasAudio && ! hasNotes)
            song.playlistTracks[(std::size_t) playlistTrack].mode = PlaylistTrackMode::audio;
        else if (hasNotes)
        {
            song.playlistTracks[(std::size_t) playlistTrack].mode = PlaylistTrackMode::instrument;
            if (const auto found = notesChannelOfTrack.find ((int) trackIndex); found != notesChannelOfTrack.end())
                song.playlistTracks[(std::size_t) playlistTrack].channel = found->second;
        }
    }

    std::stable_sort (song.playlist.begin(), song.playlist.end(),
                      [] (const lynx::Clip& a, const lynx::Clip& b) { return a.start < b.start; });

    for (const auto& marker : project.markers)
        song.addMarker ({ ticksOf (marker.time, project.unit, project.bpm), marker.name, 0, 4, lynx::Marker::none });

    if (firstPattern >= 0)
        song.currentPattern = firstPattern;
    if (! song.playlist.empty())
        song.mode = PlayMode::song;

    if (song.patterns.empty() && song.channels.empty() && song.playlist.empty() && song.playlistTracks.empty())
    {
        error = "the DAWproject has no tracks, clips or notes to import";
        return false;
    }
    return true;
}

bool importInto (Song& song, Project& project, const std::string& patternName, std::string& error)
{
    return importInto (song, project, patternName, {}, error);
}
} // namespace lynx::dawproject
