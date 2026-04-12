#pragma once

#include "CudaBridge.cuh"
#include "NcProcess.h"

#include <cuda/std/span>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>

/**
 * @param result 插值结果
 * @param latitudes 所有要插值的纬度
 * @param longitudes 所有要插值的经度
 * @param samplePoints 插值参考样本
 * @param hashGrid 空间索引
 * @param radius 插值参考范围圆的半径
 * @param neighborN 插值参考点个数（个数不够 → 拒绝插值, 个数过多 → 选择最近的）
 */
__global__ void interpolate(cuda::std::span<float> result,
                            cuda::std::span<const float> latitudes,
                            cuda::std::span<const float> longitudes,
                            cuda::std::span<const CudaBridge::FieldPoint> samplePoints,
                            CudaBridge::GeoHashGrid<std::size_t>::GeoHashGridView hashGrid,
                            float radius,
                            int neighborN);

class CudaInterpolationManager
{
    thrust::device_vector<CudaBridge::FieldPoint> m_samplePoints; // 插值参考样本
    std::unique_ptr<CudaBridge::GeoHashGrid<std::size_t>> m_hashGrid; // 空间网格索引

public:
    /**
     * @param samplePoints 样本点
     * @param cellSize 空间网格索引的网格大小
     */
    explicit CudaInterpolationManager(std::span<const NcProcess::FieldPoint> samplePoints, float cellSize = 1);

    /**
     * @param latitudes 要插值的纬度
     * @param longitudes 要插值的经度
     * @param radius 插值参考范围圆的半径
     * @param neighborN 插值参考点个数（个数不够 → 拒绝插值, 个数过多 → 选择最近的）
     * @return 插值结果
     */
    std::vector<float> interpolate(thrust::device_vector<float> latitudes,
                                   thrust::device_vector<float> longitudes,
                                   float radius,
                                   int neighborN) const;
};
