/*
 * Copyright 2025 SereneDB GmbH, Berlin, Germany
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <velox/expression/RegisterSpecialForm.h>

#include "velox/core/Expressions.h"
#include "velox/expression/Expr.h"
#include "velox/functions/prestosql/registration/RegistrationFunctions.h"
#include "velox/vector/ComplexVector.h"
#include "velox/vector/FlatVector.h"
#include "velox/vector/tests/utils/VectorTestBase.h"

namespace facebook::velox::exec {
namespace {

class UnionComparisonTest : public testing::Test,
                            public velox::test::VectorTestBase {
 protected:
  static void SetUpTestSuite() {
    facebook::velox::exec::registerFunctionCallToSpecialForms();
    memory::MemoryManager::testingSetInstance(memory::MemoryManager::Options{});
    functions::prestosql::registerComparisonFunctions();
  }

  UnionVectorPtr makeUnionVector(
    const UnionTypePtr& unionType, vector_size_t size,
    const std::vector<uint8_t>& tagValues, std::vector<VectorPtr> children,
    const std::vector<vector_size_t>& nullRows = {}) {
    auto tags = AlignedBuffer::allocate<uint8_t>(size, pool());
    auto offsets = AlignedBuffer::allocate<vector_size_t>(size, pool());
    auto* rawTags = tags->asMutable<uint8_t>();
    auto* rawOffsets = offsets->asMutable<vector_size_t>();

    for (vector_size_t i = 0; i < size; ++i) {
      rawTags[i] = tagValues[i];
      rawOffsets[i] = i;
    }

    BufferPtr nulls;
    if (!nullRows.empty()) {
      nulls = AlignedBuffer::allocate<bool>(size, pool(), bits::kNotNull);
      auto* rawNulls = nulls->asMutable<uint64_t>();
      for (auto row : nullRows) {
        bits::setNull(rawNulls, row, true);
      }
    }

    return std::make_shared<UnionVector>(pool(), unionType, std::move(nulls),
                                         size, std::move(children),
                                         std::move(tags), std::move(offsets));
  }

  VectorPtr evaluate(const std::string& functionName, const VectorPtr& lhs,
                     const VectorPtr& rhs) {
    auto queryCtx = core::QueryCtx::create();
    auto execCtx = std::make_unique<core::ExecCtx>(pool(), queryCtx.get());

    auto lhsExpr =
      std::make_shared<core::FieldAccessTypedExpr>(lhs->type(), "c0");
    auto rhsExpr =
      std::make_shared<core::FieldAccessTypedExpr>(rhs->type(), "c1");
    auto callExpr = std::make_shared<core::CallTypedExpr>(
      BOOLEAN(), std::vector<core::TypedExprPtr>{lhsExpr, rhsExpr},
      functionName);

    ExprSet exprSet({callExpr}, execCtx.get());
    auto rowVector = makeRowVector({"c0", "c1"}, {lhs, rhs});
    SelectivityVector rows(lhs->size());
    EvalCtx evalCtx(execCtx.get(), &exprSet, rowVector.get());

    std::vector<VectorPtr> results(1);
    exprSet.eval(rows, evalCtx, results);
    return results[0];
  }

  void assertNullableBools(const VectorPtr& result,
                           const std::vector<std::optional<bool>>& expected) {
    ASSERT_EQ(result->size(), expected.size());
    auto* flatResult = result->as<FlatVector<bool>>();
    ASSERT_NE(flatResult, nullptr);

    for (vector_size_t i = 0; i < expected.size(); ++i) {
      if (expected[i].has_value()) {
        EXPECT_FALSE(flatResult->isNullAt(i)) << "row " << i;
        EXPECT_EQ(flatResult->valueAt(i), expected[i].value()) << "row " << i;
      } else {
        EXPECT_TRUE(flatResult->isNullAt(i)) << "row " << i;
      }
    }
  }

  std::pair<UnionVectorPtr, UnionVectorPtr> makeComparisonVectors() {
    auto unionType = UNION({BIGINT(), VARCHAR()});

    auto leftInt =
      makeNullableFlatVector<int64_t>({10, 20, 0, 0, 10, std::nullopt});
    auto leftString = makeFlatVector<StringView>(
      {""_sv, ""_sv, "abc"_sv, "abc"_sv, ""_sv, ""_sv});
    auto left = makeUnionVector(unionType, 6, {0, 0, 1, 1, 0, 0},
                                {leftInt, leftString}, {4});

    auto rightInt = makeNullableFlatVector<int64_t>({10, 10, 0, 10, 10, 10});
    auto rightString =
      makeFlatVector<StringView>({""_sv, ""_sv, "abd"_sv, ""_sv, ""_sv, ""_sv});
    auto right = makeUnionVector(unionType, 6, {0, 0, 1, 0, 0, 0},
                                 {rightInt, rightString});

    return {left, right};
  }

  std::pair<UnionVectorPtr, UnionVectorPtr> makeOrderingVectors() {
    auto unionType = UNION({BIGINT(), VARCHAR()});

    auto leftInt = makeFlatVector<int64_t>({10, 20, 0, 0, 10});
    auto leftString =
      makeFlatVector<StringView>({""_sv, ""_sv, "abc"_sv, "abc"_sv, ""_sv});
    auto left = makeUnionVector(unionType, 5, {0, 0, 1, 1, 0},
                                {leftInt, leftString}, {4});

    auto rightInt = makeFlatVector<int64_t>({10, 10, 0, 10, 10});
    auto rightString =
      makeFlatVector<StringView>({""_sv, ""_sv, "abd"_sv, ""_sv, ""_sv});
    auto right =
      makeUnionVector(unionType, 5, {0, 0, 1, 0, 0}, {rightInt, rightString});

    return {left, right};
  }
};

TEST_F(UnionComparisonTest, equality) {
  auto [left, right] = makeComparisonVectors();

  assertNullableBools(evaluate("eq", left, right),
                      {true, false, false, false, std::nullopt, std::nullopt});
  assertNullableBools(evaluate("neq", left, right),
                      {false, true, true, true, std::nullopt, std::nullopt});
}

TEST_F(UnionComparisonTest, ordering) {
  auto [left, right] = makeOrderingVectors();

  assertNullableBools(evaluate("lt", left, right),
                      {false, false, true, false, std::nullopt});
  assertNullableBools(evaluate("gt", left, right),
                      {false, true, false, true, std::nullopt});
  assertNullableBools(evaluate("lte", left, right),
                      {true, false, true, false, std::nullopt});
  assertNullableBools(evaluate("gte", left, right),
                      {true, true, false, true, std::nullopt});
}

TEST_F(UnionComparisonTest, distinctFrom) {
  auto unionType = UNION({BIGINT(), VARCHAR()});

  auto leftInt =
    makeNullableFlatVector<int64_t>({0, 0, std::nullopt, std::nullopt});
  auto leftString = makeFlatVector<StringView>({""_sv, ""_sv, ""_sv, ""_sv});
  auto left =
    makeUnionVector(unionType, 4, {0, 0, 0, 0}, {leftInt, leftString}, {0, 1});

  auto rightInt = makeNullableFlatVector<int64_t>({0, 10, std::nullopt, 10});
  auto rightString = makeFlatVector<StringView>({""_sv, ""_sv, ""_sv, ""_sv});
  auto right =
    makeUnionVector(unionType, 4, {0, 0, 0, 0}, {rightInt, rightString}, {0});

  assertNullableBools(evaluate("distinct_from", left, right),
                      {false, true, false, true});
}

}  // namespace
}  // namespace facebook::velox::exec
