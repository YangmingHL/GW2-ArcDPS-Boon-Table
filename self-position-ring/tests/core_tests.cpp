#include "ring_math.h"
#include "settings.h"

#include <cstdlib>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace self_ring;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool Near(float a, float b) { return std::abs(a-b) < 0.01f; }

int main() {
    try {
        Camera camera;
        Require(camera.Configure({0,0,0}, {0,0,1}, {0,1,0}, kPi/2, 1920, 1080), "camera setup");
        Vec2 point;
        Require(camera.Project({0,0,10}, point) && Near(point.x,960) && Near(point.y,540), "centre projection");
        Require(camera.Project({1,0,10}, point) && Near(point.x,1014), "right direction and aspect ratio");
        Require(camera.Project({0,1,10}, point) && Near(point.y,486), "vertical direction");
        Require(!camera.Project({0,0,-1}, point), "behind-camera point must be hidden");
        Vec2 a,b;
        Require(camera.Segment({0,0,-1},{1,0,10},a,b), "near-plane crossing must be clipped");
        Require(std::isfinite(a.x) && std::isfinite(b.x), "clipped line must stay finite");
        Require(!camera.Segment({0,0,-1},{1,0,-10},a,b), "behind-camera segment must be hidden");
        Require(!camera.Configure({}, {}, {0,1,0}, kPi/2, 1920,1080), "zero facing");
        Require(!camera.Configure({}, {0,0,1}, {0,1,0}, 0, 1920,1080), "invalid FOV");
        Require(!camera.Configure({}, {0,0,1}, {0,1,0}, kPi/2, 0,1080), "zero viewport");
        Require(camera.Configure({100,5,-20}, {0,0,1}, {0,1,0}, kPi/2,1920,1080), "translated camera");
        Require(camera.Project({100,5,-10},point) && Near(point.x,960), "translated camera centre");
        Require(camera.Configure({}, {1,0,0}, {0,1,0}, kPi/2,1920,1080), "rotated camera");
        Require(camera.Project({10,0,0},point) && Near(point.x,960), "rotated camera centre");

        auto settings = Settings::Parse("radius=999\nthickness=-1\noutline=99\nopacity=0\ncolour0=-5\ncolour1=2\nenabled=0\n");
        Require(settings.radius == 200 && settings.thickness == 1 && settings.outline == 6, "bounded geometry");
        Require(Near(settings.opacity,0.1f) && settings.colour[0] == 0 && settings.colour[1] == 1, "bounded colours");
        Require(!settings.enabled, "saved visibility");
        settings = Settings::Parse("radius=nan\nthickness=oops\noutline=2garbage\nenabled=9\n");
        Require(settings.radius == 40 && settings.thickness == 3 && settings.outline == 2.5f && settings.enabled, "invalid settings defaults");
        settings.radius = 123.5f;
        settings.colour = {0.123f,0.456f,0.789f};
        settings.outlineColour = {0.7f,0.2f,0.3f};
        Require(Settings::Parse(settings.Serialize()).Serialize() == settings.Serialize(), "lossless settings round trip");
        auto invalid = settings;
        invalid.radius = std::numeric_limits<float>::infinity();
        invalid.Normalize();
        Require(invalid.radius == 40, "non-finite fallback");

        const auto root = std::filesystem::temp_directory_path()/
            ("self-ring-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto file = root/std::filesystem::u8path("中文路径")/"settings.ini";
        Require(SaveSettings(file, settings), "save to Unicode path");
        Settings loaded;
        Require(LoadSettings(file,loaded) && loaded.Serialize()==settings.Serialize(), "load from disk");
        settings.radius = 88;
        Require(SaveSettings(file, settings), "atomic replacement");
        Require(LoadSettings(file,loaded) && loaded.radius==88, "load replacement");
        Require(!SaveSettings(file/"blocked.ini",settings), "write failure must be reported");
        Require(LoadSettings(file,loaded) && loaded.radius==88, "failed save preserves existing settings");
        std::filesystem::remove(file);
        std::filesystem::remove(file.parent_path());
        std::filesystem::remove(root);
        std::cout << "PASS: projection, near-plane clipping, invalid data, settings bounds, Unicode paths and atomic persistence\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
