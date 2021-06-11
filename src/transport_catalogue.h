#pragma once

#include <deque>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "geo.h"
#include "domain.h"

namespace transport_catalogue {

using Buses = std::deque<domain::Bus>;
using Stops = std::deque<domain::Stop>;
using StopsDistances = std::unordered_map<std::pair<domain::StopPtr, domain::StopPtr>,
											uint64_t, domain::PairOfStopPtrHasher>;

namespace detail {

bool StartsWith(std::string_view source, std::string_view seek);

template <typename DomainPtr>
void SortByName(std::deque<DomainPtr>& container) {
	sort(container.begin(), container.end(),
			[](const DomainPtr& lhs, const DomainPtr& rhs) {
				return lhs->name < rhs->name;
			}
	);
}

} // namespace detail

class TransportCatalogue {


public:
	explicit TransportCatalogue() = default;

	void AddStop(domain::Stop stop);

	void AddBus(domain::Bus bus);

	void AddStopsDistances(const std::deque<domain::FromToDistance>& stops_distances);

	domain::StopPtr FindStop(std::string_view stop_name) const;

	domain::BusPtr FindBus(std::string_view bus_name) const;

	domain::BusStat GetRouteInfo(domain::BusPtr bus) const;

	std::unordered_set<domain::BusPtr> GetStopInfo(domain::StopPtr stop) const;

	std::deque<std::string> GetAllBusNames() const;

	const std::deque<domain::Bus>& GetBuses() const;

	const std::deque<domain::Stop>& GetStops() const;

	std::deque<domain::BusPtr> GetBusPtrs() const;

	std::deque<domain::StopPtr> GetStopPtrs() const;

	const StopsDistances& GetStopsDistances() const;

	std::deque<domain::BusPtr> GetAllBusesSortedByName() const;

	std::deque<domain::StopPtr> GetNonEmptyStopsSortedByName() const;

	std::deque<geo::Coordinates> GetAllStopsCoordinates() const;

	size_t BusesAmount() const;

	size_t StopsAmount() const;

	uint64_t FindDistance(domain::StopPtr start, domain::StopPtr destination) const;

private:
	Buses buses_; // хранит все маршруты
	Stops stops_; // хранит все остановки
	std::unordered_map<std::string_view, domain::BusPtr> bus_indexes_; // для быстрого поиска автобуса
	std::unordered_map<std::string_view, domain::StopPtr> stop_indexes_; // для быстрого поиска остановки
	std::unordered_map<domain::StopPtr, std::deque<domain::BusPtr>> stop_to_buses_;
	StopsDistances stops_distances_;
};

} // namespace transport_catalogue
