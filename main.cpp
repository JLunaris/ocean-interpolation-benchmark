#include "InterpolationManager.h"
#include "NcProcess.h"

#include <chrono>
#include <netcdf>
#include <print>
#include <ranges>
#include <vector>

using std::cout;
using namespace netCDF;

void f()
{
    auto eptr{std::current_exception()};
    try {
        if (eptr) std::rethrow_exception(eptr);
    } catch (const std::exception &e) {
        std::println(std::cerr, "Exception: {}", e.what());
    } catch (...) {
        std::println(std::cerr, "Encountered a non-std exception");
    }
    _Exit(1);
}

int main()
{
    std::set_terminate(f);

    NcProcess::NcReader reader{R"(E:\Projects\ocean-interpolation-benchmark\202505_sst.nc)"};
    auto result{*reader.readVarFloat("thetao")};

    // 生成真实样本点
    std::vector<NcProcess::FieldPoint> samplePoints = NcProcess::sampleRandomPoints(result, 500'000);
    InterpolationManager manager{std::move(samplePoints), 1};

    // 设定插值的范围和步长
    std::vector latitudes{NcProcess::generateRange(-90.0, 90, 0.1)};
    std::vector longitudes{NcProcess::generateRange(-180, 180, 0.1)};

    auto t1 = std::chrono::high_resolution_clock::now();
    auto values = manager.interpolate(latitudes, longitudes, 1, 10);
    auto t2 = std::chrono::high_resolution_clock::now();
    std::println("运行时间: {}ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());

    NcProcess::NcCreator writer{"example.nc"};
    writer.writeVarFloat("thetao", {latitudes, longitudes, values});
}
