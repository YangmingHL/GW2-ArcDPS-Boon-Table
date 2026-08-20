#include "BuffTracker.h"

#include <algorithm>
#include <cstring>
#include <ranges>

uint32_t BuffTracker::StackId(const cbtevent& event) {
	uint32_t id = 0;
	std::memcpy(&id, &event.pad61, sizeof(id));
	return id;
}

bool BuffTracker::IsSelf(const ag* agent, uintptr_t event_agent) const {
	return (agent && agent->self != 0) || (self_agent_ != 0 && event_agent == self_agent_);
}

void BuffTracker::Process(cbtevent* event, ag* source, ag* destination) {
	std::lock_guard lock(mutex_);
	if (!event) {
		if (source && destination && destination->self != 0) {
			if (source->prof != 0) {
				self_agent_ = source->id;
			} else if (source->id == self_agent_) {
				self_agent_ = 0;
				effects_.clear();
			}
		}
		return;
	}

	if (source && source->self != 0) self_agent_ = source->id;
	if (destination && destination->self != 0) self_agent_ = destination->id;

	switch (event->is_statechange) {
	case CBTS_MAPCHANGE:
		effects_.clear();
		break;
	case CBTS_CHANGEDEAD:
		if (IsSelf(source, event->src_agent)) effects_.clear();
		break;
	case CBTS_BUFFINITIAL:
	case CBTS_BUFFAPPLY:
		if (IsSelf(destination, event->dst_agent)) Apply(*event);
		break;
	case CBTS_BUFFCHANGE:
		if (IsSelf(destination, event->dst_agent)) Change(*event);
		break;
	case CBTS_BUFFREMOVE_SINGLE:
		if (IsSelf(source, event->src_agent)) RemoveSingle(*event);
		break;
	case CBTS_BUFFREMOVE_ALL:
		if (IsSelf(source, event->src_agent)) RemoveAll(*event);
		break;
	case CBTS_BUFFACTIVE:
		if (IsSelf(source, event->src_agent)) ActivateStack(*event);
		break;
	case CBTS_BUFFDEACTIVE:
		if (IsSelf(source, event->src_agent)) DeactivateStack(*event);
		break;
	default:
		break;
	}

	// Compatibility with pre-May 2026 ArcDPS events.
	if (event->is_statechange == CBTS_COMBAT) {
		if (event->is_buffremove != CBTB_NONE && IsSelf(source, event->src_agent)) {
			if (event->is_buffremove == CBTB_ALL) RemoveAll(*event);
			else RemoveSingle(*event);
		} else if (event->buff != 0 && IsSelf(destination, event->dst_agent)) {
			Apply(*event);
		}
	}
}

void BuffTracker::Apply(const cbtevent& event) {
	if (event.skillid == 0 || event.value <= 0) return;
	const uint32_t effect_id = CanonicalEffectId(event.skillid);
	uint32_t stack_id = StackId(event);
	if (stack_id == 0) stack_id = 0x80000000u | synthetic_stack_id_++;

	StackState& stack = effects_[effect_id].stacks[stack_id];
	stack.duration_ms = static_cast<uint32_t>(event.value);
	stack.active = event.is_shields != 0 || effects_[effect_id].stacks.size() == 1;
	stack.end_time = stack.active ? event.time + stack.duration_ms : 0;
}

void BuffTracker::Change(const cbtevent& event) {
	if (event.skillid == 0) return;
	const uint32_t effect_id = CanonicalEffectId(event.skillid);
	uint32_t stack_id = StackId(event);
	if (stack_id == 0) stack_id = 0x80000000u | synthetic_stack_id_++;
	const uint32_t duration = event.overstack_value > 0
		? event.overstack_value
		: static_cast<uint32_t>(std::max(event.value, 0));
	if (duration == 0) return;

	StackState& stack = effects_[effect_id].stacks[stack_id];
	stack.duration_ms = duration;
	stack.active = true;
	stack.end_time = event.time + duration;
}

void BuffTracker::RemoveSingle(const cbtevent& event) {
	const uint32_t effect_id = CanonicalEffectId(event.skillid);
	const auto effect = effects_.find(effect_id);
	if (effect == effects_.end()) return;

	const uint32_t stack_id = StackId(event);
	if (stack_id != 0) {
		effect->second.stacks.erase(stack_id);
	} else if (!effect->second.stacks.empty()) {
		effect->second.stacks.erase(effect->second.stacks.begin());
	}
	if (effect->second.stacks.empty()) effects_.erase(effect);
}

void BuffTracker::RemoveAll(const cbtevent& event) {
	effects_.erase(CanonicalEffectId(event.skillid));
}

void BuffTracker::ActivateStack(const cbtevent& event) {
	const uint32_t stack_id = static_cast<uint32_t>(event.dst_agent);
	for (auto& state : effects_ | std::views::values) {
		if (const auto stack = state.stacks.find(stack_id); stack != state.stacks.end()) {
			stack->second.active = true;
			stack->second.end_time = event.time + stack->second.duration_ms;
			return;
		}
	}
}

void BuffTracker::DeactivateStack(const cbtevent& event) {
	const uint32_t stack_id = StackId(event);
	for (auto& state : effects_ | std::views::values) {
		if (const auto stack = state.stacks.find(stack_id); stack != state.stacks.end()) {
			stack->second.duration_ms = static_cast<uint32_t>(std::max(event.value, 0));
			stack->second.active = false;
			stack->second.end_time = 0;
			return;
		}
	}
}

std::optional<ActiveEffect> BuffTracker::Snapshot(uint32_t effect_id, EffectStacking stacking, uint64_t now) {
	std::lock_guard lock(mutex_);
	effect_id = CanonicalEffectId(effect_id);
	const auto effect = effects_.find(effect_id);
	if (effect == effects_.end()) return std::nullopt;

	auto& stacks = effect->second.stacks;
	for (auto it = stacks.begin(); it != stacks.end();) {
		if (it->second.active && it->second.end_time <= now) it = stacks.erase(it);
		else ++it;
	}
	if (stacks.empty()) {
		effects_.erase(effect);
		return std::nullopt;
	}

	if (stacking != EffectStacking::Intensity && std::ranges::none_of(stacks, [](const auto& pair) { return pair.second.active; })) {
		auto next = stacks.begin();
		next->second.active = true;
		next->second.end_time = now + next->second.duration_ms;
	}

	ActiveEffect result;
	result.stacks = static_cast<uint32_t>(stacks.size());
	for (const auto& stack : stacks | std::views::values) {
		const uint64_t remaining = stack.active && stack.end_time > now
			? stack.end_time - now
			: stack.duration_ms;
		if (stacking == EffectStacking::Duration) result.remaining_ms += remaining;
		else result.remaining_ms = std::max(result.remaining_ms, remaining);
	}
	return result.remaining_ms > 0 ? std::optional(result) : std::nullopt;
}

void BuffTracker::Clear() {
	std::lock_guard lock(mutex_);
	effects_.clear();
}
