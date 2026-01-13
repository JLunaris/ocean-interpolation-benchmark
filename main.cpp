#include "NcProcess.h"

#include <chrono>
#include <netcdf>
#include <print>
#include <ranges>
#include <vector>

using std::cout;
using std::format;
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
    auto t1 = std::chrono::high_resolution_clock::now();
    auto result{*reader.readVarFloat("thetao")};
    auto t2 = std::chrono::high_resolution_clock::now();
    cout << format("运行时间: {}ms\n",
                   std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());

    std::println("{}", result[37, 0]);

    NcProcess::NcCreator writer{"example.nc"};
    writer.writeVarFloat("thetao", result);
}
