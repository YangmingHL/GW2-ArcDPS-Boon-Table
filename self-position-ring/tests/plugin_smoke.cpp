#include <Windows.h>
#include <Nexus.h>
#include <Mumble.h>
#include <RTAPI.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include "settings.h"
#include "ui_text.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
GUI_RENDER render{}, options{}, shortcut{};
bool* panelVisible{};
NexusLinkData_t nexus{1920, 1080, 1, false, false, true};
Mumble::Data mumble{};
Mumble::Identity identity{};
RTAPI::RealTimeData rt{};
bool useRealtime{};
std::map<std::string, EVENT_CONSUME> events;
std::vector<std::string> texts;
std::string settingsFilename;
void* fontData{};
DWORD fontSize{};
ImFontConfig fontConfig;
FONTS_RECEIVECALLBACK fontCallback{};
ImFont* font{};
int releases{};

void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void* Allocate(size_t size, void*) { return std::malloc(size); }
void Free(void* data, void*) { std::free(data); }
void Register(ERenderType type, GUI_RENDER callback) {
    if (type == RT_Render) render = callback;
    if (type == RT_OptionsRender) options = callback;
}
void Deregister(GUI_RENDER callback) {
    if (render == callback) render = nullptr;
    if (options == callback) options = nullptr;
}
const char* Directory(const char*) { return settingsFilename.c_str(); }
void* Resource(const char* id) {
    if (std::string(id) == DL_NEXUS_LINK) return &nexus;
    if (std::string(id) == DL_MUMBLE_LINK) return &mumble;
    if (std::string(id) == DL_MUMBLE_LINK_IDENTITY) return &identity;
    if (std::string(id) == "RTAPI" && useRealtime) return &rt;
    return nullptr;
}
void Log(ELogLevel, const char*, const char* text) { std::cerr << text << '\n'; }
void AddFont(const char*, float, void* data, uint64_t size, FONTS_RECEIVECALLBACK callback, void* cfg) {
    Require(data != nullptr && size > 0, "embedded font missing");
    fontData = data;
    fontSize = static_cast<DWORD>(size);
    fontConfig = *static_cast<ImFontConfig*>(cfg);
    fontConfig.FontDataOwnedByAtlas = false;
    fontCallback = callback;
}
void ReleaseFont(const char*, FONTS_RECEIVECALLBACK callback) {
    Require(callback == fontCallback, "font release mismatch");
    callback("font", nullptr);
    ++releases;
}
void Localize(const char*, const char*, const char* text) { texts.emplace_back(text); }
void Subscribe(const char* id, EVENT_CONSUME callback) { events[id] = callback; }
void Unsubscribe(const char* id, EVENT_CONSUME) { events.erase(id); }
void AddShortcut(const char*, const char*, GUI_RENDER callback) { shortcut = callback; }
void RemoveShortcut(const char*) { shortcut = nullptr; }
void RegisterEscape(const char*, bool* visible) { panelVisible = visible; }
void RemoveEscape(const char*) { panelVisible = nullptr; }
void RegisterKey(const char*, INPUTBINDS_PROCESS, const char*) {}
void RemoveKey(const char*) {}

struct FrameResult { int vertices; float meanX; };
FrameResult Frame(bool showOptions = false) {
    ImGui::NewFrame();
    render();
    if (showOptions) {
        ImGui::Begin("Nexus Options");
        options();
        ImGui::End();
    }
    ImGui::Render();
    const auto* data = ImGui::GetDrawData();
    double x = 0;
    for (int i=0; i<data->CmdListsCount; ++i)
        for (const auto& vertex : data->CmdLists[i]->VtxBuffer) {
            Require(std::isfinite(vertex.pos.x) && std::isfinite(vertex.pos.y), "non-finite render vertex");
            x += vertex.pos.x;
        }
    return {data->TotalVtxCount, data->TotalVtxCount ? static_cast<float>(x/data->TotalVtxCount) : 0};
}
}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "pass DLL path");
        const auto root = std::filesystem::temp_directory_path()/
            ("self-ring-dll-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        settingsFilename = (root/std::filesystem::u8path("中文")/"settings.ini").u8string();
        const auto dllPath = std::filesystem::u8path(argv[1]);
        HMODULE library = LoadLibraryW(dllPath.c_str());
        Require(library != nullptr, "DLL could not load");
        auto getDefinition = reinterpret_cast<AddonDefinition_t* (*)()>(GetProcAddress(library, "GetAddonDef"));
        Require(getDefinition != nullptr, "GetAddonDef export missing");
        auto* definition = getDefinition();
        Require(definition->APIVersion == 6 && definition->Signature != 31, "wrong Nexus identity");
        Require(FindResourceW(library, MAKEINTRESOURCEW(102), RT_RCDATA) != nullptr, "licenses resource missing");

        ImGui::SetAllocatorFunctions(Allocate, Free);
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.DisplaySize = {1920,1080};
        io.DeltaTime = 1.0f/60;
        io.IniFilename = nullptr;
        io.Fonts->AddFontDefault();
        AddonAPI_t api{};
        api.ImguiContext = ImGui::GetCurrentContext();
        api.ImguiMalloc = reinterpret_cast<void*>(Allocate);
        api.ImguiFree = reinterpret_cast<void*>(Free);
        api.GUI_Register = Register;
        api.GUI_Deregister = Deregister;
        api.Paths_GetAddonDirectory = Directory;
        api.DataLink_Get = Resource;
        api.Log = Log;
        api.Fonts_AddFromMemory = AddFont;
        api.Fonts_Release = ReleaseFont;
        api.Localization_Set = Localize;
        api.Events_Subscribe = Subscribe;
        api.Events_Unsubscribe = Unsubscribe;
        api.QuickAccess_AddContextMenu = AddShortcut;
        api.QuickAccess_RemoveContextMenu = RemoveShortcut;
        api.GUI_RegisterCloseOnEscape = RegisterEscape;
        api.GUI_DeregisterCloseOnEscape = RemoveEscape;
        api.InputBinds_RegisterWithString = RegisterKey;
        api.InputBinds_Deregister = RemoveKey;

        mumble.UITick = 1;
        mumble.AvatarPosition = {0,0,10};
        mumble.CameraPosition = {0,4,0};
        mumble.CameraFront = {0,-0.3f,1};
        mumble.CameraTop = {0,1,0};
        mumble.Context.MapID = 38;
        mumble.Context.MapType = Mumble::EMapType::WvW_EternalBattlegrounds;
        identity.FOV = 1.0f;
        definition->Load(&api);
        Require(render && options && shortcut && panelVisible && *panelVisible, "callbacks or initial settings panel missing");

        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        for (const auto& text : texts) builder.AddText(text.c_str());
        ImVector<ImWchar> ranges;
        builder.BuildRanges(&ranges);
        font = io.Fonts->AddFontFromMemoryTTF(fontData, static_cast<int>(fontSize), 18, &fontConfig, ranges.Data);
        Require(io.Fonts->Build() && font, "Chinese font atlas could not build");
        for (const auto* text : self_ring::ui::All) {
            for (const char* p=text; *p;) {
                unsigned int codepoint;
                const auto count = ImTextCharFromUtf8(&codepoint,p,nullptr);
                Require(count > 0, "invalid UI UTF-8");
                p += count;
                if (codepoint > 32) Require(font->FindGlyphNoFallback(static_cast<ImWchar>(codepoint)), "UI glyph missing");
            }
        }
        fontCallback("font", font);
        Require(Frame(true).vertices > 0, "Chinese settings render failed");
        *panelVisible = false;
        const auto normal = Frame();
        Require(normal.vertices > 500, "WvW ring not rendered");
        mumble.AvatarPosition.X = 2;
        Require(Frame().meanX > normal.meanX+100, "ring did not follow latest position");
        mumble.Context.IsMapOpen = true;
        Require(Frame().vertices == 0, "ring visible over world map");
        mumble.Context.IsMapOpen = false;
        nexus.IsGameplay = false;
        Require(Frame().vertices == 0, "ring visible while loading");
        nexus.IsGameplay = true;
        identity.FOV = std::numeric_limits<float>::quiet_NaN();
        Require(Frame().vertices == 0, "invalid FOV not rejected");
        identity.FOV = 1;
        mumble.AvatarPosition = {0,0,-20};
        Require(Frame().vertices == 0, "ring behind camera should be hidden");
        mumble.AvatarPosition = {0,0,10};

        rt.GameBuild = 1;
        rt.GameState = RTAPI::EGameState::Gameplay;
        rt.MapID = 38;
        rt.CameraPosition[1] = 4;
        rt.CameraFacing[1] = -0.3f;
        rt.CameraFacing[2] = 1;
        rt.CameraFOV = 1;
        rt.CharacterPosition[0] = 2;
        rt.CharacterPosition[2] = 10;
        useRealtime = true;
        uint32_t rtSignature = 620863532;
        events.at(EV_ADDON_LOADED)(&rtSignature);
        Require(Frame().meanX > normal.meanX+100, "optional RTAPI not used");
        rt.MapID = 99;
        Require(std::abs(Frame().meanX-normal.meanX) < 2, "stale RTAPI map should fall back");
        rt.MapID = 38;
        rt.CameraFacing[1] = rt.CameraFacing[2] = 0;
        Require(std::abs(Frame().meanX-normal.meanX) < 2, "invalid RTAPI camera should fall back");
        events.at(EV_ADDON_UNLOADED)(&rtSignature);
        Require(Frame().vertices > 500, "Mumble fallback after RTAPI unload");
        fontCallback("font", nullptr);
        Require(Frame(true).vertices > 0, "font rebuild null callback caused render failure");
        fontCallback("font", font);

        definition->Unload();
        Require(!render && !options && !shortcut && !panelVisible && events.empty() && releases==1, "callbacks not released");
        self_ring::Settings saved;
        Require(self_ring::LoadSettings(std::filesystem::u8path(settingsFilename),saved), "unload did not save configuration");
        Require(saved.enabled && saved.radius==40, "incorrect defaults saved");
        ImGui::DestroyContext();
        FreeLibrary(library);
        std::filesystem::remove(std::filesystem::u8path(settingsFilename));
        std::filesystem::remove(std::filesystem::u8path(settingsFilename).parent_path());
        std::filesystem::remove(root);
        std::cout << "PASS: actual DLL load/export, embedded Chinese glyphs, options, WvW drawing, follow movement, map/loading hide, RTAPI fallback, font rebuild and unload\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
