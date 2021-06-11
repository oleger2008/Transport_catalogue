#include "request_handler.h"
#include "router.h"



namespace transport_catalogue::request_handler {

namespace detail {

namespace load {

graph::Edge<transport_router::Minutes> Edge(const graph_proto::Edge& edge) {
	graph::Edge<transport_router::Minutes> result;
	result.from = edge.from();
	result.to = edge.to();
	result.weight = edge.weight();
	return result;
}

std::optional<graph::Router<transport_router::Minutes>::RouteInternalData>
RouteInternalData(const graph_proto::RouteInternalData& route) {
	graph::Router<transport_router::Minutes>::RouteInternalData result;
	if (route.has_data()) {
		result.weight = route.weight();
		if (route.has_prev_edge()) {
			result.prev_edge = route.prev_edge();
		} else {
			result.prev_edge = std::nullopt;
		}
	} else {
		return std::nullopt;
	}
	return result;
}

std::vector<std::optional<graph::Router<transport_router::Minutes>::RouteInternalData>>
VectorRouteInternalData(const graph_proto::RoutesInternalData& routes) {
	std::vector<std::optional<graph::Router<transport_router::Minutes>::RouteInternalData>> result;
	for (const auto& route : routes.route_internal_data()) {
		result.emplace_back(load::RouteInternalData(route));
	}
	return result;
}

graph::Router<transport_router::Minutes>::RoutesInternalData RoutesInternalData(const graph_proto::Router& router) {
	graph::Router<transport_router::Minutes>::RoutesInternalData result;
	for (const auto& r : router.routes_internal_data()) {
		result.emplace_back(load::VectorRouteInternalData(r));
	}
	return result;
}

}  // namespace load

}  // namespace detail

// ------------ RequestHandler ----------------------------

RequestHandler::RequestHandler (const TransportCatalogue& db,
		const renderer::MapRenderer& renderer,
		const transport_router::TransportRouter& router)
	: db_(db)
	, renderer_(renderer)
	, router_(router)
	{}

std::optional<domain::BusStat>
RequestHandler::GetBusStat(std::string_view bus_name) const {
	const auto bus = db_.FindBus(bus_name);
	if (!bus) {
		return std::nullopt;
	} else {
		return db_.GetRouteInfo(bus);
	}
}

std::optional<std::unordered_set<domain::BusPtr>>
RequestHandler::GetBusesByStop(std::string_view stop_name) const {
	const auto stop = db_.FindStop(stop_name);
	if (!stop) {
		return std::nullopt;
	} else {
		return db_.GetStopInfo(stop);
	}
}

const svg::Document& RequestHandler::RenderMap() const {
	return renderer_.GetRenderedMap();
}

std::optional<transport_router::RouteInfo>
RequestHandler::GetRouteInfo(std::string_view from_name, std::string_view to_name) const {
	auto from = db_.FindStop(from_name);
	auto to = db_.FindStop(to_name);
	if (!from || !to) {
		return std::nullopt;
	}

	auto from_ids = router_.GetPairVertexId(from);
	auto to_ids = router_.GetPairVertexId(to);
	if (!from_ids || !to_ids) {
		return std::nullopt;
	}

	const auto& route_info = router_.GetRouteInfo(from_ids->bus_wait_begin, to_ids->bus_wait_begin);
	if (!route_info) {
		return std::nullopt;
	}

	return route_info;
}

// ------------ RequestHandlerProto -----------------------

RequestHandlerProto::RequestHandlerProto(
		const transport_catalogue_proto::DataBase& db,
		const renderer::MapRenderer& renderer
		)
	: db_(db)
	, renderer_(renderer)
	{}

void RequestHandlerProto::FillGraph() {
	const auto& graph = db_.transport_router().graph();
	graph_ptr_ = std::make_unique<graph::DirectedWeightedGraph<transport_router::Minutes>>(graph.vertex_count());
	for (const auto& edge : graph.edge()) {
		graph_ptr_->AddEdge(detail::load::Edge(edge));
	}
}
void RequestHandlerProto::FillRouter() {
	router_ptr_ = std::make_unique<graph::Router<transport_router::Minutes>>(*graph_ptr_);
	router_ptr_->SetRoutesInternalData(std::move(detail::load::RoutesInternalData(db_.transport_router().router())));
}

const transport_catalogue_proto::TransportCatalogue&
RequestHandlerProto::GetTransportCatalogue() const {
	return db_.transport_catalogue();
}

std::optional<transport_catalogue_proto::Bus>
RequestHandlerProto::GetBusStat(std::string_view bus_name) const {
	const auto& buses = db_.transport_catalogue().bus();
	const auto bus_stat_iter = detail::FindByName(buses.begin(), buses.end(), bus_name);
	if (bus_stat_iter == buses.end()) {
		return std::nullopt;
	}
	return *bus_stat_iter;
}

std::optional<std::set<std::string_view>>
RequestHandlerProto::GetBusesByStop(std::string_view stop_name) const {
	const auto& stops = db_.transport_catalogue().stop();
	const auto stop_stat_iter = detail::FindByName(stops.begin(), stops.end(), stop_name);

	if (stop_stat_iter == stops.end()) {
		return std::nullopt;
	}

	std::set<std::string_view> sorted_bus_names;
	for (const auto& bus_id : stop_stat_iter->bus_id()) {
		sorted_bus_names.insert(db_.transport_catalogue().bus(bus_id).name());
	}
	return sorted_bus_names;
}

const svg::Document& RequestHandlerProto::RenderMap() const {
	return renderer_.GetRenderedMap();
}

std::optional<transport_router_proto::RouteInfo>
RequestHandlerProto::GetRouteInfo(std::string_view from_name, std::string_view to_name) const {

	const auto& stops = db_.transport_catalogue().stop();
	const auto from_iter = detail::FindByName(stops.begin(), stops.end(), from_name);
	const auto to_iter = detail::FindByName(stops.begin(), stops.end(), to_name);
	if (from_iter == stops.end() || to_iter == stops.end()) {
		return std::nullopt;
	}

	const auto& stop_id_to_vertex_ids = db_.transport_router().stop_id_to_pair_vertex_id();
	if (!stop_id_to_vertex_ids.count(from_iter->id()) || !stop_id_to_vertex_ids.count(to_iter->id())) {
		return std::nullopt;
	}

	uint64_t begin_id = stop_id_to_vertex_ids.at(from_iter->id()).bus_wait_begin();
	uint64_t end_id = stop_id_to_vertex_ids.at(to_iter->id()).bus_wait_begin();

	const auto& route_info = router_ptr_->BuildRoute(begin_id, end_id);
	if (!route_info) {
		return std::nullopt;
	}

	transport_router_proto::RouteInfo result;
	result.set_total_time(route_info->weight);
	for (const auto edge_id : route_info->edges) {
		const auto& edge_info = db_.transport_router().edge_id_to_edge_info().at(edge_id);
		*result.add_edge_info() = edge_info;
	}

	return result;
}

}  // transport_catalogue::request_handler

