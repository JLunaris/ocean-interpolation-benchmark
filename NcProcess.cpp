#include "NcProcess.h"

#include <print>

using namespace netCDF;
using namespace std;

namespace NcProcess {

Dataset::Dataset(std::vector<float> latitudes, std::vector<float> longitudes, std::vector<float> values)
        : m_latitudes{std::move(latitudes)}, m_longitudes{std::move(longitudes)}, m_values{std::move(values)}
{
}

NcReader::NcReader(const string &filePath)
        : m_file{filePath, NcFile::read}
{
}

optional<vector<pair<GeoCoord, float>>> NcReader::readVarFloat(const string &varName, size_t time, size_t depth) const
{
    const NcVar theVar{m_file.getVar(varName)};

    // ---------- check routine ----------
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

    const vector<size_t> start{time, depth, 0, 0};
    const vector<size_t> count{1, 1, nLat, nLon};

    vector<float> values(nLat * nLon);
    theVar.getVar(start, count, values.data());

    // ---------- assemble result ----------
    vector<pair<GeoCoord, float>> result;
    result.reserve(nLat * nLon);

    for (size_t i = 0; i < nLat; ++i) {
        for (size_t j = 0; j < nLon; ++j) {
            const size_t idx = i * nLon + j;
            result.emplace_back(
                    GeoCoord{latitudes[i], longitudes[j]},
                    values[idx]);
        }
    }

    return result;
}

} // namespace NcProcess
