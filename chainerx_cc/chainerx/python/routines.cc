#include "chainerx/python/routines.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nonstd/optional.hpp>

#include "chainerx/array.h"
#include "chainerx/axes.h"
#include "chainerx/constant.h"
#include "chainerx/context.h"
#include "chainerx/device.h"
#include "chainerx/dims.h"
#include "chainerx/dtype.h"
#include "chainerx/error.h"
#include "chainerx/macro.h"
#include "chainerx/routines/activation.h"
#include "chainerx/routines/arithmetic.h"
#include "chainerx/routines/binary.h"
#include "chainerx/routines/connection.h"
#include "chainerx/routines/creation.h"
#include "chainerx/routines/explog.h"
#include "chainerx/routines/hyperbolic.h"
#include "chainerx/routines/indexing.h"
#include "chainerx/routines/linalg.h"
#include "chainerx/routines/logic.h"
#include "chainerx/routines/loss.h"
#include "chainerx/routines/manipulation.h"
#include "chainerx/routines/misc.h"
#include "chainerx/routines/normalization.h"
#include "chainerx/routines/pooling.h"
#include "chainerx/routines/reduction.h"
#include "chainerx/routines/rounding.h"
#include "chainerx/routines/sorting.h"
#include "chainerx/routines/statistics.h"
#include "chainerx/routines/trigonometric.h"
#include "chainerx/scalar.h"

#include "chainerx/python/array.h"
#include "chainerx/python/array_index.h"
#include "chainerx/python/axes.h"
#include "chainerx/python/common.h"
#include "chainerx/python/device.h"
#include "chainerx/python/dtype.h"
#include "chainerx/python/shape.h"
#include "chainerx/python/stack_vector.h"
#include "chainerx/python/strides.h"

namespace chainerx {
namespace python {
namespace python_internal {

namespace py = pybind11;
using py::literals::operator""_a;

namespace {

using internal::MoveArrayBodies;
using internal::MoveArrayBody;

ArrayBodyPtr MakeArrayFromBuffer(py::buffer buffer, py::handle dtype, int64_t count, int64_t offset, py::handle device) {
    const py::buffer_info& info = buffer.request();

    int64_t n_bytes = info.size * info.itemsize;
    if (offset < 0 || offset > n_bytes) {
        throw ChainerxError{"offset must be non-negative and no greater than buffer length (", n_bytes, ")"};
    }

    if (!internal::IsContiguous(
                Shape{info.shape.begin(), info.shape.end()}, Strides{info.strides.begin(), info.strides.end()}, info.itemsize)) {
        throw ChainerxError{"ndarray is not C-contiguous"};
    }

    n_bytes -= offset;
    if (count < 0) {
        if (n_bytes % info.itemsize != 0) {
            throw ChainerxError{"buffer size must be a multiple of element size"};
        }
        count = n_bytes / info.itemsize;
    } else if (n_bytes < count * info.itemsize) {
        throw ChainerxError{"buffer is smaller than requested size"};
    }

    Shape shape{count};
    std::shared_ptr<void> data{info.ptr, [](void*) {}};

    return MoveArrayBody(chainerx::FromData(shape, GetDtype(dtype), data, nonstd::nullopt, offset, GetDevice(device)));
}

void InitChainerxCreation(pybind11::module& m) {
    // creation routines
    // TODO(niboshi): Accept CuPy ndarray in `array` and `asarray`. In principle it's CuPy's responsibility to provide some standard
    // interface to allow this, but users may want to convert cupy.ndarray to ChainerX before CuPy's support will be implemented. In such
    // case, ChainerX should provide the support for convenience.
    // TODO(niboshi): Add convenient function to convert to CuPy ndarray. Currently chainerx.ndarray exposes internal pointer
    // (ndarray.data_ptr, etc.) to support this, but users may want more convenient method. In principle ChainerX should support some
    // standard way (not depending on CuPy), but we might tentatively provide one which concretely depends on CuPy.
    m.def("array",
          [](py::handle object, py::handle dtype, bool copy, py::handle device) { return MakeArray(object, dtype, copy, device); },
          "object"_a,
          "dtype"_a = nullptr,
          "copy"_a = true,
          "device"_a = nullptr);
    // TODO(niboshi): Rename `object` to `a` as per numpy.
    m.def("asarray",
          [](py::handle object, py::handle dtype, py::handle device) { return MakeArray(object, dtype, false, device); },
          "object"_a,
          "dtype"_a = nullptr,
          "device"_a = nullptr);
    m.def("ascontiguousarray",
          [](py::handle a, py::handle dtype, py::handle device) {
              Array arr{MakeArray(a, dtype, false, device)};
              return MoveArrayBody(AsContiguousArray(arr));
          },
          "a"_a,
          "dtype"_a = nullptr,
          "device"_a = nullptr);
    m.def("empty",
          [](py::handle shape, py::handle dtype, py::handle device) {
              return MoveArrayBody(Empty(ToShape(shape), dtype.is_none() ? Dtype::kFloat32 : GetDtype(dtype), GetDevice(device)));
          },
          "shape"_a,
          "dtype"_a = nullptr,
          "device"_a = nullptr);
    m.def("full",
          [](py::handle shape, Scalar fill_value, py::handle dtype, py::handle device) {
              return MoveArrayBody(Full(ToShape(shape), fill_value, GetDtype(dtype), GetDevice(device)));
          },
          "shape"_a,
          "fill_value"_a,
          "dtype"_a,
          "device"_a = nullptr);
    m.def("full",
          [](py::int_ dim, Scalar fill_value, py::handle dtype, py::handle device) {
              return MoveArrayBody(Full(Shape{dim}, fill_value, GetDtype(dtype), GetDevice(device)));
          },
          "shape"_a,
          "fill_value"_a,
          "dtype"_a,
          "device"_a = nullptr);
    m.def("full",
          [](py::handle shape, Scalar fill_value, py::handle device) {
              return MoveArrayBody(Full(ToShape(shape), fill_value, GetDevice(device)));
          },
          "shape"_a,
          "fill_value"_a,
          "device"_a = nullptr);
    m.def("full",
          [](py::int_ dim, Scalar fill_value, py::handle device) { return MoveArrayBody(Full(Shape{dim}, fill_value, GetDevice(device))); },
          "shape"_a,
          "fill_value"_a,
          "device"_a = nullptr);
    m.def("zeros",
          [](py::handle shape, py::handle dtype, py::handle device) {
              return MoveArrayBody(Zeros(ToShape(shape), dtype.is_none() ? Dtype::kFloat32 : GetDtype(dtype), GetDevice(device)));
          },
          "shape"_a,
          "dtype"_a = nullptr,
          "device"_a = nullptr);
    m.def("zeros",
          [](py::int_ dim, py::handle dtype, py::handle device) {
              return MoveArrayBody(Zeros(Shape{dim}, dtype.is_none() ? Dtype::kFloat32 : GetDtype(dtype), GetDevice(device)));
          },
          "shape"_a,
          "dtype"_a = nullptr,
          "device"_a = nullptr);
    m.def("ones",
          [](py::handle shape, py::handle dtype, py::handle device) {
              return MoveArrayBody(Ones(ToShape(shape), dtype.is_none() ? Dtype::kFloat32 : GetDtype(dtype), GetDevice(device)));
          },
          "shape"_a,
          "dtype"_a = nullptr,
          "device"_a = nullptr);
    m.def("ones",
          [](py::int_ dim, py::handle dtype, py::handle device) {
              return MoveArrayBody(Ones(Shape{dim}, dtype.is_none() ? Dtype::kFloat32 : GetDtype(dtype), GetDevice(device)));
          },
          "shape"_a,
          "dtype"_a = nullptr,
          "device"_a = nullptr);
    m.def("arange",
          [](Scalar start_or_stop,
             const nonstd::optional<Scalar>& maybe_stop,
             const nonstd::optional<Scalar>& maybe_step,
             py::handle dtype,
             py::handle device) {
              DtypeKind start_or_stop_dtype_kind = start_or_stop.kind();
              Scalar start{0, start_or_stop_dtype_kind};
              Scalar stop{start_or_stop};
              Scalar step = maybe_step.has_value() ? maybe_step.value() : Scalar{1, start_or_stop_dtype_kind};

              if (maybe_stop.has_value()) {
                  start = start_or_stop;
                  stop = maybe_stop.value();
              }

              return dtype.is_none() ? MoveArrayBody(Arange(start, stop, step, GetDevice(device)))
                                     : MoveArrayBody(Arange(start, stop, step, GetDtype(dtype), GetDevice(device)));
          },
          "start"_a,
          "stop"_a = nullptr,
          "step"_a = nullptr,
          "dtype"_a = nullptr,
          "device"_a = nullptr);
    m.def("empty_like",
          [](const ArrayBodyPtr& a, py::handle device) { return MoveArrayBody(EmptyLike(Array{a}, GetDevice(device))); },
          "a"_a,
          "device"_a = nullptr);
    m.def("full_like",
          [](const ArrayBodyPtr& a, Scalar value, py::handle device) {
              return MoveArrayBody(FullLike(Array{a}, value, GetDevice(device)));
          },
          "a"_a,
          "fill_value"_a,
          "device"_a = nullptr);
    m.def("zeros_like",
          [](const ArrayBodyPtr& a, py::handle device) { return MoveArrayBody(ZerosLike(Array{a}, GetDevice(device))); },
          "a"_a,
          "device"_a = nullptr);
    m.def("ones_like",
          [](const ArrayBodyPtr& a, py::handle device) { return MoveArrayBody(OnesLike(Array{a}, GetDevice(device))); },
          "a"_a,
          "device"_a = nullptr);
    m.def("copy", [](const ArrayBodyPtr& a) { return MoveArrayBody(Copy(Array{a})); }, "a"_a);
    m.def("frombuffer", &MakeArrayFromBuffer, "buffer"_a, "dtype"_a = "float32", "count"_a = -1, "offset"_a = 0, "device"_a = nullptr);
    m.def("identity",
          [](int64_t n, py::handle dtype, py::handle device) {
              return MoveArrayBody(Identity(n, dtype.is_none() ? Dtype::kFloat32 : GetDtype(dtype), GetDevice(device)));
          },
          "n"_a,
          "dtype"_a = nullptr,
          "device"_a = nullptr);
    m.def("eye",
          [](int64_t n, nonstd::optional<int64_t> m, int64_t k, py::handle dtype, py::handle device) {
              if (!m.has_value()) {
                  m = n;
              }
              return MoveArrayBody(Eye(n, m.value(), k, GetDtype(dtype), GetDevice(device)));
          },
          "N"_a,
          "M"_a = nullptr,
          "k"_a = 0,
          "dtype"_a = "float64",
          "device"_a = nullptr);
    m.def("diag",
          [](const ArrayBodyPtr& v, int64_t k, py::handle device) { return MoveArrayBody(Diag(Array{v}, k, GetDevice(device))); },
          "v"_a,
          "k"_a = 0,
          "device"_a = nullptr);
    m.def("diagflat",
          [](const ArrayBodyPtr& v, int64_t k, py::handle device) { return MoveArrayBody(Diagflat(Array{v}, k, GetDevice(device))); },
          "v"_a,
          "k"_a = 0,
          "device"_a = nullptr);
    m.def("linspace",
          [](Scalar start, Scalar stop, int64_t num, bool endpoint, py::handle dtype, py::handle device) {
              return MoveArrayBody(Linspace(
                      start,
                      stop,
                      num,
                      endpoint,
                      dtype.is_none() ? nonstd::optional<Dtype>{nonstd::nullopt} : nonstd::optional<Dtype>{GetDtype(dtype)},
                      GetDevice(device)));
          },
          "start"_a,
          "stop"_a,
          "num"_a = 50,
          "endpoint"_a = true,
          "dtype"_a = nullptr,
          "device"_a = nullptr);
}

void InitChainerxIndexing(pybind11::module& m) {
    // indexing routines
    m.def("take",
          [](const ArrayBodyPtr& a, py::handle indices, const nonstd::optional<int8_t>& axis) {
              if (!axis.has_value()) {
                  throw NotImplementedError{"axis=None is not yet supported for chainerx.take."};
              }
              if (py::isinstance<ArrayBody>(indices)) {
                  return MoveArrayBody(Take(Array{a}, Array{py::cast<ArrayBodyPtr>(indices)}, axis.value()));
              }
              if (py::isinstance<py::sequence>(indices)) {
                  nonstd::optional<Dtype> dtype = Dtype::kInt64;
                  return MoveArrayBody(Take(Array{a}, Array{MakeArray(indices, dtype, false, a->device())}, axis.value()));
              }
              if (py::isinstance<py::array>(indices)) {
                  return MoveArrayBody(
                          Take(Array{a}, Array{MakeArrayFromNumpyArray(py::cast<py::array>(indices), a->device())}, axis.value()));
              }
              throw py::type_error{"only integers, slices (`:`), sequence, numpy.ndarray and chainerx.newaxis (`None`) are valid indices"};
          },
          "a"_a,
          "indices"_a,
          "axis"_a);
    m.def("where",
          [](const ArrayBodyPtr& condition, const ArrayBodyPtr& x, const ArrayBodyPtr& y) {
              return MoveArrayBody(Where(Array{condition}, Array{x}, Array{y}));
          },
          "condition"_a,
          "x"_a,
          "y"_a);
    m.def("where",
          [](const ArrayBodyPtr& condition, const ArrayBodyPtr& x, Scalar y) {
              return MoveArrayBody(Where(Array{condition}, Array{x}, y));
          },
          "condition"_a,
          "x"_a,
          "y"_a);
    m.def("where",
          [](const ArrayBodyPtr& condition, Scalar x, const ArrayBodyPtr& y) {
              return MoveArrayBody(Where(Array{condition}, x, Array{y}));
          },
          "condition"_a,
          "x"_a,
          "y"_a);
    m.def("where",
          [](const ArrayBodyPtr& condition, Scalar x, Scalar y) { return MoveArrayBody(Where(Array{condition}, x, y)); },
          "condition"_a,
          "x"_a,
          "y"_a);
}

void InitChainerxLinalg(pybind11::module& m) {
    // linalg routines
    m.def("dot", [](const ArrayBodyPtr& a, const ArrayBodyPtr& b) { return MoveArrayBody(Dot(Array{a}, Array{b})); }, "a"_a, "b"_a);
}

void InitChainerxLogic(pybind11::module& m) {
    // logic routines
    m.def("equal",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Equal(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("not_equal",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(NotEqual(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("greater",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Greater(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("greater_equal",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(GreaterEqual(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("less", [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Less(Array{x1}, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("less_equal",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(LessEqual(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("logical_and",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(LogicalAnd(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("logical_or",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(LogicalOr(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("logical_not", [](const ArrayBodyPtr& x) { return MoveArrayBody(LogicalNot(Array{x})); }, "x"_a);
    m.def("logical_xor",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(LogicalXor(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("all",
          [](const ArrayBodyPtr& a, int8_t axis, bool keepdims) { return MoveArrayBody(All(Array{a}, Axes{axis}, keepdims)); },
          "a"_a,
          "axis"_a,
          "keepdims"_a = false);
    m.def("all",
          [](const ArrayBodyPtr& a, const nonstd::optional<std::vector<int8_t>>& axis, bool keepdims) {
              return MoveArrayBody(All(Array{a}, ToAxes(axis), keepdims));
          },
          "a"_a,
          "axis"_a = nullptr,
          "keepdims"_a = false);
    m.def("any",
          [](const ArrayBodyPtr& a, int8_t axis, bool keepdims) { return MoveArrayBody(Any(Array{a}, Axes{axis}, keepdims)); },
          "a"_a,
          "axis"_a,
          "keepdims"_a = false);
    m.def("any",
          [](const ArrayBodyPtr& a, const nonstd::optional<std::vector<int8_t>>& axis, bool keepdims) {
              return MoveArrayBody(Any(Array{a}, ToAxes(axis), keepdims));
          },
          "a"_a,
          "axis"_a = nullptr,
          "keepdims"_a = false);
    m.def("isnan", [](const ArrayBodyPtr& x) { return MoveArrayBody(IsNan(Array{x})); }, "x"_a);
    m.def("isinf", [](const ArrayBodyPtr& x) { return MoveArrayBody(IsInf(Array{x})); }, "x"_a);
    m.def("isfinite", [](const ArrayBodyPtr& x) { return MoveArrayBody(IsFinite(Array{x})); }, "x"_a);
}

void InitChainerxManipulation(pybind11::module& m) {
    // manipulation routines
    m.def("transpose",
          [](const ArrayBodyPtr& a, const nonstd::optional<std::vector<int8_t>>& axes) {
              return MoveArrayBody(Transpose(Array{a}, ToAxes(axes)));
          },
          "a"_a,
          "axes"_a = nullptr);
    m.def("transpose",
          [](const ArrayBodyPtr& a, int8_t axes) { return MoveArrayBody(Transpose(Array{a}, {axes})); },
          "a"_a,
          "axes"_a = nullptr);
    m.def("flip",
          [](const ArrayBodyPtr& m, const nonstd::optional<std::vector<int8_t>>& axes) {
              return MoveArrayBody(Flip(Array{m}, ToAxes(axes)));
          },
          "m"_a,
          "axes"_a = nullptr);
    m.def("flip", [](const ArrayBodyPtr& m, int8_t axes) { return MoveArrayBody(Flip(Array{m}, {axes})); }, "m"_a, "axes"_a = nullptr);
    m.def("fliplr", [](const ArrayBodyPtr& m) { return MoveArrayBody(Fliplr(Array{m})); }, "m"_a);
    m.def("flipud", [](const ArrayBodyPtr& m) { return MoveArrayBody(Flipud(Array{m})); }, "m"_a);
    m.def("rollaxis",
          [](const ArrayBodyPtr& a, int8_t axis, int8_t start) { return MoveArrayBody(RollAxis(Array{a}, axis, start)); },
          "a"_a,
          "axis"_a,
          "start"_a = 0);
    m.def("reshape",
          [](const ArrayBodyPtr& a, py::handle newshape) { return MoveArrayBody(Reshape(Array{a}, ToShape(newshape))); },
          "a"_a,
          "newshape"_a);
    m.def("reshape",
          [](const ArrayBodyPtr& a, const std::vector<int64_t>& newshape) {
              return MoveArrayBody(Reshape(Array{a}, {newshape.begin(), newshape.end()}));
          },
          "a"_a,
          "newshape"_a);
    m.def("reshape",
          [](const ArrayBodyPtr& a, py::args args) {
              if (args.size() == 0) {
                  throw ChainerxError("Reshape is missing shape argument.");
              }
              return MoveArrayBody(Reshape(Array{a}, ToShape(args)));
          },
          "a"_a);
    m.def("squeeze",
          [](const ArrayBodyPtr& a, const nonstd::optional<std::vector<int8_t>>& axis) {
              return MoveArrayBody(Squeeze(Array{a}, ToAxes(axis)));
          },
          "a"_a,
          "axis"_a = nullptr);
    m.def("squeeze", [](const ArrayBodyPtr& a, int8_t axis) { return MoveArrayBody(Squeeze(Array{a}, Axes{axis})); }, "a"_a, "axis"_a);
    m.def("expand_dims", [](const ArrayBodyPtr& a, int8_t axis) { return MoveArrayBody(ExpandDims(Array{a}, axis)); }, "a"_a, "axis"_a);
    m.def("swapaxes",
          [](const ArrayBodyPtr& a, int8_t axis1, int8_t axis2) { return MoveArrayBody(Swapaxes(Array{a}, axis1, axis2)); },
          "a"_a,
          "axis1"_a,
          "axis2"_a);
    m.def("broadcast_to",
          [](const ArrayBodyPtr& array, py::handle shape) { return MoveArrayBody(Array{array}.BroadcastTo(ToShape(shape))); },
          "array"_a,
          "shape"_a);
    m.def("concatenate",
          [](py::sequence arrays, nonstd::optional<int8_t> axis) {
              std::vector<Array> xs;
              xs.reserve(arrays.size());
              std::transform(arrays.begin(), arrays.end(), std::back_inserter(xs), [](const auto& item) {
                  return Array{py::cast<ArrayBodyPtr>(item)};
              });
              return MoveArrayBody(Concatenate(xs, axis));
          },
          "arrays"_a,
          "axis"_a = 0);
    m.def("stack",
          [](py::sequence arrays, int8_t axis) {
              std::vector<Array> xs;
              xs.reserve(arrays.size());
              std::transform(arrays.begin(), arrays.end(), std::back_inserter(xs), [](const auto& item) {
                  return Array{py::cast<ArrayBodyPtr>(item)};
              });
              return MoveArrayBody(Stack(xs, axis));
          },
          "arrays"_a,
          "axis"_a = 0);
    m.def("atleast_2d", [](const ArrayBodyPtr& a) { return MoveArrayBody(AtLeast2D(Array{a})); }, "a"_a);
    m.def("atleast_3d", [](const ArrayBodyPtr& a) { return MoveArrayBody(AtLeast3D(Array{a})); }, "a"_a);
    m.def("hstack",
          [](py::sequence arrays) {
              std::vector<Array> xs;
              xs.reserve(arrays.size());
              std::transform(arrays.begin(), arrays.end(), std::back_inserter(xs), [](const auto& item) {
                  return Array{py::cast<ArrayBodyPtr>(item)};
              });
              return MoveArrayBody(HStack(xs));
          },
          "arrays"_a);
    m.def("vstack",
          [](py::sequence arrays) {
              std::vector<Array> xs;
              xs.reserve(arrays.size());
              std::transform(arrays.begin(), arrays.end(), std::back_inserter(xs), [](const auto& item) {
                  return Array{py::cast<ArrayBodyPtr>(item)};
              });
              return MoveArrayBody(VStack(xs));
          },
          "arrays"_a);
    m.def("dstack",
          [](py::sequence arrays) {
              std::vector<Array> xs;
              xs.reserve(arrays.size());
              std::transform(arrays.begin(), arrays.end(), std::back_inserter(xs), [](const auto& item) {
                  return Array{py::cast<ArrayBodyPtr>(item)};
              });
              return MoveArrayBody(DStack(xs));
          },
          "arrays"_a);

    m.def("split",
          [](const ArrayBodyPtr& ary, py::handle indices_or_sections, int8_t axis) {
              // TODO(niboshi): Perhaps we would want more general approach to handle multi-type arguments like indices_or_sections to
              // provide more helpful error message for users.

              auto split_sections = [](const ArrayBodyPtr& ary, int64_t sections, int8_t axis) {
                  return MoveArrayBodies(Split(Array{ary}, sections, axis));
              };
              auto split_indices = [](const ArrayBodyPtr& ary, const std::vector<int64_t>& indices, int8_t axis) {
                  return MoveArrayBodies(Split(Array{ary}, indices, axis));
              };

              // Converts an python float to sections (int64_t).
              // Raises ValueError if the value has non-zero fraction.
              auto pyfloat_to_sections_or_value_error = [](py::handle num) {
                  CHAINERX_ASSERT(py::isinstance<py::float_>(num));
                  double num_fp = py::cast<double>(num);
                  auto num_int = static_cast<int64_t>(num_fp);
                  if (static_cast<double>(num_int) != num_fp) {
                      throw py::value_error{"Sections must be an integer."};
                  }
                  return num_int;
              };

              // sections: int
              if (py::isinstance<py::int_>(indices_or_sections)) {
                  int64_t sections = py::cast<int64_t>(indices_or_sections);
                  return split_sections(ary, sections, axis);
              }
              // sections: float
              if (py::isinstance<py::float_>(indices_or_sections)) {
                  int64_t sections = pyfloat_to_sections_or_value_error(indices_or_sections);
                  return split_sections(ary, sections, axis);
              }
              // numpy.ndarray
              if (py::isinstance<py::array>(indices_or_sections)) {
                  py::array np_ios = py::cast<py::array>(indices_or_sections);
                  if (np_ios.ndim() >= 2) {
                      throw py::value_error{std::string{"Too many dimensions of indices: "} + std::to_string(np_ios.ndim())};
                  }
                  // sections: scalar
                  if (np_ios.ndim() == 0) {
                      int64_t sections{};
                      py::object scalar_np = np_ios.attr("tolist")();
                      if (py::isinstance<py::int_>(scalar_np)) {
                          sections = py::cast<int64_t>(scalar_np);
                      } else if (py::isinstance<py::float_>(scalar_np)) {
                          sections = pyfloat_to_sections_or_value_error(scalar_np);
                      } else {
                          throw py::type_error{"Sections must be an integer."};
                      }
                      return split_sections(ary, sections, axis);
                  }

                  // indices: (0,)-shape
                  if (np_ios.size() == 0) {
                      return split_indices(ary, {}, axis);
                  }

                  if (np_ios.dtype().kind() != 'i') {
                      throw py::type_error{std::string{"Indices must be integers."}};
                  }
                  // indices: non-scalar
                  std::vector<int64_t> indices{};
                  py::list indices_pylist = np_ios.attr("tolist")();
                  for (py::handle item : indices_pylist) {
                      indices.emplace_back(py::cast<int64_t>(item));
                  }

                  return split_indices(ary, indices, axis);
              }
              // indices: sequence
              if (py::isinstance<py::sequence>(indices_or_sections)) {
                  std::vector<int64_t> indices{};
                  try {
                      indices = py::cast<std::vector<int64_t>>(indices_or_sections);
                  } catch (const py::cast_error& e) {
                      throw py::type_error{std::string{"Indices not understood: "} + py::cast<std::string>(py::repr(indices_or_sections))};
                  }

                  return split_indices(ary, indices, axis);
              }
              throw py::type_error{std::string{"indices_or_sections not understood: "} +
                                   py::cast<std::string>(py::repr(indices_or_sections))};
          },
          "ary"_a,
          "indices_or_sections"_a,
          "axis"_a = 0);
    m.def("moveaxis",
          [](const ArrayBodyPtr& a, const std::vector<int8_t>& source, const std::vector<int8_t>& destination) {
              return MoveArrayBody(Moveaxis(Array{a}, Axes{source.begin(), source.end()}, Axes{destination.begin(), destination.end()}));
          },
          "a"_a,
          "source"_a = nullptr,
          "destination"_a = nullptr);
    m.def("moveaxis",
          [](const ArrayBodyPtr& a, py::tuple source, py::tuple destination) {
              return MoveArrayBody(Moveaxis(Array{a}, ToAxes(source), ToAxes(destination)));
          },
          "a"_a,
          "source"_a = nullptr,
          "destination"_a = nullptr);
    m.def("moveaxis",
          [](const ArrayBodyPtr& a, int8_t source, int8_t destination) {
              return MoveArrayBody(Moveaxis(Array{a}, {source}, {destination}));
          },
          "a"_a,
          "source"_a = nullptr,
          "destination"_a = nullptr);
}

void InitChainerxActivation(pybind11::module& m) {
    m.def("sigmoid", [](const ArrayBodyPtr& x) { return MoveArrayBody(Sigmoid(Array{x})); }, "x"_a);
    m.def("relu", [](const ArrayBodyPtr& x) { return MoveArrayBody(Relu(Array{x})); }, "x"_a);
    m.def("leaky_relu",
          [](const ArrayBodyPtr& x, Scalar slope) { return MoveArrayBody(LeakyRelu(Array{x}, slope)); },
          py::arg("x"),
          py::arg("slope") = 0.2);
}

void InitChainerxArithmetic(pybind11::module& m) {
    // math routines
    m.def("negative", [](const ArrayBodyPtr& x) { return MoveArrayBody(Negative(Array{x})); }, "x"_a);
    m.def("add", [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Array{x1} + Array{x2}); }, "x1"_a, "x2"_a);
    m.def("add", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(Add(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("add", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Add(x1, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("subtract", [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Array{x1} - Array{x2}); }, "x1"_a, "x2"_a);
    m.def("subtract", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(Subtract(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("subtract", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Subtract(x1, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("multiply", [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Array{x1} * Array{x2}); }, "x1"_a, "x2"_a);
    m.def("multiply", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(Multiply(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("multiply", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Multiply(x1, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("divide", [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Array{x1} / Array{x2}); }, "x1"_a, "x2"_a);
    m.def("divide", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(Divide(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("divide", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Divide(x1, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("floor_divide",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(FloorDivide(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("floor_divide", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(FloorDivide(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("floor_divide", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(FloorDivide(x1, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("true_divide",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(TrueDivide(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("true_divide", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(TrueDivide(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("true_divide", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(TrueDivide(x1, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("reciprocal", [](const ArrayBodyPtr& x) { return MoveArrayBody(Reciprocal(Array{x})); }, "x"_a);
    m.def("power",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Power(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("power", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(Power(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("power", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Power(x1, Array{x2})); }, "x1"_a, "x2"_a);
}

void InitChainerxBinary(pybind11::module& m) {
    m.def("bitwise_and",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(BitwiseAnd(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("bitwise_and", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(BitwiseAnd(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("bitwise_and", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(BitwiseAnd(x1, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("bitwise_or",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(BitwiseOr(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("bitwise_or", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(BitwiseOr(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("bitwise_or", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(BitwiseOr(x1, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("bitwise_xor",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(BitwiseXor(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("bitwise_xor", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(BitwiseXor(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("bitwise_xor", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(BitwiseXor(x1, Array{x2})); }, "x1"_a, "x2"_a);
}

void InitChainerxExpLog(pybind11::module& m) {
    m.def("erf", [](const ArrayBodyPtr& x) { return MoveArrayBody(Erf(Array{x})); }, "x"_a);
    m.def("exp", [](const ArrayBodyPtr& x) { return MoveArrayBody(Exp(Array{x})); }, "x"_a);
    m.def("expm1", [](const ArrayBodyPtr& x) { return MoveArrayBody(Expm1(Array{x})); }, "x"_a);
    m.def("exp2", [](const ArrayBodyPtr& x) { return MoveArrayBody(Exp2(Array{x})); }, "x"_a);
    m.def("log", [](const ArrayBodyPtr& x) { return MoveArrayBody(Log(Array{x})); }, "x"_a);
    m.def("log10", [](const ArrayBodyPtr& x) { return MoveArrayBody(Log10(Array{x})); }, "x"_a);
    m.def("log2", [](const ArrayBodyPtr& x) { return MoveArrayBody(Log2(Array{x})); }, "x"_a);
    m.def("log1p", [](const ArrayBodyPtr& x) { return MoveArrayBody(Log1p(Array{x})); }, "x"_a);
}

void InitChainerxHyperbolic(pybind11::module& m) {
    m.def("sinh", [](const ArrayBodyPtr& x) { return MoveArrayBody(Sinh(Array{x})); }, "x"_a);
    m.def("cosh", [](const ArrayBodyPtr& x) { return MoveArrayBody(Cosh(Array{x})); }, "x"_a);
    m.def("tanh", [](const ArrayBodyPtr& x) { return MoveArrayBody(Tanh(Array{x})); }, "x"_a);
    m.def("arcsinh", [](const ArrayBodyPtr& x) { return MoveArrayBody(Arcsinh(Array{x})); }, "x"_a);
    m.def("arccosh", [](const ArrayBodyPtr& x) { return MoveArrayBody(Arccosh(Array{x})); }, "x"_a);
}

void InitChainerxMisc(pybind11::module& m) {
    m.def("square", [](const ArrayBodyPtr& x) { return MoveArrayBody(Square(Array{x})); }, "x"_a);
    m.def("squared_difference",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(SquaredDifference(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("sqrt", [](const ArrayBodyPtr& x) { return MoveArrayBody(Sqrt(Array{x})); }, "x"_a);
    m.def("abs", [](const ArrayBodyPtr& x) { return MoveArrayBody(Absolute(Array{x})); }, "x"_a);
    m.attr("absolute") = m.attr("abs");
    m.def("fabs", [](const ArrayBodyPtr& x) { return MoveArrayBody(Fabs(Array{x})); }, "x"_a);
    m.def("sign", [](const ArrayBodyPtr& x) { return MoveArrayBody(Sign(Array{x})); }, "x"_a);
    m.def("maximum", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(Maximum(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("maximum", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Maximum(x1, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("maximum",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Maximum(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("minimum", [](const ArrayBodyPtr& x1, Scalar x2) { return MoveArrayBody(Minimum(Array{x1}, x2)); }, "x1"_a, "x2"_a);
    m.def("minimum", [](Scalar x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Minimum(x1, Array{x2})); }, "x1"_a, "x2"_a);
    m.def("minimum",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Minimum(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
}

void InitChainerxReduction(pybind11::module& m) {
    m.def("sum",
          [](const ArrayBodyPtr& a, int8_t axis, bool keepdims) { return MoveArrayBody(Sum(Array{a}, Axes{axis}, keepdims)); },
          "a"_a,
          "axis"_a,
          "keepdims"_a = false);
    m.def("sum",
          [](const ArrayBodyPtr& a, const nonstd::optional<std::vector<int8_t>>& axis, bool keepdims) {
              return MoveArrayBody(Sum(Array{a}, ToAxes(axis), keepdims));
          },
          "a"_a,
          "axis"_a = nullptr,
          "keepdims"_a = false);
    m.def("logsumexp",
          [](const ArrayBodyPtr& x, int8_t axis, bool keepdims) { return MoveArrayBody(LogSumExp(Array{x}, Axes{axis}, keepdims)); },
          "x"_a,
          "axis"_a,
          "keepdims"_a = false);
    m.def("logsumexp",
          [](const ArrayBodyPtr& x, const nonstd::optional<std::vector<int8_t>>& axis, bool keepdims) {
              return MoveArrayBody(LogSumExp(Array{x}, ToAxes(axis), keepdims));
          },
          "x"_a,
          "axis"_a = nullptr,
          "keepdims"_a = false);
    m.def("log_softmax",
          [](const ArrayBodyPtr& x, int8_t axis) { return MoveArrayBody(LogSoftmax(Array{x}, Axes{axis})); },
          "x"_a,
          "axis"_a);
    m.def("log_softmax",
          [](const ArrayBodyPtr& x, const nonstd::optional<std::vector<int8_t>>& axis) {
              return MoveArrayBody(LogSoftmax(Array{x}, ToAxes(axis)));
          },
          "x"_a,
          "axis"_a = nullptr);
    m.def("softmax", [](const ArrayBodyPtr& x, int8_t axis) { return MoveArrayBody(Softmax(Array{x}, Axes{axis})); }, "x"_a, "axis"_a);
    m.def("softmax",
          [](const ArrayBodyPtr& x, const nonstd::optional<std::vector<int8_t>>& axis) {
              return MoveArrayBody(Softmax(Array{x}, ToAxes(axis)));
          },
          "x"_a,
          "axis"_a = nullptr);
}

void InitChainerxRounding(pybind11::module& m) {
    m.def("ceil", [](const ArrayBodyPtr& x) { return MoveArrayBody(Ceil(Array{x})); }, "x"_a);
    m.def("floor", [](const ArrayBodyPtr& x) { return MoveArrayBody(Floor(Array{x})); }, "x"_a);
}

void InitChainerxTrigonometric(pybind11::module& m) {
    m.def("sin", [](const ArrayBodyPtr& x) { return MoveArrayBody(Sin(Array{x})); }, "x"_a);
    m.def("cos", [](const ArrayBodyPtr& x) { return MoveArrayBody(Cos(Array{x})); }, "x"_a);
    m.def("tan", [](const ArrayBodyPtr& x) { return MoveArrayBody(Tan(Array{x})); }, "x"_a);
    m.def("arcsin", [](const ArrayBodyPtr& x) { return MoveArrayBody(Arcsin(Array{x})); }, "x"_a);
    m.def("arccos", [](const ArrayBodyPtr& x) { return MoveArrayBody(Arccos(Array{x})); }, "x"_a);
    m.def("arctan", [](const ArrayBodyPtr& x) { return MoveArrayBody(Arctan(Array{x})); }, "x"_a);
    m.def("arctan2",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(Arctan2(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
}

void InitChainerxSorting(pybind11::module& m) {
    // sorting routines
    m.def("argmax",
          [](const ArrayBodyPtr& a, const nonstd::optional<int8_t>& axis) { return MoveArrayBody(ArgMax(Array{a}, ToAxes(axis))); },
          "a"_a,
          "axis"_a = nullptr);
    m.def("argmin",
          [](const ArrayBodyPtr& a, const nonstd::optional<int8_t>& axis) { return MoveArrayBody(ArgMin(Array{a}, ToAxes(axis))); },
          "a"_a,
          "axis"_a = nullptr);
}

void InitChainerxStatistics(pybind11::module& m) {
    // statistics routines
    m.def("amax",
          [](const ArrayBodyPtr& a, int8_t axis, bool keepdims) { return MoveArrayBody(AMax(Array{a}, Axes{axis}, keepdims)); },
          "a"_a,
          "axis"_a,
          "keepdims"_a = false);
    m.def("amax",
          [](const ArrayBodyPtr& a, const nonstd::optional<std::vector<int8_t>>& axis, bool keepdims) {
              return MoveArrayBody(AMax(Array{a}, ToAxes(axis), keepdims));
          },
          "a"_a,
          "axis"_a = nullptr,
          "keepdims"_a = false);
    m.attr("max") = m.attr("amax");
    m.def("amin",
          [](const ArrayBodyPtr& a, int8_t axis, bool keepdims) { return MoveArrayBody(AMin(Array{a}, Axes{axis}, keepdims)); },
          "a"_a,
          "axis"_a,
          "keepdims"_a = false);
    m.def("amin",
          [](const ArrayBodyPtr& a, const nonstd::optional<std::vector<int8_t>>& axis, bool keepdims) {
              return MoveArrayBody(AMin(Array{a}, ToAxes(axis), keepdims));
          },
          "a"_a,
          "axis"_a = nullptr,
          "keepdims"_a = false);
    m.attr("min") = m.attr("amin");
    m.def("mean",
          [](const ArrayBodyPtr& a, int8_t axis, bool keepdims) { return MoveArrayBody(Mean(Array{a}, Axes{axis}, keepdims)); },
          "a"_a,
          "axis"_a,
          "keepdims"_a = false);
    m.def("mean",
          [](const ArrayBodyPtr& a, const nonstd::optional<std::vector<int8_t>>& axis, bool keepdims) {
              return MoveArrayBody(Mean(Array{a}, ToAxes(axis), keepdims));
          },
          "a"_a,
          "axis"_a = nullptr,
          "keepdims"_a = false);
    m.def("var",
          [](const ArrayBodyPtr& a, int8_t axis, bool keepdims) { return MoveArrayBody(Var(Array{a}, Axes{axis}, keepdims)); },
          "a"_a,
          "axis"_a,
          "keepdims"_a = false);
    m.def("var",
          [](const ArrayBodyPtr& a, const nonstd::optional<std::vector<int8_t>>& axis, bool keepdims) {
              return MoveArrayBody(Var(Array{a}, ToAxes(axis), keepdims));
          },
          "a"_a,
          "axis"_a = nullptr,
          "keepdims"_a = false);
}

void InitChainerxConnection(pybind11::module& m) {
    // connection routines
    m.def("conv",
          [](const ArrayBodyPtr& x,
             const ArrayBodyPtr& w,
             const nonstd::optional<ArrayBodyPtr>& b,
             py::handle stride,
             py::handle pad,
             bool cover_all) {
              // Create an Array from x to compute the image dimensions and the expected number of stride and padding elements.
              Array x_array{x};
              int8_t ndim = x_array.ndim() - 2;
              return MoveArrayBody(
                      Conv(x_array,
                           Array{w},
                           b.has_value() ? nonstd::optional<Array>{Array{*b}} : nonstd::nullopt,
                           ToStackVector<int64_t>(stride, ndim),
                           ToStackVector<int64_t>(pad, ndim),
                           cover_all));
          },
          "x"_a,
          "w"_a,
          "b"_a = nullptr,
          "stride"_a = 1,
          "pad"_a = 0,
          "cover_all"_a = false);
    m.def("conv_transpose",
          [](const ArrayBodyPtr& x,
             const ArrayBodyPtr& w,
             const nonstd::optional<ArrayBodyPtr>& b,
             py::handle stride,
             py::handle pad,
             const nonstd::optional<py::tuple>& outsize) {
              // Create an Array from x to compute the image dimensions and the expected number of stride and padding elements.
              Array x_array{x};
              int8_t ndim = x_array.ndim() - 2;
              return MoveArrayBody(ConvTranspose(
                      x_array,
                      Array{w},
                      b.has_value() ? nonstd::optional<Array>{Array{*b}} : nonstd::nullopt,
                      ToStackVector<int64_t>(stride, ndim),
                      ToStackVector<int64_t>(pad, ndim),
                      outsize.has_value() ? nonstd::optional<Dims>{ToStackVector<int64_t>(*outsize, ndim)} : nonstd::nullopt));
          },
          "x"_a,
          "w"_a,
          "b"_a = nullptr,
          "stride"_a = 1,
          "pad"_a = 0,
          "outsize"_a = nullptr);
    m.def("linear",
          [](const ArrayBodyPtr& x, const ArrayBodyPtr& w, const nonstd::optional<ArrayBodyPtr>& b, int8_t n_batch_axes) {
              return MoveArrayBody(
                      Linear(Array{x}, Array{w}, b.has_value() ? nonstd::optional<Array>{Array{*b}} : nonstd::nullopt, n_batch_axes));
          },
          "x"_a,
          "w"_a,
          "b"_a = nullptr,
          "n_batch_axes"_a = 1);
}

void InitChainerxNormalization(pybind11::module& m) {
    // normalization routines
    m.def("batch_norm",
          [](const ArrayBodyPtr& x,
             const ArrayBodyPtr& gamma,
             const ArrayBodyPtr& beta,
             const ArrayBodyPtr& running_mean,
             const ArrayBodyPtr& running_var,
             Scalar eps,
             Scalar decay,
             const nonstd::optional<std::vector<int8_t>>& axis) {
              return MoveArrayBody(
                      BatchNorm(Array{x}, Array{gamma}, Array{beta}, Array{running_mean}, Array{running_var}, eps, decay, ToAxes(axis)));
          },
          "x"_a,
          "gamma"_a,
          "beta"_a,
          "running_mean"_a,
          "running_var"_a,
          "eps"_a = 2e-5,
          "decay"_a = 0.9,
          "axis"_a = nullptr);
    m.def("fixed_batch_norm",
          [](const ArrayBodyPtr& x,
             const ArrayBodyPtr& gamma,
             const ArrayBodyPtr& beta,
             const ArrayBodyPtr& mean,
             const ArrayBodyPtr& var,
             Scalar eps,
             const nonstd::optional<std::vector<int8_t>>& axis) {
              return MoveArrayBody(FixedBatchNorm(Array{x}, Array{gamma}, Array{beta}, Array{mean}, Array{var}, eps, ToAxes(axis)));
          },
          "x"_a,
          "gamma"_a,
          "beta"_a,
          "mean"_a,
          "var"_a,
          "eps"_a = 2e-5,
          "axis"_a = nullptr);
}

void InitChainerxPooling(pybind11::module& m) {
    // pooling routines
    // TODO(sonots): Support return_indicies option of chainer.functions.max_pooling_nd.
    m.def("max_pool",
          [](const ArrayBodyPtr& x, py::handle ksize, py::handle stride, py::handle pad, bool cover_all) {
              Array x_array{x};
              int8_t ndim = x_array.ndim() - 2;
              return MoveArrayBody(
                      MaxPool(x_array,
                              ToStackVector<int64_t>(ksize, ndim),
                              stride.is_none() ? ToStackVector<int64_t>(ksize, ndim) : ToStackVector<int64_t>(stride, ndim),
                              ToStackVector<int64_t>(pad, ndim),
                              cover_all));
          },
          "x"_a,
          "ksize"_a,
          "stride"_a = py::none(),
          "pad"_a = 0,
          "cover_all"_a = false);
    m.def("average_pool",
          [](const ArrayBodyPtr& x, py::handle ksize, py::handle stride, py::handle pad, const std::string& pad_mode) {
              Array x_array{x};
              int8_t ndim = x_array.ndim() - 2;

              AveragePoolPadMode mode{};
              if (pad_mode == "zero") {
                  mode = AveragePoolPadMode::kZero;
              } else if (pad_mode == "ignore") {
                  mode = AveragePoolPadMode::kIgnore;
              } else {
                  throw py::value_error{"pad_mode must be either of 'zero' or 'ignore'"};
              }

              return MoveArrayBody(AveragePool(
                      x_array,
                      ToStackVector<int64_t>(ksize, ndim),
                      stride.is_none() ? ToStackVector<int64_t>(ksize, ndim) : ToStackVector<int64_t>(stride, ndim),
                      ToStackVector<int64_t>(pad, ndim),
                      mode));
          },
          "x"_a,
          "ksize"_a,
          "stride"_a = py::none(),
          "pad"_a = 0,
          "pad_mode"_a = "ignore");
}

void InitChainerxLoss(pybind11::module& m) {
    m.def("absolute_error",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(AbsoluteError(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("squared_error",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2) { return MoveArrayBody(SquaredError(Array{x1}, Array{x2})); },
          "x1"_a,
          "x2"_a);
    m.def("gaussian_kl_divergence",
          [](const ArrayBodyPtr& mean, const ArrayBodyPtr& ln_var) {
              return MoveArrayBody(GaussianKLDivergence(Array{mean}, Array{ln_var}));
          },
          "mean"_a,
          "ln_var"_a);
    m.def("huber_loss",
          [](const ArrayBodyPtr& x1, const ArrayBodyPtr& x2, Scalar delta) {
              return MoveArrayBody(HuberLoss(Array{x1}, Array{x2}, delta));
          },
          "x1"_a,
          "x2"_a,
          "delta"_a);
}

}  // namespace

void InitChainerxRoutines(pybind11::module& m) {
    InitChainerxCreation(m);
    InitChainerxIndexing(m);
    InitChainerxLinalg(m);
    InitChainerxLogic(m);
    InitChainerxLoss(m);
    InitChainerxManipulation(m);
    InitChainerxActivation(m);
    InitChainerxArithmetic(m);
    InitChainerxBinary(m);
    InitChainerxExpLog(m);
    InitChainerxHyperbolic(m);
    InitChainerxMisc(m);
    InitChainerxReduction(m);
    InitChainerxRounding(m);
    InitChainerxTrigonometric(m);
    InitChainerxSorting(m);
    InitChainerxStatistics(m);
    InitChainerxConnection(m);
    InitChainerxNormalization(m);
    InitChainerxPooling(m);
}

}  // namespace python_internal
}  // namespace python
}  // namespace chainerx
