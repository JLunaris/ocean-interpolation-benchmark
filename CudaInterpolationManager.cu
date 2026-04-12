#include "CudaInterpolationManager.cuh"

__global__ void interpolate(cuda::std::span<float> result,
                            cuda::std::span<const float> latitudes,
                            cuda::std::span<const float> longitudes,
                            cuda::std::span<const CudaBridge::FieldPoint> samplePoints,
                            CudaBridge::GeoHashGrid<std::size_t>::GeoHashGridView hashGrid,
                            float radius,
                            int neighborN)
{
    auto idx = threadIdx.x + blockIdx.x * blockDim.x;
}

CudaInterpolationManager::CudaInterpolationManager(std::span<const NcProcess::FieldPoint> samplePoints, float cellSize)
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

std::vector<float> CudaInterpolationManager::interpolate(thrust::device_vector<float> latitudes,
                                                         thrust::device_vector<float> longitudes,
                                                         float radius,
                                                         int neighborN) const
{
    thrust::device_vector<float> result(latitudes.size() * longitudes.size(), thrust::no_init);

    cuda::std::span<float> resultSpan{thrust::raw_pointer_cast(result.data()), result.size()};
    cuda::std::span<const float> latitudesSpan{thrust::raw_pointer_cast(latitudes.data()), latitudes.size()};
    cuda::std::span<const float> longitudeSpan{thrust::raw_pointer_cast(longitudes.data()), longitudes.size()};
    cuda::std::span<const CudaBridge::FieldPoint> samplePointsSpan{thrust::raw_pointer_cast(m_samplePoints.data()), m_samplePoints.size()};

    ::interpolate<<<1, 1>>>(resultSpan,
                            latitudesSpan,
                            longitudeSpan,
                            samplePointsSpan,
                            m_hashGrid->view(),
                            radius,
                            neighborN);

    std::vector<float> resultOnCPU(result.size());
    thrust::copy(result.begin(), result.end(), resultOnCPU.begin());
    return resultOnCPU;
}
