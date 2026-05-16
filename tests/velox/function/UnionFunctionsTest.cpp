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

#include "velox/core/Expressions.h"
#include "velox/core/QueryConfig.h"
#include "velox/expression/EvalCtx.h"
#include "velox/expression/Expr.h"
#include "velox/expression/RegisterSpecialForm.h"
#include "velox/expression/VectorFunction.h"
#include "velox/vector/ComplexVector.h"
#include "velox/vector/FlatVector.h"
#include "velox/vector/tests/utils/VectorTestBase.h"

namespace sdb::pg::functions {
void RegisterUnionFunctions(const std::string& prefix);
}

namespace {

using namespace facebook::velox;

class UnionFunctionsTest : public testing::Test,
                           public facebook::velox::test::VectorTestBase {
 protected:
  static void SetUpTestSuite() {
    exec::registerFunctionCallToSpecialForms();
    memory::MemoryManager::testingSetInstance(memory::MemoryManager::Options{});
  }

  void SetUp() override { sdb::pg::functions::RegisterUnionFunctions(""); }

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

  VectorPtr evaluateUnionTypeExpr(const VectorPtr& unionCol) {
    auto queryCtx = core::QueryCtx::create();
    auto execCtx = std::make_unique<core::ExecCtx>(pool(), queryCtx.get());

    auto fieldExpr =
      std::make_shared<core::FieldAccessTypedExpr>(unionCol->type(), "c0");
    auto callExpr = std::make_shared<core::CallTypedExpr>(
      VARCHAR(), std::vector<core::TypedExprPtr>{fieldExpr}, "union_type");

    exec::ExprSet exprSet({callExpr}, execCtx.get());
    auto rowVector = makeRowVector({"c0"}, {unionCol});
    SelectivityVector rows(unionCol->size());
    exec::EvalCtx evalCtx(execCtx.get(), &exprSet, rowVector.get());

    std::vector<VectorPtr> results(1);
    exprSet.eval(rows, evalCtx, results);
    return results[0];
  }

  std::shared_ptr<exec::VectorFunction> getVectorFunction(
    const std::string& name, const std::vector<exec::VectorFunctionArg>& args) {
    const auto& factories = exec::vectorFunctionFactories();
    auto locked = factories.rlock();
    auto it = locked->find(name);
    VELOX_CHECK(it != locked->end(), "Function '{}' not found", name);
    return it->second.factory(name, args, core::QueryConfig{{}});
  }

  VectorPtr applyVectorFunction(
    const std::string& name,
    const std::vector<exec::VectorFunctionArg>& factoryArgs,
    std::vector<VectorPtr> args, const TypePtr& outputType,
    vector_size_t size) {
    auto func = getVectorFunction(name, factoryArgs);
    SelectivityVector rows(size);
    auto queryCtx = core::QueryCtx::create();
    auto execCtx = std::make_unique<core::ExecCtx>(pool(), queryCtx.get());
    exec::EvalCtx evalCtx(execCtx.get());
    VectorPtr result;
    func->apply(rows, args, outputType, evalCtx, result);
    return result;
  }
};

TEST_F(UnionFunctionsTest, unionTypeBasicNames) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});

  auto unionVec =
    makeUnionVector(unionType, 3, {0, 1, 0}, {intChild, varChild});

  auto result = evaluateUnionTypeExpr(unionVec);
  ASSERT_NE(result, nullptr);
  auto* sv = result->as<SimpleVector<StringView>>();
  ASSERT_NE(sv, nullptr);

  EXPECT_FALSE(sv->isNullAt(0));
  EXPECT_EQ(sv->valueAt(0).str(), "bigint");
  EXPECT_FALSE(sv->isNullAt(1));
  EXPECT_EQ(sv->valueAt(1).str(), "text");
  EXPECT_FALSE(sv->isNullAt(2));
  EXPECT_EQ(sv->valueAt(2).str(), "bigint");
}

TEST_F(UnionFunctionsTest, unionTypeNullRow) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});
  auto unionVec =
    makeUnionVector(unionType, 3, {0, 1, 0}, {intChild, varChild}, {1});

  auto result = evaluateUnionTypeExpr(unionVec);
  auto* sv = result->as<SimpleVector<StringView>>();
  ASSERT_NE(sv, nullptr);

  EXPECT_FALSE(sv->isNullAt(0));
  EXPECT_EQ(sv->valueAt(0).str(), "bigint");
  EXPECT_TRUE(sv->isNullAt(1));
  EXPECT_FALSE(sv->isNullAt(2));
  EXPECT_EQ(sv->valueAt(2).str(), "bigint");
}

TEST_F(UnionFunctionsTest, unionTypeCanonicalNames) {
  auto unionType = UNION({INTEGER(), REAL(), DOUBLE(), BOOLEAN(), VARCHAR()});
  std::vector<VectorPtr> children = {
    makeFlatVector<bool>({false}),
    makeFlatVector<double>({0}),
    makeFlatVector<int32_t>({0}),
    makeFlatVector<float>({0}),
    makeFlatVector<StringView>({""_sv}),
  };

  static const std::string kExpected[] = {"bool", "double precision", "integer",
                                          "real", "text"};

  for (uint8_t tag = 0; tag < 5; ++tag) {
    auto unionVec = makeUnionVector(unionType, 1, {tag}, children);
    auto result = evaluateUnionTypeExpr(unionVec);
    auto* sv = result->as<SimpleVector<StringView>>();
    ASSERT_NE(sv, nullptr) << "tag=" << (int)tag;
    EXPECT_EQ(sv->valueAt(0).str(), kExpected[tag])
      << "tag=" << static_cast<int>(tag);
  }
}

TEST_F(UnionFunctionsTest, unionTypeArrayName) {
  auto unionType = UNION({ARRAY(VARCHAR())});
  auto textArrays = makeArrayVector<StringView>({
    {"a"_sv, "b"_sv},
    {"c"_sv},
  });
  auto unionVec = makeUnionVector(unionType, 2, {0, 0}, {textArrays});

  auto result = evaluateUnionTypeExpr(unionVec);
  auto* sv = result->as<SimpleVector<StringView>>();
  ASSERT_NE(sv, nullptr);
  EXPECT_EQ(sv->valueAt(0).str(), "text[]");
  EXPECT_EQ(sv->valueAt(1).str(), "text[]");
}


TEST_F(UnionFunctionsTest, unionExtractMatchingSelector) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});
  auto unionVec =
    makeUnionVector(unionType, 3, {0, 0, 0}, {intChild, varChild});

  auto selectorConst = makeConstant("bigint"_sv, 3, VARCHAR());
  auto defaultVec = makeFlatVector<int64_t>({-1, -1, -1});

  std::vector<exec::VectorFunctionArg> factoryArgs = {
    {unionType, nullptr},
    {VARCHAR(), selectorConst},
    {BIGINT(), nullptr},
  };
  std::vector<VectorPtr> args = {unionVec, selectorConst, defaultVec};

  auto result =
    applyVectorFunction("union_extract", factoryArgs, args, BIGINT(), 3);
  ASSERT_NE(result, nullptr);
  auto* flat = result->as<FlatVector<int64_t>>();
  ASSERT_NE(flat, nullptr);
  EXPECT_EQ(flat->valueAt(0), 10);
  EXPECT_EQ(flat->valueAt(1), 20);
  EXPECT_EQ(flat->valueAt(2), 30);
}

TEST_F(UnionFunctionsTest, unionExtractMismatchUsesDefault) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});

  auto unionVec =
    makeUnionVector(unionType, 3, {0, 0, 0}, {intChild, varChild});

  auto selectorConst = makeConstant("text"_sv, 3, VARCHAR());
  auto defaultVec =
    makeFlatVector<StringView>({"dflt0"_sv, "dflt1"_sv, "dflt2"_sv});

  std::vector<exec::VectorFunctionArg> factoryArgs = {
    {unionType, nullptr},
    {VARCHAR(), selectorConst},
    {VARCHAR(), nullptr},
  };
  std::vector<VectorPtr> args = {unionVec, selectorConst, defaultVec};

  auto result =
    applyVectorFunction("union_extract", factoryArgs, args, VARCHAR(), 3);
  ASSERT_NE(result, nullptr);
  auto* flat = result->as<FlatVector<StringView>>();
  ASSERT_NE(flat, nullptr);
  EXPECT_EQ(flat->valueAt(0).str(), "dflt0");
  EXPECT_EQ(flat->valueAt(1).str(), "dflt1");
  EXPECT_EQ(flat->valueAt(2).str(), "dflt2");
}

TEST_F(UnionFunctionsTest, unionExtractMatchUsesChild) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});
  auto unionVec =
    makeUnionVector(unionType, 3, {1, 1, 1}, {intChild, varChild});

  auto selectorConst = makeConstant("varchar"_sv, 3, VARCHAR());
  auto defaultVec = makeFlatVector<StringView>({"x"_sv, "y"_sv, "z"_sv});

  std::vector<exec::VectorFunctionArg> factoryArgs = {
    {unionType, nullptr},
    {VARCHAR(), selectorConst},
    {VARCHAR(), nullptr},
  };
  std::vector<VectorPtr> args = {unionVec, selectorConst, defaultVec};

  auto result =
    applyVectorFunction("union_extract", factoryArgs, args, VARCHAR(), 3);
  auto* flat = result->as<FlatVector<StringView>>();
  ASSERT_NE(flat, nullptr);
  EXPECT_EQ(flat->valueAt(0).str(), "a");
  EXPECT_EQ(flat->valueAt(1).str(), "b");
  EXPECT_EQ(flat->valueAt(2).str(), "c");
}

TEST_F(UnionFunctionsTest, unionExtractNullRowUsesDefault) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});
  auto unionVec =
    makeUnionVector(unionType, 3, {0, 0, 0}, {intChild, varChild}, {1});

  auto selectorConst = makeConstant("bigint"_sv, 3, VARCHAR());
  auto defaultVec = makeFlatVector<int64_t>({-1, -1, -1});

  std::vector<exec::VectorFunctionArg> factoryArgs = {
    {unionType, nullptr},
    {VARCHAR(), selectorConst},
    {BIGINT(), nullptr},
  };
  std::vector<VectorPtr> args = {unionVec, selectorConst, defaultVec};

  auto result =
    applyVectorFunction("union_extract", factoryArgs, args, BIGINT(), 3);
  auto* flat = result->as<FlatVector<int64_t>>();
  ASSERT_NE(flat, nullptr);
  EXPECT_EQ(flat->valueAt(0), 10);
  EXPECT_EQ(flat->valueAt(1), -1);
  EXPECT_EQ(flat->valueAt(2), 30);
}

TEST_F(UnionFunctionsTest, unionExtractDictionaryWrapped) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto intChild = makeFlatVector<int64_t>({10, 20, 30});
  auto varChild = makeFlatVector<StringView>({"a"_sv, "b"_sv, "c"_sv});
  auto unionVec =
    makeUnionVector(unionType, 3, {0, 0, 0}, {intChild, varChild});

  BufferPtr indices = AlignedBuffer::allocate<vector_size_t>(3, pool());
  auto* rawIdx = indices->asMutable<vector_size_t>();
  rawIdx[0] = 2;
  rawIdx[1] = 1;
  rawIdx[2] = 0;
  auto dictUnion = wrapInDictionary(indices, 3, unionVec);

  auto selectorConst = makeConstant("bigint"_sv, 3, VARCHAR());
  auto defaultVec = makeFlatVector<int64_t>({-1, -1, -1});

  std::vector<exec::VectorFunctionArg> factoryArgs = {
    {unionType, nullptr},
    {VARCHAR(), selectorConst},
    {BIGINT(), nullptr},
  };
  std::vector<VectorPtr> args = {dictUnion, selectorConst, defaultVec};

  auto result =
    applyVectorFunction("union_extract", factoryArgs, args, BIGINT(), 3);
  auto* flat = result->as<FlatVector<int64_t>>();
  ASSERT_NE(flat, nullptr);
  EXPECT_EQ(flat->valueAt(0), 30);
  EXPECT_EQ(flat->valueAt(1), 20);
  EXPECT_EQ(flat->valueAt(2), 10);
}

TEST_F(UnionFunctionsTest, unionExtractArraySelector) {
  auto arrayType = ARRAY(VARCHAR());
  auto unionType = UNION({arrayType, BIGINT()});
  auto textArrays = makeArrayVector<StringView>({
    {"a"_sv, "b"_sv},
    {"c"_sv},
  });
  auto intChild = makeFlatVector<int64_t>({10, 20});
  auto unionVec = makeUnionVector(unionType, 2, {0, 0}, {textArrays, intChild});

  auto selectorConst = makeConstant("varchar[]"_sv, 2, VARCHAR());
  auto defaultVec = makeArrayVector<StringView>({
    {"default"_sv},
    {"default"_sv},
  });

  std::vector<exec::VectorFunctionArg> factoryArgs = {
    {unionType, nullptr},
    {VARCHAR(), selectorConst},
    {arrayType, nullptr},
  };
  std::vector<VectorPtr> args = {unionVec, selectorConst, defaultVec};

  auto result =
    applyVectorFunction("union_extract", factoryArgs, args, arrayType, 2);
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(result->type()->equivalent(*arrayType));
  EXPECT_FALSE(result->isNullAt(0));
  EXPECT_FALSE(result->isNullAt(1));
}

TEST_F(UnionFunctionsTest, unionExtractSqlAliases) {
  auto unionType = UNION({BIGINT(), INTEGER(), VARCHAR()});
  auto bigintChild = makeFlatVector<int64_t>({7});
  auto intChild = makeFlatVector<int32_t>({8});
  auto varChild = makeFlatVector<StringView>({"v"_sv});
  std::vector<VectorPtr> children = {bigintChild, intChild, varChild};
  VectorPtr dfltBigint = makeFlatVector<int64_t>({-1});
  VectorPtr dfltInt = makeFlatVector<int32_t>({-1});
  VectorPtr dfltVar = makeFlatVector<StringView>({""_sv});

  auto checkAlias = [&](std::string_view alias, uint8_t tag,
                        const TypePtr& type, const VectorPtr& dflt) {
    auto unionVec = makeUnionVector(unionType, 1, {tag}, children);
    auto sel =
      makeConstant(StringView{alias.data(), static_cast<int32_t>(alias.size())},
                   1, VARCHAR());
    std::vector<exec::VectorFunctionArg> fargs = {
      {unionType, nullptr}, {VARCHAR(), sel}, {type, nullptr}};
    std::vector<VectorPtr> args = {unionVec, sel, dflt};
    auto result = applyVectorFunction("union_extract", fargs, args, type, 1);
    ASSERT_NE(result, nullptr) << "alias=" << alias;
    EXPECT_FALSE(result->isNullAt(0)) << "alias=" << alias;
  };

  checkAlias("int8", 0, BIGINT(), dfltBigint);
  checkAlias("bigint", 0, BIGINT(), dfltBigint);
  checkAlias("int4", 1, INTEGER(), dfltInt);
  checkAlias("integer", 1, INTEGER(), dfltInt);
  checkAlias("varchar", 2, VARCHAR(), dfltVar);
  checkAlias("text", 2, VARCHAR(), dfltVar);
}

TEST_F(UnionFunctionsTest, unionExtractUnknownSelectorThrows) {
  auto unionType = UNION({BIGINT(), VARCHAR()});
  auto selectorConst = makeConstant("no_such_type"_sv, 1, VARCHAR());
  std::vector<exec::VectorFunctionArg> factoryArgs = {
    {unionType, nullptr},
    {VARCHAR(), selectorConst},
    {BIGINT(), nullptr},
  };
  EXPECT_THROW(getVectorFunction("union_extract", factoryArgs), VeloxException);
}

TEST_F(UnionFunctionsTest, unionExtractUnmappedVeloxTypeThrows) {
  auto mapType = MAP(VARCHAR(), BIGINT());
  auto unionType = UNION({mapType});
  auto selectorConst = makeConstant("map"_sv, 1, VARCHAR());
  std::vector<exec::VectorFunctionArg> factoryArgs = {
    {unionType, nullptr},
    {VARCHAR(), selectorConst},
    {mapType, nullptr},
  };
  EXPECT_THROW(getVectorFunction("union_extract", factoryArgs), VeloxException);
}

}  // namespace
