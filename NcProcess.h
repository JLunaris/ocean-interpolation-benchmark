#pragma once

#include <netcdf>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace NcProcess {

class GeoGrid
{
    std::vector<float> m_latitudes;
    std::vector<float> m_longitudes;
    std::vector<float> m_values;

public:
    GeoGrid(std::vector<float> latitudes, std::vector<float> longitudes, std::vector<float> values);

    std::span<const float> latitudes() const { return m_latitudes; }
    std::span<const float> longitudes() const { return m_longitudes; }
    std::span<const float> values() const { return m_values; }

    float operator[](size_t latIdx, size_t lonIdx) const { return m_values[latIdx * m_longitudes.size() + lonIdx]; }
};

class NcReader
{
    netCDF::NcFile m_file;

public:
    explicit NcReader(const std::string &filePath);
    std::optional<GeoGrid> readVarFloat(const std::string &varName, size_t timeIdx = 0, size_t depthIdx = 0) const;
};

class NcCreator
{
    netCDF::NcFile m_file;

public:
    explicit NcCreator(const std::string &filePath);
    void writeVarFloat(const std::string &varName, const GeoGrid &geoGrid) const;
};

struct GeoCoord {
    float lat;
    float lon;
};

struct FieldPoint {
    GeoCoord coord;
    float value;
};

// 从规则网格中，随机选出n个点，作为模拟观测数据
std::vector<FieldPoint> sampleRandomPoints(const GeoGrid &grid, std::size_t sampleCount);

} // namespace NcProcess
