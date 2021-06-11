#pragma once

#include <deque>
#include <memory>
#include <string>

#include "geo.h"

namespace transport_catalogue::domain {

struct Stop;
struct Bus;

using StopPtr = const Stop*;
using BusPtr = const Bus*;

struct Stop {
	std::string name;
	geo::Coordinates coordinates;
};

struct Bus {
	std::string name;
	bool is_circle;
	std::deque<StopPtr> route;
};

struct BusStat {
	uint64_t stop_count = 0;
	uint64_t unique_stop_count = 0;
	uint64_t route_length = 0;
	double curvature = 0.;
};

struct FromToDistance {
	std::string_view from;
	std::string_view to;
	uint64_t distance;
};

struct PairOfStopPtrHasher {
	size_t operator() (const std::pair<StopPtr, StopPtr>& p) const {
		auto ptr_h1 = ptr_hasher(p.first);
		auto ptr_h2 = ptr_hasher(p.second);

		return ptr_h1 + ptr_h2 * 43;
	}
	std::hash<const void*> ptr_hasher;
};

}  // transport_catalogue::domain
