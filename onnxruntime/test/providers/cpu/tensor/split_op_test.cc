// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "gtest/gtest.h"
#include "test/providers/provider_test_utils.h"

namespace onnxruntime {
namespace test {

template<class T> using ShapeAndData = std::pair<const std::vector<int64_t>, const std::vector<T>>;

using ShapeAndFloatData = ShapeAndData<float>;
using ShapeAndInt32Data = ShapeAndData<int32_t>;
using ExpectResult = OpTester::ExpectResult;

template<typename T> void RunTest(int64_t axis, const std::vector<int64_t> split_sizes, const ShapeAndData<T>& input,
  const std::vector<ShapeAndData<T>>& outputs,
  bool expect_failure = false, const std::string& err_msg = {}) {
  OpTester test("Split");

  test.AddAttribute("axis", axis);

  if (!split_sizes.empty())
    test.AddAttribute("split", split_sizes);

  test.AddInput<T>("input", input.first, input.second);

  int i = 0;
  for (auto& output : outputs) {
    auto& shape = output.first;
    auto& data = output.second;
    std::ostringstream oss;
    oss << "output" << i++;
    test.AddOutput<T>(oss.str().c_str(), shape, data);
  }

  test.Run(expect_failure ? ExpectResult::kExpectFailure : ExpectResult::kExpectSuccess, err_msg);
}

TEST(SplitOperatorTest, Axis0EqualSplit) {
  const int64_t axis = 0;
  std::vector<ShapeAndFloatData> outputs;

  // input shape and data
  ShapeAndFloatData input = {{4, 2},  // shape
                        {1.f, 2.f,
                         3.f, 4.f,
                         5.f, 6.f,
                         7.f, 8.f}};

  outputs.push_back({{2, 2},
                     {1.f, 2.f,
                      3.f, 4.f}});

  outputs.push_back({{2, 2},
                     {5.f, 6.f,
                      7.f, 8.f}});

  RunTest<float>(axis, {}, input, outputs);
}

TEST(SplitOperatorTest, Axis0EqualSplitInt32) {
  const int64_t axis = 0;
  std::vector<ShapeAndInt32Data> outputs;

  // input shape and data
  ShapeAndInt32Data input = {{4, 2},  // shape
                             {1, 2,
                              3, 4,
                              5, 6,
                              7, 8}};

  outputs.push_back({{2, 2},
                     {1, 2,
                      3, 4}});

  outputs.push_back({{2, 2},
                     {5, 6,
                      7, 8}});

  RunTest<int32_t>(axis, {}, input, outputs);
}

TEST(SplitOperatorTest, Axis0UnequalSplit) {
  const int64_t axis = 0;
  std::vector<ShapeAndFloatData> outputs;

  // input shape and data
  ShapeAndFloatData input = {{4, 2},  // shape
                        {1.f, 2.f,
                         3.f, 4.f,
                         5.f, 6.f,
                         7.f, 8.f}};

  std::vector<int64_t> splits{1, 3};

  outputs.push_back({{1, 2}, {1.f, 2.f}});

  outputs.push_back({{3, 2},
                     {3.f, 4.f,
                      5.f, 6.f,
                      7.f, 8.f}});

  RunTest<float>(axis, splits, input, outputs);
}

TEST(SplitOperatorTest, Axis1EqualSplit) {
  const int64_t axis = 1;
  std::vector<ShapeAndFloatData> outputs;

  // input shape and data
  ShapeAndFloatData input = {{2, 4},
                        {1.f, 2.f, 3.f, 4.f,
                         5.f, 6.f, 7.f, 8.f}};

  outputs.push_back({{2, 2},
                     {1.f, 2.f,
                      5.f, 6.f}});

  outputs.push_back({{2, 2},
                     {3.f, 4.f,
                      7.f, 8.f}});

  RunTest<float>(axis, {}, input, outputs);
}

TEST(SplitOperatorTest, Axis1UnequalSplit) {
  const int64_t axis = 1;
  std::vector<ShapeAndFloatData> outputs;

  // input shape and data
  ShapeAndFloatData input = {{2, 4},
                        {1.f, 2.f, 3.f, 4.f,
                         5.f, 6.f, 7.f, 8.f}};

  std::vector<int64_t> splits{3, 1};

  outputs.push_back({{2, 3},
                     {1.f, 2.f, 3.f,
                      5.f, 6.f, 7.f}});

  outputs.push_back({{2, 1},
                     {4.f,
                      8.f}});

  RunTest<float>(axis, splits, input, outputs);
}

ShapeAndFloatData CreateInput(std::vector<int64_t> shape) {
  auto size = TensorShape(shape).Size();

  float i = 0.f, increment = 1.f;
  // generate the elements for the data starting at 1.f
  std::vector<float> data;
  std::generate_n(std::back_inserter(data), size, [&]() { return i += increment; });

  ShapeAndFloatData input = {shape, data};

  return input;
}

TEST(SplitOperatorTest, Axis2EqualSplit) {
  const int64_t axis = 2;
  std::vector<ShapeAndFloatData> outputs;

  ShapeAndFloatData input = CreateInput({2, 2, 6});

  outputs.push_back({{2, 2, 2},
                     {1.f, 2.f,
                      7.f, 8.f,

                      13.f, 14.f,
                      19.f, 20.f}});

  outputs.push_back({{2, 2, 2},
                     {3.f, 4.f,
                      9.f, 10.f,

                      15.f, 16.f,
                      21.f, 22.f}});

  outputs.push_back({{2, 2, 2},
                     {5.f, 6.f,
                      11.f, 12.f,

                      17.f, 18.f,
                      23.f, 24.f}});

  RunTest<float>(axis, {}, input, outputs);
}

TEST(SplitOperatorTest, Axis2UnequalSplit) {
  const int64_t axis = 2;
  std::vector<ShapeAndFloatData> outputs;

  ShapeAndFloatData input = CreateInput({2, 2, 6});

  std::vector<int64_t> splits{1, 2, 3};

  outputs.push_back({{2, 2, 1},
                     {1.f,
                      7.f,

                      13.f,
                      19.f}});

  outputs.push_back({{2, 2, 2},
                     {2.f, 3.f,
                      8.f, 9.f,

                      14.f, 15.f,
                      20.f, 21.f}});

  outputs.push_back({{2, 2, 3},
                     {4.f, 5.f, 6.f,
                      10.f, 11.f, 12.f,

                      16.f, 17.f, 18.f,
                      22.f, 23.f, 24.f}});

  RunTest<float>(axis, splits, input, outputs);
}

// test a split of a dimension that has leading and trailing dimensions
TEST(SplitOperatorTest, Axis1SplitMiddleDimensionEqually) {
  const int64_t axis = 1;
  std::vector<ShapeAndFloatData> outputs;

  ShapeAndFloatData input = CreateInput({2, 4, 4});

  outputs.push_back({{2, 2, 4},
                     {1.f, 2.f, 3.f, 4.f,
                      5.f, 6.f, 7.f, 8.f,

                      17.f, 18.f, 19.f, 20.f,
                      21.f, 22.f, 23.f, 24.f}});

  outputs.push_back({{2, 2, 4},
                     {9.f, 10.f, 11.f, 12.f,
                      13.f, 14.f, 15.f, 16.f,

                      25.f, 26.f, 27.f, 28.f,
                      29.f, 30.f, 31.f, 32.f}});

  RunTest<float>(axis, {}, input, outputs);
}

// test a split of a dimension that has leading and trailing dimensions
TEST(SplitOperatorTest, Axis1SplitMiddleDimensionUnequally) {
  const int64_t axis = 1;
  std::vector<ShapeAndFloatData> outputs;

  ShapeAndFloatData input = CreateInput({2, 4, 4});

  std::vector<int64_t> splits{1, 3};

  outputs.push_back({{2, 1, 4},
                     {1.f, 2.f, 3.f, 4.f,

                      17.f, 18.f, 19.f, 20.f}});

  outputs.push_back({{2, 3, 4},
                     {5.f, 6.f, 7.f, 8.f,
                      9.f, 10.f, 11.f, 12.f,
                      13.f, 14.f, 15.f, 16.f,

                      21.f, 22.f, 23.f, 24.f,
                      25.f, 26.f, 27.f, 28.f,
                      29.f, 30.f, 31.f, 32.f}});

  RunTest<float>(axis, splits, input, outputs);
}

TEST(SplitOperatorTest, NegativeAxis) {
  const int64_t axis = -1;  // split last axis equally
  std::vector<ShapeAndFloatData> outputs;

  // input shape and data
  ShapeAndFloatData input = {{2, 4},
                        {1.f, 2.f, 3.f, 4.f,
                         5.f, 6.f, 7.f, 8.f}};

  outputs.push_back({{2, 2},
                     {1.f, 2.f,
                      5.f, 6.f}});

  outputs.push_back({{2, 2},
                     {3.f, 4.f,
                      7.f, 8.f}});

  RunTest<float>(axis, {}, input, outputs);
}

TEST(SplitOperatorTest, InvalidAxis) {
  const int64_t axis = 2;
  std::vector<ShapeAndFloatData> outputs;

  // input shape and data
  ShapeAndFloatData input = {{4, 2},  // shape
                        {1.f, 2.f,
                         3.f, 4.f,
                         5.f, 6.f,
                         7.f, 8.f}};

  outputs.push_back({{1}, {0.f}});

  RunTest<float>(axis, {}, input, outputs, true, "Invalid value of attribute 'axis'");
}

// sum of values in splits is too small
TEST(SplitOperatorTest, SplitAttributeSumTooSmall) {
  const int64_t axis = 0;
  std::vector<ShapeAndFloatData> outputs;

  // input shape and data
  ShapeAndFloatData input = {{4, 2},  // shape
                        {1.f, 2.f,
                         3.f, 4.f,
                         5.f, 6.f,
                         7.f, 8.f}};

  std::vector<int64_t> splits{1, 2};  // should sum to 4

  outputs.push_back({{1, 2}, {1.f, 2.f}});
  outputs.push_back({{2, 2}, {3.f, 4.f, 5.f, 6.f}});

  RunTest<float>(axis, splits, input, outputs, true, "Cannot split using values in 'split' attribute");
}

TEST(SplitOperatorTest, InvalidValueInSplitAttribute) {
  const int64_t axis = 0;
  std::vector<ShapeAndFloatData> outputs;

  // input shape and data
  ShapeAndFloatData input = {{4, 2},  // shape
                        {1.f, 2.f,
                         3.f, 4.f,
                         5.f, 6.f,
                         7.f, 8.f}};

  std::vector<int64_t> splits{1, 0, 3};  // 0 is not valid
  outputs.push_back({{1, 2}, {1.f, 2.f}});
  outputs.push_back({{3, 2}, {3.f, 4.f, 5.f, 6.f, 7.f, 8.f}});

  RunTest<float>(axis, splits, input, outputs, true, "Invalid value in 'split' attribute");
}

/*
Python to replicate processing:

import numpy as np

def SplitAxis0():
    input = np.array([1., 2., 3., 4., 5., 6.]).astype(np.float32)

    # 3 outputs, axis 0
    expected_outputs = np.split(input, 3, 0)
    print("Split into 3, Axis=0")
    print(expected_outputs)

    # 2 outputs, split of 2, 4
    print("Split into 2 sizes 2 and 4, Axis=0")
    # numpy split is slightly different in that the values are indices rather than sizes
    # so a onnxruntime split of sizes 2, 4 for input of size 6 equates to just the index 2 (:2, 3:)
    # see https://docs.scipy.org/doc/numpy/reference/generated/numpy.split.html
    print(np.split(input, [2]))

def SplitAxis1():
    input = np.arange(1.0, 9).reshape(2, 4)

    # 2 outputs, axis 1
    x1, x2 = np.split(input, 2, 1)
    print("Split into 2, Axis=1\nx1={}\nx2={}".format(x1, x2))

    # 2 outputs, split of 3, 1
    x1, x2 = np.split(input, [3], 1)
    print("Split into 2 sizes 3 and 1, Axis=1\nx1={}\nx2={}".format(x1, x2))

def SplitAxis2():
    x = np.arange(1.0, 25.0).reshape((2,2,6))

    # equal split. 3 outputs, axis=2
    x1, x2, x3 = np.split(x, 3, 2)
    print("Equal split. Axis=2\nx1={}\nx2={}\nx3={}".format(x1, x2, x3))

    # split axis 2 into [1, 2, 3] sizes, which is index 1 (:1, 1:3, 3:)
    x1, x2, x3 = np.split(x, [1, 3], 2)
    print("Split into sizes 1, 3, 2. Axis=2\nx1={}\nx2={}\nx3={}".format(x1, x2, x3))

def SplitMiddleDimension():
    print ("SplitMiddleDimension")
    x = np.arange(1.0, 33.0).reshape((2,4,4))

    # equal split. 2 outputs, axis=1
    x1, x2 = np.split(x, 2, 1)
    print("Equal split. Axis=2\nx1={}\nx2={}".format(x1, x2))

    # split axis 2 into [1,3] sizes, which is index 1 (:1, 1:)
    x1, x2 = np.split(x, [1], 1)
    print("Split into sizes 1 and 3. Axis=2\nx1={}\nx2={}".format(x1, x2))

SplitAxis0()
SplitAxis1()
SplitAxis2()
SplitMiddleDimension()

*/
}  // namespace test
}  // namespace onnxruntime
