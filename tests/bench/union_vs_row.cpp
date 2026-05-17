#include <benchmark/benchmark.h>
#include <velox/core/Expressions.h>
#include <velox/expression/Expr.h>
#include <velox/expression/RegisterSpecialForm.h>
#include <velox/functions/prestosql/registration/RegistrationFunctions.h>
#include <velox/vector/ComplexVector.h>
#include <velox/vector/FlatVector.h>
#include <velox/vector/tests/utils/VectorTestBase.h>

#include <random>

#include "pg/functions/union.h"

namespace {

using namespace facebook::velox;
using namespace facebook::velox::exec;

struct VeloxMicroBenchEnv {
  VeloxMicroBenchEnv() {
    memory::MemoryManager::testingSetInstance(memory::MemoryManager::Options{});
    registerFunctionCallToSpecialForms();
    functions::prestosql::registerAllScalarFunctions();
    sdb::pg::functions::RegisterUnionFunctions("");
  }
};
const VeloxMicroBenchEnv kVeloxMicroBenchEnv{};

class UnionVsRowFixture : public benchmark::Fixture,
                          public test::VectorTestBase {
 public:
  void SetUp(benchmark::State& state) override {
    numRows_ = state.range(0);
    buildData();
  }

  void buildData() {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> tagDist(0, 2);
    std::uniform_int_distribution<int64_t> intDist(0, 1'000'000);
    std::uniform_real_distribution<double> doubleDist(-1e6, 1e6);

    std::vector<uint8_t> tags(numRows_);
    std::vector<int64_t> bigintValues(numRows_);
    std::vector<std::string> varcharValues(numRows_);
    std::vector<double> doubleValues(numRows_);

    for (int i = 0; i < numRows_; ++i) {
      tags[i] = static_cast<uint8_t>(tagDist(rng));
      bigintValues[i] = intDist(rng);
      varcharValues[i] = std::to_string(i);
      doubleValues[i] = doubleDist(rng);
    }

    auto unionType = UNION({BIGINT(), DOUBLE(), VARCHAR()});

    assert(unionType->childAt(0)->kind() == TypeKind::BIGINT);
    assert(unionType->childAt(1)->kind() == TypeKind::DOUBLE);
    assert(unionType->childAt(2)->kind() == TypeKind::VARCHAR);

    unionVec_ =
      makeUnionVector(unionType, numRows_, [&](vector_size_t i) -> Variant {
        switch (tags[i]) {
          case 0:
            return Variant(bigintValues[i]);
          case 1:
            return Variant(doubleValues[i]);
          default:
            return Variant(varcharValues[i]);
        }
      });

    auto rowTagVec = makeFlatVector<int8_t>(
      numRows_, [&](auto i) { return static_cast<int8_t>(tags[i]); });

    auto rowF0 = makeFlatVector<int64_t>(
      numRows_, [&](auto i) { return bigintValues[i]; },
      [&](auto i) { return tags[i] != 0; });

    auto rowF1 = makeFlatVector<double>(
      numRows_, [&](auto i) { return doubleValues[i]; },
      [&](auto i) { return tags[i] != 1; });

    auto rowF2 = makeFlatVector<StringView>(
      numRows_, [&](auto i) { return StringView(varcharValues[i]); },
      [&](auto i) { return tags[i] != 2; });

    rowVec_ = makeRowVector({"tag", "f0", "f1", "f2"},
                            {rowTagVec, rowF0, rowF1, rowF2});
  }

 protected:
  int numRows_ = 0;
  VectorPtr unionVec_;
  RowVectorPtr rowVec_;
};

core::TypedExprPtr fieldAccess(const TypePtr& type, const std::string& name) {
  return std::make_shared<core::FieldAccessTypedExpr>(type, name);
}

core::TypedExprPtr fieldAccess(const TypePtr& type, core::TypedExprPtr input,
                               const std::string& name) {
  return std::make_shared<core::FieldAccessTypedExpr>(type, std::move(input),
                                                      name);
}

core::TypedExprPtr constant(const std::string& value) {
  return std::make_shared<core::ConstantTypedExpr>(VARCHAR(), Variant(value));
}

core::TypedExprPtr constantInt8(int8_t value) {
  return std::make_shared<core::ConstantTypedExpr>(TINYINT(), Variant(value));
}

core::TypedExprPtr call(const std::string& name, const TypePtr& returnType,
                        std::vector<core::TypedExprPtr> args) {
  return std::make_shared<core::CallTypedExpr>(returnType, std::move(args),
                                               name);
}

core::TypedExprPtr castExpr(core::TypedExprPtr input, const TypePtr& toType) {
  return std::make_shared<core::CastTypedExpr>(toType, input, false);
}

BENCHMARK_DEFINE_F(UnionVsRowFixture, DetectType_Union)(benchmark::State& st) {
  auto unionType = unionVec_->type();
  auto inputRow = makeRowVector({"c0"}, {unionVec_});

  auto expr = call("union_type", VARCHAR(), {fieldAccess(unionType, "c0")});

  auto queryCtx = core::QueryCtx::create();
  auto execCtx = std::make_unique<core::ExecCtx>(pool(), queryCtx.get());
  ExprSet exprSet({expr}, execCtx.get());

  for (auto _ : st) {
    SelectivityVector rows(numRows_);
    EvalCtx evalCtx(execCtx.get(), &exprSet, inputRow.get());
    std::vector<VectorPtr> results(1);
    exprSet.eval(rows, evalCtx, results);
    benchmark::DoNotOptimize(results[0]);
  }
  st.SetItemsProcessed(st.iterations() * numRows_);
}

BENCHMARK_DEFINE_F(UnionVsRowFixture, DetectType_Row)(benchmark::State& st) {
  auto inputRow = rowVec_;

  auto tagField = fieldAccess(TINYINT(), "tag");
  core::TypedExprPtr expr =
    call("if", VARCHAR(),
         std::vector<core::TypedExprPtr>{
           call("eq", BOOLEAN(), {tagField, constantInt8(0)}),
           constant("bigint"),
           call("if", VARCHAR(),
                std::vector<core::TypedExprPtr>{
                  call("eq", BOOLEAN(), {tagField, constantInt8(1)}),
                  constant("double precision"),
                  constant("text"),
                }),
         });

  auto queryCtx = core::QueryCtx::create();
  auto execCtx = std::make_unique<core::ExecCtx>(pool(), queryCtx.get());
  ExprSet exprSet({expr}, execCtx.get());

  for (auto _ : st) {
    SelectivityVector rows(numRows_);
    EvalCtx evalCtx(execCtx.get(), &exprSet, inputRow.get());
    std::vector<VectorPtr> results(1);
    exprSet.eval(rows, evalCtx, results);
    benchmark::DoNotOptimize(results[0]);
  }
  st.SetItemsProcessed(st.iterations() * numRows_);
}

BENCHMARK_DEFINE_F(UnionVsRowFixture,
                   ExtractBigint_Union)(benchmark::State& st) {
  auto unionType = unionVec_->type();
  auto inputRow = makeRowVector({"c0"}, {unionVec_});
  auto expr = castExpr(fieldAccess(unionType, "c0"), BIGINT());

  auto queryCtx = core::QueryCtx::create();
  auto execCtx = std::make_unique<core::ExecCtx>(pool(), queryCtx.get());
  ExprSet exprSet({expr}, execCtx.get());

  for (auto _ : st) {
    SelectivityVector rows(numRows_);
    EvalCtx evalCtx(execCtx.get(), &exprSet, inputRow.get());
    std::vector<VectorPtr> results(1);
    exprSet.eval(rows, evalCtx, results);
    benchmark::DoNotOptimize(results[0]);
  }
  st.SetItemsProcessed(st.iterations() * numRows_);
}

BENCHMARK_DEFINE_F(UnionVsRowFixture, ExtractBigint_Row)(benchmark::State& st) {
  auto inputRow = rowVec_;
  auto expr = fieldAccess(BIGINT(), "f0");

  auto queryCtx = core::QueryCtx::create();
  auto execCtx = std::make_unique<core::ExecCtx>(pool(), queryCtx.get());
  ExprSet exprSet({expr}, execCtx.get());

  for (auto _ : st) {
    SelectivityVector rows(numRows_);
    EvalCtx evalCtx(execCtx.get(), &exprSet, inputRow.get());
    std::vector<VectorPtr> results(1);
    exprSet.eval(rows, evalCtx, results);
    benchmark::DoNotOptimize(results[0]);
  }
  st.SetItemsProcessed(st.iterations() * numRows_);
}

BENCHMARK_DEFINE_F(UnionVsRowFixture, Compare_Union)(benchmark::State& st) {
  auto unionType = unionVec_->type();
  auto inputRow = makeRowVector({"c0", "c1"}, {unionVec_, unionVec_});

  auto expr =
    call("eq", BOOLEAN(),
         {fieldAccess(unionType, "c0"), fieldAccess(unionType, "c1")});

  auto queryCtx = core::QueryCtx::create();
  auto execCtx = std::make_unique<core::ExecCtx>(pool(), queryCtx.get());
  ExprSet exprSet({expr}, execCtx.get());

  for (auto _ : st) {
    SelectivityVector rows(numRows_);
    EvalCtx evalCtx(execCtx.get(), &exprSet, inputRow.get());
    std::vector<VectorPtr> results(1);
    exprSet.eval(rows, evalCtx, results);
    benchmark::DoNotOptimize(results[0]);
  }
  st.SetItemsProcessed(st.iterations() * numRows_);
}

BENCHMARK_DEFINE_F(UnionVsRowFixture, Compare_Row)(benchmark::State& st) {
  auto inputRow = makeRowVector({"c0", "c1"}, {rowVec_, rowVec_});
  auto rowType = rowVec_->type();

  auto c0 = fieldAccess(rowType, "c0");
  auto c1 = fieldAccess(rowType, "c1");

  auto c0Tag = fieldAccess(TINYINT(), c0, "tag");
  auto c1Tag = fieldAccess(TINYINT(), c1, "tag");
  auto c0F0 = fieldAccess(BIGINT(), c0, "f0");
  auto c1F0 = fieldAccess(BIGINT(), c1, "f0");
  auto c0F1 = fieldAccess(DOUBLE(), c0, "f1");
  auto c1F1 = fieldAccess(DOUBLE(), c1, "f1");
  auto c0F2 = fieldAccess(VARCHAR(), c0, "f2");
  auto c1F2 = fieldAccess(VARCHAR(), c1, "f2");

  auto tagEq = call("eq", BOOLEAN(), {c0Tag, c1Tag});
  auto f0Eq = call("eq", BOOLEAN(), {c0F0, c1F0});
  auto f1Eq = call("eq", BOOLEAN(), {c0F1, c1F1});
  auto f2Eq = call("eq", BOOLEAN(), {c0F2, c1F2});

  auto andExpr = call(
    "and", BOOLEAN(),
    {tagEq,
     call("and", BOOLEAN(), {f0Eq, call("and", BOOLEAN(), {f1Eq, f2Eq})})});

  auto queryCtx = core::QueryCtx::create();
  auto execCtx = std::make_unique<core::ExecCtx>(pool(), queryCtx.get());
  ExprSet exprSet({andExpr}, execCtx.get());

  for (auto _ : st) {
    SelectivityVector rows(numRows_);
    EvalCtx evalCtx(execCtx.get(), &exprSet, inputRow.get());
    std::vector<VectorPtr> results(1);
    exprSet.eval(rows, evalCtx, results);
    benchmark::DoNotOptimize(results[0]);
  }
  st.SetItemsProcessed(st.iterations() * numRows_);
}

BENCHMARK_DEFINE_F(UnionVsRowFixture, MemoryFootprint)(benchmark::State& st) {
  auto unionRetained = unionVec_->retainedSize();
  auto unionFlat = unionVec_->estimateFlatSize();
  auto rowRetained = rowVec_->retainedSize();
  auto rowFlat = rowVec_->estimateFlatSize();

  for (auto _ : st) {
    benchmark::DoNotOptimize(unionRetained);
    benchmark::DoNotOptimize(rowRetained);
  }

  st.counters["union_retained_bytes"] = unionRetained;
  st.counters["union_flat_bytes"] = unionFlat;
  st.counters["row_retained_bytes"] = rowRetained;
  st.counters["row_flat_bytes"] = rowFlat;
  st.counters["ratio_retained"] =
    static_cast<double>(rowRetained) / unionRetained;
  st.counters["ratio_flat"] = static_cast<double>(rowFlat) / unionFlat;
}

static const int kSmall = 10'000;
static const int kMedium = 100'000;
static const int kLarge = 1'000'000;

BENCHMARK_REGISTER_F(UnionVsRowFixture, DetectType_Union)
  ->Arg(kSmall)
  ->Arg(kMedium)
  ->Arg(kLarge)
  ->Unit(benchmark::kMicrosecond);
BENCHMARK_REGISTER_F(UnionVsRowFixture, DetectType_Row)
  ->Arg(kSmall)
  ->Arg(kMedium)
  ->Arg(kLarge)
  ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(UnionVsRowFixture, ExtractBigint_Union)
  ->Arg(kSmall)
  ->Arg(kMedium)
  ->Arg(kLarge)
  ->Unit(benchmark::kMicrosecond);
BENCHMARK_REGISTER_F(UnionVsRowFixture, ExtractBigint_Row)
  ->Arg(kSmall)
  ->Arg(kMedium)
  ->Arg(kLarge)
  ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(UnionVsRowFixture, Compare_Union)
  ->Arg(kSmall)
  ->Arg(kMedium)
  ->Arg(kLarge)
  ->Unit(benchmark::kMicrosecond);
BENCHMARK_REGISTER_F(UnionVsRowFixture, Compare_Row)
  ->Arg(kSmall)
  ->Arg(kMedium)
  ->Arg(kLarge)
  ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(UnionVsRowFixture, MemoryFootprint)
  ->Arg(kSmall)
  ->Arg(kMedium)
  ->Arg(kLarge)
  ->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
