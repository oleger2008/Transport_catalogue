#pragma once

#include <iostream>

#include "json.h"
#include "map_renderer.h"
#include "request_handler.h"
#include "transport_catalogue.h"
#include "transport_router.h"

namespace transport_catalogue::json_reader {

void MakeBase(std::istream& input);

void ProcessRequests(std::istream& input, std::ostream& output);

}  // namespace transport_catalogue::json_reader
