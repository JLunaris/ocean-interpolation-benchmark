#include "NcProcess.h"

#include <print>

using namespace netCDF;
using namespace std;

namespace NcProcess {

GeoGrid::GeoGrid(vector<float> latitudes, vector<float> longitudes, vector<float> values)
        : m_latitudes{std::move(latitudes)}, m_longitudes{std::move(longitudes)}, m_values{std::move(values)}
{
}

NcReader::NcReader(const string &filePath)
        : m_file{filePath, NcFile::read}
{
}

optional<GeoGrid> NcReader::readVarFloat(const string &varName, size_t timeIdx, size_t depthIdx) const
{
    const NcVar theVar{m_file.getVar(varName)};

    // ---------- check routine ----------
    // This reader expects a 4D float variable with dimensions ordered as (time, depth, latitude, longitude).
    {
        if (theVar.isNull()) {
            println(clog, R"(Variable "{}" does not exist)", varName);
            return {};
        }
        if (theVar.getType() != netCDF::ncFloat) {
            println(clog, R"(Variable "{}" is not of type float)", varName);
            return {};
        }
        if (theVar.getDimCount() != 4) {
            println(clog, R"(Variable "{}" has dimCount != 4)", varName);
            return {};
        }
        auto dimName = [&](int i) {
            return theVar.getDim(i).getName();
        };
        if (not(dimName(0) == "time" and
                dimName(1) == "depth" and
                dimName(2) == "latitude" and
                dimName(3) == "longitude")) {
            println(clog, R"(Variable "{}" dimensions are not expected.)", varName);
        }
    }

    // ---------- start read ----------
    const NcVar latVar{m_file.getVar("latitude")};
    const NcVar lonVar{m_file.getVar("longitude")};

    const size_t nLat{latVar.getDim(0).getSize()};
    const size_t nLon{lonVar.getDim(0).getSize()};

    vector<float> latitudes(nLat);
    vector<float> longitudes(nLon);

    latVar.getVar(latitudes.data());
    lonVar.getVar(longitudes.data());

    const vector<size_t> start{timeIdx, depthIdx, 0, 0};
    const vector<size_t> count{1, 1, nLat, nLon};

    vector<float> values(nLat * nLon);
    theVar.getVar(start, count, values.data());

    // ---------- assemble result ----------
    return GeoGrid{std::move(latitudes), std::move(longitudes), std::move(values)};
}

NcCreator::NcCreator(const std::string &filePath)
        : m_file{filePath, NcFile::replace}
{
}

void NcCreator::writeVarFloat(const string &varName, const GeoGrid &geoGrid) const
{
    // 定义维度
    const NcDim latDim{m_file.addDim("latitude", geoGrid.latitudes().size())};
    const NcDim lonDim{m_file.addDim("longitude", geoGrid.longitudes().size())};

    // 定义坐标变量
    const NcVar latVar{m_file.addVar("latitude", netCDF::ncFloat, latDim)};
    const NcVar lonVar{m_file.addVar("longitude", netCDF::ncFloat, lonDim)};

    // 定义主变量
    const vector<NcDim> dims{latDim, lonDim};
    const NcVar dataVar{m_file.addVar(varName, netCDF::ncFloat, dims)};

    // 写坐标数据
    latVar.putVar(geoGrid.latitudes().data());
    lonVar.putVar(geoGrid.longitudes().data());

    // 写主变量
    const std::vector<size_t> start{0, 0};
    const std::vector<size_t> count{geoGrid.latitudes().size(), geoGrid.longitudes().size()};
    dataVar.putVar(start, count, geoGrid.values().data());
}

} // namespace NcProcess
