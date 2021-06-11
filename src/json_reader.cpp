#include <algorithm>
#include <cassert>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "json_reader.h"
#include <transport_catalogue.pb.h>
#include <svg.pb.h>
#include <map_renderer.pb.h>
#include <graph.pb.h>
#include <transport_router.pb.h>
#include "domain.h"
#include "graph.h"
#include "json_builder.h"
#include "json.h"
#include "map_renderer.h"
#include "request_handler.h"
#include "router.h"
#include "serialization.h"
#include "transport_catalogue.h"
#include "transport_router.h"



namespace transport_catalogue::json_reader {

namespace defaulted {

namespace detail {

struct EdgeInfoGetter {
	json::Node operator()(const transport_router::WaitEdgeInfo& edge_info) {
		using namespace std::literals;
		return json::Builder{}.StartDict()
								.Key("type"s).Value("Wait"s)
								.Key("stop_name"s).Value(std::string(edge_info.stop_name))
								.Key("time").Value(edge_info.time)
							.EndDict()
							.Build();
	}

	json::Node operator()(const transport_router::BusEdgeInfo& edge_info) {
		using namespace std::literals;
		return json::Builder{}.StartDict()
								.Key("type"s).Value("Bus"s)
								.Key("bus"s).Value(std::string(edge_info.bus_name))
								.Key("span_count"s).Value(static_cast<int>(edge_info.span_count))
								.Key("time").Value(edge_info.time)
							.EndDict()
							.Build();
	}
};

}  // namespace detail

namespace request_parser {

domain::Stop ParseStop(const json::Dict& request) {
	using namespace std::literals;
	return domain::Stop{
		request.at("name"s).AsString(),
		geo::Coordinates{request.at("latitude"s).AsDouble(), request.at("longitude"s).AsDouble()}
	};
}

std::deque<domain::FromToDistance>
ParseStopsDistances(const json::Dict& request) {
	using namespace std::literals;

	std::string_view start = request.at("name"s).AsString();

	std::deque<domain::FromToDistance> result;
	for (const auto& [destination, distance] : request.at("road_distances"s).AsDict()) {
		result.emplace_back(domain::FromToDistance{
			start, destination, static_cast<uint64_t>(distance.AsInt())});
	}

	return result;
}

domain::Bus ParseBus(const json::Dict& request,
		const TransportCatalogue& transport_catalogue) {
	using namespace std::literals;

	domain::Bus bus;

	bus.name = request.at("name"s).AsString();
	bus.is_circle = request.at("is_roundtrip"s).AsBool();
	for (const auto& stop_name : request.at("stops"s).AsArray()) {
		const auto stop = transport_catalogue.FindStop(stop_name.AsString());
		bus.route.push_back(stop);
	}

	return bus;
}

svg::Color ParseColor(const json::Node& color) {
	if (color.IsString()) {
		return color.AsString();
	}
	if (color.IsArray()) {
		const auto& color_arr = color.AsArray();
		if (color_arr.size() == 3) {
			return svg::Rgb(color_arr[0].AsInt(), color_arr[1].AsInt(), color_arr[2].AsInt());
		} else if (color_arr.size() == 4) {
			return svg::Rgba(color_arr[0].AsInt(), color_arr[1].AsInt(), color_arr[2].AsInt(), color_arr[3].AsDouble());
		} else {
			throw std::logic_error("Wrong color format");
		}
	}
	throw std::logic_error("Wrong color format");
}

renderer::RenderSettings
ParseRenderSettings(const json::Dict& render_settings) {
	using namespace std::literals;

	renderer::RenderSettings result;

	result.width = render_settings.at("width"s).AsDouble();
	result.height = render_settings.at("height"s).AsDouble();
	result.padding = render_settings.at("padding"s).AsDouble();
	result.stop_radius = render_settings.at("stop_radius"s).AsDouble();
	result.line_width = render_settings.at("line_width"s).AsDouble();
	result.bus_label_font_size = render_settings.at("bus_label_font_size"s).AsInt();

	const auto& bus_label_offset = render_settings.at("bus_label_offset"s).AsArray();
	result.bus_label_offset = svg::Point(bus_label_offset[0].AsDouble(), bus_label_offset[1].AsDouble());

	result.stop_label_font_size = render_settings.at("stop_label_font_size"s).AsInt();

	const auto& stop_label_offset = render_settings.at("stop_label_offset"s).AsArray();
	result.stop_label_offset = svg::Point(stop_label_offset[0].AsDouble(), stop_label_offset[1].AsDouble());

	result.underlayer_color = ParseColor(render_settings.at("underlayer_color"s));
	result.underlayer_width = render_settings.at("underlayer_width"s).AsDouble();

	const auto& color_palette = render_settings.at("color_palette"s).AsArray();
	for (const auto& color : color_palette) {
		result.color_palette.emplace_back(ParseColor(color));
	}

	return result;
}

transport_router::RoutingSettings ParseRoutingSettings(const json::Dict& routing_settings) {
	using namespace std::literals;

	transport_router::RoutingSettings result;

	result.bus_wait_time = routing_settings.at("bus_wait_time"s).AsDouble();
	result.bus_velocity = routing_settings.at("bus_velocity"s).AsDouble();

	return result;
}

}  // request_parser

namespace add {

void Stops(const json::Array& base_requests,
		TransportCatalogue& transport_catalogue) {
	using namespace std::literals;

	for (auto& request : base_requests) {
		if (request.AsDict().at("type"s) == "Stop"s) {
			transport_catalogue.AddStop(std::move(request_parser::ParseStop(request.AsDict())));
		}
	}
}

void StopsDistances(const json::Array& base_requests,
		TransportCatalogue& transport_catalogue) {
	using namespace std::literals;

	for (auto& request : base_requests) {
		if (request.AsDict().at("type"s) == "Stop"s) {
			transport_catalogue.AddStopsDistances(request_parser::ParseStopsDistances(request.AsDict()));
		}
	}
}

void Buses(const json::Array& base_requests,
		TransportCatalogue& transport_catalogue) {
	using namespace std::literals;

	for (auto& request : base_requests) {
		if (request.AsDict().at("type"s) == "Bus"s) {
			transport_catalogue.AddBus(std::move(request_parser::ParseBus(request.AsDict(), transport_catalogue)));
		}
	}
}

}  // namespace add

namespace make_stat {

json::Node Bus(const json::Dict& request,
			const request_handler::RequestHandler& request_handler) {
		using namespace std::literals;
		using namespace json;

		const auto& bus_stat = request_handler.GetBusStat(request.at("name"s).AsString());
		if (!bus_stat.has_value()) {
			return Builder{}.StartDict()
								.Key("request_id"s).Value(request.at("id"s).AsInt())
								.Key("error_message"s).Value("not found"s)
							.EndDict()
							.Build();
		} else {
			return Builder{}.StartDict()
								.Key("curvature"s).Value(bus_stat->curvature)
								.Key("request_id"s).Value(request.at("id"s).AsInt())
								.Key("route_length"s).Value(static_cast<int>(bus_stat->route_length))
								.Key("stop_count"s).Value(static_cast<int>(bus_stat->stop_count))
								.Key("unique_stop_count"s).Value(static_cast<int>(bus_stat->unique_stop_count))
							.EndDict()
							.Build();
		}

	}

json::Node Stop(const json::Dict& request,
		const request_handler::RequestHandler& request_handler) {
	using namespace std::literals;
	using namespace json;

	std::set<std::string_view> sorted_bus_names;
	{
		const auto& buses = request_handler.GetBusesByStop(request.at("name"s).AsString());
		if (!buses.has_value()) {
			return Builder{}.StartDict()
								.Key("request_id"s).Value(request.at("id"s).AsInt())
								.Key("error_message"s).Value("not found"s)
							.EndDict()
							.Build();
		}
		for (const auto bus : *buses) {
			sorted_bus_names.insert(bus->name);
		}
	}
	Array result;
	for (const auto bus_name : sorted_bus_names) {
		result.push_back(Node{std::string(bus_name)});
	}

	return Builder{}.StartDict()
						.Key("buses"s).Value(std::move(result))
						.Key("request_id"s).Value(request.at("id"s).AsInt())
					.EndDict()
					.Build();
}

json::Node RenderedMap(const json::Dict& request,
		const request_handler::RequestHandler& request_handler) {
		using namespace std::literals;
		using namespace json;

		std::ostringstream svg_string;
		request_handler.RenderMap().Render(svg_string);

		return Builder{}.StartDict()
							.Key("map"s).Value(std::move(svg_string.str()))
							.Key("request_id"s).Value(request.at("id"s).AsInt())
						.EndDict()
						.Build();
	}

json::Node Route(const json::Dict& request,
			const request_handler::RequestHandler& request_handler) {
	using namespace std::literals;
	using namespace json;

	const auto& route_info = request_handler.GetRouteInfo(request.at("from"s).AsString(), request.at("to"s).AsString());
	if (!route_info) {
		return Builder{}.StartDict()
							.Key("request_id"s).Value(request.at("id"s).AsInt())
							.Key("error_message"s).Value("not found"s)
						.EndDict()
						.Build();
	}

	Array items;
	for (const auto& item : route_info->edges) {
		items.emplace_back(std::visit(detail::EdgeInfoGetter{}, item));
	}

	return Builder{}.StartDict()
						.Key("request_id"s).Value(request.at("id"s).AsInt())
						.Key("total_time"s).Value(route_info->total_time)
						.Key("items"s).Value(items)
					.EndDict()
					.Build();
}

}  // make_stat

namespace fill {

void TransportCatalogue(const json::Array& base_requests,
		transport_catalogue::TransportCatalogue& transport_catalogue) {
	add::Stops(base_requests, transport_catalogue);
	add::StopsDistances(base_requests, transport_catalogue);
	add::Buses(base_requests, transport_catalogue);
}

void MapRenderer(const json::Dict& render_settings,
		renderer::MapRenderer& map_renderer,
		transport_catalogue::TransportCatalogue& transport_catalogue) {
	map_renderer.SetRenderSettings(std::move(request_parser::ParseRenderSettings(render_settings)));
	map_renderer.RenderMap(transport_catalogue.GetAllBusesSortedByName(), transport_catalogue.GetNonEmptyStopsSortedByName());
}

void TransportRouter(const json::Dict& routing_settings,
		transport_router::TransportRouter& transport_router,
		transport_catalogue::TransportCatalogue& transport_catalogue
		) {

	transport_router.SetRoutingSettings(std::move(request_parser::ParseRoutingSettings(routing_settings)));
	transport_router.BuildRouter(transport_catalogue);
}

}  // namespace fill

void MakeStatAnswer(std::ostream& output, const json::Array& stat_requests,
		const request_handler::RequestHandler& request_handler) {
	using namespace std::literals;
	using namespace json;

	Array result;
	for (const auto& request : stat_requests) {
		const std::string& type = request.AsDict().at("type"s).AsString();
		if (type == "Bus"s) {
			result.emplace_back(make_stat::Bus(request.AsDict(), request_handler));
		} else if (type == "Stop"s) {
			result.emplace_back(make_stat::Stop(request.AsDict(), request_handler));
		} else if (type == "Map"s) {
			result.emplace_back(make_stat::RenderedMap(request.AsDict(), request_handler));
		} else if (type == "Route"s) {
			result.emplace_back(make_stat::Route(request.AsDict(), request_handler));
		}
	}
	Print(Document{Builder{}.Value(std::move(result)).Build()}, output);
}

}  // namespace defaulted



namespace proto {

namespace detail {

json::Node ConvertEdgeInfo(const transport_router_proto::EdgeInfo& edge_info,
		const request_handler::RequestHandlerProto& request_handler) {
	using namespace std::literals;

	switch(edge_info.edge_info_case()) {
		case transport_router_proto::EdgeInfo::EdgeInfoCase::kWaitEdgeInfo : {
			const auto& source = edge_info.wait_edge_info();
			std::string_view stop_name = request_handler.GetTransportCatalogue().stop(source.stop_id()).name();
			return json::Builder{}.StartDict()
									.Key("type"s).Value("Wait"s)
									.Key("stop_name"s).Value(std::string(stop_name))
									.Key("time").Value(source.minutes())
								.EndDict()
								.Build();
		}
		case transport_router_proto::EdgeInfo::EdgeInfoCase::kBusEdgeInfo : {
			const auto& source = edge_info.bus_edge_info();
			std::string_view bus_name = request_handler.GetTransportCatalogue().bus(source.bus_id()).name();
			return json::Builder{}.StartDict()
									.Key("type"s).Value("Bus"s)
									.Key("bus"s).Value(std::string(bus_name))
									.Key("span_count"s).Value(static_cast<int>(source.span_count()))
									.Key("time").Value(source.minutes())
								.EndDict()
								.Build();
		}
	}

	return {};
}

}  // namespace detail

namespace fill {

namespace convert_to_svg {

svg::Point Point(const svg_proto::Point& p) {
	svg::Point result;
	result.x = p.x();
	result.y = p.y();
	return result;
}

svg::Rgb Rgb(const svg_proto::Rgba& rgba) {
	svg::Rgb result;
	result.red = rgba.red();
	result.green = rgba.green();
	result.blue = rgba.blue();
	return result;
}

svg::Rgba Rgba(const svg_proto::Rgba& rgba) {
	svg::Rgba result;
	result.red = rgba.red();
	result.green = rgba.green();
	result.blue = rgba.blue();
	result.opacity = rgba.opacity();
	return result;
}

svg::Color Color(const svg_proto::Color& color) {
	svg::Color result;
	switch(color.data_case()) {
		case (svg_proto::Color::DataCase::kIsNone):
			break;
		case (svg_proto::Color::DataCase::kStr) :
			result = color.str();
			break;
		case (svg_proto::Color::DataCase::kRgba) :
			if (color.rgba().has_opacity()) {
				result = Rgba(color.rgba());
			} else {
				result = Rgb(color.rgba());
			}
			break;
		case (svg_proto::Color::DataCase::DATA_NOT_SET) :
			break;
	}
	return result;
}

renderer::RenderSettings RenderSettings(
		const map_renderer_proto::RenderSettings& render_settings) {
	renderer::RenderSettings result;

	result.width = render_settings.width();
	result.height = render_settings.height();
	result.padding = render_settings.padding();
	result.stop_radius = render_settings.stop_radius();
	result.line_width = render_settings.line_width();
	result.bus_label_font_size = render_settings.bus_label_font_size();
	result.bus_label_offset = Point(render_settings.bus_label_offset());
	result.stop_label_font_size = render_settings.stop_label_font_size();
	result.stop_label_offset = Point(render_settings.stop_label_offset());
	result.underlayer_color = Color(render_settings.underlayer_color());
	result.underlayer_width = render_settings.underlayer_width();
	const auto& color_palette = render_settings.color_palette();
	for (const auto& color : color_palette) {
		result.color_palette.emplace_back(Color(color));
	}

	return result;
}

}  // namespace convert_to_svg

namespace load {

domain::Stop Stop(const transport_catalogue_proto::Stop& proto_stop) {
	domain::Stop stop;
	stop.name = proto_stop.name();
	stop.coordinates.lat = proto_stop.coordinates().lat();
	stop.coordinates.lng = proto_stop.coordinates().lng();
	return stop;
}

domain::Bus MakeBus(const transport_catalogue_proto::Bus& proto_bus, const std::deque<domain::StopPtr> route) {
	domain::Bus bus;
	bus.name = proto_bus.name();
	bus.is_circle = proto_bus.is_circle();
	bus.route = std::move(route);
	return bus;
}

std::deque<domain::Bus> MakeBuses(const transport_catalogue_proto::TransportCatalogue& transport_catalogue,
		const std::deque<domain::Stop>& stops) {
	std::deque<domain::Bus> buses;
	for (const auto& proto_bus : transport_catalogue.bus()) {
		std::deque<domain::StopPtr> route;
		for (const auto& stop_id : proto_bus.stop_id()) {
			const std::string& stop_name = transport_catalogue.stop(stop_id).name();
			const auto stop_iter = std::find_if(stops.begin(), stops.end(),
					[&stop_name](const domain::Stop& stop) { return stop.name == stop_name; }
					);
			route.emplace_back(&(*stop_iter));
		}
		buses.emplace_back(std::move(MakeBus(proto_bus, std::move(route))));
	}
	return buses;
}

transport_router::RoutingSettings RoutingSettings(const transport_router_proto::RoutingSettings& settings) {
	transport_router::RoutingSettings result;
	result.bus_wait_time = settings.bus_wait_time();
	result.bus_velocity = settings.bus_velocity();
	return result;
}

}  // namespace load

void MapRenderer(renderer::MapRenderer& destination,
		const map_renderer_proto::MapRenderer& source,
		const transport_catalogue_proto::TransportCatalogue& transport_catalogue) {
	destination.SetRenderSettings(std::move(convert_to_svg::RenderSettings(source.settings())));
	destination.RenderMap(source, transport_catalogue);
}

void Router(request_handler::RequestHandlerProto& request_handler) {
	request_handler.FillGraph();
	request_handler.FillRouter();
}

}  // namespace fill

namespace make_stat {

json::Node Bus(const json::Dict& request,
		const request_handler::RequestHandlerProto& request_handler) {
		using namespace std::literals;
		using namespace json;

		const auto& bus_stat = request_handler.GetBusStat(request.at("name"s).AsString());

		if (!bus_stat) {
			return Builder{}.StartDict()
								.Key("request_id"s).Value(request.at("id"s).AsInt())
								.Key("error_message"s).Value("not found"s)
							.EndDict()
							.Build();
		} else {
			return Builder{}.StartDict()
								.Key("curvature"s).Value(bus_stat->curvature())
								.Key("request_id"s).Value(request.at("id"s).AsInt())
								.Key("route_length"s).Value(static_cast<int>(bus_stat->route_length()))
								.Key("stop_count"s).Value(static_cast<int>(bus_stat->stop_count()))
								.Key("unique_stop_count"s).Value(static_cast<int>(bus_stat->unique_stop_count()))
							.EndDict()
							.Build();
		}

	}

json::Node Stop(const json::Dict& request,
		const request_handler::RequestHandlerProto& request_handler) {
	using namespace std::literals;
	using namespace json;

	const auto& sorted_bus_names = request_handler.GetBusesByStop(request.at("name"s).AsString());

	if (!sorted_bus_names) {
		return Builder{}.StartDict()
							.Key("request_id"s).Value(request.at("id"s).AsInt())
							.Key("error_message"s).Value("not found"s)
						.EndDict()
						.Build();
	}

	Array result;
	for (const auto bus_name : *sorted_bus_names) {
		result.push_back(Node{std::string(bus_name)});
	}

	return Builder{}.StartDict()
						.Key("buses"s).Value(std::move(result))
						.Key("request_id"s).Value(request.at("id"s).AsInt())
					.EndDict()
					.Build();
}

json::Node RenderedMap(const json::Dict& request,
		const request_handler::RequestHandlerProto& request_handler) {
	using namespace std::literals;
	using namespace json;

	std::ostringstream svg_string;
	request_handler.RenderMap().Render(svg_string);

	return Builder{}.StartDict()
						.Key("map"s).Value(std::move(svg_string.str()))
						.Key("request_id"s).Value(request.at("id"s).AsInt())
					.EndDict()
					.Build();
}

json::Node Route(const json::Dict& request,
			const request_handler::RequestHandlerProto& request_handler) {
	using namespace std::literals;
	using namespace json;

	const auto& route_info = request_handler.GetRouteInfo(request.at("from"s).AsString(), request.at("to"s).AsString());
	if (!route_info) {
		return Builder{}.StartDict()
							.Key("request_id"s).Value(request.at("id"s).AsInt())
							.Key("error_message"s).Value("not found"s)
						.EndDict()
						.Build();
	}

	Array items;
	for (const auto& edge_info : route_info->edge_info()) {
		items.emplace_back(detail::ConvertEdgeInfo(edge_info, request_handler));
	}

	return Builder{}.StartDict()
						.Key("request_id"s).Value(request.at("id"s).AsInt())
						.Key("total_time"s).Value(route_info->total_time())
						.Key("items"s).Value(items)
					.EndDict()
					.Build();
}

}  // namespace make_stat

void MakeStatAnswer(std::ostream& output, const json::Array& stat_requests,
		const request_handler::RequestHandlerProto& request_handler) {
	using namespace std::literals;
	using namespace json;

	Array result;
	for (const auto& request : stat_requests) {
		const std::string& type = request.AsDict().at("type"s).AsString();
		if (type == "Bus"s) {
			result.emplace_back(make_stat::Bus(request.AsDict(), request_handler));
		} else if (type == "Stop"s) {
			result.emplace_back(make_stat::Stop(request.AsDict(), request_handler));
		}
		else if (type == "Map"s) {
			result.emplace_back(make_stat::RenderedMap(request.AsDict(), request_handler));
		}
		else if (type == "Route"s) {
			result.emplace_back(make_stat::Route(request.AsDict(), request_handler));
		}
	}
	Print(Document{Builder{}.Value(std::move(result)).Build()}, output);
}

}  // namespace proto

void MakeBase(std::istream& input) {
	using namespace std::literals;

	TransportCatalogue transport_catalogue;
	transport_router::TransportRouter transport_router;

	const json::Document& doc = json::Load(input);
	const auto& commands = doc.GetRoot().AsDict();
	serialization::DataBaseSerializer db;

	if (commands.count("base_requests"s)) {
		defaulted::fill::TransportCatalogue(commands.at("base_requests"s).AsArray(), transport_catalogue);
	}
	if (commands.count("serialization_settings"s)) {
		const std::string& file_name = commands.at("serialization_settings"s).AsDict().at("file"s).AsString();
		db.SetFileName(file_name);
		db.SetTransportCatalogue(transport_catalogue);
	}
	if (commands.count("render_settings"s)) {
		const auto& rs = defaulted::request_parser::ParseRenderSettings(commands.at("render_settings"s).AsDict());
		db.SetMapRenderer(rs, transport_catalogue);
	}
	if (commands.count("routing_settings"s)) {
		transport_router.SetRoutingSettings(std::move(
				defaulted::request_parser::ParseRoutingSettings(
						commands.at("routing_settings"s).AsDict())));
		transport_router.BuildRouter(transport_catalogue);
		db.SetTransportRouter(transport_router, transport_catalogue);
	}

	db.Serialize();
}

void ProcessRequests(std::istream& input, std::ostream& output) {
	using namespace std::literals;

	transport_catalogue_proto::DataBase db;
	renderer::MapRenderer map_renderer;
	request_handler::RequestHandlerProto request_handler(db, map_renderer);

	const json::Document& doc = json::Load(input);
	const auto& commands = doc.GetRoot().AsDict();
	if (commands.count("serialization_settings"s)) {
		const auto& file_name = commands.at("serialization_settings"s).AsDict().at("file"s).AsString();
		std::ifstream file(file_name, std::ios::binary);
		if (!db.ParseFromIstream(&file)) {
			std::cerr << "Deserialize failed" << std::endl;
//			assert(false);
			return;
		}

		proto::fill::MapRenderer(map_renderer,  db.map_renderer(), db.transport_catalogue());
		proto::fill::Router(request_handler);
	}
	if (commands.count("stat_requests"s)) {
		proto::MakeStatAnswer(output, commands.at("stat_requests"s).AsArray(), request_handler);
	}
}

}  // namespace transport_catalogue::json_reader
