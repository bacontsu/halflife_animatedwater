// BunnymodXT contributors & SmileyAG provided patterns.

#ifdef REGS_PATTERNS_HPP_RECURSE_GUARD
#error Recursive header files inclusion detected in reGS_patterns.hpp
#else //REGS_PATTERNS_HPP_RECURSE_GUARD

#define REGS_PATTERNS_HPP_RECURSE_GUARD

#ifndef REGS_PATTERNS_HPP_GUARD
#define REGS_PATTERNS_HPP_GUARD
#pragma once

#include "sptlib/patterns.hpp"
#include "sptlib/MemUtils.h"

namespace patterns
{
namespace engine
{
	PATTERNS(EmitWaterPolys,
		"HL-SteamPipe-8684",
		"55 8B EC 83 EC 20 56 8B 75 ?? 33 D2");
}
}

#endif //REGS_PATTERNS_HPP_GUARD

#undef REGS_PATTERNS_HPP_RECURSE_GUARD
#endif //REGS_PATTERNS_HPP_RECURSE_GUARD