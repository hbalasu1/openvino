// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "subgraph_mha.hpp"

#include "common_test_utils/data_utils.hpp"
#include <snippets/op/subgraph.hpp>
#include "ngraph_functions/builders.hpp"
#include "ov_ops/type_relaxed.hpp"
#include "lpt_ngraph_functions/common/builders.hpp"

namespace ov {
namespace test {
namespace snippets {

std::shared_ptr<ov::Model> MHAFunction::initOriginal() const {
    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precisions[0], input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precisions[1], input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precisions[2], input_shapes[2]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precisions[3], input_shapes[3]);
    ngraph::ParameterVector ngraphParam = {transpose0Param, transpose1Param, addParam, transpose2Param};

    std::vector<ov::Shape> constantShapes;
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({1, input_shapes[1].get_shape()[2], 1, 1}));
    constantShapes.push_back(ov::Shape({2}));
    constantShapes.push_back(ov::Shape({4}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));

    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[0], std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[1], std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[5], std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[6], std::vector<int64_t>{0, 2, 1, 3});

    std::vector<int64_t> reshape0ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0] *
                                                                   input_shapes[0].get_shape()[1] * input_shapes[0].get_shape()[2]),
                                              -1};
    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[3], reshape0ConstData);

    std::vector<int64_t> reshape1ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[2]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1])};
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[4], reshape1ConstData);

    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(transpose0Param, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(transpose1Param, transpose1Const);
    std::shared_ptr<ov::Node> matmul_parent1 = transpose1;
    if (with_mul) {
        std::vector<float> mulConstData(ngraph::shape_size(constantShapes[2]));
        auto mulConst = ngraph::builder::makeConstant(precisions[1], constantShapes[2], mulConstData, true);
        matmul_parent1 = std::make_shared<ngraph::opset3::Multiply>(transpose1, mulConst);
    }
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0, matmul_parent1, transA, transB);
    const auto add = std::make_shared<ngraph::opset3::Add>(matMul0, addParam);
    const auto reshape0 = std::make_shared<ngraph::opset1::Reshape>(add, reshape0Const, true);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(reshape0, 1);
    const auto reshape1 = std::make_shared<ngraph::opset1::Reshape>(softMax, reshape1Const, true);
    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(transpose2Param, transpose2Const);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(reshape1, transpose2, transA, transB);
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(matMul1, transpose3Const);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(transpose3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}
std::shared_ptr<ov::Model> MHAFunction::initReference() const {
    auto data0 = std::make_shared<ngraph::opset1::Parameter>(precisions[0], input_shapes[0]);
    auto data1 = std::make_shared<ngraph::opset1::Parameter>(precisions[1], input_shapes[1]);
    auto data2 = std::make_shared<ngraph::opset1::Parameter>(precisions[2], input_shapes[2]);
    auto data3 = std::make_shared<ngraph::opset1::Parameter>(precisions[3], input_shapes[3]);
    ngraph::ParameterVector ngraphParams = {data0, data1, data2, data3};
    NodeVector subgraph_inputs = {data0, data1, data2, data3};

    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precisions[0], input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precisions[1], input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precisions[2], input_shapes[2]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precisions[3], input_shapes[3]);

    std::vector<ov::Shape> constantShapes;
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({1, input_shapes[1].get_shape()[2], 1, 1}));
    constantShapes.push_back(ov::Shape({2}));
    constantShapes.push_back(ov::Shape({4}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));

    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[0], std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[1], std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[5], std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[6], std::vector<int64_t>{0, 2, 1, 3});

    ngraph::ParameterVector subgraph_params = {transpose0Param, transpose1Param, addParam, transpose2Param};

    std::vector<int64_t> reshape0ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0] *
                                                                   input_shapes[0].get_shape()[1] * input_shapes[0].get_shape()[2]),
                                              -1};
    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[3], reshape0ConstData);

    std::vector<int64_t> reshape1ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[2]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1])};
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[4], reshape1ConstData);

    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(transpose0Param, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(transpose1Param, transpose1Const);
    std::shared_ptr<ov::Node> matmul_parent1 = transpose1;
    if (with_mul) {
        std::vector<float> mulConstData(ngraph::shape_size(constantShapes[2]));
        auto mulConst = ngraph::builder::makeConstant(precisions[1], constantShapes[2], mulConstData, true);
        auto mulParam = std::make_shared<ngraph::opset1::Parameter>(precisions[1], mulConst->get_shape());
        matmul_parent1 = std::make_shared<ngraph::opset3::Multiply>(transpose1, mulParam);
        subgraph_params = {transpose0Param, transpose1Param, mulParam, addParam, transpose2Param};
        subgraph_inputs = {data0, data1, mulConst, data2, data3};
    }
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0, matmul_parent1, transA, transB);
    const auto add = std::make_shared<ngraph::opset3::Add>(matMul0, addParam);
    const auto reshape0 = std::make_shared<ngraph::opset1::Reshape>(add, reshape0Const, true);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(reshape0, 1);
    const auto reshape1 = std::make_shared<ngraph::opset1::Reshape>(softMax, reshape1Const, true);
    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(transpose2Param, transpose2Const);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(reshape1, transpose2, transA, transB);
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(matMul1, transpose3Const);

    auto subgraph = std::make_shared<ov::snippets::op::Subgraph>(subgraph_inputs,
            std::make_shared<ov::Model>(NodeVector{transpose3}, subgraph_params));

    return std::make_shared<ov::Model>(NodeVector{subgraph}, ngraphParams);
}

std::shared_ptr<ov::Model> MHAMatMul0TransposeFunction::initOriginal() const {
    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precisions[0], input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precisions[1], input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precisions[2], input_shapes[2]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precisions[3], input_shapes[3]);
    ngraph::ParameterVector ngraphParam = {transpose0Param, transpose1Param, addParam, transpose2Param};

    std::vector<ov::Shape> constantShapes;
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({1, input_shapes[1].get_shape()[2], 1, 1}));
    constantShapes.push_back(ov::Shape({2}));
    constantShapes.push_back(ov::Shape({4}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));

    const auto order = std::vector<int64_t>{0, 2, 1, 3};
    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[0], order);
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[1], order);
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[5], order);
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[6], order);

    std::vector<float> mulConstData(1);
    auto mulConst = ngraph::builder::makeConstant(precisions[1], ov::Shape{1}, mulConstData, true);

    std::vector<int64_t> reshape0ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0] *
                                                                   input_shapes[0].get_shape()[1] * input_shapes[0].get_shape()[2]),
                                              -1};
    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[3], reshape0ConstData);

    std::vector<int64_t> reshape1ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[2]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1])};
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[4], reshape1ConstData);

    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(transpose0Param, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(transpose1Param, transpose1Const);
    const auto mul = std::make_shared<ngraph::opset3::Multiply>(transpose1, mulConst);
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0, mul, transA, true);
    const auto add = std::make_shared<ngraph::opset3::Add>(matMul0, addParam);
    const auto reshape0 = std::make_shared<ngraph::opset1::Reshape>(add, reshape0Const, true);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(reshape0, 1);
    const auto reshape1 = std::make_shared<ngraph::opset1::Reshape>(softMax, reshape1Const, true);
    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(transpose2Param, transpose2Const);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(reshape1, transpose2, transA, transB);
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(matMul1, transpose3Const);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(transpose3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}
std::shared_ptr<ov::Model> MHAMatMul0TransposeFunction::initReference() const {
    auto data0 = std::make_shared<ngraph::opset1::Parameter>(precisions[0], input_shapes[0]);
    auto data1 = std::make_shared<ngraph::opset1::Parameter>(precisions[1], input_shapes[1]);
    auto data2 = std::make_shared<ngraph::opset1::Parameter>(precisions[2], input_shapes[2]);
    auto data3 = std::make_shared<ngraph::opset1::Parameter>(precisions[3], input_shapes[3]);
    ngraph::ParameterVector ngraphParams = {data0, data1, data2, data3};

    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precisions[0], input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precisions[1], input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precisions[2], input_shapes[2]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precisions[3], input_shapes[3]);

    std::vector<ov::Shape> constantShapes;
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({1, input_shapes[1].get_shape()[2], 1, 1}));
    constantShapes.push_back(ov::Shape({2}));
    constantShapes.push_back(ov::Shape({4}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));

    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[0], std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[1], std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[5], std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[6], std::vector<int64_t>{0, 2, 1, 3});

    std::vector<float> mulConstData(1);
    auto mulConst = ngraph::builder::makeConstant(precisions[1], ov::Shape{1}, mulConstData, true);
    ngraph::ParameterVector subgraphParams = {transpose0Param, transpose1Param, addParam, transpose2Param};

    std::vector<int64_t> reshape0ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0] *
                                                                   input_shapes[0].get_shape()[1] * input_shapes[0].get_shape()[2]),
                                              -1};
    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[3], reshape0ConstData);

    std::vector<int64_t> reshape1ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[2]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1])};
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[4], reshape1ConstData);

    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(transpose0Param, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(transpose1Param, transpose1Const);
    const auto mul = std::make_shared<ngraph::opset3::Multiply>(transpose1, mulConst);
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0, mul, transA, transB);
    const auto add = std::make_shared<ngraph::opset3::Add>(matMul0, addParam);
    const auto reshape0 = std::make_shared<ngraph::opset1::Reshape>(add, reshape0Const, true);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(reshape0, 1);
    const auto reshape1 = std::make_shared<ngraph::opset1::Reshape>(softMax, reshape1Const, true);
    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(transpose2Param, transpose2Const);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(reshape1, transpose2, transA, transB);
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(matMul1, transpose3Const);

    auto subgraph = std::make_shared<ov::snippets::op::Subgraph>(
            NodeVector{data0, data1, data2, data3},
            std::make_shared<ov::Model>(NodeVector{transpose3}, subgraphParams));

    return std::make_shared<ov::Model>(NodeVector{subgraph}, ngraphParams);
}

std::shared_ptr<ov::Model> MHASelectFunction::initOriginal() const {
    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precisions[0], input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precisions[1], input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precisions[2], input_shapes[2]);
    auto less0Param = std::make_shared<ngraph::opset1::Parameter>(precisions[3], input_shapes[3]);
    auto less1Param = std::make_shared<ngraph::opset1::Parameter>(precisions[4], input_shapes[4]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precisions[5], input_shapes[5]);
    ngraph::ParameterVector ngraphParam = {transpose0Param, transpose1Param, addParam, less0Param, less1Param, transpose2Param};

    std::vector<ov::Shape> constantShapes;
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({1, input_shapes[1].get_shape()[2], 1, 1}));
    constantShapes.push_back(ov::Shape({2}));
    constantShapes.push_back(ov::Shape({4}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));
    constantShapes.push_back(ov::Shape({input_shapes[0].get_shape().size()}));

    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[0],
                                                         std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[1],
                                                         std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[5],
                                                         std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[6],
                                                         std::vector<int64_t>{0, 2, 1, 3});

    std::vector<int64_t> reshape0ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0] *
                                                                   input_shapes[0].get_shape()[1] *
                                                                   input_shapes[0].get_shape()[2]),
                                              -1};
    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[3], reshape0ConstData);

    std::vector<int64_t> reshape1ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[2]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1])};
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[4], reshape1ConstData);
    // Value is equal to '1' - to avoid situation e^(-1000) / (sum(e^(-1000)) = 0/0 = NAN
    auto selectConst = ngraph::builder::makeConstant(precisions[2], ov::Shape{1}, std::vector<float>{1});

    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(transpose0Param, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(transpose1Param, transpose1Const);
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0, transpose1, transA, transB);
    const auto add = std::make_shared<ngraph::opset3::Add>(matMul0, addParam);
    const auto less = std::make_shared<ngraph::opset3::Less>(less0Param, less1Param);
    std::shared_ptr<ov::Node> selectCond = less;
    if (add->get_output_partial_shape(0) != input_shapes[3]) {
        const auto broadcast_shape = ngraph::builder::makeConstant(ngraph::element::i64, constantShapes[5],
                                                                   add->get_output_shape(0));
        const auto broadcast = ngraph::builder::makeBroadcast(selectCond, broadcast_shape,
                                                              ngraph::op::BroadcastType::NUMPY);
        selectCond = broadcast;
    }
    const auto select = std::make_shared<ngraph::opset1::Select>(selectCond, selectConst, add,
                                                                 ngraph::op::AutoBroadcastType::NUMPY);
    const auto reshape0 = std::make_shared<ngraph::opset1::Reshape>(select, reshape0Const, true);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(reshape0, 1);
    const auto reshape1 = std::make_shared<ngraph::opset1::Reshape>(softMax, reshape1Const, true);
    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(transpose2Param, transpose2Const);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(reshape1, transpose2, transA, transB);
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(matMul1, transpose3Const);

    // to generate valid values
    less0Param->set_friendly_name("less0");
    less0Param->set_friendly_name("less1");

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(transpose3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}

std::shared_ptr<ov::Model> MHASelectSplitMFunction::initOriginal() const {
    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    auto selectParam = std::make_shared<ngraph::opset1::Parameter>(ov::element::u8, input_shapes[3]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[4]);
    ngraph::ParameterVector ngraphParam = {transpose0Param, transpose1Param, addParam, selectParam, transpose2Param};

    // Value is equal to '1' - to avoid situation e^(-1000) / (sum(e^(-1000)) = 0/0 = NAN
    auto selectConst = ngraph::builder::makeConstant(precision, ov::Shape{1}, std::vector<float>{1});

    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0Param, transpose1Param);
    const auto add = std::make_shared<ngraph::opset3::Add>(matMul0, addParam);
    std::shared_ptr<ov::Node> selectCond = selectParam;
    if (add->get_output_partial_shape(0) != selectParam->get_output_partial_shape(0)) {
        const auto broadcast_shape =
                ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape{add->get_output_shape(0).size()}, add->get_output_shape(0));
        selectCond = std::make_shared<ngraph::opset1::Broadcast>(selectCond, broadcast_shape);
    }
    const auto select = std::make_shared<op::TypeRelaxed<ngraph::opset1::Select>>(
            std::vector<element::Type>{ element::boolean, element::f32, element::f32 },
            std::vector<element::Type>{ element::f32 },
            ov::op::TemporaryReplaceOutputType(selectCond, element::boolean).get(),
            ov::op::TemporaryReplaceOutputType(selectConst, element::f32).get(),
            ov::op::TemporaryReplaceOutputType(add, element::f32).get());

    const auto interm_shape = select->get_shape();
    std::vector<int64_t> reshape0ConstData = {-1, static_cast<int64_t>(interm_shape.back())};
    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape{reshape0ConstData.size()}, reshape0ConstData);

    std::vector<int64_t> reshape1ConstData;
    for (const auto& dim : interm_shape)
        reshape1ConstData.push_back(static_cast<int64_t>(dim));
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape{reshape1ConstData.size()}, reshape1ConstData);

    const auto reshape0 = std::make_shared<ngraph::opset1::Reshape>(select, reshape0Const, true);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(reshape0, 1);
    const auto reshape1 = std::make_shared<ngraph::opset1::Reshape>(softMax, reshape1Const, true);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(reshape1, transpose2Param);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(matMul1)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}

std::shared_ptr<ov::Model> MHASelectSplitMFunction::initReference() const {
    auto param0 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto param1 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    auto selectParam = std::make_shared<ngraph::opset1::Parameter>(ov::element::u8, input_shapes[3]);
    auto param2 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[4]);
    ngraph::ParameterVector ngraphParam = {param0, param1, addParam, selectParam, param2};

    auto make_reshape = [](const std::shared_ptr<ov::Node>& node, const ov::Shape& new_shape) {
        auto shape_const = ngraph::builder::makeConstant(ngraph::element::i32, {new_shape.size()}, new_shape);
        return std::make_shared<ov::op::v1::Reshape>(node, shape_const, true);
    };

    auto reshape0 = make_reshape(param0, reshapes[0]);
    auto reshape1 = make_reshape(param1, reshapes[1]);
    auto reshapeAdd = make_reshape(addParam, reshapes[2]);
    auto reshapeSelect = make_reshape(selectParam, reshapes[3]);
    auto reshape2 = make_reshape(param2, reshapes[4]);

    auto data0 = std::make_shared<ngraph::opset1::Parameter>(reshape0->get_element_type(), reshape0->get_shape());
    auto data1 = std::make_shared<ngraph::opset1::Parameter>(reshape1->get_element_type(), reshape1->get_shape());
    auto dataAdd = std::make_shared<ngraph::opset1::Parameter>(reshapeAdd->get_element_type(), reshapeAdd->get_shape());
    auto dataSelect = std::make_shared<ngraph::opset1::Parameter>(reshapeSelect->get_element_type(), reshapeSelect->get_shape());
    auto data2 = std::make_shared<ngraph::opset1::Parameter>(reshape2->get_element_type(), reshape2->get_shape());

    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(data0, data1);
    const auto add = std::make_shared<ngraph::opset3::Add>(matMul0, dataAdd);

    // Value is equal to '1' - to avoid situation e^(-1000) / (sum(e^(-1000)) = 0/0 = NAN
    auto selectConst = ngraph::builder::makeConstant(precision, ov::Shape{1}, std::vector<float>{1});
    std::shared_ptr<ov::Node> selectCond = dataSelect;
    if (add->get_output_partial_shape(0) != dataSelect->get_output_partial_shape(0)) {
        const auto broadcast_shape =
                ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape{add->get_output_shape(0).size()}, add->get_output_shape(0));
        selectCond = std::make_shared<ngraph::opset1::Broadcast>(selectCond, broadcast_shape);
    }
    const auto select = std::make_shared<op::TypeRelaxed<ngraph::opset1::Select>>(
            std::vector<element::Type>{ element::boolean, element::f32, element::f32 },
            std::vector<element::Type>{ element::f32 },
            ov::op::TemporaryReplaceOutputType(selectCond, element::boolean).get(),
            ov::op::TemporaryReplaceOutputType(selectConst, element::f32).get(),
            ov::op::TemporaryReplaceOutputType(add, element::f32).get());

    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(select, add->get_shape().size() - 1);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(softMax, data2);

    const auto subgraph =
            std::make_shared<ov::snippets::op::Subgraph>(
                    ov::NodeVector{reshape0, reshape1, reshapeAdd, reshapeSelect, reshape2},
                    std::make_shared<ov::Model>(ov::OutputVector{matMul1}, ov::ParameterVector{data0, data1, dataAdd, dataSelect, data2}));
    auto reshape3 = make_reshape(subgraph, reshapes[5]);
    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(reshape3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}

std::shared_ptr<ov::Model> MHAWOTransposeOnInputsFunction::initOriginal() const {
    auto param0 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto param1 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto param2 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    ngraph::ParameterVector ngraphParam = {param0, param1, param2};

    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape({4}), std::vector<int64_t>{0, 2, 1, 3});

    float transA = false;
    float transB = false;
    const auto mulConst = ngraph::builder::makeConstant(precision, ov::Shape({1}), std::vector<float>{1}, true);
    const auto mul = std::make_shared<ngraph::opset3::Multiply>(param1, mulConst);
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(param0, mul, transA, transB);
    const auto softmax = std::make_shared<ngraph::opset8::Softmax>(matMul0, -1);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(softmax, param2, transA, transB);
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(matMul1, transpose3Const);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(transpose3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}

std::shared_ptr<ov::Model> MHAWOTransposeFunction::initOriginal() const {
    auto param0 = std::make_shared<ngraph::opset1::Parameter>(precisions[0], input_shapes[0]);
    auto param1 = std::make_shared<ngraph::opset1::Parameter>(precisions[1], input_shapes[1]);
    auto param2 = std::make_shared<ngraph::opset1::Parameter>(precisions[2], input_shapes[2]);
    ngraph::ParameterVector ngraphParam = {param0, param1, param2};

    float transA = false;
    float transB = false;
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(param0, param1, transA, transB);
    const auto softmax = std::make_shared<ngraph::opset8::Softmax>(matMul0, -1);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(softmax, param2, transA, transB);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(matMul1)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}

std::shared_ptr<ov::Model> MHAWOTransposeSplitMFunction::initReference() const {
    auto param0 = std::make_shared<ngraph::opset1::Parameter>(precisions[0], input_shapes[0]);
    auto param1 = std::make_shared<ngraph::opset1::Parameter>(precisions[1], input_shapes[1]);
    auto param2 = std::make_shared<ngraph::opset1::Parameter>(precisions[2], input_shapes[2]);
    ngraph::ParameterVector ngraphParam = {param0, param1, param2};

    auto make_reshape = [](const std::shared_ptr<ov::Node>& node, const ov::Shape& new_shape) {
        auto shape_const = ngraph::builder::makeConstant(ngraph::element::i32, {new_shape.size()}, new_shape);
        return std::make_shared<ov::op::v1::Reshape>(node, shape_const, true);
    };

    auto reshape0 = make_reshape(param0, reshapes[0]);
    auto reshape1 = make_reshape(param1, reshapes[1]);
    auto reshape2 = make_reshape(param2, reshapes[2]);

    auto data0 = std::make_shared<ngraph::opset1::Parameter>(precisions[0], reshape0->get_shape());
    auto data1 = std::make_shared<ngraph::opset1::Parameter>(precisions[1], reshape1->get_shape());
    auto data2 = std::make_shared<ngraph::opset1::Parameter>(precisions[2], reshape2->get_shape());

    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(data0, data1);
    const auto softmax = std::make_shared<ngraph::opset8::Softmax>(matMul0, -1);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(softmax, data2);

    const auto subgraph = std::make_shared<ov::snippets::op::Subgraph>(ov::NodeVector{reshape0, reshape1, reshape2},
                                                                       std::make_shared<ov::Model>(ov::OutputVector{matMul1},
                                                                                                   ov::ParameterVector{data0, data1, data2}));
    auto reshape3 = make_reshape(subgraph, reshapes[3]);
    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(reshape3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}

std::shared_ptr<ov::Model> MHAFQAfterMatMulFunction::initOriginal() const {
    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[3]);
    ngraph::ParameterVector ngraphParam = {transpose0Param, transpose1Param, addParam, transpose2Param};

    const auto shape_rank = input_shapes[0].get_shape().size();
    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});

    std::vector<int64_t> reshape0ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0] *
                                                                   input_shapes[0].get_shape()[1] * input_shapes[0].get_shape()[2]),
                                              -1};
    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, {reshape0ConstData.size()}, reshape0ConstData);

    std::vector<int64_t> reshape1ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[2]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1])};
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, {reshape1ConstData.size()}, reshape1ConstData);

    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(transpose0Param, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(transpose1Param, transpose1Const);
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0, transpose1, transA, transB);
    auto fq0 = ngraph::builder::makeFakeQuantize(matMul0, ov::element::f32, 256, {1},
                                                 {-35.0172004}, {34.7436294}, {-35.0172004}, {34.7436294});
    const auto add = std::make_shared<ngraph::opset3::Add>(fq0, addParam);
    const auto reshape0 = std::make_shared<ngraph::opset1::Reshape>(add, reshape0Const, true);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(reshape0, 1);
    const auto reshape1 = std::make_shared<ngraph::opset1::Reshape>(softMax, reshape1Const, true);
    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(transpose2Param, transpose2Const);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(reshape1, transpose2, transA, transB);
    auto fq1 = ngraph::builder::makeFakeQuantize(matMul1, ov::element::f32, 256, {1},
                                                 {-35.0172004}, {34.7436294}, {-35.0172004}, {34.7436294});
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(fq1, transpose3Const);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(transpose3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}
std::shared_ptr<ov::Model> MHAINT8MatMulFunction::initOriginal() const {
    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[3]);
    ngraph::ParameterVector ngraphParam = {transpose0Param, transpose1Param, addParam, transpose2Param};

    const auto shape_rank = input_shapes[0].get_shape().size();
    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});

    std::vector<int64_t> reshape0ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0] *
                                                                   input_shapes[0].get_shape()[1] * input_shapes[0].get_shape()[2]),
                                              -1};
    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, {reshape0ConstData.size()}, reshape0ConstData);

    std::vector<int64_t> reshape1ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[2]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1])};
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, {reshape1ConstData.size()}, reshape1ConstData);

    auto fq0 = ngraph::builder::makeFakeQuantize(transpose0Param, ov::element::f32, 256, {1},
                                                 {-35.0172004}, {34.7436294}, {-35.0172004}, {34.7436294});
    auto fq1 = ngraph::builder::makeFakeQuantize(transpose1Param, ov::element::f32, 256, {1},
                                                 {-35.0172004}, {34.7436294}, {-35.0172004}, {34.7436294});
    auto fq2 = ngraph::builder::makeFakeQuantize(transpose2Param, ov::element::f32, 256, {1},
                                                 {-35.0172004}, {34.7436294}, {-35.0172004}, {34.7436294});
    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(fq0, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(fq1, transpose1Const);
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0, transpose1, transA, transB);
    auto fq3 = ngraph::builder::makeFakeQuantize(matMul0, ov::element::f32, 256, {1},
                                                 {-35.0172004}, {34.7436294}, {-35.0172004}, {34.7436294});
    const auto add = std::make_shared<ngraph::opset3::Add>(fq3, addParam);
    const auto reshape0 = std::make_shared<ngraph::opset1::Reshape>(add, reshape0Const, true);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(reshape0, 1);
    const auto reshape1 = std::make_shared<ngraph::opset1::Reshape>(softMax, reshape1Const, true);
    auto fq4 = ngraph::builder::makeFakeQuantize(reshape1, ov::element::f32, 256, {1},
                                                 {0}, {0.820726}, {0}, {0.820726});
    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(fq2, transpose2Const);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(fq4, transpose2, transA, transB);
    auto fq5 = ngraph::builder::makeFakeQuantize(matMul1, ov::element::f32, 256, {1},
                                                 {-35.0172004}, {34.7436294}, {-35.0172004}, {34.7436294});
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(fq5, transpose3Const);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(transpose3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}
std::shared_ptr<ov::Model> MHAQuantMatMul0Function::initOriginal() const {
    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[3]);
    ngraph::ParameterVector ngraphParam = {transpose0Param, transpose1Param, addParam, transpose2Param};

    const auto channel = int64_t(12);
    const auto last_dim = input_shapes[0].get_shape().back();
    OPENVINO_ASSERT(last_dim % channel == 0, "Incorrect test configuration");
    const auto new_shape = std::vector<int64_t>{0, 0, channel, static_cast<int64_t>(last_dim) / channel};

    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, {new_shape.size()}, new_shape);
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, {new_shape.size()}, new_shape);
    auto reshape2Const = ngraph::builder::makeConstant(ngraph::element::i64, {new_shape.size()}, new_shape);
    auto reshape3Const = ngraph::builder::makeConstant(ngraph::element::i64, {input_shapes[0].size()}, std::vector<int64_t>{0, 0, -1});

    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, {4}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, {4}, std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, {4}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, {4}, std::vector<int64_t>{0, 2, 1, 3});

    const auto reshape1 = std::make_shared<ov::op::v1::Reshape>(transpose1Param, reshape1Const, true);
    const auto reshape2 = std::make_shared<ov::op::v1::Reshape>(transpose2Param, reshape2Const, true);

    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(reshape1, transpose1Const);
    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(reshape2, transpose2Const);

    auto fq0 = ngraph::builder::makeFakeQuantize(transpose0Param, ov::element::f32, 256, {1},
                                                 {-12.5187311}, {12.4209289}, {-12.5187311}, {12.4209289});
    auto fq1 = ngraph::builder::makeFakeQuantize(transpose1, ov::element::f32, 256, {1},
                                                 {-1.43326699}, {1.42206954}, {-1.43326699}, {1.42206954});

    const auto reshape0 = std::make_shared<ov::op::v1::Reshape>(fq0, reshape0Const, true);
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(reshape0, transpose0Const);

    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0, fq1);
    const auto add = std::make_shared<ngraph::opset3::Add>(matMul0, addParam);
    const auto softMax = std::make_shared<ngraph::opset8::Softmax>(add, -1);

    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(softMax, transpose2);
    auto fq2 = ngraph::builder::makeFakeQuantize(matMul1, ov::element::f32, 256, {1},
                                                 {-1.81826221}, {1.804057}, {-1.81826221}, {1.804057});
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(fq2, transpose3Const);
    const auto reshape3 = std::make_shared<ov::op::v1::Reshape>(transpose3, reshape3Const, true);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(reshape3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}
std::shared_ptr<ov::Model> MHAFQFunction::initOriginal() const {
    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[3]);
    ngraph::ParameterVector ngraphParam = {transpose0Param, transpose1Param, addParam, transpose2Param};

    const auto shape_rank = input_shapes[0].get_shape().size();
    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});

    const auto fq0 = ngraph::builder::makeFakeQuantize(transpose0Param, ov::element::f32, 256, {1},
                                                       {-5.217694}, {6.661877}, {-5.217694}, {6.661877});
    const auto fq1 = ngraph::builder::makeFakeQuantize(transpose1Param, ov::element::f32, 256, {1},
                                                       {-6.40245}, {6.45286}, {-6.40245}, {6.45286});
    const auto fq_add = ngraph::builder::makeFakeQuantize(addParam, ov::element::f32, 256, {1},
                                                          {-1000}, {0}, {-1000}, {0});

    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(fq0, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(fq1, transpose1Const);
    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(transpose2Param, transpose2Const);
    const auto mul_const = ngraph::builder::makeConstant(ov::element::i8, ov::Shape{1}, std::vector<int8_t>{127});
    const auto convert = std::make_shared<ngraph::opset1::Convert>(mul_const, ov::element::f32);
    const auto mul_deq_const = ngraph::builder::makeConstant(ov::element::f32, ov::Shape{1}, std::vector<float>{0.00098425});
    const auto mul_deq = std::make_shared<ngraph::opset1::Multiply>(convert, mul_deq_const);
    const auto mul = std::make_shared<ngraph::opset1::Multiply>(transpose1, mul_deq);
    auto fq1_1 = ngraph::builder::makeFakeQuantize(mul, ov::element::f32, 256, {1},
                                                   {-0.8003067}, {0.8066083}, {-0.8003067}, {0.8066083});
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0, fq1_1, transA, transB);
    auto fq2 = ngraph::builder::makeFakeQuantize(matMul0, ov::element::f32, 256, {1},
                                                   {-14.50351}, {17.65645}, {-14.50351}, {17.65645});
    const auto add = std::make_shared<ngraph::opset1::Add>(fq2, fq_add);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(add, 3);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(softMax, transpose2, transA, transB);
    auto fq3 = ngraph::builder::makeFakeQuantize(matMul1, ov::element::f32, 256, {1},
                                                 {-1.895786}, {2.0028071}, {-1.895786}, {2.0028071});
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(fq3, transpose3Const);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(transpose3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}
std::shared_ptr<ov::Model> MHAINT8MatMulTypeRelaxedFunction::initOriginal() const {
    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[3]);
    ngraph::ParameterVector ngraphParam = {transpose0Param, transpose1Param, addParam, transpose2Param};

    const auto shape_rank = input_shapes[0].get_shape().size();
    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});

    std::vector<int64_t> reshape0ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0] *
                                                                   input_shapes[0].get_shape()[1] * input_shapes[0].get_shape()[2]),
                                              -1};
    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, {reshape0ConstData.size()}, reshape0ConstData);

    std::vector<int64_t> reshape1ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[2]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1])};
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, {reshape1ConstData.size()}, reshape1ConstData);

    const auto fq_signed_params = ngraph::builder::subgraph::FakeQuantizeOnData(256, {1}, {-36912.66015625}, {36624.28125}, {-128}, {127}, ov::element::i8);
    const auto fq0 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(transpose0Param, ov::element::i8, fq_signed_params);
    const auto fq1 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(transpose1Param, ov::element::i8, fq_signed_params);
    const auto fq2 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(transpose2Param, ov::element::i8, fq_signed_params);

    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(fq0, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(fq1, transpose1Const);
    const auto matMul0 = std::make_shared<op::TypeRelaxed<op::v0::MatMul>>(
            std::vector<element::Type>{ element::f32, element::f32 },
            std::vector<element::Type>{ element::f32 },
            ov::op::TemporaryReplaceOutputType(transpose0, element::f32).get(),
            ov::op::TemporaryReplaceOutputType(transpose1, element::f32).get(), transA, transB);

    const auto fq3 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(matMul0, ov::element::i8, fq_signed_params);
    const auto add = std::make_shared<op::TypeRelaxed<ngraph::opset3::Add>>(
            std::vector<element::Type>{ element::f32, element::f32 },
            std::vector<element::Type>{ element::f32 },
            ov::op::TemporaryReplaceOutputType(fq3, element::f32).get(),
            ov::op::TemporaryReplaceOutputType(addParam, element::f32).get());
    const auto deq = std::make_shared<ov::op::v0::Constant>(ov::element::f32, ov::Shape{1}, std::vector<float>{0.1122});
    const auto deq_mul = std::make_shared<op::TypeRelaxed<ngraph::opset3::Multiply>>(
            std::vector<element::Type>{ element::f32, element::f32 },
            std::vector<element::Type>{ element::f32 },
            ov::op::TemporaryReplaceOutputType(add, element::f32).get(),
            ov::op::TemporaryReplaceOutputType(deq, element::f32).get());

    const auto reshape0 = std::make_shared<ngraph::opset1::Reshape>(add, reshape0Const, true);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(reshape0, 1);
    const auto reshape1 = std::make_shared<ngraph::opset1::Reshape>(softMax, reshape1Const, true);

    const auto fq_unsigned_params = ngraph::builder::subgraph::FakeQuantizeOnData(256, {1}, {0}, {0.245}, {0}, {255}, ov::element::u8);
    const auto fq4 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(reshape1, ov::element::u8, fq_unsigned_params);

    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(fq2, transpose2Const);
    const auto matMul1 = std::make_shared<op::TypeRelaxed<op::v0::MatMul>>(
            std::vector<element::Type>{ element::f32, element::f32 },
            std::vector<element::Type>{ element::f32 },
            ov::op::TemporaryReplaceOutputType(fq4, element::f32).get(),
            ov::op::TemporaryReplaceOutputType(transpose2, element::f32).get(), transA, transB);
    const auto fq5 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(matMul1, ov::element::i8, fq_signed_params);
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(fq5, transpose3Const);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(transpose3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}
std::shared_ptr<ov::Model> MHAINT8MatMulTypeRelaxedFunction::initReference() const {
    auto data0 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto data1 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto data2 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    auto data3 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[3]);
    ngraph::ParameterVector ngraphParams = {data0, data1, data2, data3};

    const auto fq_signed_params = ngraph::builder::subgraph::FakeQuantizeOnData(256, {1}, {-36912.66015625}, {36624.28125}, {-128}, {127}, ov::element::i8);
    const auto fq0 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(data0, ov::element::i8, fq_signed_params);
    const auto fq1 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(data1, ov::element::i8, fq_signed_params);
    const auto fq2 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(data3, ov::element::i8, fq_signed_params);
    NodeVector subgraph_inputs = {fq0, fq1, data2, fq2};

    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto addParam = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[3]);
    ov::ParameterVector subgraph_params = {transpose0Param, transpose1Param, addParam, transpose2Param};

    const auto shape_rank = input_shapes[0].get_shape().size();
    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, {shape_rank}, std::vector<int64_t>{0, 2, 1, 3});

    std::vector<int64_t> reshape0ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0] *
                                                                   input_shapes[0].get_shape()[1] * input_shapes[0].get_shape()[2]),
                                              -1};
    auto reshape0Const = ngraph::builder::makeConstant(ngraph::element::i64, {reshape0ConstData.size()}, reshape0ConstData);

    std::vector<int64_t> reshape1ConstData = {static_cast<int64_t>(input_shapes[0].get_shape()[0]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[2]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1]),
                                              static_cast<int64_t>(input_shapes[0].get_shape()[1])};
    auto reshape1Const = ngraph::builder::makeConstant(ngraph::element::i64, {reshape1ConstData.size()}, reshape1ConstData);

    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(transpose0Param, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(transpose1Param, transpose1Const);
    const auto matMul0 = std::make_shared<op::TypeRelaxed<op::v0::MatMul>>(
            std::vector<element::Type>{ element::f32, element::f32 },
            std::vector<element::Type>{ element::f32 },
            ov::op::TemporaryReplaceOutputType(transpose0, element::f32).get(),
            ov::op::TemporaryReplaceOutputType(transpose1, element::f32).get(), transA, transB);

    const auto fq3 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(matMul0, ov::element::i8, fq_signed_params);
    const auto add = std::make_shared<op::TypeRelaxed<ngraph::opset3::Add>>(
            std::vector<element::Type>{ element::f32, element::f32 },
            std::vector<element::Type>{ element::f32 },
            ov::op::TemporaryReplaceOutputType(fq3, element::f32).get(),
            ov::op::TemporaryReplaceOutputType(addParam, element::f32).get());
    const auto deq = std::make_shared<ov::op::v0::Constant>(ov::element::f32, ov::Shape{1}, std::vector<float>{0.1122});
    const auto deq_mul = std::make_shared<op::TypeRelaxed<ngraph::opset3::Multiply>>(
            std::vector<element::Type>{ element::f32, element::f32 },
            std::vector<element::Type>{ element::f32 },
            ov::op::TemporaryReplaceOutputType(add, element::f32).get(),
            ov::op::TemporaryReplaceOutputType(deq, element::f32).get());

    const auto reshape0 = std::make_shared<ngraph::opset1::Reshape>(add, reshape0Const, true);
    const auto softMax = std::make_shared<ngraph::opset1::Softmax>(reshape0, 1);
    const auto reshape1 = std::make_shared<ngraph::opset1::Reshape>(softMax, reshape1Const, true);

    const auto fq_unsigned_params = ngraph::builder::subgraph::FakeQuantizeOnData(256, {1}, {0}, {0.245}, {0}, {255}, ov::element::u8);
    const auto fq4 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(reshape1, ov::element::u8, fq_unsigned_params);

    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(transpose2Param, transpose2Const);
    const auto matMul1 = std::make_shared<op::TypeRelaxed<op::v0::MatMul>>(
            std::vector<element::Type>{ element::f32, element::f32 },
            std::vector<element::Type>{ element::f32 },
            ov::op::TemporaryReplaceOutputType(fq4, element::f32).get(),
            ov::op::TemporaryReplaceOutputType(transpose2, element::f32).get(), transA, transB);
    const auto fq5 = ngraph::builder::subgraph::makeFakeQuantizeTypeRelaxed(matMul1, ov::element::i8, fq_signed_params);

    auto subgraph = std::make_shared<ov::snippets::op::Subgraph>(subgraph_inputs,
                                                                     std::make_shared<ov::Model>(NodeVector{fq5}, subgraph_params));
    // TODO: At the moment Snippets don't support explicitly Transpose.
    //       So we cannot collapse Transpose into Subgraph if there are ops between MatMul2 and Transpose3
    auto transpose3 = std::make_shared<ov::op::v1::Transpose>(subgraph, transpose3Const);

    return std::make_shared<ov::Model>(NodeVector{transpose3}, ngraphParams);
}

std::shared_ptr<ov::Model> MHAMulAddFunction::initOriginal() const {
    auto transpose0Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    auto transpose1Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    auto transpose2Param = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    ngraph::ParameterVector ngraphParam = {transpose0Param, transpose1Param, transpose2Param};

    auto transpose0Const = ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape{input_shapes[0].size()}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose1Const = ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape{input_shapes[1].size()}, std::vector<int64_t>{0, 2, 3, 1});
    auto transpose2Const = ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape{input_shapes[2].size()}, std::vector<int64_t>{0, 2, 1, 3});
    auto transpose3Const = ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape{input_shapes[2].size()}, std::vector<int64_t>{0, 2, 1, 3});

    float transA = false;
    float transB = false;
    const auto transpose0 = std::make_shared<ov::op::v1::Transpose>(transpose0Param, transpose0Const);
    const auto transpose1 = std::make_shared<ov::op::v1::Transpose>(transpose1Param, transpose1Const);
    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(transpose0, transpose1, transA, transB);
    auto mulConst = ngraph::builder::makeConstant(ngraph::element::f32, matMul0->get_shape(), std::vector<float>{}, true);
    auto addConst = ngraph::builder::makeConstant(ngraph::element::f32, matMul0->get_shape(), std::vector<float>{}, true);
    const auto mul = std::make_shared<ngraph::opset3::Multiply>(matMul0, mulConst);
    const auto add = std::make_shared<ngraph::opset3::Add>(mul, addConst);
    const auto softMax = std::make_shared<ov::op::v8::Softmax>(add, -1);
    const auto transpose2 = std::make_shared<ov::op::v1::Transpose>(transpose2Param, transpose2Const);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(softMax, transpose2, transA, transB);
    const auto transpose3 = std::make_shared<ov::op::v1::Transpose>(matMul1, transpose3Const);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(transpose3)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}

std::shared_ptr<ov::Model> MHATransposedInputFunction::initOriginal() const {
    const auto param0 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    const auto param1 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    const auto param2 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    ngraph::ParameterVector ngraphParam = {param0, param1, param2};

    std::shared_ptr<ov::Node> matmul0_in1 = param1;
    if (!m_order.empty()) {
        const auto transposeConst = ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape{m_order.size()}, m_order);
        matmul0_in1 = std::make_shared<ov::op::v1::Transpose>(param1, transposeConst);
    }

    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(param0, matmul0_in1, false, m_transposed_b);
    const auto softmax = std::make_shared<ngraph::opset8::Softmax>(matMul0, -1);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(softmax, param2);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(matMul1)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}

std::shared_ptr<ov::Model> MHATransposedInputFunction::initReference() const {
    const auto data0 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    const auto data1 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    const auto data2 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    ngraph::ParameterVector ngraphParam = {data0, data1, data2};

    bool is_supported = ((m_transposed_b && m_order == std::vector<int64_t>{0, 2, 1, 3}) ||
                         (!m_transposed_b && m_order == std::vector<int64_t>{0, 2, 3, 1}));

    std::shared_ptr<ov::Node> in1 = data1;
    if (!m_order.empty() && !is_supported) {
        const auto transposeConst = ngraph::builder::makeConstant(ngraph::element::i64, ov::Shape{m_order.size()}, m_order);
        in1 = std::make_shared<ov::op::v1::Transpose>(in1, transposeConst);
    }
    if (m_transposed_b) {
        if (m_order != std::vector<int64_t>{0, 2, 1, 3}) {
            const auto rank = input_shapes[1].size();
            std::vector<int32_t> transpose_order(rank, 0);
            std::iota(transpose_order.begin(), transpose_order.end(), 0);
            std::swap(transpose_order[rank - 1], transpose_order[rank - 2]);
            const auto transposeConst = ngraph::builder::makeConstant(ngraph::element::i32, ov::Shape{transpose_order.size()}, transpose_order);
            in1 = std::make_shared<ov::op::v1::Transpose>(in1, transposeConst);
        }
    }

    const auto param0 = std::make_shared<ngraph::opset1::Parameter>(precision, data0->get_shape());
    const auto param1 = std::make_shared<ngraph::opset1::Parameter>(precision, in1->get_shape());
    const auto param2 = std::make_shared<ngraph::opset1::Parameter>(precision, data2->get_shape());

    std::shared_ptr<ov::Node> matmul0_in1 = param1;
    if (!m_order.empty() && is_supported) {
        const auto transposeConst = ngraph::builder::makeConstant(ngraph::element::i32, ov::Shape{m_order.size()}, m_order);
        matmul0_in1 = std::make_shared<ov::op::v1::Transpose>(param1, transposeConst);
    }

    const auto matMul0 = std::make_shared<ngraph::opset3::MatMul>(param0, matmul0_in1);
    const auto softmax = std::make_shared<ngraph::opset8::Softmax>(matMul0, -1);
    const auto matMul1 = std::make_shared<ngraph::opset3::MatMul>(softmax, param2);

    auto subgraph = std::make_shared<ov::snippets::op::Subgraph>(ov::NodeVector{data0, in1, data2},
                                                                 std::make_shared<ov::Model>(NodeVector{matMul1}, ov::ParameterVector{param0, param1, param2}));

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(subgraph)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}

std::shared_ptr<ov::Model> MHAWithExtractedReshapeFunction::initOriginal() const {
    const auto param_0 = std::make_shared<ov::opset1::Parameter>(precision, input_shapes[0]);
    const auto param_1 = std::make_shared<ov::opset1::Parameter>(precision, input_shapes[1]);
    const auto param_2 = std::make_shared<ov::opset1::Parameter>(precision, input_shapes[2]);
    const auto param_3 = std::make_shared<ov::opset1::Parameter>(precision, input_shapes[3]);
    const auto param_4 = std::make_shared<ov::opset1::Parameter>(precision, input_shapes[4]);
    ov::ParameterVector parameters = {param_0, param_1, param_2, param_3, param_4};
    const auto matmul_0 = std::make_shared<ov::opset1::MatMul>(param_0, param_1);

    std::shared_ptr<ov::Node> add_input;
    if (add_2nd_reshape) {
        auto target_shape = input_shapes[2].to_shape();
        target_shape.push_back(1);
        const auto reshape_const = ov::opset1::Constant::create(ov::element::i32, {target_shape.size()}, target_shape);
        add_input = std::make_shared<ov::opset1::Reshape>(param_2, reshape_const, false);
    } else {
        add_input = param_2;
    }

    const auto& add_input_shape = add_input->get_output_shape(0);
    ov::Shape target_shape(add_input_shape.size());
    for (size_t i = 0; i < add_input_shape.size(); ++i)
        target_shape[i] = std::max(add_input_shape[i], static_cast<size_t>(input_shapes[3][i].get_length()));
    const auto target_shape_const_1 = ov::opset1::Constant::create(ov::element::i32, ov::Shape{target_shape.size()}, target_shape);
    const auto reshape_1 = std::make_shared<ov::opset1::Reshape>(matmul_0, target_shape_const_1, false);

    const auto add_1 = std::make_shared<ov::opset1::Add>(reshape_1, add_input);
    const auto add_2 = std::make_shared<ov::opset1::Add>(add_1, param_3);

    const auto& mm_out_shape = matmul_0->get_output_shape(0);
    const auto target_shape_const_2 = ov::opset1::Constant::create(ov::element::i32, ov::Shape{mm_out_shape.size()}, mm_out_shape);
    const auto reshape_2 = std::make_shared<ov::opset1::Reshape>(add_2, target_shape_const_2, false);

    const auto softmax = std::make_shared<ov::opset8::Softmax>(reshape_2, -1);
    const auto matmul_1 = std::make_shared<ov::opset1::MatMul>(softmax, param_4);

    ov::ResultVector results{std::make_shared<ov::opset1::Result>(matmul_1)};
    return std::make_shared<ov::Model>(results, parameters, "mha");
}

std::shared_ptr<ov::Model> MHAWithExtractedReshapeFunction::initReference() const {
    const auto data_0 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[0]);
    const auto data_1 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[1]);
    const auto data_2 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[2]);
    const auto data_3 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[3]);
    const auto data_4 = std::make_shared<ngraph::opset1::Parameter>(precision, input_shapes[4]);
    ngraph::ParameterVector ngraphParam = {data_0, data_1, data_2, data_3, data_4};

    std::shared_ptr<ov::Node> add_input;
    if (add_2nd_reshape) {
        auto target_shape = input_shapes[2].to_shape();
        target_shape.push_back(1);
        const auto reshape_const = ov::opset1::Constant::create(ov::element::i32, {target_shape.size()}, target_shape);
        add_input = std::make_shared<ov::opset1::Reshape>(data_2, reshape_const, false);
    } else {
        add_input = data_2;
    }

    const auto external_add = std::make_shared<ov::opset1::Add>(add_input, data_3);
    ov::Shape mm_out_shape = input_shapes[0].to_shape();
    mm_out_shape.back() = input_shapes[1].to_shape().back();
    const auto target_shape = ov::opset1::Constant::create(ov::element::i32, {mm_out_shape.size()}, mm_out_shape);
    const auto reshape = std::make_shared<ov::opset1::Reshape>(external_add, target_shape, false);

    const auto param_0 = std::make_shared<ngraph::opset1::Parameter>(precision, data_0->get_shape());
    const auto param_1 = std::make_shared<ngraph::opset1::Parameter>(precision, data_1->get_shape());
    const auto param_2 = std::make_shared<ngraph::opset1::Parameter>(precision, reshape->get_shape());
    const auto param_3 = std::make_shared<ngraph::opset1::Parameter>(precision, data_4->get_shape());

    const auto matmul_0 = std::make_shared<ngraph::opset3::MatMul>(param_0, param_1);
    const auto add_internal = std::make_shared<ov::opset1::Add>(matmul_0, param_2);
    const auto softmax = std::make_shared<ngraph::opset8::Softmax>(add_internal, -1);
    const auto matmul_1 = std::make_shared<ngraph::opset3::MatMul>(softmax, param_3);

    auto subgraph_model = std::make_shared<ov::Model>(NodeVector{matmul_1}, ov::ParameterVector{param_0, param_1, param_2, param_3});
    auto subgraph = std::make_shared<ov::snippets::op::Subgraph>(ov::NodeVector{data_0, data_1, reshape, data_4}, subgraph_model);

    ngraph::ResultVector results{std::make_shared<ngraph::opset1::Result>(subgraph)};
    return std::make_shared<ov::Model>(results, ngraphParam, "mha");
}

}  // namespace snippets
}  // namespace test
}  // namespace ov
