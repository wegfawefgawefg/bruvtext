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
constexpr bool kRunStartupValidation = false;
constexpr bool kDumpFirstShowcaseFrame = false;
constexpr float kLanguageRailX = 980.0f;
constexpr float kLanguageRailY = 150.0f;
constexpr float kLanguageRailStep = 28.0f;
constexpr float kLanguageSlotTopOffset = 24.0f;
constexpr float kLanguageSlotHeight = 32.0f;

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

float MeasureLineAdvance(
    bruvtext::Context& context,
    bruvtext::FontId font,
    std::string_view probeText,
    float pixelSize,
    float scale,
    float fallbackMultiplier)
{
    const std::string_view probe = probeText.empty() ? std::string_view("Ag") : probeText;
    const bruvtext::TextSize measured = bruvtext::MeasureTextEx(context, font, probe, pixelSize, scale);
    if (measured.height <= 0.0f)
    {
        return std::max(1.0f, std::ceil(pixelSize * scale * fallbackMultiplier));
    }
    return std::max(1.0f, std::ceil(measured.height * fallbackMultiplier));
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
    bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
        .font = fonts.uiSans,
        .text = "bruvtext demo scaffold",
        .x = 24.0f,
        .y = 24.0f,
        .pixelSize = 28.0f,
    });
    bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
        .font = fonts.uiSans,
        .text = samples[0].title,
        .x = 24.0f,
        .y = 64.0f,
        .pixelSize = 18.0f,
    });
    bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
        .font = fonts.uiSans,
        .text = samples[1].title,
        .x = 24.0f,
        .y = 96.0f,
        .pixelSize = 22.0f,
    });
    bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
        .font = fonts.uiSans,
        .text = samples[2].title,
        .x = 24.0f,
        .y = 132.0f,
        .pixelSize = 28.0f,
    });
    if (includeLargeSpanish)
    {
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiSans,
            .text = samples[2].title,
            .x = 24.0f,
            .y = 228.0f,
            .pixelSize = 42.0f,
        });
    }
    bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
        .font = fonts.uiMono,
        .text = "0123456789 !?[]{}() <> +-=/",
        .x = 24.0f,
        .y = includeLargeSpanish ? 288.0f : 180.0f,
        .pixelSize = 20.0f,
    });
    std::cout << "queued text items:\n";
    const std::size_t count = bruvtext::GetQueuedTextCount(context);
    const bruvtext::QueuedTextView* items = bruvtext::GetQueuedTextItems(context);
    for (std::size_t i = 0; i < count; ++i)
    {
        std::cout << "  font=" << items[i].font
                  << " pos=(" << items[i].x << ", " << items[i].y << ")"
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
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiSans,
            .text = samples[i].language,
            .x = kLanguageRailX,
            .y = y,
            .pixelSize = active ? 22.0f : 16.0f,
            .colorR = active ? 1.0f : 0.72f,
            .colorG = active ? 0.98f : 0.74f,
            .colorB = active ? 0.92f : 0.76f,
            .colorA = active ? 1.0f : 0.72f,
        });
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
    const float titleLineStep = MeasureLineAdvance(
        context,
        sampleFont,
        sample.title,
        titlePixelSize,
        settings.featureScale,
        1.08f);
    const float bodyLineStep = MeasureLineAdvance(
        context,
        sampleFont,
        sample.bodyLines.empty() ? std::string_view("Ag") : std::string_view(sample.bodyLines.front()),
        bodyPixelSize,
        settings.featureScale,
        1.18f);
    bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
        .font = fonts.uiSans,
        .text = sample.language,
        .x = 54.0f,
        .y = 270.0f,
        .pixelSize = 18.0f,
        .colorR = 0.78f,
        .colorG = 0.72f,
        .colorB = 0.62f,
    });
    bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
        .font = sampleFont,
        .text = sample.title,
        .x = 54.0f,
        .y = 312.0f,
        .pixelSize = titlePixelSize,
        .scale = settings.featureScale,
    });

    float y = 312.0f + titleLineStep + std::max(8.0f, std::round(12.0f * settings.featureScale));
    for (const std::string& line : sample.bodyLines)
    {
        const std::vector<std::string> wrappedLines = WrapDemoLine(sample, line, bodyDisplaySize);
        for (const std::string& wrapped : wrappedLines)
        {
            bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
                .font = sampleFont,
                .text = wrapped,
                .x = 56.0f,
                .y = y,
                .pixelSize = bodyPixelSize,
                .scale = settings.featureScale,
                .colorR = 0.92f,
                .colorG = 0.90f,
                .colorB = 0.86f,
            });
            y += bodyLineStep;
        }
    }
}

void PrepareDisplayFrame(
    bruvtext::Context& context,
    const DemoFonts& fonts,
    const std::array<DemoSampleText, kShowcaseLanguageCount>& samples,
    std::size_t activeIndex,
    const DemoViewSettings& settings)
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
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiSans,
            .text = "bruvtext",
            .x = 52.0f,
            .y = 58.0f,
            .pixelSize = 40.0f,
        });
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiSans,
            .text = "HarfBuzz + FreeType + Vulkan demo",
            .x = 56.0f,
            .y = 94.0f,
            .pixelSize = 18.0f,
            .colorR = 0.80f,
            .colorG = 0.76f,
            .colorB = 0.70f,
        });
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiSans,
            .text = "Languages",
            .x = 980.0f,
            .y = 110.0f,
            .pixelSize = 18.0f,
            .colorR = 0.78f,
            .colorG = 0.72f,
            .colorB = 0.62f,
        });
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiMono,
            .text = "ABCDEFGHIJKLMNOPQRSTUVWXYZ  abcdefghijklmnopqrstuvwxyz  0123456789  !?[]{}<> +-=/",
            .x = 56.0f,
            .y = 174.0f,
            .pixelSize = 16.0f,
            .colorR = 0.92f,
            .colorG = 0.90f,
            .colorB = 0.84f,
        });
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiSans,
            .text = "Feature language",
            .x = 54.0f,
            .y = 232.0f,
            .pixelSize = 18.0f,
            .colorR = 0.78f,
            .colorG = 0.72f,
            .colorB = 0.62f,
        });
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiSans,
            .text = "Up/Down language   -/= scale   [/ ] raster size   C clear active font cache",
            .x = 56.0f,
            .y = 204.0f,
            .pixelSize = 14.0f,
            .colorR = 0.66f,
            .colorG = 0.69f,
            .colorB = 0.72f,
        });
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiSans,
            .text = scaleLine,
            .x = 56.0f,
            .y = 118.0f,
            .pixelSize = 14.0f,
            .colorR = 0.72f,
            .colorG = 0.74f,
            .colorB = 0.78f,
        });
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiSans,
            .text = memoryLine,
            .x = 56.0f,
            .y = 136.0f,
            .pixelSize = 14.0f,
            .colorR = 0.72f,
            .colorG = 0.74f,
            .colorB = 0.78f,
        });
        bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
            .font = fonts.uiSans,
            .text = bucketLine,
            .x = 56.0f,
            .y = 152.0f,
            .pixelSize = 14.0f,
            .colorR = 0.72f,
            .colorG = 0.74f,
            .colorB = 0.78f,
        });

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
            bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
                .font = fonts.uiSans,
                .text = "bruvtext showcase recovery",
                .x = 52.0f,
                .y = 58.0f,
                .pixelSize = 32.0f,
            });
            bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
                .font = fonts.uiSans,
                .text = "Active font atlas cache overflowed. Press C or lower raster size/scale.",
                .x = 56.0f,
                .y = 120.0f,
                .pixelSize = 18.0f,
                .colorR = 0.90f,
                .colorG = 0.78f,
                .colorB = 0.72f,
            });
            bruvtext::DrawTextEx(context, bruvtext::DrawTextCmd{
                .font = fonts.uiSans,
                .text = samples[activeIndex].language,
                .x = 56.0f,
                .y = 164.0f,
                .pixelSize = 20.0f,
                .colorR = 0.78f,
                .colorG = 0.72f,
                .colorB = 0.62f,
            });
            bruvtext::EndFrame(context);
        }
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
                          << " pos=(" << queuedItems[i].x << ", " << queuedItems[i].y << ")"
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
    const std::array<DemoSampleText, kShowcaseLanguageCount>& samples)
{
    bool running = true;
    const std::uint64_t startTicks = SDL_GetTicks();
    std::uint64_t rotationBaseTicks = startTicks;
    std::size_t activeLanguage = 0;
    DemoViewSettings settings{};
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
                bruvtext::ClearAtlasCacheForFont(
                    textContext,
                    PickSampleFont(fonts, samples[activeLanguage])
                );
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_LEFTBRACKET)
            {
                const float newPixelSize = std::max(8.0f, settings.featurePixelSize - 1.0f);
                if (newPixelSize != settings.featurePixelSize)
                {
                    settings.featurePixelSize = newPixelSize;
                    bruvtext::ClearAtlasCacheForFont(
                        textContext,
                        PickSampleFont(fonts, samples[activeLanguage])
                    );
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_RIGHTBRACKET)
            {
                const float newPixelSize = std::min(96.0f, settings.featurePixelSize + 1.0f);
                if (newPixelSize != settings.featurePixelSize)
                {
                    settings.featurePixelSize = newPixelSize;
                }
                bruvtext::ClearAtlasCacheForFont(
                    textContext,
                    PickSampleFont(fonts, samples[activeLanguage])
                );
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
        const std::uint64_t elapsedTicks = SDL_GetTicks() - rotationBaseTicks;
        const std::uint64_t stepTicks = static_cast<std::uint64_t>(kRotationSeconds * 1000.0f);
        if (stepTicks > 0 && elapsedTicks >= stepTicks)
        {
            const std::uint64_t steps = elapsedTicks / stepTicks;
            activeLanguage = (activeLanguage + static_cast<std::size_t>(steps)) % samples.size();
            rotationBaseTicks += steps * stepTicks;
        }

        PrepareDisplayFrame(textContext, fonts, samples, activeLanguage, settings);
        if (!demo::RenderVulkanDemoFrame(app.renderer, textContext, timeSeconds))
        {
            throw std::runtime_error("RenderVulkanDemoFrame failed");
        }
        if (app.window != nullptr)
        {
            const std::string title = BuildWindowTitle() + "  |  rotating language demo";
            SDL_SetWindowTitle(app.window, title.c_str());
        }
    }
}
}

int main()
{
    DemoApp app{};
    bruvtext::Context* textContext = nullptr;
    try
    {
        PrintStartupInfo();
        const std::array<DemoSampleText, kShowcaseLanguageCount> samples = LoadDemoSampleText();
        textContext = bruvtext::CreateContext({BRUVTEXT_ASSETS_ROOT});
        const DemoFonts fonts = RegisterDemoFonts(*textContext);
        if (kRunStartupValidation)
        {
            RunValidationFrames(*textContext, fonts, samples);
        }
        Initialize(app);
        RunLoop(app, *textContext, fonts, samples);
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
