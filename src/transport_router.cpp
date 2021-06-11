#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "transport_router.h"
#include "transport_catalogue.h"
#include "graph.h"
#include "router.h"


namespace transport_catalogue::transport_router {

void TransportRouter::SetRoutingSettings(RoutingSettings settings) {
	settings_ = std::move(settings);
}

const RoutingSettings& TransportRouter::GetRoutingSettings() const {
	return settings_;
}

void TransportRouter::BuildRouter(const TransportCatalogue& transport_catalogue) {
	FillGraph(transport_catalogue);
	router_ptr_ = std::make_unique<graph::Router<double>>(*graph_ptr_);
	router_ptr_->Build();
}

const graph::DirectedWeightedGraph<Minutes>& TransportRouter::GetGraph() const {
	return *graph_ptr_;
}

const graph::Router<Minutes>& TransportRouter::GetRouter() const {
	return *router_ptr_;
}

const EdgeInfo& TransportRouter::GetEdgeInfo(graph::EdgeId id) const {
	return edge_id_to_type_.at(id);
}

std::optional<StopPairVertexId> TransportRouter::GetPairVertexId(domain::StopPtr stop) const {
	if (!stop_ptr_to_pair_id_.count(stop)) {
		return std::nullopt;
	}
	return stop_ptr_to_pair_id_.at(stop);
}

std::optional<RouteInfo> TransportRouter::GetRouteInfo(graph::VertexId from, graph::VertexId to) const {
	const auto& route_info = router_ptr_->BuildRoute(from, to);
	if (!route_info) {
		return std::nullopt;
	}
	RouteInfo result;
	result.total_time = route_info->weight;
	for (const auto edge : route_info->edges) {
		result.edges.emplace_back(GetEdgeInfo(edge));
	}
	return result;
}

const std::unordered_map<domain::StopPtr, StopPairVertexId>&
TransportRouter::GetStopToVertexIds() const {
	return stop_ptr_to_pair_id_;
}
const std::unordered_map<graph::EdgeId, EdgeInfo>&
TransportRouter::GetEdgeIdToEdgeInfo() const {
	return edge_id_to_type_;
}

void TransportRouter::FillGraph(const TransportCatalogue& transport_catalogue) {
	graph_ptr_ = std::make_unique<graph::DirectedWeightedGraph<Minutes>>(2 * transport_catalogue.GetStopPtrs().size());
	FillStopIdDictionaries(transport_catalogue.GetStopPtrs());
	AddWaitEdges();
	AddBusEdges(transport_catalogue);
}

void TransportRouter::FillStopIdDictionaries(const std::deque<domain::StopPtr>& stops) {
	size_t i = 0;
	for (const auto stop : stops) {
		graph::VertexId first_id = i++;
		graph::VertexId second_id = i++;
		stop_ptr_to_pair_id_[stop] = StopPairVertexId{first_id, second_id};
	}
}

void TransportRouter::AddWaitEdges() {
	using namespace graph;
	for (const auto [stop_ptr, ids] : stop_ptr_to_pair_id_) {
		graph::EdgeId id = graph_ptr_->AddEdge(Edge<Minutes>{
			ids.bus_wait_begin, ids.bus_wait_end, settings_.bus_wait_time});
		edge_id_to_type_[id] = WaitEdgeInfo{stop_ptr->name, settings_.bus_wait_time};
	}
}

void TransportRouter::AddBusEdges(const TransportCatalogue& transport_catalogue) {
	using namespace graph;

	for (const auto bus_ptr : transport_catalogue.GetBusPtrs()) {
		ParseBusRouteOnEdges(bus_ptr->route.begin(), bus_ptr->route.end(), transport_catalogue, bus_ptr);
		if (!bus_ptr->is_circle) {
			ParseBusRouteOnEdges(bus_ptr->route.rbegin(), bus_ptr->route.rend(), transport_catalogue, bus_ptr);
		}
	}
}

graph::Edge<Minutes> TransportRouter::MakeBusEdge(domain::StopPtr from,
		domain::StopPtr to, const double distance) const {
	using namespace graph;
	Edge<Minutes> result;
	result.from = stop_ptr_to_pair_id_.at(from).bus_wait_end;
	result.to = stop_ptr_to_pair_id_.at(to).bus_wait_begin;
	result.weight = distance * 1.0 / (settings_.bus_velocity * 1000 / 60); // перевод скорости в м/мин
	return result;
}

}  // namespace transport_catalogue::transport_router
