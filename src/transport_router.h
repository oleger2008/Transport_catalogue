#pragma once

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <unordered_map>

#include <iostream>

#include "transport_catalogue.h"
#include "graph.h"
#include "router.h"
#include "domain.h"

namespace transport_catalogue::transport_router {

using Minutes = double;

struct RoutingSettings {
	Minutes bus_wait_time = 0.; // мин
	double bus_velocity = 0.; // км/ч
};

struct StopPairVertexId {
	graph::VertexId bus_wait_begin;
	graph::VertexId bus_wait_end;
};

struct WaitEdgeInfo {
	std::string_view stop_name;
	Minutes time = 0.;
};

struct BusEdgeInfo {
	std::string_view bus_name;
	size_t span_count = 0;
	Minutes time = 0.;
};

using EdgeInfo = std::variant<WaitEdgeInfo, BusEdgeInfo>;

struct RouteInfo {
	Minutes total_time = 0.;
	std::vector<EdgeInfo> edges;
};

class TransportRouter {
public:
	void SetRoutingSettings(RoutingSettings settings);
	const RoutingSettings& GetRoutingSettings() const;

	void BuildRouter(const TransportCatalogue& transport_catalogue);

	const graph::DirectedWeightedGraph<Minutes>& GetGraph() const;

	const graph::Router<Minutes>& GetRouter() const;

	const EdgeInfo& GetEdgeInfo(graph::EdgeId id) const;

	std::optional<StopPairVertexId> GetPairVertexId(domain::StopPtr stop) const;

	std::optional<RouteInfo> GetRouteInfo(graph::VertexId from, graph::VertexId to) const;

	const std::unordered_map<domain::StopPtr, StopPairVertexId>& GetStopToVertexIds() const;
	const std::unordered_map<graph::EdgeId, EdgeInfo>& GetEdgeIdToEdgeInfo() const;



private:
	RoutingSettings settings_;
	std::unique_ptr<graph::DirectedWeightedGraph<Minutes>> graph_ptr_;
	std::unique_ptr<graph::Router<Minutes>> router_ptr_;
	std::unordered_map<domain::StopPtr, StopPairVertexId> stop_ptr_to_pair_id_;
	std::unordered_map<graph::EdgeId, EdgeInfo> edge_id_to_type_;

private:
	void FillGraph(const TransportCatalogue& transport_catalogue);

	void FillStopIdDictionaries(const std::deque<domain::StopPtr>& stops);

	void AddWaitEdges();

	void AddBusEdges(const TransportCatalogue& transport_catalogue);

	template <typename InputIt>
	void ParseBusRouteOnEdges(InputIt first, InputIt last,
			const TransportCatalogue& transport_catalogue, domain::BusPtr bus_ptr);

	graph::Edge<Minutes> MakeBusEdge(domain::StopPtr from, domain::StopPtr to, const double distance) const;
};

template <typename InputIt>
void TransportRouter::ParseBusRouteOnEdges(InputIt first, InputIt last,
		const TransportCatalogue& transport_catalogue, domain::BusPtr bus_ptr) {
	for (auto iter = first; iter != last; ++iter) {
		const auto from = *iter;

		{
			size_t distance = 0;
			size_t span_count = 0;

			for (auto jter = next(iter); jter != last; ++jter) {
				const auto before_to = *prev(jter);
				const auto to = *jter;
				distance += transport_catalogue.FindDistance(before_to, to);
				++span_count;

				graph::EdgeId id = graph_ptr_->AddEdge(MakeBusEdge(from, to, distance));
				edge_id_to_type_[id] = BusEdgeInfo{bus_ptr->name, span_count, graph_ptr_->GetEdge(id).weight};
			}
		}
	}
}

}  // namespace transport_catalogue::transport_router
