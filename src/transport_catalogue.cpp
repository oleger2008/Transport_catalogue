#include <algorithm>
#include <deque>
#include <exception>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <iostream>

#include "transport_catalogue.h"
#include "geo.h"

namespace transport_catalogue {

namespace detail {

bool StartsWith(std::string_view source, std::string_view seek) {
	std::string_view found = source.substr(0, source.find(' '));
	return found == seek;
}

} // namespace detail

void TransportCatalogue::AddStop(domain::Stop stop) {
	stops_.emplace_back(stop);
	stop_indexes_[stops_.back().name] = &stops_.back();
	stop_to_buses_[&stops_.back()];
}

void TransportCatalogue::AddBus(domain::Bus bus) {
	buses_.emplace_back(bus);

	const auto& stops = buses_.back().route;
	for (const auto& stop_ptr : stops) {
		stop_to_buses_.at(stop_ptr).push_back(&buses_.back());
	}

	bus_indexes_[buses_.back().name] = &buses_.back();
}

void TransportCatalogue::AddStopsDistances(const std::deque<domain::FromToDistance>& stops_distances) {
	for (const auto stops_distance : stops_distances) {
		auto from = FindStop(stops_distance.from);
		auto to = FindStop(stops_distance.to);

		stops_distances_[std::make_pair(from, to)] = stops_distance.distance;
	}
}

domain::BusStat TransportCatalogue::GetRouteInfo (domain::BusPtr bus) const {
	domain::BusStat route_info;
	route_info.stop_count = bus->is_circle ? bus->route.size() : 2 * bus->route.size() - 1;
	{
		std::unordered_set<std::string_view> unique_stops;
		for (const auto stop_ptr : bus->route) {
			unique_stops.insert(std::string_view(stop_ptr->name));
		}
		route_info.unique_stop_count = unique_stops.size();
	}

	double straight_length = 0.;
	route_info.route_length = bus->is_circle
			? 0
			: FindDistance(bus->route.back(), bus->route.back());
	{
		const auto& bus_route = bus->route;
		const size_t size = bus_route.size();

		for (size_t i = 1; i < size; ++i) {
			const double distance = geo::ComputeDistance(bus_route[i]->coordinates,
					bus_route[i - 1]->coordinates);
			straight_length += bus->is_circle ? distance : 2. * distance;

			route_info.route_length += bus->is_circle
					? FindDistance(bus_route[i - 1], bus_route[i])
					: (FindDistance(bus_route[i - 1], bus_route[i])
							+ FindDistance(bus_route[i], bus_route[i - 1]));
		}
	}

	route_info.curvature = route_info.route_length / straight_length;

	return route_info;
}

std::unordered_set<domain::BusPtr> TransportCatalogue::GetStopInfo(domain::StopPtr stop) const {
	const auto& buses = stop_to_buses_.at(stop);

	std::unordered_set<domain::BusPtr> unsorted_buses;
	for (const auto bus_ptr : buses) {
		unsorted_buses.insert(bus_ptr);
	}

	return unsorted_buses;
}

std::deque<std::string> TransportCatalogue::GetAllBusNames() const {
	std::deque<std::string> result;
	for (const auto& bus : buses_) {
		result.push_back(bus.name);
	}
	return result;
}

const std::deque<domain::Bus>& TransportCatalogue::GetBuses() const {
	return buses_;
}

const std::deque<domain::Stop>& TransportCatalogue::GetStops() const {
	return stops_;
}

std::deque<domain::BusPtr> TransportCatalogue::GetBusPtrs() const {
	std::deque<domain::BusPtr> result;
	for (const auto& bus : buses_) {
		result.push_back(&bus);
	}
	return result;
}

std::deque<domain::StopPtr> TransportCatalogue::GetStopPtrs() const {
	std::deque<domain::StopPtr> result;
	for (const auto& stop : stops_) {
		result.push_back(&stop);
	}
	return result;
}

const StopsDistances& TransportCatalogue::GetStopsDistances() const {
	return stops_distances_;
}

std::deque<domain::BusPtr> TransportCatalogue::GetAllBusesSortedByName() const {
	std::deque<domain::BusPtr> result;
	for (const auto& bus : buses_) {
		result.push_back(&bus);
	}
	detail::SortByName(result);
	return result;
}

std::deque<domain::StopPtr> TransportCatalogue::GetNonEmptyStopsSortedByName() const {
	std::deque<domain::StopPtr> result;
	for (const auto& stop : stops_) {
		if (!stop_to_buses_.at(&stop).empty()) {
			result.push_back(&stop);
		}
	}
	detail::SortByName(result);
	return result;
}

std::deque<geo::Coordinates> TransportCatalogue::GetAllStopsCoordinates() const {
	std::deque<geo::Coordinates> result;
	for (const auto& stop : stops_) {
		result.push_back(stop.coordinates);
	}
	return result;
}

size_t TransportCatalogue::BusesAmount() const {
	return buses_.size();
}

size_t TransportCatalogue::StopsAmount() const {
	return stops_.size();
}

domain::StopPtr TransportCatalogue::FindStop (std::string_view stop_name) const {
	if (stop_indexes_.count(stop_name)) {
		return stop_indexes_.at(stop_name);
	} else {
		return nullptr;
	}
}

domain::BusPtr TransportCatalogue::FindBus (std::string_view bus_name) const {
	if (bus_indexes_.count(bus_name)) {
		return bus_indexes_.at(bus_name);
	} else {
		return nullptr;
	}
}

uint64_t TransportCatalogue::FindDistance(
		domain::StopPtr start, domain::StopPtr destination) const {
	if (const auto& stop_ptr_pair = std::make_pair(start, destination);
			stops_distances_.count(stop_ptr_pair)) {
		return stops_distances_.at(stop_ptr_pair);
	} else if (const auto& stop_ptr_pair = std::make_pair(destination, start);
			stops_distances_.count(stop_ptr_pair)) {
		return stops_distances_.at(stop_ptr_pair);
	}
	return 0;
}

} // namespace transport_catalogue
