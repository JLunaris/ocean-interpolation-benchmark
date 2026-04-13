#pragma once

#include "GeoHashGrid.h"
#include "NcProcess.h"

#include <cuda/std/inplace_vector>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>

namespace CudaBridge {

class GeoCoord
{
    float2 m_coord;

public:
    __host__ __device__ GeoCoord(float lat, float lon)
            : m_coord{lat, lon}
    {}

    __host__ __device__ float latitude() const { return m_coord.x; }

    __host__ __device__ float longitude() const { return m_coord.y; }

    __host__ __device__ void setLatitude(float latitude) { m_coord.x = latitude; }

    __host__ __device__ void setLongitude(float longitude) { m_coord.y = longitude; }

    __host__ __device__ bool isValidLat() const
    {
        return m_coord.x >= -90.0 && m_coord.x <= 90.0;
    }

    __host__ __device__ bool isValidLon() const
    {
        return m_coord.y >= -180.0 && m_coord.y < 180.0;
    }

    __host__ __device__ bool isValid() const
    {
        return isValidLat() && isValidLon();
    }

    // lon可以是任意浮点数
    __host__ __device__ static float normalizeLongitude(float lon)
    {
        lon = fmodf(lon + 180.f, 360.f);
        if (lon < 0)
            lon += 360;
        return lon - 180;
    }

    // lon1/lon2可以是任意浮点数
    __host__ __device__ static float deltaLongitude(float lon1, float lon2)
    {
        return normalizeLongitude(lon2 - lon1);
    }
};

struct FieldPoint
{
    GeoCoord coord;
    float value;
};

inline FieldPoint toFieldPoint(const NcProcess::FieldPoint &fieldPoint)
{
    GeoCoord cuGeoCoord{fieldPoint.coord.latitude(), fieldPoint.coord.longitude()};
    return {cuGeoCoord, fieldPoint.value};
}

struct GridIndex
{
    int x, y;

    bool operator==(const GridIndex &other) const = default;
};

inline GridIndex toGridIndex(const GeoSpatial::GridIndex &gridIndex)
{
    return {gridIndex.x, gridIndex.y};
}

class ScaleGridIndexer
{
    float m_cellSize;
    int m_minX;
    int m_maxX;
    int m_minY;
    int m_maxY;

public:
    explicit ScaleGridIndexer(float cellSize = 1)
            : m_cellSize{cellSize},
              m_minX{(int) std::floorf(-180 / m_cellSize)},
              m_maxX{-m_minX - 1},
              m_minY{(int) std::floorf(-90 / m_cellSize)},
              m_maxY{-m_minY - 1}
    {
        assert(cellSize >= std::numeric_limits<float>::epsilon());
    }

    __host__ __device__ GridIndex toGrid(const CudaBridge::GeoCoord &p) const
    {
        if (!p.isValid()) printf("p is not valid!");

        return GridIndex{
                (int) floorf(p.longitude() / m_cellSize),
                (int) floorf(p.latitude() / m_cellSize),
        };
    }

    __host__ __device__ float cellSize() const { return m_cellSize; }
    __host__ __device__ int minX() const { return m_minX; }
    __host__ __device__ int maxX() const { return m_maxX; }
    __host__ __device__ int minY() const { return m_minY; }
    __host__ __device__ int maxY() const { return m_maxY; }
};

template<typename PointDataType>
class GeoHashGrid
{
public:
    // trivially copyable 的视图对象
    class GeoHashGridView;

private:
    ScaleGridIndexer m_indexer;
    thrust::device_vector<cuda::std::span<PointDataType>> m_cells;

public:
    explicit GeoHashGrid(const GeoSpatial::GeoHashGrid<PointDataType> &srcHashGrid)
            : m_indexer{srcHashGrid.indexer().cellSize()}
    {
        const std::unordered_multimap<GeoSpatial::GridIndex, PointDataType> &srcMap{srcHashGrid.map()};

        for (int y: std::views::iota(m_indexer.minY(), m_indexer.maxY() + 1) | std::views::reverse) {
            for (int x: std::views::iota(m_indexer.minX(), m_indexer.maxX() + 1)) {
                auto [begin, end]{srcMap.equal_range({x, y})};
                auto values_view = std::ranges::subrange(begin, end) |
                                   std::views::transform([](const auto &kv) -> const PointDataType & {
                                       return kv.second;
                                   });
                std::vector<PointDataType> values{values_view.begin(), values_view.end()};
                PointDataType *cell{nullptr};
                if (!values.empty()) {
                    cudaMalloc(&cell, sizeof(PointDataType) * values.size());
                    cudaMemcpy(cell, values.data(), sizeof(PointDataType) * values.size(), cudaMemcpyDefault);
                }

                m_cells.push_back({cell, values.size()});
            }
        }
    }

    GeoHashGridView view() const
    {
        return {m_indexer, thrust::raw_pointer_cast(m_cells.data())};
    }
};

// trivially copyable 的视图对象
template<typename PointDataType>
class GeoHashGrid<PointDataType>::GeoHashGridView
{
    ScaleGridIndexer m_indexer;
    cuda::std::span<PointDataType> const *m_cells{nullptr};

private:
    struct Compare
    {
        __device__ bool operator()(const cuda::std::pair<PointDataType, float> &a,
                                   const cuda::std::pair<PointDataType, float> &b) const
        {
            return a.second < b.second;
        }
    };

    __device__ std::size_t mapToArrayIdx(const GridIndex &gridIdx) const
    {
        int minX{m_indexer.minX()};
        int maxX{m_indexer.maxX()};
        int maxY{m_indexer.maxY()};

        auto [x, y]{gridIdx};
        int width = maxX - minX + 1;
        return (maxY - y) * width + (x - minX);
    }

public:
    GeoHashGridView(const ScaleGridIndexer &indexer, cuda::std::span<PointDataType> const *cells)
            : m_indexer(indexer), m_cells(cells)
    {}

    /**
     * @param transToGeoCoord FunctionType: CudaBridge::GeoCoord(const PointDataType &)
     */
    template<int K>
    __device__ cuda::std::inplace_vector<cuda::std::pair<PointDataType, float>, K> findKnnInRadius(const GeoCoord &queryCoord,
                                                                                                   float radius,
                                                                                                   auto transToGeoCoord) const
    {
        if (!queryCoord.isValid()) {
            printf("queryCoord is invalid");
            return {};
        }

        const GeoCoord boundingRectTopLeft{
                cuda::std::min(queryCoord.latitude() + radius, 90.f),
                GeoCoord::normalizeLongitude(queryCoord.longitude() - radius)};
        const GeoCoord boundingRectBottomRight{
                cuda::std::max(queryCoord.latitude() - radius, -90.f),
                GeoCoord::normalizeLongitude(queryCoord.longitude() + radius)};

        GridIndex topLeftGrid = m_indexer.toGrid(boundingRectTopLeft);
        GridIndex bottomRightGrid = m_indexer.toGrid(boundingRectBottomRight);
        const float radiusSquared = radius * radius;

        cuda::std::inplace_vector<cuda::std::pair<PointDataType, float>, K> result;

        const int x0 = topLeftGrid.x;
        const int x1 = bottomRightGrid.x;
        const int minX = m_indexer.minX();
        const int maxX = m_indexer.maxX();
        const int x1PlusOne = (x1 == maxX ? minX : x1 + 1);

        for (int x = x0; x != x1PlusOne; x = (x == maxX ? minX : x + 1)) {
            for (int y = bottomRightGrid.y; y <= topLeftGrid.y; ++y) {
                GridIndex gridIndex{x, y};
                const auto &cell = m_cells[mapToArrayIdx(gridIndex)];
                for (const auto &point: cell) {
                    const GeoCoord theCoord{transToGeoCoord(point)};
                    const float deltaLat = theCoord.latitude() - queryCoord.latitude();
                    const float deltaLon = GeoCoord::normalizeLongitude(theCoord.longitude() - queryCoord.longitude());
                    const float distSq = deltaLat * deltaLat + deltaLon * deltaLon;

                    if (distSq < radiusSquared) {
                        if (result.size() < K) {
                            result.emplace_back(cuda::std::make_pair(point, distSq));
                        } else {
                            auto it = cuda::std::max_element(
                                    result.begin(),
                                    result.end(),
                                    Compare{});
                            if (distSq < it->second) {
                                *it = {point, distSq};
                            }
                        }
                    }
                }
            }
        }

        return result;
    }
};

} // namespace CudaBridge
