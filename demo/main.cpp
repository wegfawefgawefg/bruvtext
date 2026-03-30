#include "bruvtext/bruvtext.h"
#include "vulkan_demo.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kX11DialogWindowType = "_NET_WM_WINDOW_TYPE_DIALOG";
constexpr const char* kArtifactsAtlasDir = "artifacts/text_atlas_cache";
constexpr const char* kArtifactsBenchmarkDir = "artifacts/benchmark";
constexpr bool kRunStartupValidation = false;
constexpr bool kDumpFirstShowcaseFrame = false;
constexpr float kLanguageRailX = 980.0f;
constexpr float kLanguageRailY = 150.0f;
constexpr float kLanguageRailStep = 28.0f;
constexpr float kLanguageSlotTopOffset = 24.0f;
constexpr float kLanguageSlotHeight = 32.0f;
constexpr std::size_t kBenchmarkTextCodepointLimit = 24;

struct DemoApp
{
    SDL_Window* window = nullptr;
    demo::VulkanDemo renderer;
};

struct DemoViewSettings
{
    float featureScale = 1.0f;
    float featurePixelSize = 22.0f;
};

enum class DemoMode
{
    Showcase,
    Benchmark,
};

struct DemoRuntimeStats
{
    std::size_t queuedTextCount = 0;
    std::size_t drawGlyphCount = 0;
    std::size_t drawBatchCount = 0;
    std::size_t submittedBatchCount = 0;
    std::uint32_t atlasPageCount = 0;
    std::uint64_t atlasMemoryBytes = 0;
    float prepareMs = 0.0f;
    float renderMs = 0.0f;
    float frameMs = 0.0f;
    float fps = 0.0f;
};

struct BenchmarkSettings
{
    enum class Layout
    {
        Stacked,
        Scroller,
    };

    int runCount = 800;
    int columnCount = 4;
    Layout layout = Layout::Stacked;
};

struct DemoCliOptions
{
    DemoMode initialMode = DemoMode::Showcase;
    int benchmarkRunCount = 800;
    BenchmarkSettings::Layout benchmarkLayout = BenchmarkSettings::Layout::Stacked;
    float benchmarkDurationSeconds = 0.0f;
    bool printUsage = false;
};

struct DemoSampleText
{
    std::string language;
    std::string title;
    std::vector<std::string> bodyLines;
    enum class FontKind
    {
        Sans,
        Cjk,
        Korean,
        Arabic,
        Devanagari,
    } fontKind = FontKind::Sans;
};

struct DemoTabRect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

constexpr std::size_t kShowcaseLanguageCount = bruvtext::kBuiltInDemoSampleCount;
constexpr float kRotationSeconds = 2.0f;

void CenterWindowOnPrimaryDisplay(SDL_Window* window)
{
    SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
    if (primaryDisplay == 0)
    {
        return;
    }

    SDL_Rect bounds{};
    if (!SDL_GetDisplayBounds(primaryDisplay, &bounds))
    {
        return;
    }

    const int x = bounds.x + (bounds.w - kWindowWidth) / 2;
    const int y = bounds.y + (bounds.h - kWindowHeight) / 2;
    SDL_SetWindowPosition(window, x, y);
}

std::string BuildWindowTitle()
{
    const bruvtext::Version version = bruvtext::GetVersion();
    std::string title = "bruvtext showcase ";
    title += std::to_string(version.major);
    title += ".";
    title += std::to_string(version.minor);
    title += ".";
    title += std::to_string(version.patch);
    return title;
}

void Require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string FirstNonEmptyLine(const std::string& text)
{
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty())
        {
            return line;
        }
    }
    return {};
}

std::vector<std::string> CollectNonEmptyLines(const std::string& text)
{
    std::istringstream stream(text);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(stream, line))
    {
        if (!line.empty())
        {
            lines.push_back(line);
        }
    }
    return lines;
}

const char* BenchmarkLayoutLabel(BenchmarkSettings::Layout layout)
{
    switch (layout)
    {
    case BenchmarkSettings::Layout::Stacked:
        return "stacked";
    case BenchmarkSettings::Layout::Scroller:
        return "scroller";
    }
    return "unknown";
}

bool ParseIntArg(const char* value, int& out)
{
    if (value == nullptr)
    {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0')
    {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

bool ParseFloatArg(const char* value, float& out)
{
    if (value == nullptr)
    {
        return false;
    }
    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    if (end == value || *end != '\0')
    {
        return false;
    }
    out = parsed;
    return true;
}

BenchmarkSettings::Layout ParseBenchmarkLayout(const char* value)
{
    if (value != nullptr && std::string_view(value) == "scroller")
    {
        return BenchmarkSettings::Layout::Scroller;
    }
    return BenchmarkSettings::Layout::Stacked;
}

void PrintUsage()
{
    std::cout
        << "usage: bruvtext-demo [--benchmark] [--benchmark-runs N] [--benchmark-layout stacked|scroller] [--benchmark-duration SEC]\n";
}

DemoCliOptions ParseCommandLine(int argc, char** argv)
{
    DemoCliOptions options{};
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            options.printUsage = true;
            return options;
        }
        if (arg == "--benchmark")
        {
            options.initialMode = DemoMode::Benchmark;
            continue;
        }
        if (arg == "--benchmark-runs" && i + 1 < argc)
        {
            int parsed = options.benchmarkRunCount;
            if (ParseIntArg(argv[++i], parsed))
            {
                options.benchmarkRunCount = std::clamp(parsed, 50, 20000);
            }
            continue;
        }
        if (arg == "--benchmark-layout" && i + 1 < argc)
        {
            options.benchmarkLayout = ParseBenchmarkLayout(argv[++i]);
            continue;
        }
        if (arg == "--benchmark-duration" && i + 1 < argc)
        {
            float parsed = options.benchmarkDurationSeconds;
            if (ParseFloatArg(argv[++i], parsed))
            {
                options.benchmarkDurationSeconds = std::max(parsed, 0.0f);
            }
            continue;
        }
    }
    return options;
}

void InitializeBenchmarkLog()
{
    std::filesystem::create_directories(kArtifactsBenchmarkDir);
    const std::filesystem::path path =
        std::filesystem::path(kArtifactsBenchmarkDir) / "benchmark_log.csv";
    std::ofstream output(path, std::ios::trunc);
    output << "frame_ticks,layout,run_count,queued,glyphs,library_batches,submitted_batches,atlas_pages,atlas_mem_bytes,prepare_ms,render_ms,frame_ms,fps,raster_px,scale\n";
}

void AppendBenchmarkLog(
    std::uint64_t frameTicks,
    const BenchmarkSettings& benchmark,
    const DemoRuntimeStats& stats,
    const DemoViewSettings& settings)
{
    const std::filesystem::path path =
        std::filesystem::path(kArtifactsBenchmarkDir) / "benchmark_log.csv";
    std::ofstream output(path, std::ios::app);
    output
        << frameTicks << ","
        << BenchmarkLayoutLabel(benchmark.layout) << ","
        << benchmark.runCount << ","
        << stats.queuedTextCount << ","
        << stats.drawGlyphCount << ","
        << stats.drawBatchCount << ","
        << stats.submittedBatchCount << ","
        << stats.atlasPageCount << ","
        << stats.atlasMemoryBytes << ","
        << stats.prepareMs << ","
        << stats.renderMs << ","
        << stats.frameMs << ","
        << stats.fps << ","
        << settings.featurePixelSize << ","
        << settings.featureScale
        << "\n";
}

void PrintStartupInfo()
{
    std::cout << "bruvtext-demo\n";
    std::cout << bruvtext::GetBuildInfo() << "\n";
    std::cout << "assets root: " << BRUVTEXT_ASSETS_ROOT << "\n";
    std::cout << "demo text samples:\n";
    for (std::size_t i = 0; i < bruvtext::GetBuiltInDemoTextSampleCount(); ++i)
    {
        const bruvtext::DemoTextSample& sample = bruvtext::GetBuiltInDemoTextSamples()[i];
        std::cout << "  " << sample.language << " -> " << sample.filePath << "\n";
    }
}

DemoSampleText LoadOneDemoSample(
    std::string language,
    const std::filesystem::path& path,
    DemoSampleText::FontKind fontKind = DemoSampleText::FontKind::Sans)
{
    const std::vector<std::string> lines = CollectNonEmptyLines(ReadTextFile(path));

    DemoSampleText sample{};
    sample.language = std::move(language);
    sample.fontKind = fontKind;
    if (!lines.empty())
    {
        sample.title = lines.front();
        for (std::size_t i = 1; i < lines.size(); ++i)
        {
            sample.bodyLines.push_back(lines[i]);
        }
    }
    return sample;
}

std::array<DemoSampleText, kShowcaseLanguageCount> LoadDemoSampleText()
{
    const std::filesystem::path assetsRoot = BRUVTEXT_ASSETS_ROOT;
    return {{
        LoadOneDemoSample("English", assetsRoot / "text/english.txt"),
        LoadOneDemoSample("French", assetsRoot / "text/french.txt"),
        LoadOneDemoSample("Spanish", assetsRoot / "text/spanish.txt"),
        LoadOneDemoSample("German", assetsRoot / "text/german.txt"),
        LoadOneDemoSample("Italian", assetsRoot / "text/italian.txt"),
        LoadOneDemoSample("Portuguese", assetsRoot / "text/portuguese.txt"),
        LoadOneDemoSample("Dutch", assetsRoot / "text/dutch.txt"),
        LoadOneDemoSample("Vietnamese", assetsRoot / "text/vietnamese.txt"),
        LoadOneDemoSample("Polish", assetsRoot / "text/polish.txt"),
        LoadOneDemoSample("Turkish", assetsRoot / "text/turkish.txt"),
        LoadOneDemoSample("Romanian", assetsRoot / "text/romanian.txt"),
        LoadOneDemoSample("Czech", assetsRoot / "text/czech.txt"),
        LoadOneDemoSample("Swedish", assetsRoot / "text/swedish.txt"),
        LoadOneDemoSample("Japanese", assetsRoot / "text/japanese.txt", DemoSampleText::FontKind::Cjk),
        LoadOneDemoSample("Simplified Chinese", assetsRoot / "text/simplified_chinese.txt", DemoSampleText::FontKind::Cjk),
        LoadOneDemoSample("Traditional Chinese", assetsRoot / "text/traditional_chinese.txt", DemoSampleText::FontKind::Cjk),
        LoadOneDemoSample("Korean", assetsRoot / "text/korean.txt", DemoSampleText::FontKind::Korean),
        LoadOneDemoSample("Arabic", assetsRoot / "text/arabic.txt", DemoSampleText::FontKind::Arabic),
        LoadOneDemoSample("Hindi", assetsRoot / "text/hindi.txt", DemoSampleText::FontKind::Devanagari),
    }};
}

struct DemoFonts
{
    bruvtext::FontId uiSans = 0;
    bruvtext::FontId uiMono = 0;
    bruvtext::FontId featureSans = 0;
    bruvtext::FontId featureCjk = 0;
    bruvtext::FontId featureKorean = 0;
    bruvtext::FontId featureArabic = 0;
    bruvtext::FontId featureDevanagari = 0;
};

DemoFonts RegisterDemoFonts(bruvtext::Context& context)
{
    const std::array<bruvtext::FontDesc, 8> fonts{{
        {"DejaVu Sans UI", "assets/fonts/DejaVuSans.ttf"},
        {"DejaVu Sans Mono", "assets/fonts/DejaVuSansMono.ttf"},
        {"DejaVu Sans Feature", "assets/fonts/DejaVuSans.ttf"},
        {"Droid Sans Fallback", "assets/fonts/DroidSansFallbackFull.ttf"},
        {"Noto Sans KR", "assets/fonts/NotoSansKR-Variable.ttf"},
        {"Noto Sans Arabic", "assets/fonts/NotoSansArabic-Variable.ttf"},
        {"Noto Sans Devanagari", "assets/fonts/NotoSansDevanagari-Variable.ttf"},
        {"Noto Sans Mono", "assets/fonts/NotoSansMono-Regular.ttf"},
    }};

    DemoFonts result{};
    std::cout << "registered fonts:\n";
    for (std::size_t i = 0; i < fonts.size(); ++i)
    {
        const bruvtext::FontDesc& font = fonts[i];
        const bruvtext::FontId id = bruvtext::RegisterFont(context, font);
        std::cout << "  id=" << id << "  " << font.debugName << " -> " << font.filePath << "\n";
        if (i == 0) result.uiSans = id;
        if (i == 1) result.uiMono = id;
        if (i == 2) result.featureSans = id;
        if (i == 3) result.featureCjk = id;
        if (i == 4) result.featureKorean = id;
        if (i == 5) result.featureArabic = id;
        if (i == 6) result.featureDevanagari = id;
    }
    return result;
}

bruvtext::FontId PickSampleFont(const DemoFonts& fonts, const DemoSampleText& sample)
{
    switch (sample.fontKind)
    {
        case DemoSampleText::FontKind::Cjk:
            return fonts.featureCjk != 0 ? fonts.featureCjk : fonts.featureSans;
        case DemoSampleText::FontKind::Korean:
            return fonts.featureKorean != 0
                ? fonts.featureKorean
                : (fonts.featureCjk != 0 ? fonts.featureCjk : fonts.featureSans);
        case DemoSampleText::FontKind::Arabic:
            return fonts.featureArabic != 0 ? fonts.featureArabic : fonts.featureSans;
        case DemoSampleText::FontKind::Devanagari:
            return fonts.featureDevanagari != 0 ? fonts.featureDevanagari : fonts.featureSans;
        case DemoSampleText::FontKind::Sans:
        default:
            return fonts.featureSans;
    }
}

bool IsUtf8ContinuationByte(unsigned char c)
{
    return (c & 0xC0u) == 0x80u;
}

std::size_t CountUtf8Codepoints(std::string_view text)
{
    std::size_t count = 0;
    for (unsigned char c : text)
    {
        if (!IsUtf8ContinuationByte(c))
        {
            ++count;
        }
    }
    return count;
}

std::string Utf8PrefixByCodepoints(std::string_view text, std::size_t codepointCount)
{
    if (codepointCount == 0)
    {
        return {};
    }

    std::size_t seen = 0;
    std::size_t end = 0;
    while (end < text.size())
    {
        unsigned char c = static_cast<unsigned char>(text[end]);
        if (!IsUtf8ContinuationByte(c))
        {
            if (seen == codepointCount)
            {
                break;
            }
            ++seen;
        }
        ++end;
    }
    return std::string(text.substr(0, end));
}

std::string TruncateUtf8Codepoints(std::string_view text, std::size_t maxCodepoints)
{
    if (CountUtf8Codepoints(text) <= maxCodepoints)
    {
        return std::string(text);
    }
    return Utf8PrefixByCodepoints(text, maxCodepoints);
}

std::vector<std::string> WrapByCodepointCount(std::string_view text, std::size_t maxCodepoints)
{
    std::vector<std::string> wrapped;
    std::string_view remaining = text;
    while (!remaining.empty())
    {
        const std::size_t total = CountUtf8Codepoints(remaining);
        if (total <= maxCodepoints)
        {
            wrapped.push_back(std::string(remaining));
            break;
        }

        std::string chunk = Utf8PrefixByCodepoints(remaining, maxCodepoints);
        wrapped.push_back(chunk);
        remaining.remove_prefix(chunk.size());
    }
    return wrapped;
}

std::vector<std::string> WrapByWords(std::string_view text, std::size_t maxCodepoints)
{
    std::vector<std::string> wrapped;
    std::istringstream stream{std::string(text)};
    std::string word;
    std::string current;
    while (stream >> word)
    {
        const std::size_t currentCount = CountUtf8Codepoints(current);
        const std::size_t wordCount = CountUtf8Codepoints(word);
        const std::size_t separator = current.empty() ? 0 : 1;
        if (!current.empty() && currentCount + separator + wordCount > maxCodepoints)
        {
            wrapped.push_back(current);
            current.clear();
        }

        if (!current.empty())
        {
            current += " ";
        }
        current += word;
    }

    if (!current.empty())
    {
        wrapped.push_back(current);
    }
    return wrapped;
}

std::vector<std::string> WrapDemoLine(
    const DemoSampleText& sample,
    std::string_view text,
    float bodySize = 22.0f)
{
    const float scale = std::max(bodySize, 8.0f) / 22.0f;
    switch (sample.fontKind)
    {
        case DemoSampleText::FontKind::Cjk:
        case DemoSampleText::FontKind::Korean:
            return WrapByCodepointCount(text, std::max<std::size_t>(10, static_cast<std::size_t>(30.0f / scale)));
        case DemoSampleText::FontKind::Arabic:
            return WrapByWords(text, std::max<std::size_t>(14, static_cast<std::size_t>(42.0f / scale)));
        case DemoSampleText::FontKind::Devanagari:
            return WrapByWords(text, std::max<std::size_t>(14, static_cast<std::size_t>(40.0f / scale)));
        case DemoSampleText::FontKind::Sans:
        default:
            return WrapByWords(text, std::max<std::size_t>(24, static_cast<std::size_t>(78.0f / scale)));
    }
}

bruvtext::Color MakeColor(float r, float g, float b, float a = 1.0f)
{
    return bruvtext::Color{r, g, b, a};
}

bruvtext::DrawTextCmd MakeDrawCmd(
    bruvtext::FontId font,
    std::string_view text,
    float x,
    float y,
    float pixelSize,
    bruvtext::Color color = {},
    float scale = 1.0f)
{
    return bruvtext::DrawTextCmd{
        .font = font,
        .text = text,
        .position = {x, y},
        .pixelSize = pixelSize,
        .scale = scale,
        .color = color,
    };
}

float MeasureDemoLineAdvance(
    bruvtext::Context& context,
    bruvtext::FontId font,
    std::string_view probeText,
    float pixelSize,
    float scale,
    float fallbackMultiplier)
{
    (void)probeText;
    const float measured = bruvtext::MeasureLineAdvance(context, font, pixelSize, scale);
    if (measured <= 0.0f)
    {
        return std::max(1.0f, std::ceil(pixelSize * scale * fallbackMultiplier));
    }
    return std::max(1.0f, std::ceil(measured * fallbackMultiplier));
}

std::string FormatMemoryMiB(std::uint64_t bytes)
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    out << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MiB";
    return out.str();
}

std::uint64_t GetAtlasMemoryUsageBytesForFont(const bruvtext::Context& context, bruvtext::FontId font)
{
    const std::size_t pageCount = bruvtext::GetAtlasPageCount(context);
    const bruvtext::AtlasPageView* pages = bruvtext::GetAtlasPages(context);
    std::uint64_t total = 0;
    for (std::size_t pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const bruvtext::AtlasPageView& page = pages[pageIndex];
        if (page.font == font)
        {
            total += static_cast<std::uint64_t>(page.width) * page.height * 4ull;
        }
    }
    return total;
}

std::uint32_t CountAtlasPagesForFont(const bruvtext::Context& context, bruvtext::FontId font)
{
    const std::size_t pageCount = bruvtext::GetAtlasPageCount(context);
    const bruvtext::AtlasPageView* pages = bruvtext::GetAtlasPages(context);
    std::uint32_t total = 0;
    for (std::size_t pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        if (pages[pageIndex].font == font)
        {
            ++total;
        }
    }
    return total;
}

std::uint32_t CountAtlasSizeBucketsForFont(const bruvtext::Context& context, bruvtext::FontId font)
{
    const std::size_t pageCount = bruvtext::GetAtlasPageCount(context);
    const bruvtext::AtlasPageView* pages = bruvtext::GetAtlasPages(context);
    std::vector<std::uint32_t> uniqueSizes;
    for (std::size_t pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const bruvtext::AtlasPageView& page = pages[pageIndex];
        if (page.font != font)
        {
            continue;
        }
        if (std::find(uniqueSizes.begin(), uniqueSizes.end(), page.pixelSize) == uniqueSizes.end())
        {
            uniqueSizes.push_back(page.pixelSize);
        }
    }
    return static_cast<std::uint32_t>(uniqueSizes.size());
}

void FillRuntimeStats(bruvtext::Context& context, DemoRuntimeStats& stats)
{
    const bruvtext::AtlasStats atlasStats = bruvtext::GetAtlasStats(context);
    stats.queuedTextCount = bruvtext::GetQueuedTextCount(context);
    stats.drawGlyphCount = bruvtext::GetDrawGlyphCount(context);
    stats.drawBatchCount = bruvtext::GetDrawBatchCount(context);
    stats.submittedBatchCount = stats.drawBatchCount;
    stats.atlasPageCount = atlasStats.pageCount;
    stats.atlasMemoryBytes = atlasStats.memoryBytes;
}

void DumpAtlasPages(const bruvtext::Context& context)
{
    std::filesystem::create_directories(kArtifactsAtlasDir);

    const std::size_t atlasPageCount = bruvtext::GetAtlasPageCount(context);
    const bruvtext::AtlasPageView* atlasPages = bruvtext::GetAtlasPages(context);
    for (std::size_t pageIndex = 0; pageIndex < atlasPageCount; ++pageIndex)
    {
        const bruvtext::AtlasPageView& page = atlasPages[pageIndex];
        SDL_Surface* surface = SDL_CreateSurfaceFrom(
            static_cast<int>(page.width),
            static_cast<int>(page.height),
            SDL_PIXELFORMAT_RGBA32,
            const_cast<std::uint8_t*>(page.pixels),
            static_cast<int>(page.rowPitch)
        );
        if (surface == nullptr)
        {
            continue;
        }

        const std::filesystem::path outputPath =
            std::filesystem::path(kArtifactsAtlasDir) /
            ("atlas_page_" + std::to_string(page.pageIndex) + ".bmp");
        SDL_SaveBMP(surface, outputPath.string().c_str());
        SDL_DestroySurface(surface);
    }
}

void QueueDemoText(
    bruvtext::Context& context,
    const DemoFonts& fonts,
    const std::array<DemoSampleText, kShowcaseLanguageCount>& samples,
    bool includeLargeSpanish)
{
    bruvtext::BeginFrame(context);
    bruvtext::DrawTextEx(context, MakeDrawCmd(fonts.uiSans, "bruvtext demo scaffold", 24.0f, 24.0f, 28.0f));
    bruvtext::DrawTextEx(context, MakeDrawCmd(fonts.uiSans, samples[0].title, 24.0f, 64.0f, 18.0f));
    bruvtext::DrawTextEx(context, MakeDrawCmd(fonts.uiSans, samples[1].title, 24.0f, 96.0f, 22.0f));
    bruvtext::DrawTextEx(context, MakeDrawCmd(fonts.uiSans, samples[2].title, 24.0f, 132.0f, 28.0f));
    if (includeLargeSpanish)
    {
        bruvtext::DrawTextEx(context, MakeDrawCmd(fonts.uiSans, samples[2].title, 24.0f, 228.0f, 42.0f));
    }
    bruvtext::DrawTextEx(
        context,
        MakeDrawCmd(
            fonts.uiMono,
            "0123456789 !?[]{}() <> +-=/",
            24.0f,
            includeLargeSpanish ? 288.0f : 180.0f,
            20.0f));
    std::cout << "queued text items:\n";
    const std::size_t count = bruvtext::GetQueuedTextCount(context);
    const bruvtext::QueuedTextView* items = bruvtext::GetQueuedTextItems(context);
    for (std::size_t i = 0; i < count; ++i)
    {
        std::cout << "  font=" << items[i].font
                  << " pos=(" << items[i].position.x << ", " << items[i].position.y << ")"
                  << " px=" << items[i].pixelSize
                  << " scale=" << items[i].scale
                  << " text=\"" << items[i].text << "\"\n";
    }

    if (!bruvtext::EndFrame(context))
    {
        std::cout << "shaping failed\n";
        return;
    }

    std::cout << "shaped runs:\n";
    const std::size_t runCount = bruvtext::GetShapedRunCount(context);
    const bruvtext::ShapedRunView* runs = bruvtext::GetShapedRuns(context);
    for (std::size_t runIndex = 0; runIndex < runCount; ++runIndex)
    {
        const bruvtext::ShapedRunView& run = runs[runIndex];
        std::cout << "  run text=\"" << run.text << "\" glyphs=" << run.glyphCount << "\n";
        const std::size_t previewCount = std::min<std::size_t>(run.glyphCount, 6);
        for (std::size_t glyphIndex = 0; glyphIndex < previewCount; ++glyphIndex)
        {
            const bruvtext::ShapedGlyphView& glyph = run.glyphs[glyphIndex];
            std::cout << "    glyph=" << glyph.glyphIndex
                      << " cluster=" << glyph.cluster
                      << " pos=(" << glyph.x << ", " << glyph.y << ")"
                      << " adv=(" << glyph.advanceX << ", " << glyph.advanceY << ")\n";
        }
    }

    const bruvtext::AtlasStats atlasStats = bruvtext::GetAtlasStats(context);
    std::cout << "atlas pages=" << atlasStats.pageCount
              << " cached glyphs=" << atlasStats.glyphCount << "\n";

    const std::size_t atlasPageCount = bruvtext::GetAtlasPageCount(context);
    const bruvtext::AtlasPageView* atlasPages = bruvtext::GetAtlasPages(context);
    for (std::size_t pageIndex = 0; pageIndex < atlasPageCount; ++pageIndex)
    {
        const bruvtext::AtlasPageView& page = atlasPages[pageIndex];
        std::cout << "  page=" << page.pageIndex
                  << " font=" << page.font
                  << " pixelSize=" << page.pixelSize
                  << " extent=" << page.width << "x" << page.height
                  << " dirty=" << (page.dirty ? "yes" : "no") << "\n";
    }

    const std::size_t drawGlyphCount = bruvtext::GetDrawGlyphCount(context);
    const bruvtext::DrawGlyphView* drawGlyphs = bruvtext::GetDrawGlyphs(context);
    std::cout << "draw glyphs=" << drawGlyphCount << "\n";
    for (std::size_t glyphIndex = 0; glyphIndex < std::min<std::size_t>(drawGlyphCount, 6); ++glyphIndex)
    {
        const bruvtext::DrawGlyphView& glyph = drawGlyphs[glyphIndex];
        std::cout << "  draw glyph=" << glyph.glyphIndex
                  << " page=" << glyph.atlasPage
                  << " rect=(" << glyph.x0 << ", " << glyph.y0
                  << ")-(" << glyph.x1 << ", " << glyph.y1 << ")"
                  << " uv=(" << glyph.u0 << ", " << glyph.v0
                  << ")-(" << glyph.u1 << ", " << glyph.v1 << ")\n";
    }

    const std::size_t drawBatchCount = bruvtext::GetDrawBatchCount(context);
    const bruvtext::DrawBatchView* drawBatches = bruvtext::GetDrawBatches(context);
    std::cout << "draw batches=" << drawBatchCount << "\n";
    for (std::size_t batchIndex = 0; batchIndex < drawBatchCount; ++batchIndex)
    {
        const bruvtext::DrawBatchView& batch = drawBatches[batchIndex];
        std::cout << "  batch page=" << batch.atlasPage
                  << " firstGlyph=" << batch.firstGlyph
                  << " glyphCount=" << batch.glyphCount << "\n";
    }

    DumpAtlasPages(context);
    bruvtext::ClearAtlasDirtyFlags(context);
}

void RunValidationFrames(
    bruvtext::Context& context,
    const DemoFonts& fonts,
    const std::array<DemoSampleText, kShowcaseLanguageCount>& samples)
{
    std::cout << "\nframe A: initial sample set\n";
    QueueDemoText(context, fonts, samples, false);

    std::cout << "\nframe B: identical sample set, should reuse cached glyphs\n";
    QueueDemoText(context, fonts, samples, false);

    std::cout << "\nframe C: adds a larger Spanish run, should grow size-keyed cache\n";
    QueueDemoText(context, fonts, samples, true);
}

void DrawLanguageTabs(
    const std::array<DemoSampleText, kShowcaseLanguageCount>& samples,
    std::size_t activeIndex,
    std::array<DemoTabRect, kShowcaseLanguageCount>* outRects)
{
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        const bool active = i == activeIndex;
        const float size = active ? 22.0f : 17.0f;
        const float y = kLanguageRailY + static_cast<float>(i) * kLanguageRailStep;
        if (outRects != nullptr)
        {
            DemoTabRect& rect = (*outRects)[i];
            rect.x = kLanguageRailX - 14.0f;
            rect.y = y - kLanguageSlotTopOffset;
            rect.w = 220.0f;
            rect.h = kLanguageSlotHeight;
        }
    }
}

void DrawLanguageTabsText(
    bruvtext::Context& context,
    const DemoFonts& fonts,
    const std::array<DemoSampleText, kShowcaseLanguageCount>& samples,
    std::size_t activeIndex)
{
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        const bool active = i == activeIndex;
        const float y = kLanguageRailY + static_cast<float>(i) * kLanguageRailStep;
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(
                fonts.uiSans,
                samples[i].language,
                kLanguageRailX,
                y,
                active ? 22.0f : 16.0f,
                active ? MakeColor(1.0f, 0.98f, 0.92f, 1.0f)
                       : MakeColor(0.72f, 0.74f, 0.76f, 0.72f)));
    }
}

std::size_t PickHoveredLanguage(
    const std::array<DemoSampleText, kShowcaseLanguageCount>& samples,
    float mouseX,
    float mouseY)
{
    std::array<DemoTabRect, kShowcaseLanguageCount> rects{};
    DrawLanguageTabs(samples, samples.size(), &rects);
    for (std::size_t i = 0; i < rects.size(); ++i)
    {
        const DemoTabRect& rect = rects[i];
        if (mouseX >= rect.x && mouseX <= rect.x + rect.w &&
            mouseY >= rect.y && mouseY <= rect.y + rect.h)
        {
            return i;
        }
    }
    return samples.size();
}

void DrawFeatureCard(
    bruvtext::Context& context,
    const DemoFonts& fonts,
    const DemoSampleText& sample,
    const DemoViewSettings& settings)
{
    const bruvtext::FontId sampleFont = PickSampleFont(fonts, sample);
    const float titlePixelSize = std::round(settings.featurePixelSize * (34.0f / 22.0f));
    const float bodyPixelSize = settings.featurePixelSize;
    const float bodyDisplaySize = bodyPixelSize * settings.featureScale;
    const float titleLineStep = MeasureDemoLineAdvance(
        context,
        sampleFont,
        sample.title,
        titlePixelSize,
        settings.featureScale,
        1.08f);
    const float bodyLineStep = MeasureDemoLineAdvance(
        context,
        sampleFont,
        sample.bodyLines.empty() ? std::string_view("Ag") : std::string_view(sample.bodyLines.front()),
        bodyPixelSize,
        settings.featureScale,
        1.18f);
    bruvtext::DrawTextEx(
        context,
        MakeDrawCmd(fonts.uiSans, sample.language, 54.0f, 270.0f, 18.0f, MakeColor(0.78f, 0.72f, 0.62f)));
    bruvtext::DrawTextEx(
        context,
        MakeDrawCmd(sampleFont, sample.title, 54.0f, 312.0f, titlePixelSize, {}, settings.featureScale));

    float y = 312.0f + titleLineStep + std::max(8.0f, std::round(12.0f * settings.featureScale));
    for (const std::string& line : sample.bodyLines)
    {
        const std::vector<std::string> wrappedLines = WrapDemoLine(sample, line, bodyDisplaySize);
        for (const std::string& wrapped : wrappedLines)
        {
            bruvtext::DrawTextEx(
                context,
                MakeDrawCmd(
                    sampleFont,
                    wrapped,
                    56.0f,
                    y,
                    bodyPixelSize,
                    MakeColor(0.92f, 0.90f, 0.86f),
                    settings.featureScale));
            y += bodyLineStep;
        }
    }
}

void PrepareDisplayFrame(
    bruvtext::Context& context,
    const DemoFonts& fonts,
    const std::array<DemoSampleText, kShowcaseLanguageCount>& samples,
    std::size_t activeIndex,
    const DemoViewSettings& settings,
    DemoRuntimeStats* outStats)
{
    const bruvtext::FontId activeFont = PickSampleFont(fonts, samples[activeIndex]);
    const bruvtext::AtlasStats atlasStats = bruvtext::GetAtlasStats(context);
    const std::uint64_t activeFontAtlasBytes = GetAtlasMemoryUsageBytesForFont(context, activeFont);
    const std::uint32_t activeFontPageCount = CountAtlasPagesForFont(context, activeFont);
    const std::uint32_t activeFontBucketCount = CountAtlasSizeBucketsForFont(context, activeFont);
    const std::string scaleLine =
        "Raster " + std::to_string(static_cast<int>(settings.featurePixelSize + 0.5f)) +
        " px   Scale " + std::to_string(static_cast<int>(settings.featureScale * 100.0f + 0.5f)) + "%";
    const std::string memoryLine =
        "Atlas memory total " + FormatMemoryMiB(atlasStats.memoryBytes) +
        "   active font " + FormatMemoryMiB(activeFontAtlasBytes);
    const std::string bucketLine =
        "Active font pages " + std::to_string(activeFontPageCount) +
        "   size buckets " + std::to_string(activeFontBucketCount);

    const auto queueShowcase = [&]() {
        bruvtext::BeginFrame(context);
        bruvtext::DrawTextEx(context, MakeDrawCmd(fonts.uiSans, "bruvtext", 52.0f, 58.0f, 40.0f));
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(
                fonts.uiSans,
                "HarfBuzz + FreeType + Vulkan demo",
                56.0f,
                94.0f,
                18.0f,
                MakeColor(0.80f, 0.76f, 0.70f)));
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(fonts.uiSans, "Languages", 980.0f, 110.0f, 18.0f, MakeColor(0.78f, 0.72f, 0.62f)));
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(
                fonts.uiMono,
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ  abcdefghijklmnopqrstuvwxyz  0123456789  !?[]{}<> +-=/",
                56.0f,
                174.0f,
                16.0f,
                MakeColor(0.92f, 0.90f, 0.84f)));
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(fonts.uiSans, "Feature language", 54.0f, 232.0f, 18.0f, MakeColor(0.78f, 0.72f, 0.62f)));
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(
                fonts.uiSans,
                "Up/Down language   -/= scale   [/ ] raster size   C clear active font cache",
                56.0f,
                204.0f,
                14.0f,
                MakeColor(0.66f, 0.69f, 0.72f)));
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(fonts.uiSans, scaleLine, 56.0f, 118.0f, 14.0f, MakeColor(0.72f, 0.74f, 0.78f)));
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(fonts.uiSans, memoryLine, 56.0f, 136.0f, 14.0f, MakeColor(0.72f, 0.74f, 0.78f)));
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(fonts.uiSans, bucketLine, 56.0f, 152.0f, 14.0f, MakeColor(0.72f, 0.74f, 0.78f)));

        DrawLanguageTabsText(context, fonts, samples, activeIndex);
        DrawFeatureCard(context, fonts, samples[activeIndex], settings);
    };

    queueShowcase();
    if (!bruvtext::EndFrame(context))
    {
        std::cerr << "showcase EndFrame failed for language: "
                  << samples[activeIndex].language << "\n";
        bruvtext::ClearAtlasCacheForFont(context, activeFont);
        queueShowcase();
        if (!bruvtext::EndFrame(context))
        {
            bruvtext::BeginFrame(context);
            bruvtext::DrawTextEx(context, MakeDrawCmd(fonts.uiSans, "bruvtext showcase recovery", 52.0f, 58.0f, 32.0f));
            bruvtext::DrawTextEx(
                context,
                MakeDrawCmd(
                    fonts.uiSans,
                    "Active font atlas cache overflowed. Press C or lower raster size/scale.",
                    56.0f,
                    120.0f,
                    18.0f,
                    MakeColor(0.90f, 0.78f, 0.72f)));
            bruvtext::DrawTextEx(
                context,
                MakeDrawCmd(
                    fonts.uiSans,
                    samples[activeIndex].language,
                    56.0f,
                    164.0f,
                    20.0f,
                    MakeColor(0.78f, 0.72f, 0.62f)));
            bruvtext::EndFrame(context);
        }
    }

    if (outStats != nullptr)
    {
        FillRuntimeStats(context, *outStats);
    }

    static bool dumpedShowcaseFrame = false;
    if constexpr (kDumpFirstShowcaseFrame)
    {
        if (!dumpedShowcaseFrame)
        {
            dumpedShowcaseFrame = true;
            std::cout << "\nshowcase frame debug\n";
            const std::size_t queuedCount = bruvtext::GetQueuedTextCount(context);
            const bruvtext::QueuedTextView* queuedItems = bruvtext::GetQueuedTextItems(context);
            for (std::size_t i = 0; i < queuedCount; ++i)
            {
                std::cout << "  queued[" << i << "]"
                          << " font=" << queuedItems[i].font
                          << " px=" << queuedItems[i].pixelSize
                          << " scale=" << queuedItems[i].scale
                          << " pos=(" << queuedItems[i].position.x << ", " << queuedItems[i].position.y << ")"
                          << " text=\"" << queuedItems[i].text << "\"\n";
            }

            const std::size_t pageCount = bruvtext::GetAtlasPageCount(context);
            const bruvtext::AtlasPageView* pages = bruvtext::GetAtlasPages(context);
            for (std::size_t i = 0; i < pageCount; ++i)
            {
                std::cout << "  page[" << i << "]"
                          << " font=" << pages[i].font
                          << " px=" << pages[i].pixelSize
                          << " dirty=" << (pages[i].dirty ? "yes" : "no")
                          << " size=" << pages[i].width << "x" << pages[i].height << "\n";
            }

            const std::size_t runCount = bruvtext::GetShapedRunCount(context);
            const bruvtext::ShapedRunView* runs = bruvtext::GetShapedRuns(context);
            const std::size_t drawGlyphCount = bruvtext::GetDrawGlyphCount(context);
            const bruvtext::DrawGlyphView* drawGlyphs = bruvtext::GetDrawGlyphs(context);
            std::size_t drawCursor = 0;
            for (std::size_t runIndex = 0; runIndex < runCount; ++runIndex)
            {
                const bruvtext::ShapedRunView& run = runs[runIndex];
                std::cout << "  run[" << runIndex << "]"
                          << " font=" << run.font
                          << " px=" << run.pixelSize
                          << " scale=" << run.scale
                          << " glyphs=" << run.glyphCount
                          << " text=\"" << run.text << "\"";
                if (drawCursor < drawGlyphCount)
                {
                    const std::size_t previewStart = drawCursor;
                    const std::size_t previewEnd = std::min<std::size_t>(drawCursor + std::min<std::size_t>(run.glyphCount, 8), drawGlyphCount);
                    std::cout << " pages=";
                    for (std::size_t gi = previewStart; gi < previewEnd; ++gi)
                    {
                        std::cout << drawGlyphs[gi].atlasPage;
                        if (gi + 1 < previewEnd)
                        {
                            std::cout << ",";
                        }
                    }
                }
                std::cout << "\n";
                drawCursor = std::min<std::size_t>(drawCursor + run.glyphCount, drawGlyphCount);
            }

            DumpAtlasPages(context);
        }
    }
}

void PrepareBenchmarkFrame(
    bruvtext::Context& context,
    const DemoFonts& fonts,
    const std::array<DemoSampleText, kShowcaseLanguageCount>& samples,
    std::size_t activeIndex,
    const DemoViewSettings& settings,
    const BenchmarkSettings& benchmark,
    const DemoRuntimeStats& previousStats,
    DemoRuntimeStats* outStats)
{
    const float rasterSize = settings.featurePixelSize;
    const float scale = settings.featureScale;
    const float lineAdvance = MeasureDemoLineAdvance(
        context,
        fonts.uiSans,
        "Ag",
        rasterSize,
        scale,
        1.10f);
    const int requestedRunCount = benchmark.runCount;
    const int activeRunCount = requestedRunCount;
    const std::string modeLine =
        "Mode benchmark (" + std::string(BenchmarkLayoutLabel(benchmark.layout)) +
        ")   B toggle mode   M layout   Up/Down language   -/= scale   [/ ] raster size   ,/. run count";
    const std::string loadLine =
        "Runs " + std::to_string(activeRunCount) +
        "/" + std::to_string(requestedRunCount) +
        "   Glyphs " + std::to_string(previousStats.drawGlyphCount) +
        "   Batches " + std::to_string(previousStats.submittedBatchCount);
    const std::string perfLine =
        "CPU " + std::to_string(static_cast<int>(previousStats.prepareMs + 0.5f)) +
        " ms   GPU/frame " + std::to_string(static_cast<int>(previousStats.renderMs + 0.5f)) +
        " ms   FPS " + std::to_string(static_cast<int>(previousStats.fps + 0.5f)) +
        "   Atlas pages " + std::to_string(previousStats.atlasPageCount) +
        "   Atlas mem " + FormatMemoryMiB(previousStats.atlasMemoryBytes);
    const float baseX = 48.0f;
    const float baseY = 196.0f;
    const float columnWidth = 280.0f;
    const float scrollerSpeed = 40.0f;

    bruvtext::BeginFrame(context);
    bruvtext::DrawTextEx(context, MakeDrawCmd(fonts.uiSans, "bruvtext benchmark", 52.0f, 58.0f, 40.0f));
    bruvtext::DrawTextEx(
        context,
        MakeDrawCmd(
            fonts.uiSans,
            "GPU text stress mode using the same public API and renderer path",
            56.0f,
            94.0f,
            18.0f,
            MakeColor(0.80f, 0.76f, 0.70f)));
    bruvtext::DrawTextEx(
        context,
        MakeDrawCmd(fonts.uiSans, modeLine, 56.0f, 120.0f, 14.0f, MakeColor(0.66f, 0.69f, 0.72f)));
    bruvtext::DrawTextEx(
        context,
        MakeDrawCmd(fonts.uiSans, loadLine, 56.0f, 140.0f, 14.0f, MakeColor(0.72f, 0.74f, 0.78f)));
    bruvtext::DrawTextEx(
        context,
        MakeDrawCmd(fonts.uiSans, perfLine, 56.0f, 156.0f, 14.0f, MakeColor(0.72f, 0.74f, 0.78f)));

    for (int runIndex = 0; runIndex < activeRunCount; ++runIndex)
    {
        const DemoSampleText& sample = samples[static_cast<std::size_t>(runIndex) % samples.size()];
        const bruvtext::FontId font = PickSampleFont(fonts, sample);
        const std::string_view sourceText =
            (runIndex % 3 == 0 || sample.bodyLines.empty())
            ? std::string_view(sample.title)
            : std::string_view(sample.bodyLines[static_cast<std::size_t>(runIndex) % sample.bodyLines.size()]);
        const std::string text = TruncateUtf8Codepoints(sourceText, kBenchmarkTextCodepointLimit);

        float x = baseX;
        float y = baseY;
        if (benchmark.layout == BenchmarkSettings::Layout::Stacked)
        {
            const std::size_t slotCount = static_cast<std::size_t>(std::max(1, benchmark.columnCount * 12));
            const std::size_t slot = static_cast<std::size_t>(runIndex) % slotCount;
            const std::size_t col = slot % static_cast<std::size_t>(benchmark.columnCount);
            const std::size_t row = slot / static_cast<std::size_t>(benchmark.columnCount);
            const std::size_t layer = static_cast<std::size_t>(runIndex) / slotCount;
            const float jitterX = static_cast<float>(static_cast<int>(layer % 7) - 3) * 0.6f;
            const float jitterY = static_cast<float>(static_cast<int>((layer / 7) % 5) - 2) * 0.6f;
            x = baseX + static_cast<float>(col) * columnWidth + jitterX;
            y = baseY + static_cast<float>(row) * lineAdvance + jitterY;
        }
        else
        {
            const float scrollOffset = std::fmod(
                static_cast<float>(SDL_GetTicks()) * (scrollerSpeed / 1000.0f),
                lineAdvance * static_cast<float>(std::max(activeRunCount, 1)));
            x = baseX + static_cast<float>(runIndex % 2) * 520.0f;
            y = baseY + static_cast<float>(runIndex) * lineAdvance - scrollOffset;
        }

        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(
                font,
                text,
                x,
                y,
                rasterSize,
                MakeColor(0.92f, 0.90f, 0.86f),
                scale));
    }

    if (!bruvtext::EndFrame(context))
    {
        bruvtext::ClearAtlasCache(context);
        bruvtext::BeginFrame(context);
        bruvtext::DrawTextEx(context, MakeDrawCmd(fonts.uiSans, "bruvtext benchmark recovery", 52.0f, 58.0f, 32.0f));
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(
                fonts.uiSans,
                "Benchmark load exceeded current cache or GPU limits. Lower run count or raster size.",
                56.0f,
                110.0f,
                18.0f,
                MakeColor(0.90f, 0.78f, 0.72f)));
        bruvtext::DrawTextEx(
            context,
            MakeDrawCmd(
                fonts.uiSans,
                "This mode still truncates each sample for repeatable cache pressure and draw-load testing.",
                56.0f,
                138.0f,
                16.0f,
                MakeColor(0.76f, 0.78f, 0.80f)));
        bruvtext::EndFrame(context);
    }

    if (outStats != nullptr)
    {
        FillRuntimeStats(context, *outStats);
    }
}

void Initialize(DemoApp& app)
{
    Require(SDL_Init(SDL_INIT_VIDEO), "SDL_Init(SDL_INIT_VIDEO) failed");
    SDL_SetHint(SDL_HINT_X11_WINDOW_TYPE, kX11DialogWindowType);

    const std::string title = BuildWindowTitle();
    app.window = SDL_CreateWindow(
        title.c_str(),
        kWindowWidth,
        kWindowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    Require(app.window != nullptr, "SDL_CreateWindow failed");
    CenterWindowOnPrimaryDisplay(app.window);
    Require(demo::InitializeVulkanDemo(app.renderer, app.window), "InitializeVulkanDemo failed");
}

void Shutdown(DemoApp& app)
{
    demo::ShutdownVulkanDemo(app.renderer);
    if (app.window != nullptr)
    {
        SDL_DestroyWindow(app.window);
        app.window = nullptr;
    }
    SDL_Quit();
}

void RunLoop(
    DemoApp& app,
    bruvtext::Context& textContext,
    const DemoFonts& fonts,
    const std::array<DemoSampleText, kShowcaseLanguageCount>& samples,
    const DemoCliOptions& options)
{
    bool running = true;
    const std::uint64_t startTicks = SDL_GetTicks();
    std::uint64_t rotationBaseTicks = startTicks;
    std::size_t activeLanguage = 0;
    DemoViewSettings settings{};
    BenchmarkSettings benchmark{};
    benchmark.runCount = options.benchmarkRunCount;
    benchmark.layout = options.benchmarkLayout;
    DemoRuntimeStats runtimeStats{};
    DemoMode mode = options.initialMode;
    std::uint64_t previousFrameTicks = SDL_GetTicks();
    std::uint64_t lastBenchmarkLogTicks = 0;
    const std::uint64_t benchmarkStartTicks = SDL_GetTicks();
    InitializeBenchmarkLog();
    while (running)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
                break;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
            {
                running = false;
                break;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_DOWN)
            {
                activeLanguage = (activeLanguage + 1) % samples.size();
                rotationBaseTicks = SDL_GetTicks();
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_UP)
            {
                activeLanguage = activeLanguage == 0 ? samples.size() - 1 : activeLanguage - 1;
                rotationBaseTicks = SDL_GetTicks();
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_MINUS)
            {
                const float newScale = std::max(0.60f, settings.featureScale - 0.10f);
                if (newScale != settings.featureScale)
                {
                    settings.featureScale = newScale;
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_EQUALS)
            {
                const float newScale = std::min(2.00f, settings.featureScale + 0.10f);
                if (newScale != settings.featureScale)
                {
                    settings.featureScale = newScale;
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_C)
            {
                if (mode == DemoMode::Benchmark)
                {
                    bruvtext::ClearAtlasCache(textContext);
                }
                else
                {
                    bruvtext::ClearAtlasCacheForFont(
                        textContext,
                        PickSampleFont(fonts, samples[activeLanguage])
                    );
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_LEFTBRACKET)
            {
                const float newPixelSize = std::max(8.0f, settings.featurePixelSize - 1.0f);
                if (newPixelSize != settings.featurePixelSize)
                {
                    settings.featurePixelSize = newPixelSize;
                    if (mode == DemoMode::Benchmark)
                    {
                        bruvtext::ClearAtlasCache(textContext);
                    }
                    else
                    {
                        bruvtext::ClearAtlasCacheForFont(
                            textContext,
                            PickSampleFont(fonts, samples[activeLanguage])
                        );
                    }
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_RIGHTBRACKET)
            {
                const float newPixelSize = std::min(96.0f, settings.featurePixelSize + 1.0f);
                if (newPixelSize != settings.featurePixelSize)
                {
                    settings.featurePixelSize = newPixelSize;
                }
                if (mode == DemoMode::Benchmark)
                {
                    bruvtext::ClearAtlasCache(textContext);
                }
                else
                {
                    bruvtext::ClearAtlasCacheForFont(
                        textContext,
                        PickSampleFont(fonts, samples[activeLanguage])
                    );
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_B)
            {
                mode = mode == DemoMode::Showcase ? DemoMode::Benchmark : DemoMode::Showcase;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_M)
            {
                benchmark.layout =
                    benchmark.layout == BenchmarkSettings::Layout::Stacked
                    ? BenchmarkSettings::Layout::Scroller
                    : BenchmarkSettings::Layout::Stacked;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_COMMA)
            {
                benchmark.runCount = std::max(50, benchmark.runCount - 50);
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_PERIOD)
            {
                benchmark.runCount = std::min(20000, benchmark.runCount + 50);
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION)
            {
                const std::size_t hoveredLanguage = PickHoveredLanguage(
                    samples,
                    event.motion.x,
                    event.motion.y);
                if (hoveredLanguage < samples.size() && hoveredLanguage != activeLanguage)
                {
                    activeLanguage = hoveredLanguage;
                    rotationBaseTicks = SDL_GetTicks();
                }
            }
        }

        const float timeSeconds = static_cast<float>(SDL_GetTicks() - startTicks) / 1000.0f;
        const std::uint64_t frameTicks = SDL_GetTicks();
        const float frameMs = static_cast<float>(frameTicks - previousFrameTicks);
        previousFrameTicks = frameTicks;
        runtimeStats.frameMs = frameMs;
        runtimeStats.fps = frameMs > 0.0f ? 1000.0f / frameMs : 0.0f;
        const std::uint64_t elapsedTicks = SDL_GetTicks() - rotationBaseTicks;
        const std::uint64_t stepTicks = static_cast<std::uint64_t>(kRotationSeconds * 1000.0f);
        if (mode == DemoMode::Showcase && stepTicks > 0 && elapsedTicks >= stepTicks)
        {
            const std::uint64_t steps = elapsedTicks / stepTicks;
            activeLanguage = (activeLanguage + static_cast<std::size_t>(steps)) % samples.size();
            rotationBaseTicks += steps * stepTicks;
        }
        if (mode == DemoMode::Benchmark && options.benchmarkDurationSeconds > 0.0f)
        {
            const float benchmarkElapsedSeconds =
                static_cast<float>(frameTicks - benchmarkStartTicks) / 1000.0f;
            if (benchmarkElapsedSeconds >= options.benchmarkDurationSeconds)
            {
                running = false;
                continue;
            }
        }

        const std::uint64_t prepareStartTicks = SDL_GetTicks();
        if (mode == DemoMode::Showcase)
        {
            PrepareDisplayFrame(textContext, fonts, samples, activeLanguage, settings, &runtimeStats);
        }
        else
        {
            PrepareBenchmarkFrame(textContext, fonts, samples, activeLanguage, settings, benchmark, runtimeStats, &runtimeStats);
        }
        const std::uint64_t prepareEndTicks = SDL_GetTicks();
        runtimeStats.prepareMs = static_cast<float>(prepareEndTicks - prepareStartTicks);

        const std::uint64_t renderStartTicks = SDL_GetTicks();
        if (!demo::RenderVulkanDemoFrame(
                app.renderer,
                textContext,
                timeSeconds,
                mode == DemoMode::Benchmark))
        {
            throw std::runtime_error("RenderVulkanDemoFrame failed");
        }
        const std::uint64_t renderEndTicks = SDL_GetTicks();
        runtimeStats.renderMs = static_cast<float>(renderEndTicks - renderStartTicks);
        runtimeStats.submittedBatchCount = app.renderer.cpuBatches.size();
        if (mode == DemoMode::Benchmark && frameTicks - lastBenchmarkLogTicks >= 250)
        {
            AppendBenchmarkLog(frameTicks, benchmark, runtimeStats, settings);
            lastBenchmarkLogTicks = frameTicks;
        }
        if (app.window != nullptr)
        {
            const std::string title = BuildWindowTitle() +
                (mode == DemoMode::Showcase ? "  |  rotating language demo" : "  |  benchmark mode");
            SDL_SetWindowTitle(app.window, title.c_str());
        }
    }

    if (mode == DemoMode::Benchmark)
    {
        std::cout
            << "benchmark-summary"
            << " layout=" << BenchmarkLayoutLabel(benchmark.layout)
            << " runs=" << benchmark.runCount
            << " queued=" << runtimeStats.queuedTextCount
            << " glyphs=" << runtimeStats.drawGlyphCount
            << " library_batches=" << runtimeStats.drawBatchCount
            << " submitted_batches=" << runtimeStats.submittedBatchCount
            << " atlas_pages=" << runtimeStats.atlasPageCount
            << " atlas_mem_bytes=" << runtimeStats.atlasMemoryBytes
            << " prepare_ms=" << runtimeStats.prepareMs
            << " render_ms=" << runtimeStats.renderMs
            << " frame_ms=" << runtimeStats.frameMs
            << " fps=" << runtimeStats.fps
            << "\n";
    }
}
}

int main(int argc, char** argv)
{
    DemoApp app{};
    bruvtext::Context* textContext = nullptr;
    try
    {
        const DemoCliOptions options = ParseCommandLine(argc, argv);
        if (options.printUsage)
        {
            PrintUsage();
            return EXIT_SUCCESS;
        }
        PrintStartupInfo();
        const std::array<DemoSampleText, kShowcaseLanguageCount> samples = LoadDemoSampleText();
        textContext = bruvtext::CreateContext({BRUVTEXT_ASSETS_ROOT});
        const DemoFonts fonts = RegisterDemoFonts(*textContext);
        if (kRunStartupValidation)
        {
            RunValidationFrames(*textContext, fonts, samples);
        }
        Initialize(app);
        RunLoop(app, *textContext, fonts, samples, options);
        bruvtext::DestroyContext(textContext);
        Shutdown(app);
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: " << e.what() << "\n";
        if (textContext != nullptr)
        {
            bruvtext::DestroyContext(textContext);
        }
        Shutdown(app);
        return EXIT_FAILURE;
    }
}
