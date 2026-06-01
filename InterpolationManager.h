#pragma once

#include "GeoHashGrid.h"
#include "NcProcess.h"

class InterpolationManager
{
    std::vector<NcProcess::FieldPoint> m_samplePoints; // 插值参考样本
    GeoSpatial::GeoHashGrid<std::size_t> m_hashGrid; // 空间网格索引

public:
    /**
     * @param samplePoints 样本点
     * @param cellSize 空间网格索引的网格大小
     */
    explicit InterpolationManager(std::vector<NcProcess::FieldPoint> samplePoints, float cellSize = 1)
            : m_samplePoints{std::move(samplePoints)}, m_hashGrid{cellSize}
    {
        // 为参考样本建立空间索引
        // for (const auto &[i, p]: std::views::enumerate(m_samplePoints))
        for (int i = 0; i < m_samplePoints.size(); ++i) {
            m_hashGrid.insertPoint(i, m_samplePoints[i].coord);
        }
        //     m_hashGrid.insertPoint(i, p.coord);
    }

    /**
     * @param latitudes 要插值的纬度
     * @param longitudes 要插值的经度
     * @param radius 插值参考范围圆的半径
     * @param neighborN 插值参考点个数（个数不够 → 拒绝插值, 个数过多 → 选择最近的）
     * @return 插值结果
     */
    std::vector<float> interpolate(std::span<const float> latitudes, std::span<const float> longitudes, float radius, int neighborN) const
    {
        std::vector<float> values;
        values.reserve(latitudes.size() * longitudes.size());

        // for (const auto &[lat, lon]: std::views::cartesian_product(latitudes, longitudes)) {

        for (const auto lat: latitudes) {
            for (const auto lon: longitudes) {
                GeoCoord interpPoint{lat, lon};
                auto samplesInRadius = m_hashGrid.findGeoPointsInRadius(interpPoint, radius, [this](std::size_t i) { return m_samplePoints[i].coord; });

                float result{std::numeric_limits<float>::quiet_NaN()};

                if (samplesInRadius.size() > neighborN) {
                    std::ranges::nth_element(samplesInRadius, begin(samplesInRadius) + neighborN - 1,
                                             {}, [](const auto &element) { return element.second; });
                    samplesInRadius.resize(neighborN);
                }

                if (samplesInRadius.size() == neighborN) {
                    float sumWeight{};
                    float sumValue{};
                    for (auto [sampleIdx, distSq]: samplesInRadius ) {
                        if (distSq < std::numeric_limits<float>::epsilon()) {
                            result = m_samplePoints[sampleIdx].value; // 精确命中
                            goto Out;
                        }
                        float weight{1 / distSq};
                        sumWeight += weight;
                        sumValue += weight * m_samplePoints[sampleIdx].value;
                    }
                    result = sumValue / sumWeight;
                }
                Out:
                    values.push_back(result);
            }
        }

        return values;
    }
};
