cmake_minimum_required(VERSION 3.15)
project(aoo_wasm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(AOO_LOG_LEVEL "Info" CACHE STRING "" FORCE)
endif()

set(AOO_USE_OPUS                   	ON 	CACHE 	BOOL "" FORCE)
set(AOO_NET                        	OFF	CACHE 	BOOL "" FORCE)
set(AOO_BUILD_EXAMPLES             	OFF	CACHE 	BOOL "" FORCE)
set(AOO_BUILD_TESTS                	OFF	CACHE 	BOOL "" FORCE)
set(AOO_INSTALL_LIBRARY            	OFF	CACHE 	BOOL "" FORCE)
set(AOO_INSTALL_PKG_CONFIG_MODULE  	OFF	CACHE 	BOOL "" FORCE)
set(AOO_INSTALL_CMAKE_CONFIG_MODULE OFF	CACHE 	BOOL "" FORCE)
set(AOO_BUILD_SHARED_LIBRARY 		OFF	CACHE 	BOOL "" FORCE)

option(AOO_WASM_THREADED "Build the threaded AudioWorklet (browser) variant" OFF)

if(AOO_WASM_THREADED)
	add_compile_options(-pthread)
	# add_link_options(-pthread)
endif()

find_program(NPM_EXECUTABLE npm)

add_executable(aoo_wasm 
	src/aoo_js.cpp
	src/aoo_sink_js.cpp
	src/aoo_source_js.cpp
)
target_include_directories(aoo_wasm PRIVATE "include")
target_link_libraries(aoo_wasm PRIVATE Aoo::aoo)

set(AOO_PACKAGE_DIR "${CMAKE_SOURCE_DIR}/wasm/aoo")
set(WEB_PACKAGE_DIR "${AOO_PACKAGE_DIR}/dist")

target_link_options(aoo_wasm PRIVATE
	"--bind"
	"-sMODULARIZE=1"
	"-sEXPORT_ES6=1"
	"-sEXPORT_NAME=createModule"
	"-sALLOW_MEMORY_GROWTH=1"          # AOO allocates an RT memory pool on init
	"-sNO_DISABLE_EXCEPTION_CATCHING"
	# If you want to step through C++ line-by-line in the browser, 
	# drop SINGLE_FILE...
	# and add -gsource-map
	# "-gsource-map"
)

if(AOO_WASM_THREADED)
	target_sources(aoo_wasm PRIVATE src/aoo_audioworklet.cpp)

	set_target_properties(aoo_wasm PROPERTIES OUTPUT_NAME "aoo.web" SUFFIX ".mjs")
	target_link_options(aoo_wasm PRIVATE
		# FIXME: Why not SINGLE_FILE=1 in browser
		"-sAUDIO_WORKLET=1"
		"-sWASM_WORKERS=1"
		"-sENVIRONMENT=web,worker"
		"SHELL:--emit-tsd aoo.web.d.ts"
	)
else()
	set_target_properties(aoo_wasm PROPERTIES OUTPUT_NAME "aoo.node" SUFFIX ".mjs")
	target_link_options(aoo_wasm PRIVATE
		"-sSINGLE_FILE=1"
		"-sENVIRONMENT=node"
		"SHELL:--emit-tsd aoo.node.d.ts"
	)
endif()
# "-sENVIRONMENT=web,worker,node"

if(CMAKE_BUILD_TYPE STREQUAL "Release")
	target_compile_options(aoo_wasm PRIVATE "-O3")
	target_link_options(aoo_wasm PRIVATE "-O3")
else()
	target_compile_options(aoo_wasm PRIVATE "-O0")
	target_link_options(aoo_wasm PRIVATE
		"-g"
		"-sASSERTIONS=2"
		"-sSAFE_HEAP=1"					# catches OOB / misaligned heap access
		"-sSTACK_OVERFLOW_CHECK=2"      # detect stack smashing
		"-sEXCEPTION_STACK_TRACES=1"    # C++ stack trace on uncaught exceptions
	)
endif()

if(AOO_WASM_THREADED)
	add_custom_command(TARGET aoo_wasm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${WEB_PACKAGE_DIR}"
		COMMAND ${CMAKE_COMMAND} -E copy
			$<TARGET_FILE_DIR:aoo_wasm>/aoo.web.mjs
			$<TARGET_FILE_DIR:aoo_wasm>/aoo.web.wasm
			${WEB_PACKAGE_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy
			$<TARGET_FILE_DIR:aoo_wasm>/aoo.web.d.ts 
			"${WEB_PACKAGE_DIR}/aoo.web.d.mts"
			
			COMMENT "Copying aoo.web + types to dist"
	)
else()
	add_custom_command(TARGET aoo_wasm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${WEB_PACKAGE_DIR}"
		COMMAND ${CMAKE_COMMAND} -E copy
			$<TARGET_FILE_DIR:aoo_wasm>/aoo.node.mjs
			${WEB_PACKAGE_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy
			$<TARGET_FILE_DIR:aoo_wasm>/aoo.node.d.ts 
			"${WEB_PACKAGE_DIR}/aoo.node.d.mts"
			
			COMMENT "Copying aoo.node + types to dist"
	)
	endif()
	
if(NPM_EXECUTABLE)
	add_custom_command(TARGET aoo_wasm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E chdir "${AOO_PACKAGE_DIR}"
		${NPM_EXECUTABLE} run build
		COMMENT "Building Typescript wrapper (npm run build)"
		VERBATIM
) 
else()
	message(WARNING "npm not found — skipping TypeScript wrapper build; run 'npm run build' in wasm/aoo manually")
endif()