#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

enum class EffectStacking {
	Duration,
	Intensity,
	Single,
};

struct EffectDefinition {
	uint32_t id;
	std::string_view name;
	EffectStacking stacking;
	uint32_t icon_resource;
	std::vector<uint32_t> aliases;
};

const std::vector<EffectDefinition>& EffectCatalog();
const EffectDefinition* FindEffect(uint32_t id);
uint32_t CanonicalEffectId(uint32_t id);
