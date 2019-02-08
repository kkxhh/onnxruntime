// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <functional>

#include "core/common/exceptions.h"
#include "core/common/logging/logging.h"
#include "core/common/status.h"
#include "core/framework/execution_provider.h"
#include "core/framework/kernel_def_builder.h"
#include "core/framework/ml_value.h"
#include "core/framework/op_kernel_info.h"
#include "core/framework/op_node_proto_helper.h"
#include "core/framework/tensor.h"
#include "core/graph/constants.h"
#include "core/graph/graph_viewer.h"
#include "gsl/span"
#include "onnx/defs/schema.h"

namespace onnxruntime {
class ExecutionFrame;
class OpKernelContext;
class OpKernelWrapper;

class OpKernel {
 public:
  using DoneCallback = std::function<void()>;

  explicit OpKernel(const OpKernelInfo& info) : op_kernel_info_(info) {}
  virtual ~OpKernel() = default;

  const onnxruntime::Node& Node() const {
    return op_kernel_info_.node();
  }

  const ::onnxruntime::KernelDef& KernelDef() const {
    return op_kernel_info_.GetKernelDef();
  }

  virtual Status Compute(OpKernelContext* context) const = 0;

  virtual Status ComputeAsync(OpKernelContext*,
                              DoneCallback) const {
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  const OrtAllocatorInfo& Allocator(int id, OrtMemType mem_type) const {
    return op_kernel_info_.GetAllocatorInfo(id, mem_type);
  }

  const OpKernelInfo& Info() const { return op_kernel_info_; }

 private:
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(OpKernel);
  OpKernelInfo op_kernel_info_;
};

class OpKernelContext {
 public:
  using ArgMap = std::unordered_map<std::string, size_t>;

  explicit OpKernelContext(const OpKernel* kernel, const logging::Logger& logger) : kernel_(kernel),
                                                                                    logger_(&logger) {
    ORT_ENFORCE(kernel != nullptr, "OpKernel was null");
  };

  virtual ~OpKernelContext() = default;

  const logging::Logger& Logger() const {
    return *logger_;
  }

  /**
  Return the number of inputs for a variadic argument.
  @param arg_num The operator argument number.
  @returns Number of inputs the argument has.
  */
  int NumVariadicInputs(size_t arg_num) const {
    auto& arg_counts = kernel_->Node().InputArgCount();

    ORT_ENFORCE(arg_num < arg_counts.size(), "Invalid arg_num of ", arg_num, ". Num args is ", arg_counts.size());

    return arg_counts[arg_num];
  }

  int InputCount() const {
    return static_cast<int>(kernel_->Node().InputDefs().size());
  }

  int ImplicitInputCount() const {
    return static_cast<int>(kernel_->Node().ImplicitInputDefs().size());
  }

  int OutputCount() const {
    return static_cast<int>(kernel_->Node().OutputDefs().size());
  }

  template <typename T>
  const T* Input(int index) const {
    const MLValue* p_ml_value = GetInputMLValue(index);
    return p_ml_value ? &(p_ml_value->Get<T>()) : nullptr;
  }

  // Fetch output (non-tensor) with specified index.
  template <typename T>
  T* Output(int index) {
    if (index < 0 || index >= OutputCount())
      return nullptr;

    MLValue* p_ml_value = nullptr;
    ORT_ENFORCE(GetOrCreateOutputMLValue(index, p_ml_value).IsOK());
    return p_ml_value ? p_ml_value->GetMutable<T>() : nullptr;
  }

  virtual MLDataType InputType(int index) const = 0;
  virtual MLDataType OutputType(int index) const = 0;

  // In the case that memory allocation has not been done for an output tensor,
  // The memory allocation will be done on-the-fly with given tensor shape.
  // Return nullptr if the output is an unused optional output.
  virtual Tensor* Output(int index, const TensorShape& shape) = 0;

  /**
   * return an allocator on device 0, with memtype of OrtMemTypeDefault
   *
   */
  virtual Status GetTempSpaceAllocator(AllocatorPtr* output) const = 0;

  /**
  Return the fence of current node's input.
  @param index The index of the input.
  @returns Point to the Fence of the input MLValue.
  It is null if the input MLValue doesn't have fence or the input is optional.
  */
  virtual Fence_t InputFence(int index) const = 0;

  /**
  Return the fence of current node's implicit input.
  @param index The index of the implicit input.
  @returns Point to the Fence of the implicit input MLValue.
  It is null if the input MLValue doesn't have fence or the input is optional.
  */
  virtual Fence_t ImplicitInputFence(int index) const = 0;

  /**
  Return the fence of current node's output identifed by index.
  @param index The index of the output.
  @returns Point to the Fence of the output MLValue.
  It is null if the output MLValue doesn't have fence or the output is optional.
  */
  virtual Fence_t OutputFence(int index) const = 0;

  virtual const SessionState* SubgraphSessionState(const std::string& attribute_name) = 0;
  virtual std::unordered_map<std::string, const MLValue*> GetImplicitInputs() const = 0;

  virtual const MLValue* GetInputMLValue(int index) const = 0;
  virtual MLValue* GetOutputMLValue(int index) = 0;

protected:
  virtual Status GetOrCreateOutputMLValue(int index, MLValue*& p_value) = 0;

  const OpKernel* kernel_{nullptr};
  const logging::Logger* logger_{nullptr};
};

// Fetching output tensor without shape is not allowed except when it already exists
template <>
inline Tensor* OpKernelContext::Output<Tensor>(int index) {
  MLValue* p_ml_value = GetOutputMLValue(index);
  ORT_ENFORCE(p_ml_value, "Please fetch output tensor with specified shape.");
  return p_ml_value->GetMutable<Tensor>();
}

using KernelCreateFn = std::function<OpKernel*(const OpKernelInfo& info)>;

struct KernelCreateInfo {
  std::unique_ptr<KernelDef> kernel_def;  // Owned and stored in the global kernel registry.
  KernelCreateFn kernel_create_func;
  Status status;

  KernelCreateInfo(std::unique_ptr<KernelDef> definition,
                   KernelCreateFn create_func)
      : kernel_def(std::move(definition)),
        kernel_create_func(create_func) {}

  KernelCreateInfo(KernelCreateInfo&& other)
      : kernel_def(std::move(other.kernel_def)),
        kernel_create_func(std::move(other.kernel_create_func)) {}
};

using KernelCreateMap = std::multimap<std::string, KernelCreateInfo>;

// Forward declarations for the non-specialized BuildKernelCreateInfo method.
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();

namespace ml {
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();
}  // namespace ml

namespace contrib {
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();
}  // namespace contrib

// Naming convention for operator kernel classes
#define ONNX_OPERATOR_KERNEL_CLASS_NAME(provider, domain, ver, name) \
  provider##_##name##_##domain##_ver##ver

#define ONNX_CPU_OPERATOR_KERNEL(name, ver, builder, ...) \
  ONNX_OPERATOR_KERNEL_EX(name, kOnnxDomain, ver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_ML_KERNEL(name, ver, builder, ...) \
  ONNX_OPERATOR_KERNEL_EX(name, kMLDomain, ver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_OPERATOR_KERNEL_EX(name, domain, ver, provider, builder, ...)            \
  class ONNX_OPERATOR_KERNEL_CLASS_NAME(provider, domain, ver, name);                 \
  template <>                                                                         \
  KernelCreateInfo                                                                    \
  BuildKernelCreateInfo<ONNX_OPERATOR_KERNEL_CLASS_NAME(provider, domain, ver, name)>() {       \
    return KernelCreateInfo(                                                          \
        builder.SetName(#name)                                                        \
            .SetDomain(domain)                                                        \
            .SinceVersion(ver)                                                        \
            .Provider(provider)                                                       \
            .Build(),                                                                 \
        [](const OpKernelInfo& info) -> OpKernel* { return new __VA_ARGS__(info); }); \
  }

#define ONNX_OPERATOR_VERSIONED_KERNEL_CLASS_NAME(provider, domain, startver, endver, name) \
  provider##_##name##_##domain##_ver##startver##_##endver

#define ONNX_CPU_OPERATOR_VERSIONED_KERNEL(name, startver, endver, builder, ...) \
  ONNX_OPERATOR_VERSIONED_KERNEL_EX(name, kOnnxDomain, startver, endver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_VERSIONED_ML_KERNEL(name, startver, endver, builder, ...) \
  ONNX_OPERATOR_VERSIONED_KERNEL_EX(name, kMLDomain, startver, endver, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_OPERATOR_VERSIONED_KERNEL_EX(name, domain, startver, endver, provider, builder, ...)      \
  class ONNX_OPERATOR_VERSIONED_KERNEL_CLASS_NAME(provider, domain, startver, endver, name);           \
  template <>                                                                                          \
  KernelCreateInfo                                                                                     \
  BuildKernelCreateInfo<ONNX_OPERATOR_VERSIONED_KERNEL_CLASS_NAME(provider, domain, startver, endver, name)>() { \
    return KernelCreateInfo(                                                                           \
        builder.SetName(#name)                                                                         \
            .SetDomain(domain)                                                                         \
            .SinceVersion(startver, endver)                                                            \
            .Provider(provider)                                                                        \
            .Build(),                                                                                  \
        [](const OpKernelInfo& info) -> OpKernel* { return new __VA_ARGS__(info); });                  \
  }

#define ONNX_OPERATOR_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type, name) \
  provider##_##name##_##domain##_ver##ver##_##type

#define ONNX_CPU_OPERATOR_TYPED_KERNEL(name, ver, type, builder, ...) \
  ONNX_OPERATOR_TYPED_KERNEL_EX(name, kOnnxDomain, ver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_TYPED_ML_KERNEL(name, ver, type, builder, ...) \
  ONNX_OPERATOR_TYPED_KERNEL_EX(name, kMLDomain, ver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_TYPED_MS_KERNEL(name, ver, type, builder, ...) \
  ONNX_OPERATOR_TYPED_KERNEL_EX(name, kMSDomain, ver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_OPERATOR_TYPED_KERNEL_EX(name, domain, ver, type, provider, builder, ...)      \
  class ONNX_OPERATOR_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type, name);           \
  template <>                                                                               \
  KernelCreateInfo                                                                          \
  BuildKernelCreateInfo<ONNX_OPERATOR_TYPED_KERNEL_CLASS_NAME(provider, domain, ver, type, name)>() { \
    return KernelCreateInfo(                                                                \
        builder.SetName(#name)                                                              \
            .SetDomain(domain)                                                              \
            .SinceVersion(ver)                                                              \
            .Provider(provider)                                                             \
            .Build(),                                                                       \
        [](const OpKernelInfo& info) -> OpKernel* { return new __VA_ARGS__(info); });       \
  }

#define ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_CLASS_NAME(provider, domain, startver, endver, type, name) \
  provider##_##name##_##domain##_ver##startver##_##endver##_##type

#define ONNX_CPU_OPERATOR_VERSIONED_TYPED_KERNEL(name, startver, endver, type, builder, ...) \
  ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_EX(name, kOnnxDomain, startver, endver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_VERSIONED_TYPED_ML_KERNEL(name, startver, endver, type, builder, ...) \
  ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_EX(name, kMLDomain, startver, endver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_CPU_OPERATOR_VERSIONED_TYPED_MS_KERNEL(name, startver, endver, type, builder, ...) \
  ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_EX(name, kMSDomain, startver, endver, type, kCpuExecutionProvider, builder, __VA_ARGS__)

#define ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_EX(name, domain, startver, endver, type, provider, builder, ...)      \
  class ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_CLASS_NAME(provider, domain, startver, endver, type, name);           \
  template <>                                                                                                      \
  KernelCreateInfo                                                                                                 \
  BuildKernelCreateInfo<ONNX_OPERATOR_VERSIONED_TYPED_KERNEL_CLASS_NAME(provider, domain, startver, endver, type, name)>() { \
    return KernelCreateInfo(                                                                                       \
        builder.SetName(#name)                                                                                     \
            .SetDomain(domain)                                                                                     \
            .SinceVersion(startver, endver)                                                                        \
            .Provider(provider)                                                                                    \
            .Build(),                                                                                              \
        [](const OpKernelInfo& info) -> OpKernel* { return new __VA_ARGS__(info); });                              \
  }

}  // namespace onnxruntime
