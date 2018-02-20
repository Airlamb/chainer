#include "xchainer/cuda/cuda_device.h"

#include <gtest/gtest.h>

#include "xchainer/cuda/cuda_backend.h"

namespace xchainer {
namespace cuda {
namespace {

TEST(CudaDeviceTest, Ctor) {
    CudaBackend backend;

    {
        CudaDevice device{backend, 0};
        EXPECT_EQ(&backend, &deivce.backend());
        EXPECT_EQ(0, device.index());
    }
    {
        CudaDevice device{backend, 1};
        EXPECT_EQ(&backend, &deivce.backend());
        EXPECT_EQ(1, device.index());
    }
}

}  // namespace
}  // namespace cuda
}  // namespace xchainer
