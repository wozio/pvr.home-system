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

set(DEPLIBS ${kodiplatform_LIBRARIES}
            ${p8-platform_LIBRARIES})

INCLUDE_EXTERNAL_MSPROJECT(common lib/common/common/common.vcxproj)
include_directories(lib/common/common/src/)

set(PVRHOMESYSTEM_SOURCES src/client.cpp)

build_addon(pvr.home-system PVRHOMESYSTEM DEPLIBS)

include(CPack)
