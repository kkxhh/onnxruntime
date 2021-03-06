// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/optimizer/graph_transformer.h"

namespace onnxruntime {

class ConvActivationFusion : public GraphTransformer {
 public:
  ConvActivationFusion(const std::unordered_set<std::string>& compatible_execution_providers = {}) noexcept 
      : GraphTransformer("ConvActivationFusion", "Fusing Activation into Conv", compatible_execution_providers) {}

 private:
  Status ApplyImpl(onnxruntime::Graph& graph, bool& modified, int graph_level) const override;
};

}  // namespace onnxruntime
