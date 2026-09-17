#include "settings.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace self_ring {
namespace {
float Clamp(float value, float low, float high, float fallback) {
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}
}
void Settings::Normalize() {
    const Settings defaults;
    radius = Clamp(radius, 20, 200, defaults.radius);
    thickness = Clamp(thickness, 1, 12, defaults.thickness);
    outline = Clamp(outline, 0, 6, defaults.outline);
    opacity = Clamp(opacity, 0.1f, 1, defaults.opacity);
    for (size_t i=0; i<3; ++i) {
        colour[i] = Clamp(colour[i], 0, 1, defaults.colour[i]);
        outlineColour[i] = Clamp(outlineColour[i], 0, 1, defaults.outlineColour[i]);
    }
}
Settings Settings::Parse(const std::string& text) {
    Settings result;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        const auto split = line.find('=');
        if (split == std::string::npos) continue;
        const auto key = line.substr(0, split);
        std::istringstream value(line.substr(split+1));
        value.imbue(std::locale::classic());
        float number;
        if (!(value >> number) || !std::isfinite(number)) continue;
        value >> std::ws;
        if (!value.eof()) continue;
        if (key == "enabled" && (number == 0 || number == 1)) result.enabled = number != 0;
        else if (key == "radius") result.radius = number;
        else if (key == "thickness") result.thickness = number;
        else if (key == "outline") result.outline = number;
        else if (key == "opacity") result.opacity = number;
        for (size_t i=0; i<3; ++i) {
            if (key == "colour"+std::to_string(i)) result.colour[i] = number;
            if (key == "outlineColour"+std::to_string(i)) result.outlineColour[i] = number;
        }
    }
    result.Normalize();
    return result;
}
std::string Settings::Serialize() const {
    Settings safe = *this;
    safe.Normalize();
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(9) << "version=1\nenabled=" << safe.enabled
           << "\nradius=" << safe.radius << "\nthickness=" << safe.thickness
           << "\noutline=" << safe.outline << "\nopacity=" << safe.opacity << '\n';
    for (size_t i=0; i<3; ++i)
        output << "colour" << i << '=' << safe.colour[i] << '\n'
               << "outlineColour" << i << '=' << safe.outlineColour[i] << '\n';
    return output.str();
}
bool LoadSettings(const std::filesystem::path& path, Settings& settings) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    // Settings should be tiny. Reject oversized files instead of allocating without a bound.
    std::string content(8192, '\0');
    input.read(content.data(), static_cast<std::streamsize>(content.size()));
    if (input.bad() || !input.eof()) return false;
    content.resize(static_cast<size_t>(input.gcount()));
    settings = Settings::Parse(content);
    return true;
}
bool SaveSettings(const std::filesystem::path& path, const Settings& settings) {
    if (path.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output << settings.Serialize();
        output.close();
        if (!output) return false;
    }
#ifdef _WIN32
    // Replace only after the full new configuration was written successfully.
    return MoveFileExW(temporary.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temporary, path, error);
    return !error;
#endif
}
} // namespace self_ring
