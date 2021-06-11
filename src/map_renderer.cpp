#include "map_renderer.h"

namespace transport_catalogue::renderer {

namespace detail {

std::deque<geo::Coordinates>
MakeStopCoordinatesDeque(const std::deque<domain::StopPtr>& stops) {
	std::deque<geo::Coordinates> result;
	for (const auto& stop : stops) {
		result.push_back(stop->coordinates);
	}
	return result;
}

svg::Point LoadPoint(const svg_proto::Point& p) {
	svg::Point result;
	result.x = p.x();
	result.y = p.y();
	return result;
}

}  // namespace detail

// ---------- SphereProjector -------------------

bool IsZero(double value) {
	return std::abs(value) < EPSILON;
}

svg::Point SphereProjector::operator()(geo::Coordinates coords) const {
	return {(coords.lng - min_lon_) * zoom_coeff_ + padding_,
			(max_lat_ - coords.lat) * zoom_coeff_ + padding_};
}

// ---------- MapRenderer -----------------------

void MapRenderer::SetRenderSettings(RenderSettings settings) {
	settings_ = std::move(settings);
}

void MapRenderer::RenderMap(const std::deque<domain::BusPtr>& buses, const std::deque<domain::StopPtr>& stops) {
	const auto& stop_coordinates = detail::MakeStopCoordinatesDeque(stops);
	SphereProjector sphere_projector(stop_coordinates.begin(), stop_coordinates.end(),
			settings_.width, settings_.height, settings_.padding);

	AddBusLines(buses, sphere_projector);
	AddBusNames(buses, sphere_projector);
	AddStopCircles(stops, sphere_projector);
	AddStopNames(stops, sphere_projector);
}

const svg::Document& MapRenderer::GetRenderedMap() const {
	return map_;
}

// добавляет линии автобусных маршрутов
void MapRenderer::AddBusLines(const std::deque<domain::BusPtr>& buses,
		const SphereProjector& sphere_projector) {
	size_t non_empty_bus_counter = 0;
	size_t color_pallete_size = settings_.color_palette.size();
	for (const auto& bus : buses) {
		if (bus->route.empty()) {
			continue;
		}
		svg::Polyline bus_line;
		bus_line.SetFillColor(svg::NoneColor)
				.SetStrokeColor(settings_.color_palette[non_empty_bus_counter++ % color_pallete_size])
				.SetStrokeWidth(settings_.line_width)
				.SetStrokeLineCap(svg::StrokeLineCap::ROUND)
				.SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
		for (const auto& stop : bus->route) {
			bus_line.AddPoint(sphere_projector(stop->coordinates));
		}
		if (!bus->is_circle) {
			bool is_first = true;
			for (auto stop_iter = bus->route.rbegin(); stop_iter != bus->route.rend(); ++stop_iter) {
				if (is_first) {
					is_first = false;
					continue;
				}
				bus_line.AddPoint(sphere_projector((*stop_iter)->coordinates));
			}
		}
		map_.Add(bus_line);
	}
}

svg::Text MapRenderer::MakeBusUnderlayerText(const std::string& bus_name,
		svg::Point stop_coordinates) {
	using namespace std::literals;

	svg::Text underlayer;
	underlayer.SetFillColor(settings_.underlayer_color)
			.SetStrokeColor(settings_.underlayer_color)
			.SetStrokeWidth(settings_.underlayer_width)
			.SetStrokeLineCap(svg::StrokeLineCap::ROUND)
			.SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
	underlayer.SetPosition(stop_coordinates)
			.SetOffset(settings_.bus_label_offset)
			.SetFontSize(settings_.bus_label_font_size)
			.SetFontFamily("Verdana"s)
			.SetFontWeight("bold"s)
			.SetData(bus_name);

	return underlayer;
}

svg::Text MapRenderer::MakeBusLabelText(const std::string& bus_name,
		svg::Point stop_coordinates, const size_t palette_index) {
	using namespace std::literals;

	svg::Text bus_label;
	bus_label.SetFillColor(settings_.color_palette[palette_index]);
	bus_label.SetPosition(stop_coordinates)
			.SetOffset(settings_.bus_label_offset)
			.SetFontSize(settings_.bus_label_font_size)
			.SetFontFamily("Verdana"s)
			.SetFontWeight("bold"s)
			.SetData(bus_name);

	return bus_label;
}

// добавляет названия автобусных маршрутов
void MapRenderer::AddBusNames(const std::deque<domain::BusPtr>& buses,
		const SphereProjector& sphere_projector) {
	size_t non_empty_bus_counter = 0;
	size_t color_pallete_size = settings_.color_palette.size();
	for (const auto& bus : buses) {
		if (bus->route.empty()) {
			continue;
		}
		const size_t palette_index = non_empty_bus_counter++ % color_pallete_size;
		map_.Add(MakeBusUnderlayerText(bus->name, sphere_projector(bus->route.front()->coordinates)));
		map_.Add(MakeBusLabelText(bus->name, sphere_projector(bus->route.front()->coordinates), palette_index));

		if (!bus->is_circle && bus->route.front() != bus->route.back()) {
			map_.Add(MakeBusUnderlayerText(bus->name, sphere_projector(bus->route.back()->coordinates)));
			map_.Add(MakeBusLabelText(bus->name, sphere_projector(bus->route.back()->coordinates), palette_index));
		}
	}
}

svg::Circle MapRenderer::MakeStopCircle(svg::Point stop_coordinates) {
	using namespace std::literals;

	svg::Circle stop_circle;
	stop_circle.SetCenter(stop_coordinates)
			.SetRadius(settings_.stop_radius)
			.SetFillColor("white"s);
	return stop_circle;
}

void MapRenderer::AddStopCircles(const std::deque<domain::StopPtr>& stops,
		const SphereProjector& sphere_projector) {
	for (const auto& stop : stops) {
		map_.Add(MakeStopCircle(sphere_projector(stop->coordinates)));
	}
}

svg::Text MapRenderer::MakeStopUnderlayerText(const std::string& stop_name,
		svg::Point stop_coordinates) {
	using namespace std::literals;

	svg::Text underlayer;
	underlayer.SetFillColor(settings_.underlayer_color)
			.SetStrokeColor(settings_.underlayer_color)
			.SetStrokeWidth(settings_.underlayer_width)
			.SetStrokeLineCap(svg::StrokeLineCap::ROUND)
			.SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
	underlayer.SetPosition(stop_coordinates)
			.SetOffset(settings_.stop_label_offset)
			.SetFontSize(settings_.stop_label_font_size)
			.SetFontFamily("Verdana"s)
			.SetData(stop_name);

	return underlayer;
}

svg::Text MapRenderer::MakeStopLabelText(const std::string& stop_name,
		svg::Point stop_coordinates) {
	using namespace std::literals;

	svg::Text bus_label;
	bus_label.SetFillColor("black"s);
	bus_label.SetPosition(stop_coordinates)
			.SetOffset(settings_.stop_label_offset)
			.SetFontSize(settings_.stop_label_font_size)
			.SetFontFamily("Verdana"s)
			.SetData(stop_name);

	return bus_label;
}

void MapRenderer::AddStopNames(const std::deque<domain::StopPtr>& stops,
		const SphereProjector& sphere_projector) {
	for (const auto& stop : stops) {
		map_.Add(MakeStopUnderlayerText(stop->name, sphere_projector(stop->coordinates)));
		map_.Add(MakeStopLabelText(stop->name, sphere_projector(stop->coordinates)));
	}
}

void MapRenderer::RenderMap(const map_renderer_proto::MapRenderer& map_renderer,
		const transport_catalogue_proto::TransportCatalogue& transport_catalogue) {
	AddBusLines(map_renderer, transport_catalogue);
	AddBusNames(map_renderer, transport_catalogue);
	AddStopCircles(map_renderer, transport_catalogue);
	AddStopNames(map_renderer, transport_catalogue);
}

void MapRenderer::AddBusLines(const map_renderer_proto::MapRenderer& map_renderer,
		const transport_catalogue_proto::TransportCatalogue& transport_catalogue) {
	size_t non_empty_bus_counter = 0;
	size_t color_pallete_size = settings_.color_palette.size();
	for (const auto& polyline : map_renderer.polyline()) {
		const size_t size = polyline.point_size();
		if (!size) {
			continue;
		}
		svg::Polyline bus_line;
		bus_line.SetFillColor(svg::NoneColor)
				.SetStrokeColor(settings_.color_palette[non_empty_bus_counter++ % color_pallete_size])
				.SetStrokeWidth(settings_.line_width)
				.SetStrokeLineCap(svg::StrokeLineCap::ROUND)
				.SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
		for (const auto& point : polyline.point()) {
			bus_line.AddPoint(detail::LoadPoint(point));
		}
		if (!transport_catalogue.bus(polyline.bus_id()).is_circle() && size != 1) {
			for (int i = static_cast<int>(size) - 2; i >= 0; --i) {
				bus_line.AddPoint(detail::LoadPoint(polyline.point(i)));
			}
		}
		map_.Add(bus_line);
	}
}

void MapRenderer::AddBusNames(const map_renderer_proto::MapRenderer& map_renderer,
		const transport_catalogue_proto::TransportCatalogue& transport_catalogue) {
	size_t non_empty_bus_counter = 0;
	size_t color_pallete_size = settings_.color_palette.size();
	for (const auto& polyline : map_renderer.polyline()) {
		const size_t size = polyline.point_size();
		if (!size) {
			continue;
		}
		const size_t palette_index = non_empty_bus_counter++ % color_pallete_size;
		const auto& bus = transport_catalogue.bus(polyline.bus_id());

		map_.Add(MakeBusUnderlayerText(bus.name(), detail::LoadPoint(polyline.point(0))));
		map_.Add(MakeBusLabelText(bus.name(), detail::LoadPoint(polyline.point(0)), palette_index));

		if (!bus.is_circle() && size != 1) {
			map_.Add(MakeBusUnderlayerText(bus.name(), detail::LoadPoint(polyline.point(size - 1u))));
			map_.Add(MakeBusLabelText(bus.name(), detail::LoadPoint(polyline.point(size - 1u)), palette_index));
		}
	}
}

void MapRenderer::AddStopCircles(const map_renderer_proto::MapRenderer& map_renderer,
		const transport_catalogue_proto::TransportCatalogue& transport_catalogue) {
	for (const auto& circle : map_renderer.circle()) {
		map_.Add(MakeStopCircle(detail::LoadPoint(circle.pos())));
	}
}

void MapRenderer::AddStopNames(const map_renderer_proto::MapRenderer& map_renderer,
		const transport_catalogue_proto::TransportCatalogue& transport_catalogue) {
	for (const auto& circle : map_renderer.circle()) {
		const std::string& stop_name = transport_catalogue.stop(circle.stop_id()).name();
		map_.Add(MakeStopUnderlayerText(stop_name, detail::LoadPoint(circle.pos())));
		map_.Add(MakeStopLabelText(stop_name, detail::LoadPoint(circle.pos())));
	}
}

}  // namespace transport_catalogue::renderer
