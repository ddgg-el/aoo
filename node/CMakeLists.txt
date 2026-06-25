cmake_minimum_required(VERSION 3.15)
project(aoo_native CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(AOO_NET ON CACHE BOOL "" FORCE)
set(AOO_USE_OPUS ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/aoo)

add_library(${PROJECT_NAME} SHARED 
	src/addon.cpp 
	src/utils_node.cpp
	src/aoo_client_node.cpp
	${CMAKE_JS_SOURCE}
)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_JS_INC}
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/../include"
	"${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-api"
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_JS_LIB} Aoo::aoo)
target_compile_definitions(${PROJECT_NAME} PRIVATE NAPI_CPP_EXCETIONS)