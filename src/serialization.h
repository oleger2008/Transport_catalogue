#include <iostream>

#include <transport_catalogue.pb.h>
#include <svg.pb.h>
#include <map_renderer.pb.h>
#include <graph.pb.h>
#include <transport_router.pb.h>

#include "transport_catalogue.h"
#include "map_renderer.h"
#include "transport_router.h"

namespace transport_catalogue::serialization {

class DataBaseSerializer {
public:
	DataBaseSerializer() = default;
	void SetFileName(const std::string& file_name);
	void SetTransportCatalogue(const transport_catalogue::TransportCatalogue& transport_catalogue);
	void SetMapRenderer(const renderer::RenderSettings& render_settings,
			const transport_catalogue::TransportCatalogue& transport_catalogue);
	void SetTransportRouter(const transport_router::TransportRouter& transport_router,
			const transport_catalogue::TransportCatalogue& transport_catalogue);
	void Serialize() const;

private:
	std::string file_name_;
	transport_catalogue_proto::DataBase db_;
};

}  // namespace transport_catalogue::serialization
