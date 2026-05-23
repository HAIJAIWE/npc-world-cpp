cmake_minimum_required(VERSION 3.20)
project(NPCWorld VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

if(MSVC)
    add_compile_options(/W3 /EHsc /utf-8 /Zc:char8_t-)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _upper_build_type)
    if(_upper_build_type STREQUAL "RELEASE" OR _upper_build_type STREQUAL "MINSIZEREL" OR _upper_build_type STREQUAL "RELWITHDEBINFO")
        add_compile_options(/O2)
    else()
        add_compile_options(/Od)
    endif()
else()
    add_compile_options(-Wall -Wextra -O3 -march=native)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_ROOT     ${CMAKE_CURRENT_SOURCE_DIR})
set(SRC_DIR          ${PROJECT_ROOT}/src)
set(EXTERNAL_DIR     ${PROJECT_ROOT}/external)

if(NOT TARGET nlohmann_json::nlohmann_json)
    if(EXISTS "${EXTERNAL_DIR}/nlohmann/json.hpp")
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE "${EXTERNAL_DIR}/nlohmann")
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
        message(STATUS "nlohmann/json: single-header found at ${EXTERNAL_DIR}/nlohmann/")
    elseif(EXISTS "${EXTERNAL_DIR}/nlohmann/single_include")
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE
            "${EXTERNAL_DIR}/nlohmann/single_include"
            "${EXTERNAL_DIR}/nlohmann/include")
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
        message(STATUS "nlohmann/json: CMake-installed version found.")
    else()
        message(WARNING "nlohmann/json not found at ${EXTERNAL_DIR}/nlohmann/")
        message(WARNING "  Expected: ${EXTERNAL_DIR}/nlohmann/json.hpp (single-header)")
        message(WARNING "  Or: ${EXTERNAL_DIR}/nlohmann/single_include/nlohmann/json.hpp")
    endif()
endif()

if(NOT TARGET sqlite3)
    if(EXISTS "${EXTERNAL_DIR}/sqlite3/sqlite3.c")
        message(STATUS "SQLite3: amalgamation found at ${EXTERNAL_DIR}/sqlite3/")
        add_library(sqlite3 STATIC
            "${EXTERNAL_DIR}/sqlite3/sqlite3.c"
        )
        target_include_directories(sqlite3 PUBLIC "${EXTERNAL_DIR}/sqlite3")
    else()
        message(WARNING "SQLite3 amalgamation not found at ${EXTERNAL_DIR}/sqlite3/sqlite3.c")
        message(WARNING "  Download from: https://www.sqlite.org/download.html")
    endif()
endif()

if(NOT TARGET llama)
    if(EXISTS "${EXTERNAL_DIR}/llama.cpp/CMakeLists.txt")
        message(STATUS "llama.cpp: submodule found at ${EXTERNAL_DIR}/llama.cpp/")
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
        add_subdirectory("${EXTERNAL_DIR}/llama.cpp" EXCLUDE_FROM_ALL)
    else()
        message(WARNING "llama.cpp not found at ${EXTERNAL_DIR}/llama.cpp/")
        message(WARNING "  Run: git submodule add https://github.com/ggerganov/llama.cpp.git external/llama.cpp")
    endif()
endif()

if(NOT TARGET imgui)
    set(IMGUI_DIR "${EXTERNAL_DIR}/imgui")
    if(EXISTS "${IMGUI_DIR}/imgui.h")
        message(STATUS "Dear ImGui: found at ${IMGUI_DIR}/")

        file(GLOB IMGUI_SOURCES
            "${IMGUI_DIR}/*.cpp"
        )
        list(APPEND IMGUI_SOURCES
            "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
            "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
        )

        add_library(imgui STATIC ${IMGUI_SOURCES})
        target_include_directories(imgui PUBLIC
            "${IMGUI_DIR}"
            "${IMGUI_DIR}/backends"
        )
        target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler)
    else()
        message(WARNING "Dear ImGui not found at ${IMGUI_DIR}/imgui.h")
        message(WARNING "  Run: git submodule add -b docking https://github.com/ocornut/imgui.git external/imgui")
    endif()
endif()

set(COMMON_SOURCES
    ${SRC_DIR}/ipc/shared_memory.cpp
    ${SRC_DIR}/db/database.cpp
    ${SRC_DIR}/db/npc_repo.cpp
    ${SRC_DIR}/behavior/npc_architecture.cpp
    ${SRC_DIR}/behavior/npc_autonomous.cpp
    ${SRC_DIR}/behavior/npc_brain.cpp
    ${SRC_DIR}/behavior/npc_data_guard.cpp
    ${SRC_DIR}/behavior/npc_emotion.cpp
    ${SRC_DIR}/behavior/npc_filter.cpp
    ${SRC_DIR}/behavior/npc_gossip.cpp
    ${SRC_DIR}/behavior/npc_identity_lock.cpp
    ${SRC_DIR}/behavior/npc_learning.cpp
    ${SRC_DIR}/behavior/npc_local_response.cpp
    ${SRC_DIR}/behavior/npc_self_improve.cpp
    ${SRC_DIR}/behavior/npc_think.cpp
    ${SRC_DIR}/engine/llm_cache.cpp
    ${SRC_DIR}/engine/training_logger.cpp
    ${SRC_DIR}/engine/world_state.cpp
    ${SRC_DIR}/engine/context_manager.cpp
    ${SRC_DIR}/engine/world_clock.cpp
    ${SRC_DIR}/engine/npc_memory.cpp
    ${SRC_DIR}/engine/api_client.cpp
    ${SRC_DIR}/engine/intent_classifier.cpp
    ${SRC_DIR}/engine/react_loop.cpp
    ${SRC_DIR}/engine/memory_compressor.cpp
    ${SRC_DIR}/engine/user_preferences.cpp
    ${SRC_DIR}/engine/approval_gate.cpp
    ${SRC_DIR}/engine/multi_agent.cpp
    ${SRC_DIR}/engine/multimodal_perception.cpp
    ${SRC_DIR}/engine/workflow_engine.cpp
    ${SRC_DIR}/engine/filesystem_agent.cpp
    ${SRC_DIR}/engine/web_automation.cpp
    ${SRC_DIR}/engine/task_scheduler.cpp
    ${SRC_DIR}/engine/api_model_backend.cpp
    ${SRC_DIR}/engine/model_provider.cpp
    ${SRC_DIR}/engine/request_gate.cpp
)

set(COMMON_INCLUDES
    ${SRC_DIR}
    ${EXTERNAL_DIR}
)

if(TARGET imgui AND TARGET sqlite3)
    add_executable(npc_frontend WIN32
        ${SRC_DIR}/main_frontend.cpp
        ${COMMON_SOURCES}
    )

    target_include_directories(npc_frontend PRIVATE ${COMMON_INCLUDES})

    target_link_libraries(npc_frontend PRIVATE
        imgui
        sqlite3
        d3d11 dxgi dwmapi
    )

    if(TARGET nlohmann_json::nlohmann_json)
        target_link_libraries(npc_frontend PRIVATE nlohmann_json::nlohmann_json)
    endif()

    set_target_properties(npc_frontend PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "npc_frontend"
    )

    message(STATUS "npc_frontend: configured (IMGUI + SQLite3 + DX11)")
else()
    message(WARNING "npc_frontend: SKIPPED (missing imgui or sqlite3)")
endif()

if(TARGET llama)
    add_executable(npc_backend
        ${SRC_DIR}/main_backend.cpp
        ${COMMON_SOURCES}
        ${SRC_DIR}/engine/model_engine.cpp
        ${SRC_DIR}/engine/inference_pipeline.cpp
        ${SRC_DIR}/engine/dialogue_engine.cpp
        ${SRC_DIR}/engine/model_registry.cpp
    )

    target_link_libraries(npc_backend PRIVATE
        llama
        ggml
        sqlite3
    )

    if(TARGET nlohmann_json::nlohmann_json)
        target_link_libraries(npc_backend PRIVATE nlohmann_json::nlohmann_json)
    endif()

    target_include_directories(npc_backend PRIVATE ${COMMON_INCLUDES})
    target_compile_options(npc_backend PRIVATE /FS)

    set_target_properties(npc_backend PROPERTIES
        WIN32_EXECUTABLE FALSE
        OUTPUT_NAME "npc_backend"
    )

    message(STATUS "npc_backend: configured (llama.cpp)")
else()
    message(WARNING "npc_backend: SKIPPED (missing llama.cpp)")
endif()

install(TARGETS npc_frontend npc_backend
    RUNTIME DESTINATION bin
)

message(STATUS "")
message(STATUS "=== NPCWorld Build Configuration ===")
message(STATUS "  C++ Standard:  C++20")
message(STATUS "  Frontend:      ${SRC_DIR}/main_frontend.cpp -> npc_frontend.exe")
message(STATUS "  Backend:       ${SRC_DIR}/main_backend.cpp  -> npc_backend.exe")
message(STATUS "  IPC:           Shared memory ring buffer (16 MB)")
message(STATUS "  Database:      SQLite3 (amalgamation)")
message(STATUS "  JSON:          nlohmann/json (header-only)")
message(STATUS "  UI:            Dear ImGui (docking) + Direct3D 11")
message(STATUS "  Inference:     llama.cpp")
message(STATUS "=====================================")
