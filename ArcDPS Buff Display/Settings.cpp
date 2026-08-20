#include "Settings.h"

#include "EffectCatalog.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <string_view>

namespace {
bool ParseBool(std::string_view value, bool fallback) {
	if (value == "1" || value == "true") return true;
	if (value == "0" || value == "false") return false;
	return fallback;
}

template <typename T>
T ParseNumber(std::string_view value, T fallback) {
	T result{};
	const auto [ptr, error] = std::from_chars(value.data(), value.data() + value.size(), result);
	return error == std::errc{} && ptr == value.data() + value.size() ? result : fallback;
}

template <>
float ParseNumber(std::string_view value, float fallback) {
	try {
		return std::stof(std::string(value));
	} catch (...) {
		return fallback;
	}
}

std::string SanitizeName(std::string name) {
	std::erase_if(name, [](char ch) { return ch == '\r' || ch == '\n' || ch == '|'; });
	return name;
}
}

std::filesystem::path BuffDisplaySettings::ConfigPath() {
	return std::filesystem::path("addons") / "arcdps" / "arcdps_buff_display.ini";
}

void BuffDisplaySettings::SetDefaults() {
	show = true;
	locked = false;
	horizontal = true;
	show_background = false;
	show_duration = true;
	show_stacks = true;
	icon_size = 48.0f;
	spacing = 4.0f;
	items_per_line = 8;
	effects = {
		{1122, {}},
		{26980, {}},
		{717, {}},
		{743, {}},
		{5974, {}},
		{10269, {}},
	};
}

void BuffDisplaySettings::Load() {
	SetDefaults();
	std::ifstream input(ConfigPath());
	if (!input) return;

	std::vector<TrackedEffect> loaded_effects;
	bool effects_configured = false;
	std::string line;
	while (std::getline(input, line)) {
		if (line.empty() || line[0] == '#' || line[0] == ';') continue;
		const size_t separator = line.find('=');
		if (separator == std::string::npos) continue;

		const std::string_view key(line.data(), separator);
		const std::string_view value(line.data() + separator + 1, line.size() - separator - 1);
		if (key == "show") show = ParseBool(value, show);
		else if (key == "locked") locked = ParseBool(value, locked);
		else if (key == "horizontal") horizontal = ParseBool(value, horizontal);
		else if (key == "show_background") show_background = ParseBool(value, show_background);
		else if (key == "show_duration") show_duration = ParseBool(value, show_duration);
		else if (key == "show_stacks") show_stacks = ParseBool(value, show_stacks);
		else if (key == "icon_size") icon_size = std::clamp(ParseNumber<float>(value, icon_size), 24.0f, 96.0f);
		else if (key == "spacing") spacing = std::clamp(ParseNumber<float>(value, spacing), 0.0f, 24.0f);
		else if (key == "items_per_line") items_per_line = std::clamp(ParseNumber<int>(value, items_per_line), 1, 20);
		else if (key == "effects_configured") effects_configured = ParseBool(value, effects_configured);
		else if (key == "effect") {
			const size_t name_separator = value.find('|');
			const std::string_view id_text = value.substr(0, name_separator);
			const uint32_t id = ParseNumber<uint32_t>(id_text, 0);
			if (id == 0) continue;
			std::string custom_name;
			if (name_separator != std::string_view::npos) {
				custom_name = SanitizeName(std::string(value.substr(name_separator + 1)));
			}
			const uint32_t canonical_id = CanonicalEffectId(id);
			if (std::ranges::none_of(loaded_effects, [canonical_id](const TrackedEffect& effect) { return effect.id == canonical_id; })) {
				loaded_effects.push_back({canonical_id, std::move(custom_name)});
			}
		}
	}
	if (effects_configured || !loaded_effects.empty()) effects = std::move(loaded_effects);
}

void BuffDisplaySettings::Save() const {
	const std::filesystem::path path = ConfigPath();
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);
	std::ofstream output(path, std::ios::trunc);
	if (!output) return;

	output << "show=" << show << '\n';
	output << "locked=" << locked << '\n';
	output << "horizontal=" << horizontal << '\n';
	output << "show_background=" << show_background << '\n';
	output << "show_duration=" << show_duration << '\n';
	output << "show_stacks=" << show_stacks << '\n';
	output << "icon_size=" << icon_size << '\n';
	output << "spacing=" << spacing << '\n';
	output << "items_per_line=" << items_per_line << '\n';
	output << "effects_configured=1\n";
	for (const TrackedEffect& effect : effects) {
		output << "effect=" << effect.id << '|' << SanitizeName(effect.custom_name) << '\n';
	}
}

bool BuffDisplaySettings::Contains(uint32_t id) const {
	const uint32_t canonical_id = CanonicalEffectId(id);
	return std::ranges::any_of(effects, [canonical_id](const TrackedEffect& effect) { return effect.id == canonical_id; });
}

bool BuffDisplaySettings::Add(uint32_t id, std::string custom_name) {
	if (id == 0) return false;
	id = CanonicalEffectId(id);
	if (Contains(id)) return false;
	effects.push_back({id, SanitizeName(std::move(custom_name))});
	return true;
}

bool BuffDisplaySettings::Remove(uint32_t id) {
	id = CanonicalEffectId(id);
	const auto previous_size = effects.size();
	std::erase_if(effects, [id](const TrackedEffect& effect) { return effect.id == id; });
	return effects.size() != previous_size;
}
