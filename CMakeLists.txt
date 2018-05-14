project(alac_converter)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
file(GLOB srcs "src/*.cpp")
add_executable(alac_converter ${srcs})
target_include_directories(alac_converter PRIVATE
    "include/"
    "spdlog/include/"
    )
