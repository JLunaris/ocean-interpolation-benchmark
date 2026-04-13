#include "CudaInterpolationManager.cuh"
#include "NcProcess.h"

#include <chrono>

int main()
{
    NcProcess::NcReader reader{R"(E:\Projects\ocean-interpolation-benchmark\202505_sst.nc)"};
    auto result{*reader.readVarFloat("thetao")};

    // 生成真实样本点
    std::vector<NcProcess::FieldPoint> samplePoints = NcProcess::sampleRandomPoints(result, 100'000);
    CudaInterpolationManager manager{samplePoints, 1};

    // 设定插值的范围和步长
    std::vector latitudes{NcProcess::generateRange(-80.0, 80, 0.083)};
    std::vector longitudes{NcProcess::generateRange(-170, 170, 0.083)};

    auto t1 = std::chrono::high_resolution_clock::now();
    auto values = manager.interpolate<10>(latitudes, longitudes, 2);
    auto t2 = std::chrono::high_resolution_clock::now();
    std::cout << std::format("运行时间: {}ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());

    NcProcess::NcCreator writer{"example.nc"};
    writer.writeVarFloat("thetao", {latitudes, longitudes, values});

    return 0;
}
