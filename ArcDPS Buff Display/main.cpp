#include "BuffTracker.h"
#include "EffectCatalog.h"
#include "Settings.h"

#include <ArcdpsExtension/IconLoader.h>
#include <ArcdpsExtension/Singleton.h>
#include <ArcdpsExtension/arcdps_structs.h>
#include <imgui/imgui.h>

#include <Windows.h>
#include <atlbase.h>
#include <d3d11.h>
#include <dxgi.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <ranges>
#include <string>
#include <string_view>

namespace {
HMODULE g_self_dll = nullptr;
CComPtr<ID3D11Device> g_device;
bool g_icons_ready = false;
BuffDisplaySettings g_settings;
BuffTracker g_tracker;
arcdps_exports g_exports{};

uintptr_t ProcessEvent(cbtevent* event, ag* source, ag* destination, const char*, uint64_t, uint64_t) {
	g_tracker.Process(event, source, destination, GetTickCount64());
	return 0;
}

std::string DisplayName(const TrackedEffect& effect) {
	if (!effect.custom_name.empty()) return effect.custom_name;
	if (const EffectDefinition* definition = FindEffect(effect.id)) return std::string(definition->name);
	return std::format("效果 {}", effect.id);
}

bool ContainsCaseInsensitive(std::string_view text, std::string_view query) {
	if (query.empty()) return true;
	const auto lower = [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); };
	return std::ranges::search(text, query, {}, lower, lower).begin() != text.end();
}

void SaveIf(bool changed) {
	if (changed) g_settings.Save();
}

void RenderEffectIcon(const TrackedEffect& tracked, const EffectDefinition* definition,
	const std::optional<ActiveEffect>& active, float size, bool show_duration, bool show_stacks,
	float expiration_warning_seconds, uint64_t now, size_t index) {
	ImGui::PushID(static_cast<int>(index));
	const ImVec2 position = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##effect", ImVec2(size, size));
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	const ImVec2 maximum(position.x + size, position.y + size);
	const bool is_active = active.has_value();
	const uint64_t warning_ms = static_cast<uint64_t>(expiration_warning_seconds * 1000.0f);
	const bool is_expiring = active && warning_ms > 0 && active->remaining_ms <= warning_ms;
	const bool flash_visible = is_expiring && (now / 200) % 2 == 0;
	const ImU32 tint = is_active ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 70);

	ID3D11ShaderResourceView* texture = nullptr;
	if (g_icons_ready && definition && definition->icon_resource != 0) {
		texture = ArcdpsExtension::IconLoader::instance().Draw(definition->id);
	}
	if (texture) {
		draw_list->AddImage(reinterpret_cast<ImTextureID>(texture), position, maximum, ImVec2(0, 0), ImVec2(1, 1), tint);
	} else {
		draw_list->AddRectFilled(position, maximum, is_active ? IM_COL32(46, 59, 68, 230) : IM_COL32(46, 59, 68, 70), 3.0f);
		const std::string fallback = std::to_string(tracked.id);
		const ImVec2 text_size = ImGui::CalcTextSize(fallback.c_str());
		draw_list->AddText(ImVec2(position.x + (size - text_size.x) * 0.5f, position.y + (size - text_size.y) * 0.5f), tint, fallback.c_str());
	}
	if (flash_visible) {
		draw_list->AddRectFilled(position, maximum, IM_COL32(255, 32, 24, 72), 3.0f);
		draw_list->AddRect(ImVec2(position.x + 1.0f, position.y + 1.0f),
			ImVec2(maximum.x - 1.0f, maximum.y - 1.0f), IM_COL32(255, 56, 40, 255), 3.0f, 0, 3.0f);
	} else {
		draw_list->AddRect(position, maximum,
			is_active ? IM_COL32(255, 255, 255, 220) : IM_COL32(255, 255, 255, 45), 3.0f, 0, 1.0f);
	}

	if (active && show_duration) {
		const double seconds = static_cast<double>(active->remaining_ms) / 1000.0;
		const std::string duration = seconds < 10.0 ? std::format("{:.1f}", seconds) : std::format("{:.0f}", seconds);
		const ImVec2 text_size = ImGui::CalcTextSize(duration.c_str());
		const ImVec2 text_position(position.x + (size - text_size.x) * 0.5f, maximum.y - text_size.y - 2.0f);
		draw_list->AddText(ImVec2(text_position.x + 1, text_position.y + 1), IM_COL32(0, 0, 0, 230), duration.c_str());
		draw_list->AddText(text_position, IM_COL32_WHITE, duration.c_str());
	}
	if (active && show_stacks && active->stacks > 1) {
		const std::string stacks = std::to_string(active->stacks);
		const ImVec2 text_size = ImGui::CalcTextSize(stacks.c_str());
		const ImVec2 text_position(maximum.x - text_size.x - 3.0f, position.y + 2.0f);
		draw_list->AddText(ImVec2(text_position.x + 1, text_position.y + 1), IM_COL32(0, 0, 0, 230), stacks.c_str());
		draw_list->AddText(text_position, IM_COL32_WHITE, stacks.c_str());
	}

	if (ImGui::IsItemHovered()) {
		const std::string name = DisplayName(tracked);
		ImGui::SetTooltip("%s\nID: %u", name.c_str(), tracked.id);
	}
	ImGui::PopID();
}

void RenderDisplay(uint32_t not_character_select_or_loading, uint32_t hidden_by_combat_state) {
	if (!g_settings.show || !not_character_select_or_loading || hidden_by_combat_state) return;

	struct DisplayItem {
		const TrackedEffect* tracked;
		const EffectDefinition* definition;
		std::optional<ActiveEffect> active;
	};
	std::vector<DisplayItem> items;
	const uint64_t now = GetTickCount64();
	for (const TrackedEffect& tracked : g_settings.effects) {
		const EffectDefinition* definition = FindEffect(tracked.id);
		const EffectStacking stacking = definition ? definition->stacking : EffectStacking::Duration;
		auto active = g_tracker.Snapshot(tracked.id, stacking, now);
		items.push_back({&tracked, definition, active});
	}
	if (items.empty()) return;

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize
		| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing;
	if (g_settings.locked) flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
	if (!g_settings.show_background) flags |= ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
	if (ImGui::Begin("WvW Buff Display##wvw_buff_display", nullptr, flags)) {
		constexpr float drag_height = 28.0f;
		const size_t line_length = static_cast<size_t>(g_settings.items_per_line);
		const size_t columns = g_settings.horizontal
			? std::min(items.size(), line_length)
			: (items.size() + line_length - 1) / line_length;
		const float content_width = static_cast<float>(columns) * g_settings.icon_size
			+ static_cast<float>(columns > 0 ? columns - 1 : 0) * g_settings.spacing;
		const float drag_width = std::max(content_width, 140.0f);
		ImGui::InvisibleButton("##drag_area", ImVec2(drag_width, drag_height));
		if (!g_settings.locked) {
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
				const ImVec2 position = ImGui::GetWindowPos();
				const ImVec2 delta = ImGui::GetIO().MouseDelta;
				ImGui::SetWindowPos(ImVec2(position.x + delta.x, position.y + delta.y), ImGuiCond_Always);
			}
		}
		const ImVec2 minimum = ImGui::GetItemRectMin();
		const ImVec2 maximum = ImGui::GetItemRectMax();
		const float center_x = (minimum.x + maximum.x) * 0.5f;
		const float center_y = (minimum.y + maximum.y) * 0.5f;
		const ImU32 grip_color = g_settings.locked
			? IM_COL32(255, 255, 255, 45)
			: IM_COL32(255, 255, 255, 120);
		for (int offset = -1; offset <= 1; ++offset) {
			ImGui::GetWindowDrawList()->AddLine(
				ImVec2(center_x - 16.0f, center_y + static_cast<float>(offset) * 5.0f),
				ImVec2(center_x + 16.0f, center_y + static_cast<float>(offset) * 5.0f),
				grip_color, 2.0f);
		}
		const ImVec2 origin = ImGui::GetCursorPos();
		const float stride = g_settings.icon_size + g_settings.spacing;
		for (size_t index = 0; index < items.size(); ++index) {
			const DisplayItem& item = items[index];
			const size_t column = g_settings.horizontal ? index % line_length : index / line_length;
			const size_t row = g_settings.horizontal ? index / line_length : index % line_length;
			ImGui::SetCursorPos(ImVec2(origin.x + static_cast<float>(column) * stride,
				origin.y + static_cast<float>(row) * stride));
			RenderEffectIcon(*item.tracked, item.definition, item.active, g_settings.icon_size,
				g_settings.show_duration, g_settings.show_stacks,
				g_settings.expiration_warning_seconds, now, index);
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void RenderOptions() {
	bool changed = false;
	if (!ImGui::CollapsingHeader("WvW 个人增益显示", ImGuiTreeNodeFlags_DefaultOpen)) return;

	changed |= ImGui::Checkbox("显示浮窗", &g_settings.show);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("锁定位置", &g_settings.locked);
	changed |= ImGui::Checkbox("显示背景", &g_settings.show_background);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("显示倒计时", &g_settings.show_duration);
	ImGui::SameLine();
	changed |= ImGui::Checkbox("显示层数", &g_settings.show_stacks);
	changed |= ImGui::SliderFloat("到期提醒", &g_settings.expiration_warning_seconds, 0.0f, 10.0f, "%.1f 秒");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 秒关闭提醒");
	changed |= ImGui::SliderFloat("图标大小", &g_settings.icon_size, 24.0f, 96.0f, "%.0f");
	changed |= ImGui::SliderFloat("图标间距", &g_settings.spacing, 0.0f, 24.0f, "%.0f");
	const char* layouts[] = {"横向", "纵向"};
	int layout = g_settings.horizontal ? 0 : 1;
	if (ImGui::Combo("排列方向", &layout, layouts, 2)) {
		g_settings.horizontal = layout == 0;
		changed = true;
	}
	changed |= ImGui::SliderInt(g_settings.horizontal ? "每行数量" : "每列数量",
		&g_settings.items_per_line, 1, 20);

	ImGui::SeparatorText("选择要监控的增益");
	static std::array<char, 96> search{};
	ImGui::InputTextWithHint("##effect_search", "搜索名称或效果 ID", search.data(), search.size());
	if (ImGui::BeginChild("effect_catalog", ImVec2(0, 210), true)) {
		const std::string_view query(search.data());
		for (const EffectDefinition& definition : EffectCatalog()) {
			const std::string id_text = std::to_string(definition.id);
			if (!ContainsCaseInsensitive(definition.name, query) && !ContainsCaseInsensitive(id_text, query)) continue;
			bool selected = g_settings.Contains(definition.id);
			const std::string label = std::format("{}  [{}]", definition.name, definition.id);
			if (ImGui::Checkbox(label.c_str(), &selected)) {
				changed |= selected ? g_settings.Add(definition.id) : g_settings.Remove(definition.id);
			}
		}
	}
	ImGui::EndChild();

	static uint32_t custom_id = 0;
	static std::array<char, 64> custom_name{};
	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputScalar("效果 ID", ImGuiDataType_U32, &custom_id);
	ImGui::SetNextItemWidth(240.0f);
	ImGui::InputTextWithHint("自定义名称", "可选", custom_name.data(), custom_name.size());
	ImGui::SameLine();
	if (ImGui::Button("添加") && custom_id != 0) {
		if (g_settings.Add(custom_id, custom_name.data())) {
			custom_id = 0;
			custom_name.fill('\0');
			changed = true;
		}
	}

	ImGui::SeparatorText("显示顺序");
	for (size_t index = 0; index < g_settings.effects.size();) {
		TrackedEffect& effect = g_settings.effects[index];
		ImGui::PushID(static_cast<int>(effect.id));
		if (ImGui::SmallButton("^") && index > 0) {
			std::swap(g_settings.effects[index], g_settings.effects[index - 1]);
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("v") && index + 1 < g_settings.effects.size()) {
			std::swap(g_settings.effects[index], g_settings.effects[index + 1]);
			changed = true;
		}
		ImGui::SameLine();
		const bool remove = ImGui::SmallButton("x");
		ImGui::SameLine();
		const std::string name = DisplayName(effect);
		ImGui::TextUnformatted(std::format("{}  [{}]", name, effect.id).c_str());
		ImGui::PopID();
		if (remove) {
			g_settings.effects.erase(g_settings.effects.begin() + static_cast<std::ptrdiff_t>(index));
			changed = true;
			continue;
		}
		++index;
	}
	SaveIf(changed);
}

void CombatCallback(cbtevent* event, ag* source, ag* destination, const char* skill_name, uint64_t id, uint64_t revision) {
	ProcessEvent(event, source, destination, skill_name, id, revision);
}

void ImGuiCallback(uint32_t not_character_select_or_loading, uint32_t hidden_by_combat_state) {
	RenderDisplay(not_character_select_or_loading, hidden_by_combat_state);
}

void OptionsCallback() {
	RenderOptions();
}

void OptionsWindowsCallback(const char* window_name) {
	if (!window_name) SaveIf(ImGui::Checkbox("WvW Buff Display", &g_settings.show));
}

arcdps_exports* ModInit() {
	g_settings.Load();
	if (g_device) {
		ArcdpsExtension::IconLoader::init(g_self_dll, static_cast<ID3D11Device*>(g_device));
		g_icons_ready = true;
		for (const EffectDefinition& effect : EffectCatalog()) {
			ArcdpsExtension::IconLoader::instance().RegisterResource(effect.id, effect.icon_resource);
		}
	}

	g_exports.size = sizeof(arcdps_exports);
	g_exports.sig = 0xB4F0C39D;
	g_exports.imguivers = IMGUI_VERSION_NUM;
	g_exports.out_name = "WvW Buff Display";
	g_exports.out_build = "1.2.0";
	g_exports.combat = CombatCallback;
	g_exports.imgui = ImGuiCallback;
	g_exports.options_end = OptionsCallback;
	g_exports.options_windows = OptionsWindowsCallback;
	return &g_exports;
}

void ModRelease() {
	g_settings.Save();
	ArcdpsExtension::g_singletonManagerInstance.Shutdown();
	g_icons_ready = false;
	g_device = nullptr;
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) g_self_dll = module;
	return TRUE;
}

extern "C" __declspec(dllexport) void* get_init_addr(char*, void* imgui_context, void* dx_pointer,
	HMODULE, void* malloc_function, void* free_function, UINT) {
	ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imgui_context));
	ImGui::SetAllocatorFunctions(
		reinterpret_cast<void* (*)(size_t, void*)>(malloc_function),
		reinterpret_cast<void (*)(void*, void*)>(free_function));

	auto* swap_chain = static_cast<IDXGISwapChain*>(dx_pointer);
	if (swap_chain) swap_chain->GetDevice(IID_PPV_ARGS(&g_device));
	return ModInit;
}

extern "C" __declspec(dllexport) void* get_release_addr(uint32_t) {
	return ModRelease;
}
