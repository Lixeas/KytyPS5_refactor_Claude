cmake_minimum_required(VERSION 3.12...4.3)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for bundled dependencies")
endif()

project(Kyty)

if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
	set(LINUX TRUE)
endif()

if (NOT (WIN32 OR LINUX))
    message(FATAL_ERROR "only Windows and Linux builds are supported")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(utils.cmake)

set(KYTY_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty")

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include(TestBigEndian)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "only 64-bit builds are supported")
endif()

test_big_endian(BIG_ENDIAN)
if(BIG_ENDIAN)
	set(KYTY_ENDIAN KYTY_ENDIAN_BIG)
else()
	set(KYTY_ENDIAN KYTY_ENDIAN_LITTLE)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(KYTY_BUILD KYTY_BUILD_DEBUG)
else()
	set(KYTY_BUILD KYTY_BUILD_RELEASE)
endif()

if(LINUX)
	set(KYTY_PLATFORM KYTY_PLATFORM_LINUX)	
else()
	set(KYTY_PLATFORM KYTY_PLATFORM_WINDOWS)
endif()

set(CLANG FALSE)
set(GCC FALSE)
set(KYTY_CLANG_CL FALSE)
get_filename_component(KYTY_CXX_COMPILER_NAME "${CMAKE_CXX_COMPILER}" NAME_WE)
string(TOLOWER "${KYTY_CXX_COMPILER_NAME}" KYTY_CXX_COMPILER_NAME)

if(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
	set(CLANG TRUE)
	set(KYTY_COMPILER CLANG)
	if(WIN32 AND KYTY_CXX_COMPILER_NAME STREQUAL "clang-cl")
		set(KYTY_CLANG_CL TRUE)
		set(KYTY_LINKER LLD_LINK)
	else()
		set(KYTY_LINKER LLD)
	endif()
	if ((CMAKE_AR MATCHES "llvm") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0.0) AND (CMAKE_CXX_COMPILER MATCHES "winlibs"))
		string(REPLACE "llvm-ar.exe" "ar.exe" CMAKE_AR ${CMAKE_AR}) # llvm-ar.exe 11.0.0 from winlibs is broken
	endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(GCC TRUE)
	set(KYTY_COMPILER GCC)
	set(KYTY_LINKER LD)
else()
	message(FATAL_ERROR "only Clang and GCC are supported; found ${CMAKE_CXX_COMPILER_ID}")
endif()

set(KYTY_PROJECT_NAME "Emulator" CACHE STRING "Project name suffix")

string(TOLOWER ${KYTY_COMPILER} KYTY_COMPILER_ID)
string(TOLOWER ${KYTY_LINKER} KYTY_LINKER_ID)

if (CLANG AND (KYTY_LINKER STREQUAL LLD))
	set(KYTY_LD_OPTIONS "-fuse-ld=lld")
endif()

if (KYTY_LINKER STREQUAL LD AND NOT LINUX)
	set(KYTY_LD_OPTIONS "-Wl,--image-base=0x100000000000")
endif()

project(Kyty${KYTY_PROJECT_NAME}${CMAKE_BUILD_TYPE}${KYTY_COMPILER} VERSION 0.2.2)

include_directories(	
	${KYTY_THIRD_PARTY_DIR}/gtest/include
	${KYTY_THIRD_PARTY_DIR}/gtest
	${KYTY_THIRD_PARTY_DIR}/fmt/include
	${KYTY_THIRD_PARTY_DIR}/magic_enum/include/magic_enum
	${PROJECT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
)

set(KYTY_VERSION "${PROJECT_VERSION}")

configure_file(
  ${PROJECT_SOURCE_DIR}/cmake_config.h.in
  ${PROJECT_BINARY_DIR}/cmake_config.h
  )

find_package(Git)
  
add_custom_target( KytyGitVersion
    COMMAND ${CMAKE_COMMAND} 
	-D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/kytyGitVersion.h.in
    -D OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/kytyGitVersion.h
	-D GIT_EXECUTABLE=${GIT_EXECUTABLE}
	-P ${CMAKE_CURRENT_SOURCE_DIR}/generate_version.cmake
	COMMENT "Generate kytyGitVersion.h"
)

if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0.0)
	list(APPEND KYTY_IWYU 
		kyty_emulator
		common
		#launcher
	)
	list(APPEND KYTY_CLANG_TYDY 
		kyty_emulator
		#common
		launcher
	)
endif()

option(KYTY_BUILD_LAUNCHER "Build Qt launcher" ON)

# OFF by default: clang_tidy_check runs with -warnings-as-errors=*, and the tree has never been built
# against .clang-tidy (the check was skipped on every clang-cl build), so the backlog is unknown.
option(KYTY_ENABLE_CLANG_TIDY "Run clang-tidy as part of the build" OFF)
  
config_compiler_and_linker()  
  
add_subdirectory("${KYTY_THIRD_PARTY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/3rdparty")
add_subdirectory(common)

file(GLOB kyty_emulator_src
	libs/*.cpp
	libs/*.h
	graphics/*.cpp
	graphics/*.h
	graphics/guest_gpu/*.cpp
	graphics/guest_gpu/*.h
	graphics/guest_gpu/command_processor/*.cpp
	graphics/guest_gpu/command_processor/*.h
	graphics/host_gpu/*.cpp
	graphics/host_gpu/*.h
	graphics/host_gpu/renderer/*.cpp
	graphics/host_gpu/renderer/*.h
	graphics/shader/*.cpp
	graphics/shader/*.h
	graphics/shader/recompiler/*.cpp
	graphics/shader/recompiler/*.h
	graphics/shader/recompiler/shaderIR/*.cpp
	graphics/shader/recompiler/shaderIR/*.h
	graphics/shader/recompiler/spirvEmitter/*.cpp
	graphics/shader/recompiler/spirvEmitter/*.h
	graphics/host_gpu/objects/*.cpp
	graphics/host_gpu/objects/*.h
	graphics/presentation/*.cpp
	graphics/presentation/*.h
	graphics/presentation/window/*.cpp
	graphics/presentation/window/*.h
	kernel/*.cpp
	kernel/*.h
	loader/*.cpp
	loader/*.h
)

list(APPEND kyty_emulator_src
	emulator.h
	emulator.cpp
)
list(REMOVE_DUPLICATES kyty_emulator_src)

if (KYTY_CLANG_CL)
	set_source_files_properties(${kyty_emulator_src} PROPERTIES COMPILE_FLAGS "-Wno-pragma-pack -Wno-deprecated-declarations -D_TIMESPEC_DEFINED")
endif()

set(kyty_emulator_link_libraries common vulkan-1 spirv-tools-opt spirv-tools SDL2-static xxhash FFmpeg::ffmpeg fmt::fmt nlohmann_json::nlohmann_json LibAtrac9)

set(inc_headers
	${CMAKE_CURRENT_SOURCE_DIR}
	${KYTY_THIRD_PARTY_DIR}/SDL2/include
	${KYTY_THIRD_PARTY_DIR}/Vulkan-Headers/include
	${KYTY_THIRD_PARTY_DIR}/VulkanMemoryAllocator/include
	${KYTY_THIRD_PARTY_DIR}/SPIRV-Tools/include
	${KYTY_THIRD_PARTY_DIR}/fmt/include
	${KYTY_THIRD_PARTY_DIR}/xxhash
	${KYTY_THIRD_PARTY_DIR}/cpuinfo/include
	${KYTY_THIRD_PARTY_DIR}/stb
)

if(DEFINED ENV{VULKAN_SDK})
	list(APPEND inc_headers $ENV{VULKAN_SDK}/Include)
endif()

if (KYTY_CLANG_CL)
	list(APPEND kyty_emulator_link_libraries winpthread)
	list(APPEND inc_headers 
		${KYTY_THIRD_PARTY_DIR}/winpthread/include
	)
endif()

list(APPEND check_headers
	${CMAKE_CURRENT_SOURCE_DIR}
)

enable_testing()

# Tests are EXCLUDE_FROM_ALL, so `ctest` alone cannot build them. Depend on kyty_tests to build the
# whole suite, then run it:
#
#   cmake --build <dir> --target kyty_tests
#   ctest --test-dir <dir> --output-on-failure
add_custom_target(kyty_tests)

# Registers an already-defined test executable with CTest and attaches it to the kyty_tests target.
# Each test binary is a standalone main() that returns non-zero on failure.
function(add_kyty_test target)
	add_test(NAME ${target} COMMAND ${target})
	add_dependencies(kyty_tests ${target})
endfunction()

function(add_kyty_full_emulator_test target source)
	add_executable(${target} EXCLUDE_FROM_ALL ${source} ${kyty_emulator_src})
	target_link_libraries(${target} ${kyty_emulator_link_libraries})
	target_include_directories(${target} PRIVATE ${inc_headers})
	if (WIN32)
		target_link_libraries(${target} iphlpapi)
	endif()
	if (KYTY_CLANG_CL)
		target_link_libraries(${target} onecore)
		add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KYTY_THIRD_PARTY_DIR}/winpthread/bin/libwinpthread-1.dll" $<TARGET_FILE_DIR:${target}>/libwinpthread-1.dll)
	endif()
	add_kyty_test(${target})
endfunction()

add_kyty_full_emulator_test(shader_cfg_tests ../tests/shaderCfgTests.cpp)

add_executable(scalar_provenance_tests EXCLUDE_FROM_ALL
	../tests/ScalarProvenanceTests.cpp
	graphics/host_gpu/hostMemory.cpp
	graphics/shader/recompiler/ScalarProvenance.cpp
	graphics/shader/recompiler/SrtWalker.cpp
)
target_link_libraries(scalar_provenance_tests fmt::fmt)
target_include_directories(scalar_provenance_tests PRIVATE ${inc_headers})
add_kyty_test(scalar_provenance_tests)

add_executable(singleton_tests EXCLUDE_FROM_ALL
	../tests/SingletonTests.cpp
)
target_include_directories(singleton_tests PRIVATE ${inc_headers})
add_kyty_test(singleton_tests)

add_executable(string_utils_tests EXCLUDE_FROM_ALL
	../tests/StringUtilsTests.cpp
)
target_link_libraries(string_utils_tests fmt::fmt)
target_include_directories(string_utils_tests PRIVATE ${inc_headers})
add_kyty_test(string_utils_tests)

# Decoding is pure logic, so this needs no GPU and stays out of the `gpu` label. ShaderDecoder.cpp
# dispatches to every family, so all six decoder translation units have to come along.
add_executable(shader_memory_decode_tests EXCLUDE_FROM_ALL
	../tests/ShaderMemoryDecodeTests.cpp
	graphics/shader/recompiler/ShaderDecoder.cpp
	graphics/shader/recompiler/ScalarAluOps.cpp
	graphics/shader/recompiler/VectorAluOps.cpp
	graphics/shader/recompiler/MemoryOps.cpp
	graphics/shader/recompiler/ImageOps.cpp
	graphics/shader/recompiler/ExportOps.cpp
)
target_link_libraries(shader_memory_decode_tests fmt::fmt)
target_include_directories(shader_memory_decode_tests PRIVATE ${inc_headers})
add_kyty_test(shader_memory_decode_tests)

add_executable(page_manager_tests EXCLUDE_FROM_ALL
	../tests/PageManagerTests.cpp
	graphics/host_gpu/pageManager.cpp
)
target_include_directories(page_manager_tests PRIVATE ${inc_headers})
add_kyty_test(page_manager_tests)

add_executable(memory_tracker_tests EXCLUDE_FROM_ALL
	../tests/MemoryTrackerTests.cpp
	graphics/host_gpu/pageManager.cpp
	graphics/host_gpu/memoryTracker.cpp
)
target_link_libraries(memory_tracker_tests fmt::fmt common)
target_include_directories(memory_tracker_tests PRIVATE ${inc_headers})
target_compile_definitions(memory_tracker_tests PRIVATE KYTY_MEMORY_TRACKER_TESTS=1)
add_kyty_test(memory_tracker_tests)

add_executable(shader_vertex_metadata_tests EXCLUDE_FROM_ALL
	../tests/ShaderVertexMetadataTests.cpp
	graphics/host_gpu/hostMemory.cpp
	graphics/shader/shaderVertexMetadata.cpp
)
target_include_directories(shader_vertex_metadata_tests PRIVATE ${inc_headers})
add_kyty_test(shader_vertex_metadata_tests)

add_executable(shader_stage_runtime_tests EXCLUDE_FROM_ALL
	../tests/ShaderStageRuntimeTests.cpp
	graphics/guest_gpu/gpu_format.cpp
	graphics/host_gpu/hostMemory.cpp
	graphics/shader/shaderStageRuntime.cpp
	graphics/shader/recompiler/ResourceMaterialization.cpp
	graphics/shader/recompiler/ScalarProvenance.cpp
	graphics/shader/recompiler/SrtWalker.cpp
)
target_link_libraries(shader_stage_runtime_tests fmt::fmt)
target_include_directories(shader_stage_runtime_tests PRIVATE ${inc_headers})
add_kyty_test(shader_stage_runtime_tests)

add_executable(resource_tracking_tests EXCLUDE_FROM_ALL
	../tests/ResourceTrackingTests.cpp
	graphics/guest_gpu/gpu_format.cpp
	graphics/host_gpu/hostMemory.cpp
	graphics/shader/recompiler/ScalarProvenance.cpp
	graphics/shader/recompiler/SrtWalker.cpp
	graphics/shader/recompiler/SrtPatcher.cpp
	graphics/shader/recompiler/ResourceTracking.cpp
	graphics/shader/recompiler/ResourceMaterialization.cpp
	graphics/shader/recompiler/ShaderInfoCollection.cpp
	graphics/shader/recompiler/BindingLayout.cpp
)
target_link_libraries(resource_tracking_tests fmt::fmt)
target_include_directories(resource_tracking_tests PRIVATE ${inc_headers})
add_kyty_test(resource_tracking_tests)

add_executable(resource_mutex_tests EXCLUDE_FROM_ALL
	../tests/ResourceMutexTests.cpp
	graphics/host_gpu/renderer/resourceMutex.cpp
)
target_link_libraries(resource_mutex_tests common)
target_include_directories(resource_mutex_tests PRIVATE ${inc_headers})
add_kyty_test(resource_mutex_tests)

add_kyty_full_emulator_test(shader_recompiler_compute_tests ../tests/ShaderRecompilerComputeTests.cpp)
# Needs a real Vulkan device: the harness creates an instance and dispatches a compute shader, so it
# fails with VK_ERROR_INCOMPATIBLE_DRIVER on a machine without a GPU. Labelled so those environments
# can skip it with `ctest -LE gpu`, which is how CI runs it.
set_tests_properties(shader_recompiler_compute_tests PROPERTIES LABELS gpu)

add_kyty_full_emulator_test(virtual_memory_allocation_tests ../tests/VirtualMemoryAllocationTests.cpp)


add_executable(kyty_emulator main.cpp ${kyty_emulator_src})

target_link_libraries(kyty_emulator ${kyty_emulator_link_libraries})
if (WIN32)
	target_link_libraries(kyty_emulator opengl32)
	target_link_libraries(kyty_emulator iphlpapi)
	target_link_libraries(kyty_emulator setupapi)
	target_link_libraries(kyty_emulator ws2_32)
endif()
if (KYTY_CLANG_CL)
	target_link_libraries(kyty_emulator onecore)
endif()
if (CLANG AND NOT KYTY_CLANG_CL)
	target_link_libraries(kyty_emulator pthread)
endif()
target_include_directories(kyty_emulator PRIVATE ${inc_headers})

clang_tidy_check(kyty_emulator "" "${check_headers}" "${inc_headers}")

include_what_you_use(kyty_emulator "${inc_headers}")

set(KYTY_EMULATOR_MAP_NAME "kyty_emulator_${KYTY_COMPILER_ID}_${KYTY_LINKER_ID}.map")

set(KYTY_EMULATOR_MAP_LINK_PATH "${CMAKE_CURRENT_BINARY_DIR}/${KYTY_EMULATOR_MAP_NAME}")
set(KYTY_EMULATOR_PDB_LINK_PATH "${CMAKE_CURRENT_BINARY_DIR}/kyty_emulator.pdb")

if(KYTY_CLANG_CL)
	set_target_properties(kyty_emulator PROPERTIES LINK_FLAGS "/DYNAMICBASE:NO /DEBUG:FULL /PDB:${KYTY_EMULATOR_PDB_LINK_PATH} /lldmap:${KYTY_EMULATOR_MAP_LINK_PATH}")
	add_custom_command(TARGET kyty_emulator POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KYTY_THIRD_PARTY_DIR}/winpthread/bin/libwinpthread-1.dll" $<TARGET_FILE_DIR:kyty_emulator>/libwinpthread-1.dll)
elseif(WIN32 OR LINUX)
	set_target_properties(kyty_emulator PROPERTIES LINK_FLAGS "${KYTY_LD_OPTIONS} -Wl,-Map=${KYTY_EMULATOR_MAP_LINK_PATH}")
endif()

add_dependencies(kyty_emulator KytyGitVersion)

install(TARGETS kyty_emulator DESTINATION .)
if(KYTY_BUILD_LAUNCHER)
	add_subdirectory(launcher)
endif()
if(KYTY_CLANG_CL)
	install(FILES "${KYTY_THIRD_PARTY_DIR}/winpthread/bin/libwinpthread-1.dll" DESTINATION .)
endif()
