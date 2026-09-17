#include <Windows.h>
#include <Nexus.h>
#include <Mumble.h>
#include <RTAPI.hpp>
#include <imgui.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>

#include "ring_math.h"
#include "settings.h"
#include "ui_text.h"

namespace {
using namespace self_ring;
using Clock = std::chrono::steady_clock;
constexpr char kFontId[] = "SELF_POSITION_RING_CN_1";
constexpr char kShortcutId[] = "SELF_POSITION_RING_SETTINGS";
constexpr char kKeybindId[] = "SELF_POSITION_RING_OPEN_SETTINGS";
constexpr uint32_t kRealtimeSignature = 620863532;
constexpr int kSegments = 128;

HMODULE module{};
AddonAPI_t* api{};
NexusLinkData_t* nexus{};
Mumble::Data* mumble{};
Mumble::Identity* identity{};
std::atomic<RTAPI::RealTimeData*> realtime{};
std::atomic<ImFont*> chineseFont{};
Settings settings;
std::filesystem::path settingsPath;
bool panelOpen{}, dirty{}, saveFailed{}, positionReady{};
Clock::time_point lastChange{}, lastSaveAttempt{};

Vec3 Convert(const Mumble::Vector3& v) { return {v.X, v.Y, v.Z}; }
Vec3 Convert(const float (&v)[3]) { return {v[0], v[1], v[2]}; }

void Changed() {
    dirty = true;
    lastChange = Clock::now();
}
void Save() {
    lastSaveAttempt = Clock::now();
    saveFailed = !SaveSettings(settingsPath, settings);
    dirty = saveFailed;
    if (saveFailed) api->Log(LOGL_WARNING, "Self Position Ring", "Could not save settings.");
}
void FontReceived(const char*, void* font) {
    chineseFont.store(static_cast<ImFont*>(font));
}
void AddonLoaded(void* data) {
    if (data && *static_cast<uint32_t*>(data) == kRealtimeSignature)
        realtime.store(static_cast<RTAPI::RealTimeData*>(api->DataLink_Get("RTAPI")));
}
void AddonUnloaded(void* data) {
    if (data && *static_cast<uint32_t*>(data) == kRealtimeSignature) realtime.store(nullptr);
}
void Keybind(const char*, bool released) {
    if (!released) panelOpen = !panelOpen;
}

ImU32 Colour(const std::array<float, 3>& rgb) {
    return ImGui::ColorConvertFloat4ToU32({rgb[0], rgb[1], rgb[2], settings.opacity});
}

bool GetCamera(Camera& camera, Vec3& position) {
    if (!nexus) nexus = static_cast<NexusLinkData_t*>(api->DataLink_Get(DL_NEXUS_LINK));
    if (!mumble) mumble = static_cast<Mumble::Data*>(api->DataLink_Get(DL_MUMBLE_LINK));
    if (!identity) identity = static_cast<Mumble::Identity*>(api->DataLink_Get(DL_MUMBLE_LINK_IDENTITY));
    if (!nexus || !mumble || !identity || !nexus->IsGameplay ||
        !mumble->UITick || mumble->Context.IsMapOpen) return false;

    // The raw position has no multi-frame moving average, so movement does not trail behind.
    position = Convert(mumble->AvatarPosition);
    Vec3 cameraPosition = Convert(mumble->CameraPosition);
    Vec3 facing = Convert(mumble->CameraFront);
    Vec3 top = Convert(mumble->CameraTop);
    float fov = identity->FOV;
    const auto size = ImGui::GetIO().DisplaySize;
    if (const auto* rt = realtime.load(); rt && rt->GameBuild > 0 &&
        rt->GameState == RTAPI::EGameState::Gameplay &&
        static_cast<uint32_t>(rt->MapID) == mumble->Context.MapID &&
        Finite(Convert(rt->CharacterPosition)) &&
        camera.Configure(Convert(rt->CameraPosition), Convert(rt->CameraFacing), {0, 1, 0},
                         rt->CameraFOV, size.x, size.y)) {
        position = Convert(rt->CharacterPosition);
        return true;
    }
    return Finite(position) && camera.Configure(cameraPosition, facing, top, fov, size.x, size.y);
}

void DrawRing() {
    Camera camera;
    Vec3 position;
    positionReady = GetCamera(camera, position);
    if (!settings.enabled || !positionReady) return;

    std::array<Vec3, kSegments> world;
    std::array<ImVec2, kSegments> screen;
    bool allInFront = true;
    const float radius = settings.radius * kGameUnitToMetres;
    for (int i=0; i<kSegments; ++i) {
        const float angle = 2*kPi*static_cast<float>(i)/kSegments;
        world[i] = {position.x + radius*std::cos(angle), position.y,
                    position.z + radius*std::sin(angle)};
        Vec2 projected;
        if (!camera.Project(world[i], projected)) allInFront = false;
        screen[i] = {projected.x, projected.y};
    }

    auto* draw = ImGui::GetBackgroundDrawList();
    draw->PushClipRect({0, 0}, ImGui::GetIO().DisplaySize, true);
    const auto stroke = [&](ImU32 colour, float width) {
        if (allInFront) {
            draw->AddPolyline(screen.data(), kSegments, colour, true, width);
        } else {
            for (int i=0; i<kSegments; ++i) {
                Vec2 a, b;
                if (camera.Segment(world[i], world[(i+1)%kSegments], a, b))
                    draw->AddLine({a.x, a.y}, {b.x, b.y}, colour, width);
            }
        }
    };
    if (settings.outline > 0)
        stroke(Colour(settings.outlineColour), settings.thickness + 2*settings.outline);
    stroke(Colour(settings.colour), settings.thickness);
    draw->PopClipRect();
}

bool ColourPicker(const char* label, std::array<float, 3>& colour) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    const float swatchHeight = ImGui::GetFrameHeight();
    if (ImGui::ColorButton("##swatch", {colour[0], colour[1], colour[2], 1},
                           ImGuiColorEditFlags_NoTooltip, {64, swatchHeight}))
        ImGui::OpenPopup("picker");
    if (ImGui::BeginPopup("picker")) {
        ImGui::TextUnformatted(label);
        ImGui::PushItemWidth(270);
        changed |= ImGui::ColorPicker3("##picker", colour.data(),
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions |
            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_PickerHueBar);
        ImGui::PopItemWidth();
        ImGui::TextUnformatted(ui::ColourHelp);
        ImGui::TextUnformatted(ui::Presets);
        constexpr std::array<std::array<float, 3>, 6> presets{{
            {0, 0.95f, 1}, {1, 0.9f, 0}, {0.2f, 1, 0.25f},
            {1, 1, 1}, {1, 0.3f, 0.7f}, {0, 0, 0}
        }};
        constexpr std::array names{ui::Cyan, ui::Yellow, ui::Green, ui::White, ui::Pink, ui::Black};
        for (size_t i=0; i<presets.size(); ++i) {
            if (i) ImGui::SameLine();
            if (ImGui::ColorButton(names[i], {presets[i][0], presets[i][1], presets[i][2], 1},
                                   ImGuiColorEditFlags_NoTooltip, {32, 26})) {
                colour = presets[i];
                changed = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", names[i]);
        }
        if (ImGui::Button(ui::Done, {100, 0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

void Preview() {
    ImGui::TextUnformatted(ui::Preview);
    const auto start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##preview", {width, 106});
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(start, {start.x+width, start.y+106}, IM_COL32(24, 28, 34, 255), 6);
    const ImVec2 centre{start.x+width/2, start.y+54};
    const float radius = std::min(width*0.4f, 25 + (settings.radius-20)*0.3f);
    std::array<ImVec2, 96> points;
    for (size_t i=0; i<points.size(); ++i) {
        const float angle = 2*kPi*static_cast<float>(i)/points.size();
        points[i] = {centre.x+radius*std::cos(angle), centre.y+radius*0.4f*std::sin(angle)};
    }
    if (settings.outline > 0)
        draw->AddPolyline(points.data(), static_cast<int>(points.size()), Colour(settings.outlineColour),
                          true, settings.thickness+2*settings.outline);
    draw->AddPolyline(points.data(), static_cast<int>(points.size()), Colour(settings.colour),
                      true, settings.thickness);
    draw->AddCircleFilled(centre, 4, IM_COL32(220, 230, 235, 255));
}

void Controls() {
    ImGui::PushID("SelfPositionRing");
    ImGui::TextWrapped("%s", ui::Intro);
    bool changed = ImGui::Checkbox(ui::Enabled, &settings.enabled);
    ImGui::Separator();
    const float available = ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth(std::max(140.0f, available-110));
    constexpr auto sliderFlags = ImGuiSliderFlags_NoInput;
    changed |= ImGui::SliderFloat(ui::Radius, &settings.radius, 20, 200, ui::UnitRadius, sliderFlags);
    changed |= ImGui::SliderFloat(ui::Thickness, &settings.thickness, 1, 12, ui::UnitPixel, sliderFlags);
    changed |= ImGui::SliderFloat(ui::Outline, &settings.outline, 0, 6, ui::UnitPixel, sliderFlags);
    float opacity = settings.opacity*100;
    if (ImGui::SliderFloat(ui::Opacity, &opacity, 10, 100, "%.0f%%", sliderFlags)) {
        settings.opacity = opacity/100;
        changed = true;
    }
    ImGui::PopItemWidth();
    changed |= ColourPicker(ui::Colour, settings.colour);
    changed |= ColourPicker(ui::OutlineColour, settings.outlineColour);
    Preview();
    if (ImGui::Button(ui::Restore)) {
        settings = Settings{};
        changed = true;
    }
    if (changed) Changed();
    ImGui::TextWrapped("%s", ui::Footnote);
    if (!settings.enabled) ImGui::TextWrapped("%s", ui::Hidden);
    else if (!positionReady) ImGui::TextWrapped("%s", ui::PositionWaiting);
    if (saveFailed) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.5f, 0.35f, 1));
        ImGui::TextWrapped("%s", ui::SaveError);
        ImGui::PopStyleColor();
    }
    if (ImGui::CollapsingHeader(ui::About)) {
        ImGui::TextWrapped("%s", ui::AboutText);
        const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(102), RT_RCDATA);
        const auto loaded = resource ? LoadResource(module, resource) : nullptr;
        const auto* text = loaded ? static_cast<const char*>(LockResource(loaded)) : nullptr;
        if (text) {
            ImGui::BeginChild("licenses", {0, 160}, true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(text, text+SizeofResource(module, resource));
            ImGui::EndChild();
        }
    }
    ImGui::PopID();
}

void Options() {
    if (auto* font = chineseFont.load()) {
        ImGui::PushFont(font);
        if (ImGui::Button(ui::Open)) panelOpen = true;
        Controls();
        ImGui::PopFont();
    } else {
        ImGui::TextUnformatted("Loading Chinese font...");
    }
}
void Shortcut() {
    if (auto* font = chineseFont.load()) {
        ImGui::PushFont(font);
        if (ImGui::MenuItem(ui::Title)) panelOpen = true;
        ImGui::PopFont();
    }
}
void Render() {
    DrawRing();
    if (panelOpen) {
        if (auto* font = chineseFont.load()) {
            ImGui::PushFont(font);
            ImGui::SetNextWindowSize({540, 600}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints({390, 350}, {1000, 1200});
            if (ImGui::Begin(ui::Window, &panelOpen)) Controls();
            ImGui::End();
            ImGui::PopFont();
        }
    }
    const auto now = Clock::now();
    if (dirty && now-lastChange > std::chrono::milliseconds(400) &&
        now-lastSaveAttempt > std::chrono::seconds(2) && !ImGui::IsAnyItemActive()) Save();
}

void Load(AddonAPI_t* host) {
    api = host;
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(api->ImguiContext));
    ImGui::SetAllocatorFunctions(reinterpret_cast<void* (*)(size_t, void*)>(api->ImguiMalloc),
                                reinterpret_cast<void (*)(void*, void*)>(api->ImguiFree));
    try {
        const auto* path = api->Paths_GetAddonDirectory("SelfPositionRing/settings.ini");
        if (path) settingsPath = std::filesystem::u8path(path);
        panelOpen = !LoadSettings(settingsPath, settings);
    } catch (const std::exception&) {
        panelOpen = true;
        saveFailed = true;
    }
    if (panelOpen) Changed();

    // Nexus rebuilds managed fonts from registered localization text. A custom GlyphRanges
    // alone is insufficient: the host replaces it during its atlas rebuild.
    for (size_t i=0; i<ui::All.size(); ++i) {
        const auto key = "SELF_POSITION_RING_TEXT_"+std::to_string(i);
        api->Localization_Set(key.c_str(), "en", ui::All[i]);
    }
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 1;
    api->Fonts_AddFromResource(kFontId, 18, 101, module, FontReceived, &fontConfig);
    api->Events_Subscribe(EV_ADDON_LOADED, AddonLoaded);
    api->Events_Subscribe(EV_ADDON_UNLOADED, AddonUnloaded);
    realtime.store(static_cast<RTAPI::RealTimeData*>(api->DataLink_Get("RTAPI")));
    api->GUI_Register(RT_Render, Render);
    api->GUI_Register(RT_OptionsRender, Options);
    api->GUI_RegisterCloseOnEscape(ui::Window, &panelOpen);
    api->QuickAccess_AddContextMenu(kShortcutId, "!Nexus", Shortcut);
    api->InputBinds_RegisterWithString(kKeybindId, Keybind, "(null)");
}
void Unload() {
    api->GUI_Deregister(Render);
    api->GUI_Deregister(Options);
    api->QuickAccess_RemoveContextMenu(kShortcutId);
    api->InputBinds_Deregister(kKeybindId);
    api->GUI_DeregisterCloseOnEscape(ui::Window);
    api->Events_Unsubscribe(EV_ADDON_LOADED, AddonLoaded);
    api->Events_Unsubscribe(EV_ADDON_UNLOADED, AddonUnloaded);
    if (dirty) Save();
    api->Fonts_Release(kFontId, FontReceived);
    chineseFont.store(nullptr);
    realtime.store(nullptr);
}
} // namespace

BOOL WINAPI DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) module = instance;
    return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    static AddonDefinition_t definition{
        0xE7A5C319u, NEXUS_API_VERSION, "Self Position Ring", {1, 0, 0, 0},
        "YangmingHL", "Chinese personal position ring with sliders and a visual colour picker.",
        Load, Unload, AF_None, UP_None, nullptr
    };
    return &definition;
}
