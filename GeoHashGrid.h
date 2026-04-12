#pragma once

#include "GeoCoord.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <ranges>

#ifndef __CUDACC__
#include <print>
#endif

namespace GeoSpatial {

struct GridIndex
{
    int x, y;

    bool operator==(const GridIndex &other) const = default;
};

} // namespace GeoSpatial


namespace std {

#ifndef __CUDACC__
template<>
struct formatter<GeoSpatial::GridIndex>
{
    // 解析格式
    constexpr auto parse(std::format_parse_context &ctx)
    {
        return ctx.begin();
    }

    // 定义如何格式化输出
    auto format(const GeoSpatial::GridIndex &p, auto &ctx) const
    {
        return std::format_to(ctx.out(), "({}, {})", p.x, p.y);
    }
};
#endif

template<>
struct hash<GeoSpatial::GridIndex>
{
    size_t operator()(const GeoSpatial::GridIndex &index) const noexcept
    {
        // key = (x << 32) | y
        return std::hash<uint64_t>{}(uint64_t(index.x) << 32 | uint32_t(index.y));
    }
};

} // namespace std


namespace GeoSpatial {

class ScaleGridIndexer
{
    float m_cellSize;
    int m_minX;
    int m_maxX;

public:
    explicit ScaleGridIndexer(float cellSize = 1)
            : m_cellSize{cellSize}, m_minX{(int) std::floorf(-180 / m_cellSize)}, m_maxX{-m_minX - 1}
    {
        assert(cellSize >= std::numeric_limits<float>::epsilon());
    }

    GridIndex toGrid(const GeoCoord &p) const
    {
        assert(p.isValid());
        return GridIndex{
                (int) std::floorf(p.longitude() / m_cellSize),
                (int) std::floorf(p.latitude() / m_cellSize),
        };
    }

    float cellSize() const { return m_cellSize; }
    int minX() const { return m_minX; }
    int maxX() const { return m_maxX; }
};

template<typename PointDataType>
class GeoHashGrid
{
    std::unordered_multimap<GridIndex, PointDataType> m_map;
    ScaleGridIndexer m_indexer;

public:
    explicit GeoHashGrid(float cellSize)
            : m_indexer{cellSize}
    {}

    void insertPoint(const PointDataType &point, const GeoCoord &coord)
    {
        m_map.emplace(m_indexer.toGrid(coord), point);
    }

    const auto &map() const { return m_map; }
    const auto &indexer() const { return m_indexer; }

    /**
     * @param transToGeoCoord FunctionType: GeoCoord(const PointDataType &)
     */
    std::vector<std::pair<PointDataType, float>> findGeoPointsInRadius(const GeoCoord &queryCoord, float radius, auto transToGeoCoord) const
    {
        assert(queryCoord.isValid());
        if (m_map.empty()) {
            return {};
        }

        const GeoCoord boundingRectTopLeft{
                std::min(queryCoord.latitude() + radius, 90.f),
                GeoCoord::normalizeLongitude(queryCoord.longitude() - radius)};
        const GeoCoord boundingRectBottomRight{
                std::max(queryCoord.latitude() - radius, -90.f),
                GeoCoord::normalizeLongitude(queryCoord.longitude() + radius)};

        GridIndex topLeftGrid = m_indexer.toGrid(boundingRectTopLeft);/*std::println("topLeftGrid: {}",topLeftGrid);*/
        GridIndex bottomRightGrid = m_indexer.toGrid(boundingRectBottomRight);/*std::println("bottomRightGrid: {}",bottomRightGrid);*/
        const float radiusSquared = radius * radius;

        std::vector<std::pair<PointDataType, float>> result;

        int x0 = topLeftGrid.x;
        int x1 = bottomRightGrid.x;
        int minX = m_indexer.minX();
        int maxX = m_indexer.maxX();
        int x1PlusOne = (x1 == maxX ? minX : x1 + 1);

        for (int x = x0; x != x1PlusOne; x = (x == maxX ? minX : x + 1)) {
            for (int y = bottomRightGrid.y; y <= topLeftGrid.y; ++y) {
                GridIndex gridIndex{x, y};
                auto [begin, end]{m_map.equal_range(gridIndex)};
                for (auto iter{begin}; iter != end; ++iter) {
                    const GeoCoord theCoord{transToGeoCoord(iter->second)};
                    const float deltaLat = theCoord.latitude() - queryCoord.latitude();
                    const float deltaLon = GeoCoord::normalizeLongitude(theCoord.longitude() - queryCoord.longitude());
                    const float distSq = deltaLat * deltaLat + deltaLon * deltaLon;

                    if (distSq < radiusSquared)
                        result.emplace_back(iter->second, distSq);
                }
            }
        }

        return result;
    }
};

} // namespace GeoSpatial
