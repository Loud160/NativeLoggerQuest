# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: © 2026 Loud160 (AKA Whisp) and the Big Screen contributors

add_executable(native-logger-quest-tests NativeLoggerTests.cpp)
target_compile_features(native-logger-quest-tests PRIVATE cxx_std_20)
target_link_libraries(native-logger-quest-tests PRIVATE
    NativeLoggerQuest::NativeLoggerQuest)
if(MSVC)
    target_compile_options(native-logger-quest-tests PRIVATE /UNDEBUG)
else()
    target_compile_options(native-logger-quest-tests PRIVATE -UNDEBUG)
endif()
if(NOT MSVC)
    target_compile_options(native-logger-quest-tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME native-logger-quest-tests COMMAND native-logger-quest-tests)
