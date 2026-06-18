cmake_minimum_required(VERSION 3.15)
project(aoo_wasm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE "Debug")

set(AOO_USE_OPUS                   	ON 	CACHE 	BOOL "" FORCE)
set(AOO_NET                        	OFF	CACHE 	BOOL "" FORCE)
set(AOO_BUILD_EXAMPLES             	OFF	CACHE 	BOOL "" FORCE)
set(AOO_BUILD_TESTS                	OFF	CACHE 	BOOL "" FORCE)
set(AOO_INSTALL_LIBRARY            	OFF	CACHE 	BOOL "" FORCE)
set(AOO_INSTALL_PKG_CONFIG_MODULE  	OFF	CACHE 	BOOL "" FORCE)
set(AOO_INSTALL_CMAKE_CONFIG_MODULE OFF	CACHE 	BOOL "" FORCE)
set(AOO_BUILD_SHARED_LIBRARY 		OFF	CACHE 	BOOL "" FORCE)

set(AOO_LOG_LEVEL 			"Verbose"	CACHE	STRING "" FORCE)



add_executable(aoo_wasm 
	src/aoo_js.cpp
	src/aoo_js_receive.cpp
)
target_include_directories(aoo_wasm PRIVATE "include")

target_link_libraries(aoo_wasm PRIVATE Aoo::aoo)

set_target_properties(aoo_wasm PROPERTIES OUTPUT_NAME "aoo" SUFFIX ".mjs")

set(WEB_TEMPLATE_DIR "${CMAKE_SOURCE_DIR}/web/js")


add_custom_command(TARGET aoo_wasm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${WEB_TEMPLATE_DIR}"
		COMMAND ${CMAKE_COMMAND} -E copy
			$<TARGET_FILE_DIR:aoo_wasm>/aoo.mjs
			# $<TARGET_FILE_DIR:aoo_wasm>/aoo_wasm.wasm
			${WEB_TEMPLATE_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy
        	$<TARGET_FILE_DIR:aoo_wasm>/aoo.d.ts 
			"${WEB_TEMPLATE_DIR}/aoo.d.mts"
			
			COMMENT "Copying aoo.mjs + types to ${WEB_TEMPLATE_DIR}"
	)

target_link_options(aoo_wasm PRIVATE
	"--bind"
	"-sMODULARIZE=1"
	"-sEXPORT_ES6=1"
	# If you later want to step through C++ line-by-line in the browser, 
	# we drop SINGLE_FILE...
	"-sSINGLE_FILE=1"
	# and add -gsource-map
	# "-gsource-map"
	"-sENVIRONMENT=web,worker,node"
	"-sEXPORT_NAME=createModule"
	"-sALLOW_MEMORY_GROWTH=1"          # AOO allocates an RT memory pool on init
	"SHELL:--emit-tsd aoo.d.ts"
	"-g"
	"-sASSERTIONS=2"
	"-sSAFE_HEAP=1"					# catches OOB / misaligned heap access
	"-sSTACK_OVERFLOW_CHECK=2"      # detect stack smashing
    "-sEXCEPTION_STACK_TRACES=1"    # C++ stack trace on uncaught exceptions
	"-sNO_DISABLE_EXCEPTION_CATCHING"
)

