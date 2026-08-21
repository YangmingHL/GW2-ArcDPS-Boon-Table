#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct TrackedEffect {
	uint32_t id = 0;
	std::string custom_name;
};
class BuffDisplaySettings {
public:
	bool show = true;
	bool locked = false;
	bool horizontal = true;
	bool show_background = false;
	bool show_duration = true;
	bool show_stacks = true;
	float expiration_warning_seconds = 1.0f;
	float icon_size = 48.0f;
	float spacing = 4.0f;
	int items_per_line = 8;
	std::vector<TrackedEffect> effects;

	void Load();
	void Save() const;
	bool Contains(uint32_t id) const;
	bool Add(uint32_t id, std::string custom_name = {});
	bool Remove(uint32_t id);

private:
	static std::filesystem::path ConfigPath();
	void SetDefaults();
};
