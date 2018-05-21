#include "xchainer/routines/connection.h"

#include <cstdint>

#include <nonstd/optional.hpp>

#include "xchainer/array.h"
#include "xchainer/constant.h"
#include "xchainer/device.h"
#include "xchainer/stack_vector.h"

namespace xchainer {

Array Conv(
        const Array& x,
        const Array& w,
        const nonstd::optional<Array>& b,
        const StackVector<int64_t, kMaxNdim>& stride,
        const StackVector<int64_t, kMaxNdim>& pad,
        bool cover_all) {
    return x.device().Conv(x, w, b, stride, pad, cover_all);
}

Array ConvTranspose(
        const Array& x,
        const Array& w,
        const nonstd::optional<Array>& b,
        const StackVector<int64_t, kMaxNdim>& stride,
        const StackVector<int64_t, kMaxNdim>& pad,
        const nonstd::optional<StackVector<int64_t, kMaxNdim>>& out_size) {
    return x.device().ConvTranspose(x, w, b, stride, pad, out_size);
}

}  // namespace xchainer
