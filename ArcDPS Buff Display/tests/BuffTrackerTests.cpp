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
	tracker.Process(&stability, &other, &self, 1'000);
	auto active = tracker.Snapshot(1122, EffectStacking::Intensity, 2'000);
	assert(active && active->stacks == 1 && active->remaining_ms == 4'000);

	auto other_stability = ApplyEvent(1122, other_id, 2'000, 5'000, 2);
	tracker.Process(&other_stability, &self, &other, 2'000);
	active = tracker.Snapshot(1122, EffectStacking::Intensity, 2'000);
	assert(active && active->stacks == 1);

	auto dwarf = ApplyEvent(80245, self_id, 3'000, 6'000, 3);
	tracker.Process(&dwarf, &other, &self, 3'000);
	auto canonical = tracker.Snapshot(26596, EffectStacking::Single, 4'000);
	assert(canonical && canonical->remaining_ms == 5'000);

	cbtevent remove_dwarf{};
	remove_dwarf.time = 4'500;
	remove_dwarf.src_agent = self_id;
	remove_dwarf.skillid = 33330;
	remove_dwarf.is_statechange = CBTS_BUFFREMOVE_ALL;
	tracker.Process(&remove_dwarf, &self, &other, 4'500);
	assert(!tracker.Snapshot(26596, EffectStacking::Single, 4'500));

	// Processing delay must not extend an effect beyond its event timestamp.
	auto delayed_protection = ApplyEvent(717, self_id, 10'000, 5'000, 10);
	tracker.Process(&delayed_protection, &other, &self, 12'000);
	auto delayed = tracker.Snapshot(717, EffectStacking::Duration, 12'000);
	assert(delayed && delayed->remaining_ms == 3'000);

	// Queued duration stacks start at the previous stack's exact expiration time.
	auto first_fury = ApplyEvent(725, self_id, 20'000, 2'000, 11);
	auto queued_fury = ApplyEvent(725, self_id, 20'500, 3'000, 12);
	queued_fury.is_shields = 0;
	tracker.Process(&first_fury, &other, &self, 20'000);
	tracker.Process(&queued_fury, &other, &self, 20'500);
	auto queued = tracker.Snapshot(725, EffectStacking::Duration, 24'000);
	assert(queued && queued->stacks == 1 && queued->remaining_ms == 1'000);

	// BUFFACTIVE value is the current duration and replaces stale apply data.
	auto superspeed = ApplyEvent(5974, self_id, 30'000, 9'000, 21);
	tracker.Process(&superspeed, &other, &self, 30'000);
	cbtevent deactivate{};
	deactivate.time = 30'500;
	deactivate.src_agent = self_id;
	deactivate.value = 8'500;
	deactivate.is_statechange = CBTS_BUFFDEACTIVE;
	SetStackId(deactivate, 21);
	tracker.Process(&deactivate, &self, &other, 30'500);
	cbtevent activate{};
	activate.time = 31'000;
	activate.src_agent = self_id;
	activate.dst_agent = 21;
	activate.value = 2'500;
	activate.is_statechange = CBTS_BUFFACTIVE;
	tracker.Process(&activate, &self, &other, 31'000);
	auto reactivated = tracker.Snapshot(5974, EffectStacking::Single, 32'000);
	assert(reactivated && reactivated->remaining_ms == 1'500);

	// ArcDPS event timestamps use timeGetTime's low 32 bits and wrap at 49.7 days.
	constexpr uint64_t wrap = uint64_t{1} << 32;
	auto wrapped_aegis = ApplyEvent(743, self_id, wrap - 16, 64, 30);
	tracker.Process(&wrapped_aegis, &other, &self, wrap + 16);
	auto wrapped = tracker.Snapshot(743, EffectStacking::Duration, wrap + 16);
	assert(wrapped && wrapped->remaining_ms == 32);

	cbtevent map_change{};
	map_change.time = 5'000;
	map_change.is_statechange = CBTS_MAPCHANGE;
	tracker.Process(&map_change, &other, &other, 5'000);
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
