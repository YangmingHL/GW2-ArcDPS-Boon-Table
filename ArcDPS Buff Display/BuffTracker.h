#pragma once

#include "EffectCatalog.h"

#include <ArcdpsExtension/arcdps_structs_slim.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

struct ActiveEffect {
	uint32_t stacks = 0;
	uint64_t remaining_ms = 0;
};
class BuffTracker {
public:
	void Process(cbtevent* event, ag* source, ag* destination);
	std::optional<ActiveEffect> Snapshot(uint32_t effect_id, EffectStacking stacking, uint64_t now);
	void Clear();

private:
	struct StackState {
		uint32_t duration_ms = 0;
		uint64_t end_time = 0;
		bool active = false;
	};

	struct EffectState {
		std::unordered_map<uint32_t, StackState> stacks;
	};

	std::mutex mutex_;
	std::unordered_map<uint32_t, EffectState> effects_;
	uintptr_t self_agent_ = 0;
	uint32_t synthetic_stack_id_ = 1;

	static uint32_t StackId(const cbtevent& event);
	bool IsSelf(const ag* agent, uintptr_t event_agent) const;
	void Apply(const cbtevent& event);
	void Change(const cbtevent& event);
	void RemoveSingle(const cbtevent& event);
	void RemoveAll(const cbtevent& event);
	void ActivateStack(const cbtevent& event);
	void DeactivateStack(const cbtevent& event);
};
