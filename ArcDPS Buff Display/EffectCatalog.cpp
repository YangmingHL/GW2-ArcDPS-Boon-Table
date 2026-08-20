#include "EffectCatalog.h"

#include "../ArcDPS Boon Table/resource.h"

#include <algorithm>

const std::vector<EffectDefinition>& EffectCatalog() {
	static const std::vector<EffectDefinition> catalog = {
		{740, "威能 Might", EffectStacking::Intensity, ID_Might, {740}},
		{725, "激怒 Fury", EffectStacking::Duration, ID_Fury, {725}},
		{718, "再生 Regeneration", EffectStacking::Duration, ID_Regeneration, {718}},
		{717, "保护 Protection", EffectStacking::Duration, ID_Protection, {717}},
		{1187, "急速 Quickness", EffectStacking::Duration, ID_Quickness, {1187}},
		{30328, "敏捷 Alacrity", EffectStacking::Duration, ID_Alacrity, {30328}},
		{873, "决心 Resolution", EffectStacking::Duration, ID_Resolution, {873}},
		{726, "活力 Vigor", EffectStacking::Duration, ID_Vigor, {726}},
		{1122, "稳固 Stability", EffectStacking::Intensity, ID_Stability, {1122}},
		{743, "圣盾 Aegis", EffectStacking::Duration, ID_Aegis, {743}},
		{719, "迅捷 Swiftness", EffectStacking::Duration, ID_Swiftness, {719}},
		{26980, "抗性 Resistance", EffectStacking::Duration, ID_Resistance, {26980}},
		{10269, "潜行 Stealth", EffectStacking::Duration, ID_Stealth2, {10269, 13017, 26142, 32747, 58026}},
		{5974, "超级速度 Superspeed", EffectStacking::Single, ID_Super_Speed2, {5974}},
		{10332, "混沌光环 Chaos Aura", EffectStacking::Single, ID_Chaos_Aura, {10332}},
		{39978, "黑暗光环 Dark Aura", EffectStacking::Single, ID_Dark_Aura, {39978}},
		{5677, "火焰光环 Fire Aura", EffectStacking::Single, ID_Fire_Aura, {5677}},
		{5579, "冰霜光环 Frost Aura", EffectStacking::Single, ID_Frost_Aura, {5579}},
		{25518, "光明光环 Light Aura", EffectStacking::Single, ID_Light_Aura, {25518, 68927}},
		{5684, "磁力光环 Magnetic Aura", EffectStacking::Single, ID_Magnetic_Aura, {5684}},
		{5577, "电击光环 Shocking Aura", EffectStacking::Single, ID_Shocking_Aura, {5577}},
		{41815, "石牦牛姿态 Dolyak Stance", EffectStacking::Duration, ID_Dolyak_Stance, {41815}},
		{46280, "狮鹫姿态 Griffon Stance", EffectStacking::Duration, ID_Griffon_Stance, {46280}},
		{45038, "恐鸟姿态 Moa Stance", EffectStacking::Duration, ID_Moa_Stance, {45038}},
		{44651, "秃鹫姿态 Vulture Stance", EffectStacking::Duration, ID_Vulture_Stance, {44651}},
		{40045, "熊之姿态 Bear Stance", EffectStacking::Duration, ID_Bear_Stance, {40045}},
		{44139, "狼群之力 One Wolf Pack", EffectStacking::Duration, ID_One_Wolf_Pack, {44139}},
		{45026, "断魂山 Soulcleave's Summit", EffectStacking::Single, ID_Soulcleaves_Summit, {45026}},
		{41016, "剃刀之爪怒火 Razorclaw's Rage", EffectStacking::Single, ID_Razorclaws_Rage, {41016}},
		{44682, "破刃壁垒 Breakrazor's Bastion", EffectStacking::Single, ID_Breakrazors_Bastion, {44682}},
		{53489, "灵魂倒钩 Soul Barbs", EffectStacking::Duration, ID_Soul_Barbs, {53489}},
		{10582, "幽灵铠甲 Spectral Armor", EffectStacking::Single, ID_Spectral_Armor, {10582}},
		{59592, "鼓舞美德 Inspiring Virtue", EffectStacking::Single, ID_Inspiring_Virtue, {59592}},
		{44871, "永恒绿洲 Eternal Oasis", EffectStacking::Single, ID_Eternal_Oasis, {44871}},
		{43194, "坚不可摧 Unbroken Lines", EffectStacking::Single, ID_Unbroken_Lines, {43194}},
		{26596, "崇高矮人仪式 Rite of the Great Dwarf", EffectStacking::Single, ID_Rite_of_the_Great_Dwarf, {26596, 33330, 80245}},
		{56890, "象征复仇者 Symbolic Avenger", EffectStacking::Single, ID_Symbolic_Avenger, {56890}},
		{30207, "鼓舞壁垒 Invigorated Bulwark", EffectStacking::Single, ID_Invigorated_Bulwark, {30207}},
		{33652, "严谨确信 Rigorous Certainty", EffectStacking::Single, ID_Rigorous_Certainty, {33652}},
		{69795, "贵族圣物 Relic of the Aristocracy", EffectStacking::Intensity, ID_Relic_Aristocracy, {69795}},
		{71132, "僧侣圣物 Relic of the Monk", EffectStacking::Intensity, ID_Relic_Monk, {71132}},
		{70913, "斗士圣物 Relic of the Brawler", EffectStacking::Single, ID_Relic_Brawler, {70913}},
		{70767, "潜行者圣物 Relic of the Thief", EffectStacking::Intensity, ID_Relic_Thief, {70767}},
		{69855, "烟花圣物 Relic of Fireworks", EffectStacking::Single, ID_Relic_Fireworks, {69855}},
		{70839, "冒险家圣物 Relic of the Daredevil", EffectStacking::Single, ID_Relic_Daredevil, {70839}},
		{70282, "神枪手圣物 Relic of the Deadeye", EffectStacking::Single, ID_Relic_Deadeye, {70282}},
		{71217, "燃火者圣物 Relic of the Firebrand", EffectStacking::Single, ID_Relic_Firebrand, {71217}},
		{69606, "先驱圣物 Relic of the Herald", EffectStacking::Intensity, ID_Relic_Herald, {69606}},
		{70390, "编织者圣物 Relic of the Weaver", EffectStacking::Single, ID_Relic_Weaver, {70390}},
		{70460, "和风圣物 Relic of the Zephyrite", EffectStacking::Single, ID_Relic_Zephyrite, {70460}},
		{70353, "莱尔圣物 Relic of Lyhr", EffectStacking::Single, ID_Relic_Lyhr, {70353}},
		{69620, "马邦圣物 Relic of Mabon", EffectStacking::Intensity, ID_Relic_Mabon, {69620}},
		{69961, "瓦斯圣物 Relic of Vass", EffectStacking::Intensity, ID_Relic_Vass, {69961}},
		{71431, "诺丽斯圣物 Relic of Nourys", EffectStacking::Single, ID_Relic_Nourys, {71431}},
		{73455, "唤雷者圣物 Relic of the Stormsinger", EffectStacking::Single, ID_Relic_Stormsinger, {73455}},
		{74410, "悲伤圣物 Relic of Sorrow", EffectStacking::Single, ID_Relic_Sorrow, {74410}},
		{73181, "凋零使者圣物 Relic of the Blightbringer", EffectStacking::Single, ID_Relic_Blightbringer, {73181}},
		{73955, "利爪圣物 Relic of the Claw", EffectStacking::Single, ID_Relic_Claw, {73955}},
		{74793, "巴里奥山圣物 Relic of Mount Balrior", EffectStacking::Single, ID_Relic_MountBalrior, {74793}},
		{75432, "荆棘圣物 Relic of Thorns", EffectStacking::Intensity, ID_Relic_Thorns, {75432}},
		{76372, "泰坦潜能圣物 Relic of Titanic Potential", EffectStacking::Intensity, ID_Relic_TitanicPotential, {76372}},
		{76351, "泰坦之魂圣物 Relic of the Titan Soul", EffectStacking::Single, ID_Relic_SoulOfTheTitan, {76351}},
		{104800, "血石波动圣物 Bloodstone Volatility", EffectStacking::Intensity, ID_Relic_Bloodstone, {104800}},
		{76326, "血石热忱圣物 Bloodstone Fervor", EffectStacking::Single, ID_Relic_Bloodstone, {76326}},
	};
	return catalog;
}

const EffectDefinition* FindEffect(uint32_t id) {
	const auto& catalog = EffectCatalog();
	const auto it = std::ranges::find_if(catalog, [id](const EffectDefinition& effect) {
		return std::ranges::find(effect.aliases, id) != effect.aliases.end();
	});
	return it == catalog.end() ? nullptr : &*it;
}

uint32_t CanonicalEffectId(uint32_t id) {
	if (const EffectDefinition* effect = FindEffect(id)) {
		return effect->id;
	}
	return id;
}
