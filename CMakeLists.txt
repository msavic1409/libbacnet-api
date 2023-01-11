cmake_minimum_required(VERSION 3.0)
project(bacnet-api)

option(CERTIFICATION "Whether to build for certification or not" OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBBACNET REQUIRED libbacnet IMPORTED_TARGET)

set(SOURCE_FILES
        objects/c_wrapper.cpp
        objects/container.cpp
        objects/device.cpp
        objects/analog_input_intrinsic.cpp
        objects/analog_value.cpp
        objects/multi_state_input.cpp
        objects/multi_state_value.cpp
        objects/characterstring_value.cpp
        objects/time_value.cpp
        objects/date_value.cpp
        objects/bitstring_value.cpp
        objects/notification_class.cpp
        src/bvlc.cpp
        src/bip.cpp
        src/bacnet_sink.cpp
        src/bacnet.cpp)

if (${CERTIFICATION})
    list(APPEND SOURCE_FILES objects/analog_input.cpp)
endif()

add_library("${PROJECT_NAME}" SHARED
        "${SOURCE_FILES}"
        )

target_include_directories(
        "${PROJECT_NAME}"
        PRIVATE
        "include"
        "objects"
)
target_link_libraries(
        "${PROJECT_NAME}"
        PUBLIC PkgConfig::LIBBACNET)
set_property(
        TARGET "${PROJECT_NAME}"
        PROPERTY POSITION_INDEPENDENT_CODE ON)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_options(
            "${PROJECT_NAME}"
            PRIVATE
            "-O0"
            "-g"
    )
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    target_compile_options(
            "${PROJECT_NAME}"
            PRIVATE
            "-O2"
    )
endif()

if (${CERTIFICATION})
target_compile_definitions("${PROJECT_NAME}" PUBLIC
        # Enable this flag ONLY if building certification software
        CERTIFICATION_SOFTWARE
        )
endif()

include(GNUInstallDirs)
install(
        TARGETS "${PROJECT_NAME}"
        EXPORT "${PROJECT_NAME}"
)
install(
        EXPORT "${PROJECT_NAME}"
        DESTINATION "lib/cmake/${PROJECT_NAME}"
        NAMESPACE "${PROJECT_NAME}::"
)
install(
        DIRECTORY include/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}"
)
install(
        FILES "${PROJECT_NAME}Config.cmake"
        DESTINATION "lib/cmake/${PROJECT_NAME}"
)