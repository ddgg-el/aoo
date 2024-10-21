set(MAX_INCLUDE_DIR "" CACHE PATH "Max include directory")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architecture" FORCE)

include(${MAX_INCLUDE_DIR}/max-sdk-base/script/max-pretarget.cmake)

#############################################################
# MAX EXTERNAL
#############################################################

include_directories( 
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
)

file(GLOB PROJECT_SRC
     "*.h"
	 "*.c"
     "*.cpp"
)
add_library( 
	${PROJECT_NAME} 
	MODULE
	${PROJECT_SRC}
)

include(${MAX_INCLUDE_DIR}/max-sdk-base/script/max-posttarget.cmake)
