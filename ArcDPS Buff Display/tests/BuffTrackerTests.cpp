#include "../BuffTracker.h"
#include "../Settings.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>

namespace {
void SetStackId(cbtevent& event, uint32_t stack_id) {
	std::memcpy(&event.pad61, &stack_id, sizeof(stack_id));
}

cbtevent ApplyEvent(uint32_t effect_id, uintptr_t self_id, uint64_t time, uint32_t duration, uint32_t stack_id) {
	cbtevent event{};
	event.time = time;
	event.dst_agent = self_id;
	event.value = static_cast<int32_t>(duration);
	event.skillid = effect_id;
	event.is_statechange = CBTS_BUFFAPPLY;
	event.is_shields = 1;
	SetStackId(event, stack_id);
	return event;
}
}

int main() {
	constexpr uintptr_t self_id = 101;
	constexpr uintptr_t other_id = 202;
	ag self{};
	self.id = self_id;
	self.self = 1;
	ag other{};
	other.id = other_id;
	BuffTracker tracker;

	auto stability = ApplyEvent(1122, self_id, 1'000, 5'000, 1);
	tracker.Process(&stability, &other, &self);
	auto active = tracker.Snapshot(1122, EffectStacking::Intensity, 2'000);
	assert(active && active->stacks == 1 && active->remaining_ms == 4'000);

	auto other_stability = ApplyEvent(1122, other_id, 2'000, 5'000, 2);
	tracker.Process(&other_stability, &self, &other);
	active = tracker.Snapshot(1122, EffectStacking::Intensity, 2'000);
	assert(active && active->stacks == 1);

	auto dwarf = ApplyEvent(80245, self_id, 3'000, 6'000, 3);
	tracker.Process(&dwarf, &other, &self);
	auto canonical = tracker.Snapshot(26596, EffectStacking::Single, 4'000);
	assert(canonical && canonical->remaining_ms == 5'000);

	cbtevent remove_dwarf{};
	remove_dwarf.time = 4'500;
	remove_dwarf.src_agent = self_id;
	remove_dwarf.skillid = 33330;
	remove_dwarf.is_statechange = CBTS_BUFFREMOVE_ALL;
	tracker.Process(&remove_dwarf, &self, &other);
	assert(!tracker.Snapshot(26596, EffectStacking::Single, 4'500));

	cbtevent map_change{};
	map_change.time = 5'000;
	map_change.is_statechange = CBTS_MAPCHANGE;
	tracker.Process(&map_change, &other, &other);
	assert(!tracker.Snapshot(1122, EffectStacking::Intensity, 5'000));

	const std::filesystem::path original_path = std::filesystem::current_path();
	const std::filesystem::path settings_test_path = std::filesystem::temp_directory_path()
		/ ("arcdps-buff-display-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(settings_test_path);
	std::filesystem::current_path(settings_test_path);
	BuffDisplaySettings settings;
	settings.Load();
	assert(!settings.effects.empty());
	settings.effects.clear();
	settings.Save();
	BuffDisplaySettings reloaded;
	reloaded.Load();
	assert(reloaded.effects.empty());
	std::filesystem::current_path(original_path);
	std::filesystem::remove_all(settings_test_path);
}
