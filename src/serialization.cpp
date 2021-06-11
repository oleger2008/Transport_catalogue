#include <algorithm>
#include <cassert>
#include <deque>
#include <fstream>

#include "serialization.h"

#include "domain.h"
#include "map_renderer.h"



namespace transport_catalogue::serialization {

namespace {

namespace make {
svg_proto::Rgba Rgba(const svg::Rgb& rgb);
svg_proto::Rgba Rgba(const svg::Rgba& rgba);
transport_router_proto::WaitEdgeInfo WaitEdgeInfo(const transport_router::WaitEdgeInfo& edge_info,
		const std::deque<domain::Stop>& stops);
transport_router_proto::BusEdgeInfo BusEdgeInfo(const transport_router::BusEdgeInfo& edge_info,
		const std::deque<domain::Bus>& buses);
}  // namespace make

namespace detail {

struct ColorGetter {
	svg_proto::Color operator()(std::monostate) {
		svg_proto::Color result;
		result.set_is_none(true);
		return result;
	}
	svg_proto::Color operator()(const std::string& s) {
		svg_proto::Color result;
		result.set_str(s);
		return result;
	}
	svg_proto::Color operator()(const svg::Rgb& rgb) {
		svg_proto::Color result;
		*result.mutable_rgba() = std::move(make::Rgba(rgb));
		return result;
	}
	svg_proto::Color operator()(const svg::Rgba& rgba) {
		svg_proto::Color result;
		*result.mutable_rgba() = std::move(make::Rgba(rgba));
		return result;
	}
};

const uint64_t GetStopId(std::string_view stop_name, const std::deque<domain::Stop>& stops) {
	const auto pos = std::find_if(stops.begin(), stops.end(),
			[&stop_name](const domain::Stop& stop) { return stop.name == stop_name;});
	assert(pos != stops.end());
	return static_cast<uint64_t>(std::distance(stops.begin(), pos));
}
const uint64_t GetBusId(std::string_view bus_name, const std::deque<domain::Bus>& buses) {
	const auto pos = std::find_if(buses.begin(), buses.end(),
			[&bus_name](const domain::Bus& bus) { return bus.name == bus_name;});
	assert(pos != buses.end());
	return static_cast<uint64_t>(std::distance(buses.begin(), pos));
}

struct EdgeInfoGetter {
	const transport_catalogue::TransportCatalogue& transport_catalogue;
	transport_router_proto::EdgeInfo operator()(const transport_router::WaitEdgeInfo& edge_info) {
		transport_router_proto::EdgeInfo result;
		const auto& stops = transport_catalogue.GetStops();
		*result.mutable_wait_edge_info() = std::move(make::WaitEdgeInfo(edge_info, stops));
		return result;
	}
	transport_router_proto::EdgeInfo operator()(const transport_router::BusEdgeInfo& edge_info) {
		transport_router_proto::EdgeInfo result;
		const auto& buses = transport_catalogue.GetBuses();
		*result.mutable_bus_edge_info() = std::move(make::BusEdgeInfo(edge_info, buses));
		return result;
	}
};

}  // namespace detail

namespace make {

transport_catalogue_proto::Coordinates Coordinates(const domain::Stop& stop) {
	transport_catalogue_proto::Coordinates coordinates;
	coordinates.set_lat(stop.coordinates.lat);
	coordinates.set_lng(stop.coordinates.lng);
	return coordinates;
}

transport_catalogue_proto::Stop Stop(uint64_t id, const domain::Stop& stop, const std::deque<domain::Bus>& buses,
		const transport_catalogue::TransportCatalogue& db) {
	transport_catalogue_proto::Stop proto_stop;

	proto_stop.set_id(id);
	proto_stop.set_name(stop.name);
	*proto_stop.mutable_coordinates() = std::move(make::Coordinates(stop));

	const auto& buses_by_stop = db.GetStopInfo(&stop);
	for (const auto bus_ptr : buses_by_stop) {
		proto_stop.add_bus_id(detail::GetBusId(bus_ptr->name, buses));
	}

	return proto_stop;
}

transport_catalogue_proto::Bus Bus(uint64_t id, const domain::Bus& bus, const std::deque<domain::Stop>& stops,
		const transport_catalogue::TransportCatalogue& db) {
	transport_catalogue_proto::Bus proto_bus;

	const auto& bus_info = db.GetRouteInfo(&bus);
	proto_bus.set_id(id);
	proto_bus.set_name(bus.name);
	proto_bus.set_is_circle(bus.is_circle);

	for (const auto& stop_ptr : bus.route) {
		proto_bus.add_stop_id(detail::GetStopId(stop_ptr->name, stops));
	}

	proto_bus.set_curvature(bus_info.curvature);
	proto_bus.set_route_length(bus_info.route_length);
	proto_bus.set_stop_count(bus_info.stop_count);
	proto_bus.set_unique_stop_count(bus_info.unique_stop_count);

	return proto_bus;
}

svg_proto::Point Point(const svg::Point& p) {
	svg_proto::Point result;
	result.set_x(p.x);
	result.set_y(p.y);
	return result;
}

svg_proto::Rgba Rgba(const svg::Rgb& rgb) {
	svg_proto::Rgba result;
	result.set_has_opacity(false);
	result.set_red(rgb.red);
	result.set_green(rgb.green);
	result.set_blue(rgb.blue);
	return result;
}

svg_proto::Rgba Rgba(const svg::Rgba& rgba) {
	svg_proto::Rgba result;
	result.set_has_opacity(true);
	result.set_red(rgba.red);
	result.set_green(rgba.green);
	result.set_blue(rgba.blue);
	result.set_opacity(rgba.opacity);
	return result;
}

svg_proto::Color Color(const svg::Color& c) {
	svg_proto::Color result;
	result = std::move(std::visit(detail::ColorGetter{}, c));
	return result;
}

map_renderer_proto::RenderSettings RenderSettings(const renderer::RenderSettings& render_settings) {
	map_renderer_proto::RenderSettings settings;

	settings.set_width(render_settings.width);
	settings.set_height(render_settings.height);
	settings.set_padding(render_settings.padding);
	settings.set_stop_radius(render_settings.stop_radius);
	settings.set_line_width(render_settings.line_width);
	settings.set_bus_label_font_size(render_settings.bus_label_font_size);
	*settings.mutable_bus_label_offset() = std::move(make::Point(render_settings.bus_label_offset));
	settings.set_stop_label_font_size(render_settings.stop_label_font_size);
	*settings.mutable_stop_label_offset() = std::move(make::Point(render_settings.stop_label_offset));
	*settings.mutable_underlayer_color() = std::move(make::Color(render_settings.underlayer_color));
	settings.set_underlayer_width(render_settings.underlayer_width);
	for (const auto& color : render_settings.color_palette) {
		*settings.add_color_palette() = std::move(make::Color(color));
	}
//	std::cerr << settings.color_palette_size() << std::endl;
	return settings;
}

svg_proto::Polyline Polyline(const domain::BusPtr bus_ptr, uint64_t bus_id,
		const renderer::SphereProjector& projector) {
	svg_proto::Polyline result;
	result.set_bus_id(bus_id);
	for (const auto stop_ptr : bus_ptr->route) {
		*result.add_point() = std::move(make::Point(projector(stop_ptr->coordinates)));
	}
	return result;
}

svg_proto::Circle Circle(const domain::StopPtr stop_ptr, uint64_t stop_id,
		const renderer::SphereProjector& projector) {
	svg_proto::Circle result;
	result.set_stop_id(stop_id);
	*result.mutable_pos() = std::move(make::Point(projector(stop_ptr->coordinates)));
	return result;
}

transport_router_proto::RoutingSettings RoutingSettings(
		const transport_router::RoutingSettings& settings) {
	transport_router_proto::RoutingSettings result;
	result.set_bus_wait_time(settings.bus_wait_time);
	result.set_bus_velocity(settings.bus_velocity);
	return result;

}

graph_proto::Edge Edge(const graph::Edge<double>& edge) {
	graph_proto::Edge result;
	result.set_from(edge.from);
	result.set_to(edge.to);
	result.set_weight(edge.weight);
	return result;
}

graph_proto::DirectedWeightedGraph Graph(const graph::DirectedWeightedGraph<transport_router::Minutes>& graph) {
	graph_proto::DirectedWeightedGraph result;

	for (size_t i = 0; i < graph.GetEdgeCount(); ++i) {
		*result.add_edge() = std::move(make::Edge(graph.GetEdge(i)));
	}
	result.set_vertex_count(graph.GetVertexCount());

	return result;
}

graph_proto::RouteInternalData RouteInternalData(
		const std::optional<graph::Router<transport_router::Minutes>::RouteInternalData>& route) {
	graph_proto::RouteInternalData result;
	if (route.has_value()) {
		result.set_has_data(true);
		result.set_weight(route->weight);
		if (route->prev_edge.has_value()) {
			result.set_has_prev_edge(true);
			result.set_prev_edge(*(route->prev_edge));
		} else {
			result.set_has_prev_edge(false);
		}
	} else {
		result.set_has_data(false);
	}
	return result;
}

graph_proto::RoutesInternalData RoutesInternalData(
		const std::vector<std::optional<graph::Router<transport_router::Minutes>::RouteInternalData>>& routes) {
	graph_proto::RoutesInternalData result;
	for (const auto& route : routes) {
		*result.add_route_internal_data() = std::move(make::RouteInternalData(route));
	}
	return result;
}

graph_proto::Router Router(const graph::Router<transport_router::Minutes>& router) {
	graph_proto::Router result;
	for (const auto& routes : router.GetRoutesInternalDataRange()) {
		*result.add_routes_internal_data() = std::move(make::RoutesInternalData(routes));
	}
	return result;
}

transport_router_proto::StopPairVertexId StopPairVertexId(const transport_router::StopPairVertexId& vertex_ids) {
	transport_router_proto::StopPairVertexId result;
	result.set_bus_wait_begin(vertex_ids.bus_wait_begin);
	result.set_bus_wait_end(vertex_ids.bus_wait_end);
	return result;
}

void SetStopIdToPairVertexId(transport_router_proto::TransportRouter& transport_router,
		const std::unordered_map<domain::StopPtr, transport_router::StopPairVertexId>& source,
		const transport_catalogue::TransportCatalogue& transport_catalogue) {
	const auto& stops = transport_catalogue.GetStops();
	auto& destination = *transport_router.mutable_stop_id_to_pair_vertex_id();

	for (const auto& [stop_ptr, vertex_ids] : source) {
		const uint64_t stop_id = detail::GetStopId(stop_ptr->name, stops);
		destination[stop_id] = std::move(make::StopPairVertexId(vertex_ids));
	}
}

void SetEdgeIdToEdgeInfo(transport_router_proto::TransportRouter& transport_router,
		const std::unordered_map<graph::EdgeId, transport_router::EdgeInfo>& source,
		const transport_catalogue::TransportCatalogue& transport_catalogue) {
	auto& destination = *transport_router.mutable_edge_id_to_edge_info();

	for (const auto& [edge_id, edge_info] : source) {
		destination[edge_id] = std::move(std::visit(detail::EdgeInfoGetter{transport_catalogue}, edge_info)); // make::EdgeInfo(edge_info, transport_catalogue)
	}

}

transport_router_proto::WaitEdgeInfo WaitEdgeInfo(const transport_router::WaitEdgeInfo& edge_info,
		const std::deque<domain::Stop>& stops) {
	transport_router_proto::WaitEdgeInfo result;
	result.set_stop_id(detail::GetStopId(edge_info.stop_name, stops));
	result.set_minutes(edge_info.time);
	return result;
}
transport_router_proto::BusEdgeInfo BusEdgeInfo(const transport_router::BusEdgeInfo& edge_info,
		const std::deque<domain::Bus>& buses) {
	transport_router_proto::BusEdgeInfo result;
	result.set_bus_id(detail::GetBusId(edge_info.bus_name, buses));
	result.set_span_count(edge_info.span_count);
	result.set_minutes(edge_info.time);
	return result;
}

}  // namespace make

namespace add {

void Stops(
		const transport_catalogue::TransportCatalogue& db,
		transport_catalogue_proto::TransportCatalogue& transport_catalogue) {

	const auto& stops = db.GetStops();
	const auto& buses = db.GetBuses();
	for (size_t i = 0; i < stops.size(); ++i) {
		*transport_catalogue.add_stop() = std::move(make::Stop(i, stops[i], buses, db));
	}
}

void Buses(
		const transport_catalogue::TransportCatalogue& db,
		transport_catalogue_proto::TransportCatalogue& transport_catalogue) {

	const auto& stops = db.GetStops();
	const auto& buses = db.GetBuses();
	for (size_t i = 0; i < buses.size(); ++i) {
		*transport_catalogue.add_bus() = std::move(make::Bus(i, buses[i], stops, db));
	}
}

void Polylines(map_renderer_proto::MapRenderer& map_renderer,
		const renderer::SphereProjector& projector,
		const transport_catalogue::TransportCatalogue& transport_catalogue) {
	const auto& buses = transport_catalogue.GetAllBusesSortedByName();
	for (const auto bus_ptr : buses) {
		uint64_t bus_id = detail::GetBusId(bus_ptr->name, transport_catalogue.GetBuses());
		*map_renderer.add_polyline() = std::move(make::Polyline(bus_ptr, bus_id, projector));
	}
}

void Circles(map_renderer_proto::MapRenderer& map_renderer,
		const renderer::SphereProjector& projector,
		const transport_catalogue::TransportCatalogue& transport_catalogue) {
	const auto& stops = transport_catalogue.GetNonEmptyStopsSortedByName();
	for (const auto stop_ptr : stops) {
		uint64_t stop_id = detail::GetStopId(stop_ptr->name, transport_catalogue.GetStops());
		*map_renderer.add_circle() = std::move(make::Circle(stop_ptr, stop_id, projector));
	}
}

}  // namespace add

transport_catalogue_proto::TransportCatalogue SaveTransportCatalogue(
		const transport_catalogue::TransportCatalogue& db
		) {
	transport_catalogue_proto::TransportCatalogue transport_catalogue;

	add::Stops(db, transport_catalogue);
	add::Buses(db, transport_catalogue);

	return transport_catalogue;
}



map_renderer_proto::MapRenderer SaveMapRenderer(
		const renderer::RenderSettings& render_settings,
		const transport_catalogue::TransportCatalogue& transport_catalogue
		) {

	const auto& coordinates = renderer::detail::MakeStopCoordinatesDeque(transport_catalogue.GetNonEmptyStopsSortedByName());
	renderer::SphereProjector projector(coordinates.begin(), coordinates.end(),
			render_settings.width, render_settings.height, render_settings.padding);

	map_renderer_proto::MapRenderer map_renderer_proto;

	*map_renderer_proto.mutable_settings() = std::move(make::RenderSettings(render_settings));
	add::Polylines(map_renderer_proto, projector, transport_catalogue);
	add::Circles(map_renderer_proto, projector, transport_catalogue);

	return map_renderer_proto;
}

transport_router_proto::TransportRouter SaveTransportRouter(
		const transport_router::TransportRouter& transport_router,
		const transport_catalogue::TransportCatalogue& transport_catalogue
		) {
	transport_router_proto::TransportRouter transport_router_proto;

	const auto& stops = transport_catalogue.GetStops();
	*transport_router_proto.mutable_routing_settings() = std::move(make::RoutingSettings(transport_router.GetRoutingSettings()));
	*transport_router_proto.mutable_graph() = std::move(make::Graph(transport_router.GetGraph()));
	*transport_router_proto.mutable_router() = std::move(make::Router(transport_router.GetRouter()));
	make::SetStopIdToPairVertexId(transport_router_proto, transport_router.GetStopToVertexIds(), transport_catalogue);
	make::SetEdgeIdToEdgeInfo(transport_router_proto, transport_router.GetEdgeIdToEdgeInfo(), transport_catalogue);

	return transport_router_proto;
}

}  // namespace

// -------- DataBaseSerializer ------------------

void DataBaseSerializer::SetFileName(const std::string& file_name) {
	file_name_ = file_name;
}
void DataBaseSerializer::SetTransportCatalogue(const transport_catalogue::TransportCatalogue& transport_catalogue) {
	*db_.mutable_transport_catalogue() = std::move(SaveTransportCatalogue(transport_catalogue));
}
void DataBaseSerializer::SetMapRenderer(const renderer::RenderSettings& render_settings,
		const transport_catalogue::TransportCatalogue& transport_catalogue) {
	*db_.mutable_map_renderer() = std::move(SaveMapRenderer(render_settings, transport_catalogue));
}
void DataBaseSerializer::SetTransportRouter(const transport_router::TransportRouter& transport_router,
		const transport_catalogue::TransportCatalogue& transport_catalogue) {
	*db_.mutable_transport_router() = std::move(SaveTransportRouter(transport_router, transport_catalogue));
}
void DataBaseSerializer::Serialize() const {
	std::ofstream file(file_name_, std::ios::binary);
	db_.SerializeToOstream(&file);
}

}  // namespace transport_catalogue::serialization
