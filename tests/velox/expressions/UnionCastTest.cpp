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
#include "velox/vector/ComplexVector.h"
#include "velox/vector/DecodedVector.h"
#include "velox/vector/FlatVector.h"
#include "velox/vector/tests/utils/VectorTestBase.h"

namespace facebook::velox::exec {
namespace {

class UnionCastTest : public testing::Test, public velox::test::VectorTestBase {
 protected:
  static void SetUpTestSuite() {
    facebook::velox::exec::registerFunctionCallToSpecialForms();
    memory::MemoryManager::testingSetInstance(memory::MemoryManager::Options{});
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

  VectorPtr castVector(const VectorPtr& input, const TypePtr& toType) {
    auto queryCtx = core::QueryCtx::create();
    auto execCtx = std::make_unique<core::ExecCtx>(pool(), queryCtx.get());

    auto inputRowType = ROW({"c0"}, {input->type()});
    auto fieldAccess =
      std::make_shared<core::FieldAccessTypedExpr>(input->type(), "c0");
    auto castTypedExpr = std::make_shared<core::CastTypedExpr>(
      toType, fieldAccess, false /* nullOnFailure */);

    ExprSet exprSet({castTypedExpr}, execCtx.get());

    auto rowVector = makeRowVector({"c0"}, {input});
    SelectivityVector rows(input->size());
    EvalCtx evalCtx(execCtx.get(), &exprSet, rowVector.get());

    std::vector<VectorPtr> results(1);
    exprSet.eval(rows, evalCtx, results);
    return results[0];
  }
};

// Cast TO Union tests
TEST_F(UnionCastTest, castIntToUnion) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto input = makeFlatVector<int64_t>({10, 20, 30});

  auto result = castVector(input, unionType);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->typeKind(), TypeKind::UNION);

  auto* unionResult = result->loadedVector()->as<UnionVector>();
  ASSERT_NE(unionResult, nullptr);
  ASSERT_EQ(unionResult->size(), 3);

  for (vector_size_t i = 0; i < 3; ++i) {
    EXPECT_FALSE(unionResult->isNullAt(i));
    EXPECT_EQ(unionResult->tagAt(i), 0);
  }

  auto* childInt = unionResult->childAt(0)->as<FlatVector<int64_t>>();
  EXPECT_EQ(childInt->valueAt(0), 10);
  EXPECT_EQ(childInt->valueAt(1), 20);
  EXPECT_EQ(childInt->valueAt(2), 30);
}

TEST_F(UnionCastTest, castVarcharToUnion) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto input = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});

  auto result = castVector(input, unionType);
  ASSERT_NE(result, nullptr);

  auto* unionResult = result->loadedVector()->as<UnionVector>();
  ASSERT_NE(unionResult, nullptr);

  for (vector_size_t i = 0; i < 3; ++i) {
    EXPECT_FALSE(unionResult->isNullAt(i));
    EXPECT_EQ(unionResult->tagAt(i), 1);
  }
}

TEST_F(UnionCastTest, castNullableIntToUnion) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto input = makeNullableFlatVector<int64_t>({10, std::nullopt, 30});

  auto result = castVector(input, unionType);
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->typeKind(), TypeKind::UNION);
  ASSERT_NE(result->encoding(), VectorEncoding::Simple::FLAT);

  DecodedVector decoded(*result, result->size());
  auto* unionResult = decoded.base()->as<UnionVector>();
  ASSERT_NE(unionResult, nullptr);
  auto* childInt = unionResult->childAt(0)->as<FlatVector<int64_t>>();
  ASSERT_NE(childInt, nullptr);

  EXPECT_FALSE(result->isNullAt(0));
  EXPECT_FALSE(decoded.isNullAt(0));
  {
    const auto baseRow = decoded.index(0);
    EXPECT_FALSE(unionResult->isNullAt(baseRow));
    EXPECT_EQ(unionResult->tagAt(baseRow), 0);
    EXPECT_EQ(childInt->valueAt(unionResult->offsetAt(baseRow)), 10);
  }

  EXPECT_TRUE(result->isNullAt(1));
  EXPECT_TRUE(decoded.isNullAt(1));

  EXPECT_FALSE(result->isNullAt(2));
  EXPECT_FALSE(decoded.isNullAt(2));
  {
    const auto baseRow = decoded.index(2);
    EXPECT_FALSE(unionResult->isNullAt(baseRow));
    EXPECT_EQ(unionResult->tagAt(baseRow), 0);
    EXPECT_EQ(childInt->valueAt(unionResult->offsetAt(baseRow)), 30);
  }
}

// Cast FROM Union tests
TEST_F(UnionCastTest, castUnionToInt_allMatch) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});

  auto unionVec =
    makeUnionVector(unionType, 3, {0, 0, 0}, {intChild, varChild});

  auto result = castVector(unionVec, BIGINT());
  ASSERT_NE(result, nullptr);

  auto* flatResult = result->as<FlatVector<int64_t>>();
  ASSERT_NE(flatResult, nullptr);

  EXPECT_FALSE(flatResult->isNullAt(0));
  EXPECT_EQ(flatResult->valueAt(0), 10);
  EXPECT_FALSE(flatResult->isNullAt(1));
  EXPECT_EQ(flatResult->valueAt(1), 20);
  EXPECT_FALSE(flatResult->isNullAt(2));
  EXPECT_EQ(flatResult->valueAt(2), 30);
}

TEST_F(UnionCastTest, castUnionToInt_mixedTags) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});

  auto unionVec =
    makeUnionVector(unionType, 3, {0, 1, 0}, {intChild, varChild});

  auto result = castVector(unionVec, BIGINT());
  ASSERT_NE(result, nullptr);

  EXPECT_FALSE(result->isNullAt(0));
  EXPECT_EQ(result->as<FlatVector<int64_t>>()->valueAt(0), 10);

  EXPECT_TRUE(result->isNullAt(1));

  EXPECT_FALSE(result->isNullAt(2));
  EXPECT_EQ(result->as<FlatVector<int64_t>>()->valueAt(2), 30);
}

TEST_F(UnionCastTest, castUnionToVarchar_mixedTags) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});

  auto unionVec =
    makeUnionVector(unionType, 3, {0, 1, 1}, {intChild, varChild});

  auto result = castVector(unionVec, VARCHAR());
  ASSERT_NE(result, nullptr);

  EXPECT_TRUE(result->isNullAt(0));

  EXPECT_FALSE(result->isNullAt(1));
  EXPECT_EQ(result->as<FlatVector<StringView>>()->valueAt(1).str(), "b");

  EXPECT_FALSE(result->isNullAt(2));
  EXPECT_EQ(result->as<FlatVector<StringView>>()->valueAt(2).str(), "c");
}

TEST_F(UnionCastTest, castUnionToInt_nullUnionRows) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});

  auto unionVec =
    makeUnionVector(unionType, 3, {0, 0, 0}, {intChild, varChild}, {1});

  auto result = castVector(unionVec, BIGINT());
  ASSERT_NE(result, nullptr);

  EXPECT_FALSE(result->isNullAt(0));
  EXPECT_EQ(result->as<FlatVector<int64_t>>()->valueAt(0), 10);
  EXPECT_TRUE(result->isNullAt(1));
  EXPECT_FALSE(result->isNullAt(2));
  EXPECT_EQ(result->as<FlatVector<int64_t>>()->valueAt(2), 30);
}

TEST_F(UnionCastTest, castUnionToInt_nonMatchingTag) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});

  auto unionVec =
    makeUnionVector(unionType, 3, {1, 1, 1}, {intChild, varChild});

  auto result = castVector(unionVec, BIGINT());
  ASSERT_NE(result, nullptr);

  for (vector_size_t i = 0; i < 3; ++i) {
    EXPECT_TRUE(result->isNullAt(i))
      << "Expected null at row " << i << " (tag mismatch)";
  }
}

}  // namespace
}  // namespace facebook::velox::exec
