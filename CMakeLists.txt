
cmake_minimum_required(VERSION 3.19)
project(3DAndreas VERSION 0.7.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

set(DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY ON CACHE BOOL "" FORCE)
set(USE_SSE4_1 ON CACHE BOOL "" FORCE)
set(USE_SSE4_2 ON CACHE BOOL "" FORCE)
set(USE_AVX OFF CACHE BOOL "" FORCE)
set(USE_AVX2 OFF CACHE BOOL "" FORCE)
set(USE_AVX512 OFF CACHE BOOL "" FORCE)
set(USE_LZCNT OFF CACHE BOOL "" FORCE)
set(USE_TZCNT OFF CACHE BOOL "" FORCE)
set(USE_F16C OFF CACHE BOOL "" FORCE)
set(USE_FMADD OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

# Enable RTTI for Jolt Physics
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)

FetchContent_Declare(omp-sdk
    GIT_REPOSITORY https://github.com/openmultiplayer/open.mp-sdk.git
    GIT_TAG 08cc69a32d96c0eb8fe063d26c78259e54ca853f
)

FetchContent_Declare(pawn
    GIT_REPOSITORY https://github.com/openmultiplayer/compiler.git
    GIT_TAG 29f1a8c7fd2e01929807cd8c50353fbc93bbd651
)

FetchContent_Declare(jolt
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.6.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build
)

set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_DT_POLYREF64 ON CACHE BOOL "" FORCE)

FetchContent_Declare(recast
    GIT_REPOSITORY https://github.com/recastnavigation/recastnavigation.git
    GIT_TAG v1.6.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(omp-sdk pawn jolt recast)

add_library(3DAndreas SHARED
    src/actors.cpp
    src/bodies.cpp
    src/database.cpp
    src/geometry.cpp
    src/main.cpp
    src/map.cpp
    src/models.cpp
    src/natives.cpp
    src/navfile.cpp
    src/probes.cpp
    src/queries.cpp
    src/routeline.cpp
    src/routes.cpp
    src/routing.cpp
    src/screen.cpp
    src/sync.cpp
    src/vehicles.cpp
    src/walkmesh.cpp
    src/walks.cpp
    src/water.cpp
    src/world.cpp
)

set_target_properties(3DAndreas PROPERTIES PREFIX "")

target_include_directories(3DAndreas PRIVATE
    "${pawn_SOURCE_DIR}/source"
    "${pawn_SOURCE_DIR}/source/linux"
)

target_compile_definitions(3DAndreas PRIVATE
    HAVE_STDINT_H=1
    PAWN_CELL_SIZE=32
    A3D_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    A3D_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    A3D_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    A3D_VERSION="${PROJECT_VERSION}"
)

# Ensure RTTI is enabled when compiling the plugin
if(NOT MSVC)
    target_compile_options(3DAndreas PRIVATE -frtti)
endif()

if(WIN32)
    target_compile_definitions(3DAndreas PRIVATE
        HAVE_ALLOCA_H=0
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

if(MSVC)
    target_compile_options(3DAndreas PRIVATE /W3 /utf-8)
endif()

target_link_libraries(3DAndreas PRIVATE
    OMP-SDK
    Jolt
    Detour
)

add_executable(phydb
    tools/builder.cpp
    src/database.cpp
)

if(WIN32)
    target_compile_definitions(phydb PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(phydb PRIVATE /W3 /utf-8)
endif()

add_executable(routebench
    tools/routebench.cpp
    src/database.cpp
    src/routing.cpp
)

if(WIN32)
    target_compile_definitions(routebench PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(routebench PRIVATE /W3 /utf-8)
endif()

add_executable(linetest
    tools/linetest.cpp
    src/routeline.cpp
)

if(WIN32)
    target_compile_definitions(linetest PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(linetest PRIVATE /W3 /utf-8)
endif()

add_executable(navbuild
    tools/navbuild.cpp
    src/database.cpp
    src/navfile.cpp
)

if(WIN32)
    target_compile_definitions(navbuild PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(navbuild PRIVATE /W3 /utf-8)
endif()

target_link_libraries(navbuild PRIVATE
    Recast
    Detour
)
