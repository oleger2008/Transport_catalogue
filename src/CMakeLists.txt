cmake_minimum_required(VERSION 3.10)

project(Transport_catalogue CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS transport_catalogue.proto svg.proto map_renderer.proto graph.proto transport_router.proto)

set(JSON_LIBRARY json.h json.cpp)
set(JSON_BUILDER_FILES json_builder.h json_builder.cpp)
set(JSON_READER_FILES json_reader.h json_reader.cpp)

set(SVG_LIBRARY svg.h svg.cpp svg.proto)
set(MAP_RENDERER map_renderer.h map_renderer.cpp map_renderer.proto)

set(TRANSPORT_ROUTER_FILES ranges.h router.h graph.h transport_router.h transport_router.cpp graph.proto transport_router.proto)

set(REQUEST_HANDLER request_handler.h request_handler.cpp)

set(TRANSPORT_CATALOGUE_FILES ${JSON_LIBRARY} ${JSON_BUILDER_FILES} ${JSON_READER_FILES} 
                              ${SVG_LIBRARY} ${MAP_RENDERER}
                              ${TRANSPORT_ROUTER_FILES}
                              ${REQUEST_HANDLER}
                              domain.h geo.h geo.cpp main.cpp
                              serialization.h serialization.cpp
                              transport_catalogue.h transport_catalogue.cpp
                              transport_catalogue.proto
                              )

add_executable(transport_catalogue ${PROTO_SRCS} ${PROTO_HDRS} ${TRANSPORT_CATALOGUE_FILES})
target_include_directories(transport_catalogue PUBLIC ${Protobuf_INCLUDE_DIRS})
target_include_directories(transport_catalogue PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

string(REPLACE "protobuf.lib" "protobufd.lib" "Protobuf_LIBRARY_DEBUG" "${Protobuf_LIBRARY_DEBUG}")
string(REPLACE "protobuf.a" "protobufd.a" "Protobuf_LIBRARY_DEBUG" "${Protobuf_LIBRARY_DEBUG}")

target_link_libraries(transport_catalogue "$<IF:$<CONFIG:Debug>,${Protobuf_LIBRARY_DEBUG},${Protobuf_LIBRARY}>" Threads::Threads)