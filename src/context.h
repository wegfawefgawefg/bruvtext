#pragma once

#include <filesystem>
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace bruvtext
{
struct Context
{
    std::string assetsRoot;
    std::filesystem::path assetsRootPath;
    FT_Library ftLibrary = nullptr;
};
}
