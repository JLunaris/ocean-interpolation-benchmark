cmake_minimum_required(VERSION 4.1)
project(ocean-interpolation-benchmark CXX CUDA)

set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CUDA_ARCHITECTURES native)
set(CMAKE_PREFIX_PATH "E:/DE/netCDF 4.9.3")

add_executable(ocean-interpolation-benchmark
        main.cpp
        NcProcess.cpp
        CudaMain.cu
)

set_target_properties(ocean-interpolation-benchmark PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
)

target_compile_options(ocean-interpolation-benchmark PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/Zc:preprocessor>
)

add_subdirectory(third-party/netcdf-cxx4)

target_link_libraries(ocean-interpolation-benchmark PUBLIC netcdf-cxx4)

# 拷贝dll工作
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    foreach (NC_C_LIB netcdf)
        add_custom_command(TARGET ocean-interpolation-benchmark POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "E:/DE/netCDF 4.9.3/bin/${NC_C_LIB}.dll"
                "$<TARGET_FILE_DIR:ocean-interpolation-benchmark>")
    endforeach (NC_C_LIB)
endif ()
