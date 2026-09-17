#pragma once

#include <array>
#include <filesystem>
#include <string>

namespace self_ring {
struct Settings {
    bool enabled = true;
    float radius = 40.0f;
    float thickness = 3.0f;
    float outline = 2.5f;
    float opacity = 1.0f;
    std::array<float, 3> colour{0.0f, 0.95f, 1.0f};
    std::array<float, 3> outlineColour{0.0f, 0.0f, 0.0f};

    void Normalize();
    static Settings Parse(const std::string& text);
    std::string Serialize() const;
};
bool LoadSettings(const std::filesystem::path& path, Settings& settings);
bool SaveSettings(const std::filesystem::path& path, const Settings& settings);
} // namespace self_ring
