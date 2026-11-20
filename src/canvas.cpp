/* The editor's drawing surface (canvas.h). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <optional>
#include "canvas.h"
#include "imgui_internal.h"
#include "json.h"
#include "zh_cn.h"

namespace ui
{
namespace
{
const int LAYERS = 16;
/* The sizes the editor draws text at (1080p units): every one is baked at the screen's scale. */
const float SIZES[] = {12, 13, 15, 16, 19, 20, 22, 24};
const int SIZE_COUNT = sizeof SIZES / sizeof SIZES[0];

struct Metrics { float pixelHeightPerEm = 1.2f, capPerEm = 0.7f; };
/* faux: the title face is the body font drawn twice (no FS Sinclair Medium) */
struct Baked { ImFont* fonts[2][SIZE_COUNT] = {}; float scale = 0; Metrics metrics[2], fallback, symbols; bool faux = false, chinese = false; };
Baked g_baked;
std::string g_folder, g_faceFile[2], g_fallbackFile, g_symbolFile, g_chineseFile;
uint64_t g_fontGeneration = 0;

std::string WindowsFont(const wchar_t* name)
{
    wchar_t folder[MAX_PATH];
    UINT n = GetWindowsDirectoryW(folder, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return "";
    const std::wstring path = std::wstring(folder, n) + L"\\Fonts\\" + name;
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES ? hfjson::Utf8(path) : "";
}

uint16_t U16(const unsigned char* p) { return (uint16_t)(p[0] << 8 | p[1]); }
int16_t I16(const unsigned char* p) { return (int16_t)U16(p); }
uint32_t U32(const unsigned char* p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

/* A file's bytes, read 64 KB at a time straight into the vector (none when it cannot be read). */
std::vector<unsigned char> FileBytes(const std::string& file)
{
    std::vector<unsigned char> data;
    FILE* f = _wfopen(hfjson::Wide(file).c_str(), L"rb");
    if (!f) return data;
    for (size_t n = 1; n > 0;)
    {
        size_t had = data.size();
        data.resize(had + 65536);
        n = fread(&data[had], 1, 65536, f);
        data.resize(had + n);
    }
    if (ferror(f)) data.clear();
    fclose(f);
    return data;
}

/* TTC offsets select an sfnt directory. Table offsets within that directory are still file-relative. */
std::optional<size_t> FontFace(const std::vector<unsigned char>& data, unsigned index)
{
    if (data.size() < 12) return std::nullopt;
    size_t offset = 0;
    if (!memcmp(data.data(), "ttcf", 4))
    {
        if (index >= U32(data.data() + 8) || index >= (data.size() - 12) / 4) return std::nullopt;
        offset = U32(data.data() + 12 + 4 * (size_t)index);
    }
    else if (index != 0) return std::nullopt;
    if (offset > data.size() - 12) return std::nullopt;
    const unsigned char* face = data.data() + offset;
    if (U32(face) != 0x00010000 && memcmp(face, "OTTO", 4) && memcmp(face, "true", 4)) return std::nullopt;
    const unsigned count = U16(face + 4);
    if (count > (data.size() - offset - 12) / 16) return std::nullopt;
    for (unsigned i = 0; i < count; ++i)
    {
        const unsigned char* table = face + 12 + 16 * (size_t)i;
        const size_t start = U32(table + 8), length = U32(table + 12);
        if (start > data.size() || length > data.size() - start) return std::nullopt;
    }
    return offset;
}

const unsigned char* FontTable(const std::vector<unsigned char>& data, size_t face, const char* tag, size_t minimum)
{
    unsigned count = U16(data.data() + face + 4);
    for (unsigned i = 0; i < count; ++i)
    {
        const unsigned char* record = data.data() + face + 12 + 16 * (size_t)i;
        if (memcmp(record, tag, 4)) continue;
        size_t offset = U32(record + 8), length = U32(record + 12);
        if (offset <= data.size() && length <= data.size() - offset && length >= minimum)
            return data.data() + offset;
        break;
    }
    return nullptr;
}

/* Convert the selected face's vertical metrics to the pixel height and cap height used by the canvas. */
Metrics ReadMetrics(const std::vector<unsigned char>& data, size_t face)
{
    const unsigned char* head = FontTable(data, face, "head", 20);
    const unsigned char* hhea = FontTable(data, face, "hhea", 8);
    const unsigned char* os2 = FontTable(data, face, "OS/2", 90);
    const int upem = head ? U16(head + 18) : 0;
    const int height = hhea ? I16(hhea + 4) - I16(hhea + 6) : 0;
    const int cap = os2 && U16(os2) >= 2 ? I16(os2 + 88) : 0;
    Metrics m;
    if (upem > 0 && height > 0) m.pixelHeightPerEm = (float)height / (float)upem;
    if (upem > 0 && cap > 0) m.capPerEm = (float)cap / (float)upem;
    return m;
}

Metrics ReadMetrics(const std::string& file)
{
    const auto bytes = FileBytes(file);
    const auto face = FontFace(bytes, 0);
    return face ? ReadMetrics(bytes, *face) : Metrics{};
}

struct ChineseGlyphs
{
    ImFontGlyphRangesBuilder builder;
    ImVector<ImWchar> required, visible;
    bool pending = false;

    ChineseGlyphs()
    {
        zh_cn::AddGlyphs(builder);
        builder.BuildRanges(&required);
    }

    const ImWchar* Ranges()
    {
        if (pending)
        {
            visible.clear();
            builder.BuildRanges(&visible);
            pending = false;
        }
        return visible.empty() ? required.Data : visible.Data;
    }
};

ChineseGlyphs& Chinese()
{
    static ChineseGlyphs glyphs;
    return glyphs;
}

/* The atlas can be rebuilt before the next frame. Remember requested code points even when YaHei lacks them. */
void TrackVisibleGlyphs(const std::string& text, ImFont& font)
{
    if (!g_baked.chinese) return;
    ChineseGlyphs& glyphs = Chinese();
    const char* end = text.data() + text.size();
    for (const char* at = text.data(); at < end;)
    {
        unsigned int ch;
        int bytes = ImTextCharFromUtf8(&ch, at, end);
        if (bytes == 0) break;
        at += bytes;
        if (ch < 0x80 || ch > IM_UNICODE_CODEPOINT_MAX || ch == IM_UNICODE_CODEPOINT_INVALID ||
            glyphs.builder.GetBit((int)ch) || font.FindGlyphNoFallback((ImWchar)ch)) continue;
        glyphs.builder.AddChar((ImWchar)ch);
        glyphs.pending = true;
    }
}

bool HasChineseGlyphs(const ImWchar* ranges)
{
    for (auto& face : g_baked.fonts)
        for (ImFont* font : face)
            for (const ImWchar* range = ranges; range[0] != 0; range += 2)
                for (int ch = range[0] < 0x80 ? 0x80 : range[0]; ch <= range[1]; ++ch)
                    if (!font->FindGlyphNoFallback((ImWchar)ch)) return false;
    return true;
}

int Nearest(float size)
{
    int best = 0;
    for (int i = 1; i < SIZE_COUNT; i++) if (fabsf(SIZES[i] - size) < fabsf(SIZES[best] - size)) best = i;
    return best;
}
} // namespace

void SetFontFolder(const std::string& folder) { g_folder = folder; }

/* The game's own FS Sinclair and FS Sinclair Medium (extracted into the editor's folder), else Windows' Bahnschrift */
static void FindFontFiles()
{
    auto own = [](const char* name) {
        std::string path = g_folder + "\\" + name;
        return !g_folder.empty() && GetFileAttributesW(hfjson::Wide(path).c_str()) != INVALID_FILE_ATTRIBUTES ? path : std::string();
    };
    g_fallbackFile = WindowsFont(L"bahnschrift.ttf");
    if (g_fallbackFile.empty()) g_fallbackFile = WindowsFont(L"segoeui.ttf");
    g_faceFile[BODY] = own("fs-sinclair.ttf");
    g_faceFile[TITLE] = own("fs-sinclair-medium.ttf");
    g_symbolFile = WindowsFont(L"seguisym.ttf");
}

static const ImWchar TEXT_RANGES[] = {0x0020, 0x00FF, 0x2010, 0x2027, 0x2030, 0x203A, 0x2122, 0x2122, 0x2212, 0x2212, 0};
static const ImWchar SYMBOL_RANGES[] = {0x2190, 0x2193, 0x2715, 0x2715, 0};

/* a font merged in at the same em size (a font with a taller line takes a larger pixel height) */
static void MergeFont(const std::string& file, const Metrics& m, float size, float s, float multiply, const ImWchar* ranges)
{
    if (file.empty()) return;
    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;
    config.OversampleH = 2;
    config.RasterizerMultiply = multiply;
    float pixels = size * s * m.pixelHeightPerEm;
    ImGui::GetIO().Fonts->AddFontFromFileTTF(file.c_str(), pixels < 6 ? 6 : pixels, &config, ranges);
}

/* A face at one of the sizes, from its file (ImGui's own font when that cannot be read). */
static ImFont* BakeFont(const std::string& file, int face, int i, float s)
{
    ImGuiIO& io = ImGui::GetIO();
    float pixels = SIZES[i] * s * g_baked.metrics[face].pixelHeightPerEm;
    if (pixels < 6) pixels = 6;
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 1;
    config.PixelSnapH = true;
    config.RasterizerMultiply = face == TITLE && g_baked.faux ? 1.15f : 1.0f;
    ImFont* font = !file.empty() ? io.Fonts->AddFontFromFileTTF(file.c_str(), pixels, &config, TEXT_RANGES) : nullptr;
    if (font == nullptr) { ImFontConfig fallback; fallback.SizePixels = pixels; return io.Fonts->AddFontDefault(&fallback); }
    /* what FS Sinclair lacks: other characters from the fallback, the arrows from Segoe UI Symbol */
    if (file != g_fallbackFile) MergeFont(g_fallbackFile, g_baked.fallback, SIZES[i], s, config.RasterizerMultiply, TEXT_RANGES);
    MergeFont(g_symbolFile, g_baked.symbols, SIZES[i], s, config.RasterizerMultiply, SYMBOL_RANGES);
    return font;
}

bool BuildFonts(float s, const char* chineseFontFile)
{
    ImGuiIO& io = ImGui::GetIO();
    std::string chinesePath = chineseFontFile ? chineseFontFile : WindowsFont(L"msyh.ttc");
    ChineseGlyphs& glyphs = Chinese();
    if (!glyphs.pending && fabsf(g_baked.scale - s) < 0.005f && chinesePath == g_chineseFile && io.Fonts->IsBuilt()) return false;
    g_chineseFile = std::move(chinesePath);
    zh_cn::SetFontReady(false);
    if (g_fallbackFile.empty()) FindFontFiles();
    g_baked = Baked{};
    g_baked.scale = s;

    std::string files[2] = {g_faceFile[BODY], g_faceFile[TITLE]};
    if (files[BODY].empty()) files[BODY] = g_fallbackFile;
    g_baked.faux = files[TITLE].empty();
    if (g_baked.faux) files[TITLE] = files[BODY];
    for (int face = 0; face < 2; face++) if (!files[face].empty()) g_baked.metrics[face] = ReadMetrics(files[face]);
    if (!g_fallbackFile.empty()) g_baked.fallback = ReadMetrics(g_fallbackFile);
    if (!g_symbolFile.empty()) g_baked.symbols = ReadMetrics(g_symbolFile);

    const ImWchar* requested = glyphs.Ranges();
    constexpr unsigned regularFace = 0;
    const auto chinese = FileBytes(g_chineseFile);
    const auto directory = FontFace(chinese, regularFace);
    const ImWchar* ranges = nullptr;
    Metrics chineseMetrics;
    if (directory && chinese.size() <= INT_MAX &&
        FontTable(chinese, *directory, "head", 54) && FontTable(chinese, *directory, "hhea", 36) &&
        FontTable(chinese, *directory, "hmtx", 4) && FontTable(chinese, *directory, "cmap", 4))
    {
        chineseMetrics = ReadMetrics(chinese, *directory);
        ranges = requested;
    }

    ImFontAtlas& atlas = *io.Fonts;
    const auto bake = [&](const ImWchar* glyphs) {
        atlas.Clear();
        void* shared = glyphs ? ImGui::MemAlloc(chinese.size()) : nullptr;
        if (shared) memcpy(shared, chinese.data(), chinese.size());
        bool ownsData = true;
        for (int face = 0; face < 2; face++)
        {
            for (int i = 0; i < SIZE_COUNT; i++)
            {
                g_baked.fonts[face][i] = BakeFont(files[face], face, i, s);
                if (!shared) continue;
                ImFontConfig config;
                config.FontNo = regularFace;
                config.MergeMode = true;
                config.PixelSnapH = true;
                config.OversampleH = 2;
                config.OversampleV = 1;
                config.RasterizerMultiply = face == TITLE && g_baked.faux ? 1.15f : 1.0f;
                const float size = fmaxf(6, SIZES[i] * s * chineseMetrics.pixelHeightPerEm);
                atlas.AddFontFromMemoryTTF(shared, (int)chinese.size(), size, &config, glyphs);
                // ImGui would copy a borrowed source. One configuration owns the shared buffer for all sizes.
                atlas.Sources.back().FontDataOwnedByAtlas = ownsData;
                ownsData = false;
            }
        }
        return atlas.Build();
    };

    const bool built = bake(ranges);
    bool ready = ranges && built && HasChineseGlyphs(glyphs.required.Data);
    if (ranges && !ready)
    {
        if (ranges != glyphs.required.Data)
            ready = bake(glyphs.required.Data) && HasChineseGlyphs(glyphs.required.Data);
        if (!ready) bake(nullptr);
    }
    g_baked.chinese = ready;
    io.FontDefault = g_baked.fonts[BODY][3];
    zh_cn::SetFontReady(ready);
    ++g_fontGeneration;
    return true;
}

float FontScale() { return g_baked.scale; }

uint64_t Id(const char* text, uint64_t salt)
{
    uint64_t h = 1469598103934665603ull ^ (salt * 1099511628211ull);
    for (const unsigned char* p = (const unsigned char*)text; *p; p++) { h ^= *p; h *= 1099511628211ull; }
    return h ? h : 1;
}
uint64_t Id(const std::string& text, uint64_t salt) { return Id(text.c_str(), salt); }

void Canvas::Begin(ImDrawList* drawList, float originX, float originY, float scale, float screenW, float screenH)
{
    list = drawList;
    ox = originX; oy = originY; s = scale;
    width = screenW / s; height = screenH / s;
    previous.swap(regions);
    regions.clear();
    splitter.Split(list, LAYERS);
    if (measuredAt != g_fontGeneration) { widths.clear(); measuredAt = g_fontGeneration; }
    if (widths.size() > 8192) widths.clear();
}

void Canvas::End()
{
    splitter.Merge(list);
    click = rightClick = wheelTarget = 0;
    wheel = 0;
}

void Canvas::Input(float mouseX, float mouseY, bool clicked, bool rightClicked, float wheelSteps)
{
    float ux, uy;
    ToUnits(mouseX, mouseY, &ux, &uy);
    hovered = 0;
    uint64_t scrolls = 0;
    for (size_t i = previous.size(); i-- > 0;)
    {
        const Region& r = previous[i];
        if (ux < r.x || ux >= r.x + r.w || uy < r.y || uy >= r.y + r.h) continue;
        if (r.scroll) { if (scrolls == 0) scrolls = r.id; continue; }
        if (hovered == 0) hovered = r.id;
    }
    click = clicked ? hovered : 0;
    clickedAny = false;
    rightClick = rightClicked ? hovered : 0;
    wheelTarget = wheelSteps != 0 ? scrolls : 0;
    wheel = wheelSteps;
}

void Canvas::Layer(int z) { splitter.SetCurrentChannel(list, z < 0 ? 0 : z >= LAYERS ? LAYERS - 1 : z); }

ImVec2 Canvas::Px(float x, float y) const { return ImVec2(floorf((ox + x) * s + 0.5f), floorf((oy + y) * s + 0.5f)); }

void Canvas::Rect(float x, float y, float w, float h, ImU32 colour, int z)
{
    if (w <= 0 || h <= 0) return;
    ImVec2 a = Px(x, y), b = Px(x + w, y + h);
    if (b.x <= a.x) b.x = a.x + 1;
    if (b.y <= a.y) b.y = a.y + 1;
    Layer(z);
    list->AddRectFilled(a, b, colour);
}

void Canvas::Frame(float x, float y, float w, float h, ImU32 colour, int z, float t)
{
    Rect(x, y, w, t, colour, z);
    Rect(x, y + h - t, w, t, colour, z);
    Rect(x, y + t, t, h - 2 * t, colour, z);
    Rect(x + w - t, y + t, t, h - 2 * t, colour, z);
}

void Canvas::Line(float x1, float y1, float x2, float y2, ImU32 colour, int z, float t)
{
    Layer(z);
    ImVec2 a = Px(x1, y1), b = Px(x2, y2);
    list->AddLine(a, b, colour, t * s);
    list->AddCircleFilled(a, t * s / 2, colour); /* round caps: the strokes of an arrow meet cleanly */
    list->AddCircleFilled(b, t * s / 2, colour);
}

void Canvas::Chevron(ImVec2 a, ImVec2 b, ImVec2 c, ImU32 colour, int z, float t)
{
    Layer(z);
    auto snap = [&](ImVec2 at) { ImVec2 p = Px(at.x, at.y); return ImVec2(floorf(p.x) + 0.5f, floorf(p.y) + 0.5f); };
    ImVec2 points[3] = {snap(a), snap(b), snap(c)};
    list->AddPolyline(points, 3, colour, ImDrawFlags_None, fmaxf(1.0f, roundf(t * s)));
}

static ImFont* FontFor(float size, Face face, float* pixels)
{
    int i = Nearest(size);
    ImFont* font = g_baked.fonts[face][i];
    if (font == nullptr) font = ImGui::GetFont();
    /* A size between the baked ones: the nearest font, scaled. */
    *pixels = font->FontSize * size / SIZES[i];
    return font;
}

float Canvas::Measure(const std::string& text, float size, Face face)
{
    std::string key = std::to_string((int)(size * 10)) + (face == TITLE ? "T" : "B") + text;
    auto found = widths.find(key);
    if (found != widths.end()) return found->second;
    float pixels;
    ImFont* font = FontFor(size, face, &pixels);
    TrackVisibleGlyphs(text, *font);
    float w = font->CalcTextSizeA(pixels, FLT_MAX, 0, text.c_str(), text.c_str() + text.size()).x / s;
    if (face == TITLE && g_baked.faux) w += 1.0f / s;
    widths[key] = w;
    return w;
}

std::string Canvas::Fit(const std::string& text, float max, float size, Face face)
{
    if (max <= 0) return "";
    if (Measure(text, size, face) <= max) return text;
    size_t lo = 0, hi = text.size();
    while (lo < hi)
    {
        size_t mid = (lo + hi + 1) / 2;
        if (Measure(text.substr(0, mid) + "...", size, face) <= max) lo = mid; else hi = mid - 1;
    }
    /* never cut inside a UTF-8 sequence */
    while (lo > 0 && lo < text.size() && ((unsigned char)text[lo] & 0xC0) == 0x80) lo--;
    std::string cut = text.substr(0, lo);
    while (!cut.empty() && cut.back() == ' ') cut.pop_back();
    return cut.empty() ? "" : cut + "...";
}

std::vector<std::string> Canvas::Wrap(const std::string& text, float max, float size)
{
    std::vector<std::string> lines;
    if (max <= 0) return lines;
    float pixels;
    ImFont* font = FontFor(size, BODY, &pixels);
    const char* at = text.data();
    const char* end = at + text.size();
    while (at < end)
    {
        const char* stop = (const char*)memchr(at, '\n', end - at);
        if (!stop) stop = end;
        if (at == stop) { ++at; continue; }
        const char* next = font->CalcWordWrapPositionA(pixels / font->FontSize, at, stop, max * s);
        if (next == at) ++next;
        while (next < stop && ((unsigned char)*next & 0xc0) == 0x80) ++next;
        const char* lineEnd = next;
        while (lineEnd > at && (lineEnd[-1] == ' ' || lineEnd[-1] == '\r')) --lineEnd;
        lines.emplace_back(at, lineEnd);
        at = next;
        while (at < end && (*at == ' ' || *at == '\r' || *at == '\n')) ++at;
    }
    return lines;
}

float Canvas::Text(const std::string& raw, float x, float cy, const TextOpts& o)
{
    std::string text = o.max > 0 ? Fit(raw, o.max, o.size, o.face) : raw;
    if (text.empty()) return 0;
    float w = Measure(text, o.size, o.face);
    if (o.align == RIGHT) x -= w;
    else if (o.align == CENTER) x -= w / 2;
    float pixels;
    ImFont* font = FontFor(o.size, o.face, &pixels);
    float ratio = pixels / font->FontSize;
    /* the baseline sits half a cap height below cy */
    float baseline = (oy + cy) * s + o.size * s * g_baked.metrics[o.face].capPerEm / 2;
    ImVec2 at(floorf((ox + x) * s + 0.5f), floorf(baseline - font->Ascent * ratio + 0.5f));
    Layer(o.z);
    list->AddText(font, pixels, at, o.colour, text.c_str(), text.c_str() + text.size());
    if (o.face == TITLE && g_baked.faux) list->AddText(font, pixels, ImVec2(at.x + 1, at.y), o.colour, text.c_str(), text.c_str() + text.size());
    return w;
}

void Canvas::Image(ImTextureID texture, ImVec2 at, ImVec2 size, ImVec2 uv0, ImVec2 uv1, ImU32 tint, int z)
{
    if (!texture || size.x <= 0 || size.y <= 0) return;
    Layer(z);
    list->AddImage(texture, ImVec2((ox + at.x) * s, (oy + at.y) * s), ImVec2((ox + at.x + size.x) * s, (oy + at.y + size.y) * s), uv0, uv1, tint);
}

bool Canvas::Hit(float x, float y, float w, float h, uint64_t id)
{
    regions.push_back({x, y, w, h, id, false});
    if (click != 0 && click == id) { click = 0; clickedAny = true; return true; }
    return false;
}

float Canvas::Scroll(float x, float y, float w, float h, uint64_t id)
{
    regions.push_back({x, y, w, h, id, true});
    if (wheelTarget != 0 && wheelTarget == id) { wheelTarget = 0; return wheel; }
    return 0;
}

void Canvas::Block() { regions.push_back({-10000, -10000, 20000, 20000, 1, false}); }
} // namespace ui
