#pragma once

#include <netcdf>
#include <optional>
#include <string>
#include <vector>

namespace NcProcess {

struct GeoCoord {
    float lat;
    float lon;
};

class Dataset
{
    std::vector<float> m_latitudes;
    std::vector<float> m_longitudes;
    std::vector<float> m_values;

public:
    Dataset(std::vector<float> latitudes, std::vector<float> longitudes, std::vector<float> values);
};

class NcReader
{
    netCDF::NcFile m_file;

public:
    explicit NcReader(const std::string &filePath);
    [[nodiscard]] std::optional<std::vector<std::pair<GeoCoord, float>>> readVarFloat(const std::string &varName, size_t time = 0, size_t depth = 0) const;
};

} // namespace NcProcess
