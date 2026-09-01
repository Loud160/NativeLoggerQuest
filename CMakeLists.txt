# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: © 2026 Loud160 (AKA Whisp) and the Big Screen contributors
#
# Part of Big Screen.
# Distributed under GPL-3.0-only with additional terms under GPLv3
# section 7(b)/(c) and an interoperability permission under section 7;
# see LICENSE and LICENSE-ADDITIONAL-TERMS.md.

cmake_minimum_required(VERSION 3.20)
project(NativeLoggerQuest VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)

set(NATIVE_LOGGER_QUEST_TAG "NativeLoggerQuest" CACHE STRING
    "Logcat and file-record tag compiled into this private logger copy")
if(NOT NATIVE_LOGGER_QUEST_TAG MATCHES "^[A-Za-z0-9_.-]+$")
    message(FATAL_ERROR
        "NATIVE_LOGGER_QUEST_TAG may contain only letters, digits, dot, underscore, and hyphen")
endif()

add_library(NativeLoggerQuest STATIC src/NativeLogger.cpp)
add_library(NativeLoggerQuest::NativeLoggerQuest ALIAS NativeLoggerQuest)
target_compile_features(NativeLoggerQuest PUBLIC cxx_std_20)
target_include_directories(NativeLoggerQuest
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(NativeLoggerQuest PRIVATE
    NATIVE_LOGGER_QUEST_TAG="${NATIVE_LOGGER_QUEST_TAG}")
target_link_libraries(NativeLoggerQuest PUBLIC Threads::Threads)
if(ANDROID)
    target_link_libraries(NativeLoggerQuest PUBLIC log)
endif()
set_target_properties(NativeLoggerQuest PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)

# beatsaber-hook's inline abort helpers still refer to two Paper2 C-ABI
# functions. A consuming mod that deliberately has no Paper2 link can call
# this helper for its final shared-library target. The wrappers are compiled
# into that target with hidden visibility and cannot replace or intercept
# Paper2 for any other mod.
function(native_logger_quest_enable_paper2_abort_bridge target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR
            "native_logger_quest_enable_paper2_abort_bridge target does not exist: ${target_name}")
    endif()
    target_sources("${target_name}" PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/Paper2AbortBridge.cpp")
    target_link_options("${target_name}" PRIVATE
        "-Wl,--wrap=paper2_queue_log_bytes_ffi"
        "-Wl,--wrap=paper2_wait_for_flush")
endfunction()

option(NATIVE_LOGGER_QUEST_BUILD_TESTS
    "Build Native Logger Quest host tests" ${PROJECT_IS_TOP_LEVEL})
if(NATIVE_LOGGER_QUEST_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS NativeLoggerQuest
    EXPORT NativeLoggerQuestTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT NativeLoggerQuestTargets
    FILE NativeLoggerQuestTargets.cmake
    NAMESPACE NativeLoggerQuest::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/NativeLoggerQuest")
configure_package_config_file(
    cmake/NativeLoggerQuestConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/NativeLoggerQuestConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/NativeLoggerQuest")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NativeLoggerQuestConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NativeLoggerQuestConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NativeLoggerQuestConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/NativeLoggerQuest")
