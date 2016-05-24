project(pvr.home-system)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR})



add_subdirectory(lib/common/common)

include_directories(lib/common/common/src/
                    lib/common/yami4/include/)

set(PVRHOMESYSTEM_SOURCES src/client.cpp
                          src/pvrclient.cpp
                          src/pvrclient.h)
						  
add_definitions(-D_ITERATOR_DEBUG_LEVEL=2 -DPOCO_STATIC)

set(DEPLIBS ${kodiplatform_LIBRARIES}
            ${p8-platform_LIBRARIES}
			common)
			
link_directories(d:/develop/pvr.home-system/lib/common/yami4/lib/)

build_addon(pvr.home-system PVRHOMESYSTEM DEPLIBS)

target_link_libraries(pvr.home-system yamicore yamicpp)

include(CPack)