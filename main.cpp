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

    NcFile file{
            R"(C:\Users\Lunaris\Desktop\cmems_mod_glo_phy-thetao_anfc_0.083deg_P1M-m_1748333258908.nc)",
            netCDF::NcFile::read};

    auto dims{file.getDims()};
    std::vector<std::string> dimsName;
    for (const auto &key: dims | std::views::keys) {
        dimsName.push_back(key);
    }
    println("file.dims: {}", dimsName);

    auto vars{file.getVars()};
    std::vector<std::string> varsName;
    for (const auto &key: vars | std::views::keys) {
        varsName.push_back(key);
    }
    println("file.vars: {}", varsName);

    auto atts{file.getAtts()};
    std::vector<std::string> attsName;
    for (const auto &key: atts | std::views::keys) {
        attsName.push_back(key);
    }
    println("file.atts: {}", attsName);

    auto coordVars{file.getCoordVars()};
    std::vector<std::string> coordVarsName;
    for (const auto &key: coordVars | std::views::keys) {
        coordVarsName.push_back(key);
    }
    println("file.coordVars: {}", coordVarsName);

    auto groups{file.getGroups()};
    std::vector<std::string> groupsName;
    for (const auto &key: groups | std::views::keys) {
        groupsName.push_back(key);
    }
    println("file.groups: {}", groupsName);

    NcVar thetao{file.getVar("thetao")};
    println("{}", thetao.getType().getTypeClassName());
    auto thetaoDims = thetao.getDims();
    std::vector<std::string> thetaoDimsName;
    for (const auto &dim: thetaoDims) {
        thetaoDimsName.push_back(dim.getName());
    }
    println("thetaoDims: {}", thetaoDimsName);
    NcDim latDim, lonDim;
    for (const auto &dim: thetaoDims) {
        if (dim.getName() == "latitude") {
            latDim = dim;
        } else if (dim.getName() == "longitude") {
            lonDim = dim;
        }
    }

    std::vector<float> lats(latDim.getSize());
    std::vector<float> lons(lonDim.getSize());

    std::vector<size_t> start = {0, 0, 0, 0};
    std::vector<size_t> count = {1, 1, 1, 1};
    float value;


    NcProcess::NcReader reader{R"(E:\Projects\ocean-interpolation-benchmark\202505_sst.nc)"};
    auto t1 = std::chrono::high_resolution_clock::now();
    auto result{*reader.readVarFloat("so")};
    auto t2 = std::chrono::high_resolution_clock::now();
    cout << format("运行时间: {}ms\n",
                   std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count());

    std::println("{}", result[0].first);
}
