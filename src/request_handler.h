#pragma once

#include <optional>
#include <unordered_set>

#include <transport_catalogue.pb.h>
#include <graph.pb.h>
#include <transport_router.pb.h>

#include "map_renderer.h"
#include "svg.h"
#include "router.h"
#include "transport_catalogue.h"
#include "transport_router.h"


namespace transport_catalogue::request_handler {

namespace detail {

template <typename InputIt>
InputIt FindByName(InputIt first, InputIt last, std::string_view name) {
	InputIt result = last;
	for (auto iter = first; iter != last; ++iter) {
		if (iter->name() == name) {
			result = iter;
			break;
		}
	}
	return result;
}

}  // namespace detail

class RequestHandler {
public:
	RequestHandler(const TransportCatalogue& db,
			const renderer::MapRenderer& renderer,
			const transport_router::TransportRouter& router);

	// Возвращает информацию о маршруте (запрос Bus)
	std::optional<domain::BusStat>
	GetBusStat(std::string_view bus_name) const;

	// Возвращает маршруты, проходящие через остановку
	std::optional<std::unordered_set<domain::BusPtr>>
	GetBusesByStop(std::string_view stop_name) const;

	const svg::Document& RenderMap() const;

	std::optional<transport_router::RouteInfo>
	GetRouteInfo(std::string_view from_name, std::string_view to_name) const;

private:
	const TransportCatalogue& db_;
	const renderer::MapRenderer& renderer_;
	const transport_router::TransportRouter& router_;
};

class RequestHandlerProto {

public:
	RequestHandlerProto(const transport_catalogue_proto::DataBase& db,
			const renderer::MapRenderer& renderer
			);

	void FillGraph();
	void FillRouter();

	const transport_catalogue_proto::TransportCatalogue& GetTransportCatalogue() const;

	std::optional<transport_catalogue_proto::Bus>
	GetBusStat(std::string_view bus_name) const;

	std::optional<std::set<std::string_view>>
	GetBusesByStop(std::string_view stop_name) const;

	const svg::Document& RenderMap() const;

	std::optional<transport_router_proto::RouteInfo>
	GetRouteInfo(std::string_view from_name, std::string_view to_name) const;

private:
	const transport_catalogue_proto::DataBase& db_;
	const renderer::MapRenderer& renderer_;
	std::unique_ptr<graph::DirectedWeightedGraph<transport_router::Minutes>> graph_ptr_;
	std::unique_ptr<graph::Router<transport_router::Minutes>> router_ptr_;
};

}  // transport_catalogue::request_handler
