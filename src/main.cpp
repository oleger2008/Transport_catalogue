#include <fstream>
#include <iostream>
#include <string_view>

#include "json_reader.h"
#include "map_renderer.h"
#include "request_handler.h"
#include "transport_catalogue.h"
#include "transport_router.h"

using namespace std::literals;

void PrintUsage(std::ostream& stream = std::cerr) {
	stream << "Usage: transport_catalogue [make_base|process_requests]\n"sv;
}

int main(int argc, char* argv[]) {
	using namespace transport_catalogue;

	if (argc != 2) {
		PrintUsage();
		return 1;
	}

	const std::string_view mode(argv[1]);

	if (mode == "make_base"sv) {

		json_reader::MakeBase(std::cin);

	} else if (mode == "process_requests"sv) {

		json_reader::ProcessRequests(std::cin, std::cout);

	} else {
		PrintUsage();
		return 1;
	}
}
