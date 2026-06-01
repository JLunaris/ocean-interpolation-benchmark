#pragma once

#include "CudaBridge.cuh"
#include "NcProcess.h"

#include <cuda/std/span>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>

/**
 * @tparam neighborN 插值参考点个数（个数不够 → 拒绝插值, 个数过多 → 选择最近的）
 * @param results 插值结果
 * @param latitudes 所有要插值的纬度
 * @param longitudes 所有要插值的经度
 * @param samplePoints 插值参考样本
 * @param hashGrid 空间索引
 * @param radius 插值参考范围圆的半径
 */
template<int neighborN>
__global__ void interpolateKernel(cuda::std::span<float> results,
                                  cuda::std::span<const float> latitudes,
                                  cuda::std::span<const float> longitudes,
                                  cuda::std::span<const CudaBridge::FieldPoint> samplePoints,
                                  CudaBridge::GeoHashGrid<std::size_t>::GeoHashGridView hashGrid,
                                  float radius)
{
    int nLat = latitudes.size();
    int nLon = longitudes.size();
    int total = nLat * nLon;

    for (int idx = blockIdx.x * blockDim.x + threadIdx.x; idx < total; idx += blockDim.x * gridDim.x) {
        const int i = idx / nLon;   // latitude idx
        const int j = idx % nLon;   // longitude idx

        float lat = latitudes[i];
        float lon = longitudes[j];

        CudaBridge::GeoCoord interpPoint{lat, lon};
        auto samplesInRadius = hashGrid.findKnnInRadius<neighborN>(interpPoint, radius, [&samplePoints](std::size_t i) { return samplePoints[i].coord; });

        float result{cuda::std::numeric_limits<float>::quiet_NaN()};
        if (samplesInRadius.size() != neighborN) {
            results[idx] = result;
            continue;
        }

        float sumWeight{};
        float sumValue{};
        for (size_t i = 0; i < neighborN; ++i) {
            auto [sampleIdx, distSq] = samplesInRadius[i];
            if (distSq < cuda::std::numeric_limits<float>::epsilon()) {
                result = samplePoints[sampleIdx].value; // 精确命中
                goto Out;
            }
            float weight{1 / distSq};
            sumWeight += weight;
            sumValue += weight * samplePoints[sampleIdx].value;
        }
        result = sumValue / sumWeight;
    Out:
        results[idx] = result;/* 插值结果 */
    }
}

class CudaInterpolationManager
{
    thrust::device_vector<CudaBridge::FieldPoint> m_samplePoints; // 插值参考样本
    std::unique_ptr<CudaBridge::GeoHashGrid<std::size_t>> m_hashGrid; // 空间网格索引

public:
    /**
     * @param samplePoints 样本点
     * @param cellSize 空间网格索引的网格大小
     */
    explicit CudaInterpolationManager(std::span<const NcProcess::FieldPoint> samplePoints, float cellSize = 1)
    {
        auto result{samplePoints | std::views::transform(&CudaBridge::toFieldPoint)};
        m_samplePoints = std::vector(result.begin(), result.end());

        GeoSpatial::GeoHashGrid<std::size_t> hashGrid{cellSize};

        // 为参考样本建立空间索引
        for (std::size_t i = 0; i < samplePoints.size(); ++i)
            hashGrid.insertPoint(i, samplePoints[i].coord);

        // 转换为 trivially copyable 的 GeoHashGrid
        m_hashGrid = std::make_unique<CudaBridge::GeoHashGrid<std::size_t>>(hashGrid);
    }

    /**
     * @tparam neighborN 插值参考点个数（个数不够 → 拒绝插值, 个数过多 → 选择最近的）
     * @param latitudes 要插值的纬度
     * @param longitudes 要插值的经度
     * @param radius 插值参考范围圆的半径
     * @return 插值结果
     */
    template<int neighborN>
    std::vector<float> interpolate(thrust::device_vector<float> latitudes,
                                   thrust::device_vector<float> longitudes,
                                   float radius) const
    {
        thrust::device_vector<float> result(latitudes.size() * longitudes.size(), thrust::no_init);

        cuda::std::span<float> resultSpan{thrust::raw_pointer_cast(result.data()), result.size()};
        cuda::std::span<const float> latitudesSpan{thrust::raw_pointer_cast(latitudes.data()), latitudes.size()};
        cuda::std::span<const float> longitudeSpan{thrust::raw_pointer_cast(longitudes.data()), longitudes.size()};
        cuda::std::span<const CudaBridge::FieldPoint> samplePointsSpan{thrust::raw_pointer_cast(m_samplePoints.data()), m_samplePoints.size()};

        int nLat = latitudesSpan.size();
        int nLon = longitudeSpan.size();
        int total = nLat * nLon;

        int blockSize = 256;

        int gridSize = cuda::ceil_div(total, blockSize);

        interpolateKernel<neighborN><<<gridSize, blockSize>>>(resultSpan,
                                                              latitudesSpan,
                                                              longitudeSpan,
                                                              samplePointsSpan,
                                                              m_hashGrid->view(),
                                                              radius);
        cudaDeviceSynchronize();

        std::vector<float> resultOnCPU(result.size());
        thrust::copy(result.begin(), result.end(), resultOnCPU.begin());
        return resultOnCPU;
    }
};
