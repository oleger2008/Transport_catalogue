#pragma once

#include <algorithm>
#include <deque>
#include <iostream>
#include <set>
#include <variant>
#include <vector>

#include <transport_catalogue.pb.h>
#include <svg.pb.h>
#include <map_renderer.pb.h>

#include "domain.h"
#include "svg.h"
#include "geo.h"

namespace transport_catalogue::renderer {

namespace detail {

std::deque<geo::Coordinates> MakeStopCoordinatesDeque(const std::deque<domain::StopPtr>& stops);

svg::Point LoadPoint(const svg_proto::Point& p);

}  // namespace detail

using Pallete = std::vector<svg::Color>;

struct RenderSettings {
	double width = 0.;
	double height = 0.;
	double padding = 0.;
	double stop_radius = 0.;
	double line_width = 0.;
	int bus_label_font_size = 0;
	svg::Point bus_label_offset;
	int stop_label_font_size = 0;
	svg::Point stop_label_offset;
	svg::Color underlayer_color;
	double underlayer_width = 0.;
	Pallete color_palette;
};

inline const double EPSILON = 1e-6;
bool IsZero(double value);

class SphereProjector {
public:
	template <typename PointInputIt>
	SphereProjector(PointInputIt points_begin, PointInputIt points_end, double max_width,
					double max_height, double padding)
		: padding_(padding) {
		if (points_begin == points_end) {
			return;
		}

		const auto [left_it, right_it]
			= std::minmax_element(points_begin, points_end, [](auto lhs, auto rhs) {
				return lhs.lng < rhs.lng;
			});
		min_lon_ = left_it->lng;
		const double max_lon = right_it->lng;

		const auto [bottom_it, top_it]
			= std::minmax_element(points_begin, points_end, [](auto lhs, auto rhs) {
				  return lhs.lat < rhs.lat;
			  });
		const double min_lat = bottom_it->lat;
		max_lat_ = top_it->lat;

		std::optional<double> width_zoom;
		if (!IsZero(max_lon - min_lon_)) {
			width_zoom = (max_width - 2 * padding) / (max_lon - min_lon_);
		}

		std::optional<double> height_zoom;
		if (!IsZero(max_lat_ - min_lat)) {
			height_zoom = (max_height - 2 * padding) / (max_lat_ - min_lat);
		}

		if (width_zoom && height_zoom) {
			zoom_coeff_ = std::min(*width_zoom, *height_zoom);
		} else if (width_zoom) {
			zoom_coeff_ = *width_zoom;
		} else if (height_zoom) {
			zoom_coeff_ = *height_zoom;
		}
	}

	svg::Point operator()(geo::Coordinates coords) const;

private:
	double padding_;
	double min_lon_ = 0;
	double max_lat_ = 0;
	double zoom_coeff_ = 0;
};

class MapRenderer {
public:
	explicit MapRenderer() = default;

	void SetRenderSettings(RenderSettings settings);

	void RenderMap(const std::deque<domain::BusPtr>& buses, const std::deque<domain::StopPtr>& stops);

	void RenderMap(const map_renderer_proto::MapRenderer& map_renderer,
			const transport_catalogue_proto::TransportCatalogue& transport_catalogue);

	const svg::Document& GetRenderedMap() const;

private:
	RenderSettings settings_;
	svg::Document map_;

private:
	void AddBusLines(const std::deque<domain::BusPtr>& buses, const SphereProjector& sphere_projector);

	svg::Text MakeBusUnderlayerText(const std::string& bus_name, svg::Point stop_coordinates);

	svg::Text MakeBusLabelText(const std::string& bus_name, svg::Point stop_coordinates, const size_t palette_index);

	void AddBusNames(const std::deque<domain::BusPtr>& buses, const SphereProjector& sphere_projector);

	svg::Circle MakeStopCircle(svg::Point stop_coordinates);

	void AddStopCircles(const std::deque<domain::StopPtr>& stops, const SphereProjector& sphere_projector);

	svg::Text MakeStopUnderlayerText(const std::string& stop_name, svg::Point stop_coordinates);

	svg::Text MakeStopLabelText(const std::string& stop_name, svg::Point stop_coordinates);

	void AddStopNames(const std::deque<domain::StopPtr>& stops, const SphereProjector& sphere_projector);

private:
	void AddBusLines(const map_renderer_proto::MapRenderer& map_renderer,
			const transport_catalogue_proto::TransportCatalogue& transport_catalogue);
	void AddBusNames(const map_renderer_proto::MapRenderer& map_renderer,
			const transport_catalogue_proto::TransportCatalogue& transport_catalogue);
	void AddStopCircles(const map_renderer_proto::MapRenderer& map_renderer,
			const transport_catalogue_proto::TransportCatalogue& transport_catalogue);
	void AddStopNames(const map_renderer_proto::MapRenderer& map_renderer,
			const transport_catalogue_proto::TransportCatalogue& transport_catalogue);

};

}  // transport_catalogue::renderer
