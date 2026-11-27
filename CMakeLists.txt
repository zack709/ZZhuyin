cmake_minimum_required(VERSION 3.20)
project(ZZhuyin VERSION 0.1.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(zzhuyin_core STATIC
    src/core/key_mapper.cpp
    src/core/composer.cpp
    src/core/imechlex_dict.cpp
    src/core/zdict.cpp
    src/core/candidate.cpp
    src/core/engine.cpp
)

target_include_directories(zzhuyin_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(zzhuyin_core PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(zzhuyin_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_library(zzhuyin_tsf SHARED
        src/tsf/dll_main.cpp
        src/tsf/text_service.cpp
        src/tsf/display_attr.cpp
        src/tsf/candidate_wnd.cpp
        src/tsf/symbol_wnd.cpp
        src/tsf/lang_bar.cpp
        src/tsf/config_reg.cpp
        src/tsf/settings_dlg.cpp
        src/tsf/zzhuyin_tsf.def
        src/tsf/zzhuyin_tsf.rc
    )
    target_link_libraries(zzhuyin_tsf PRIVATE
        zzhuyin_core
        comctl32
        comdlg32
        ole32
        oleaut32
        advapi32
        user32
        gdi32
        uuid
    )
    target_compile_definitions(zzhuyin_tsf PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    if(MSVC)
        target_compile_options(zzhuyin_tsf PRIVATE /W4 /utf-8 /permissive-)
    endif()
endif()

enable_testing()

add_executable(zzhuyin_tests
    tests/test_main.cpp
    tests/test_dictionary.cpp
    tests/test_zdict.cpp
    tests/test_composer.cpp
    tests/test_candidate.cpp
    tests/test_engine_e2e.cpp
)

target_link_libraries(zzhuyin_tests PRIVATE zzhuyin_core)
target_compile_definitions(zzhuyin_tests PRIVATE
    ZZHUYIN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(zzhuyin_tests PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(zzhuyin_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME zzhuyin_core_tests COMMAND zzhuyin_tests)

if(WIN32)
    add_executable(zzhuyin_tsf_smoke
        tests/test_tsf_smoke.cpp
        src/tsf/config_reg.cpp
    )
    target_include_directories(zzhuyin_tsf_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(zzhuyin_tsf_smoke PRIVATE ole32 advapi32 uuid)
    target_compile_definitions(zzhuyin_tsf_smoke PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    )
    if(MSVC)
        target_compile_options(zzhuyin_tsf_smoke PRIVATE /W4 /utf-8 /permissive-)
    endif()
    add_dependencies(zzhuyin_tsf_smoke zzhuyin_tsf)
    add_test(NAME zzhuyin_tsf_smoke_test COMMAND zzhuyin_tsf_smoke)
endif()
