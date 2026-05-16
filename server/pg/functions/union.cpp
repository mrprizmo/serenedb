#include "pg/functions/union.h"

#include <absl/container/flat_hash_map.h>
#include <fmt/format.h>
#include <folly/String.h>
#include <velox/expression/EvalCtx.h>
#include <velox/expression/FunctionSignature.h>
#include <velox/expression/VectorFunction.h>
#include <velox/functions/prestosql/types/JsonType.h>
#include <velox/functions/prestosql/types/TimestampWithTimeZoneType.h>
#include <velox/functions/prestosql/types/UuidType.h>
#include <velox/vector/ComplexVector.h>
#include <velox/vector/DecodedVector.h>
#include <velox/vector/FlatVector.h>

#include "query/types.h"

namespace sdb::pg::functions {
namespace {

using namespace facebook;

std::string normalizeTypeSelector(std::string_view selector) {
  auto s = std::string(folly::trimWhitespace(
    folly::StringPiece(selector.data(), selector.size())));
  folly::toLowerAscii(s);
  return s;
}

std::string canonicalizeScalarSelector(std::string_view selector) {
  static const absl::flat_hash_map<std::string_view, std::string_view> kAliases{
    {"boolean", "bool"},
    {"bpchar", "char"},
    {"int2", "smallint"},
    {"int4", "integer"},
    {"int", "integer"},
    {"int8", "bigint"},
    {"int16", "hugeint"},
    {"float4", "real"},
    {"float8", "double precision"},
    {"double", "double precision"},
    {"float", "real"},
    {"varchar", "text"},
    {"character varying", "text"},
    {"varbinary", "bytea"},
    {"decimal", "numeric"},
    {"timestamp with time zone", "timestamptz"},
    {"cidr", "ipprefix"},
  };

  if (const auto it = kAliases.find(selector); it != kAliases.end()) {
    return std::string(it->second);
  }
  return std::string(selector);
}

std::string canonicalizeTypeSelector(std::string_view selector) {
  auto normalized = normalizeTypeSelector(selector);

  uint32_t arrayDepth = 0;
  while (normalized.size() >= 2 &&
         normalized.compare(normalized.size() - 2, 2, "[]") == 0) {
    normalized.resize(normalized.size() - 2);
    normalized = normalizeTypeSelector(normalized);
    ++arrayDepth;
  }

  auto canonical = canonicalizeScalarSelector(normalized);
  while (arrayDepth-- > 0) {
    canonical += "[]";
  }
  return canonical;
}

std::string veloxTypeToSdbSqlName(const velox::Type& type) {
  if (sdb::pg::IsInterval(type))
    return "interval";
  if (sdb::pg::IsPgUnknown(type))
    return "unknown";
  if (type.isDate())
    return "date";

  const auto typePtr = std::shared_ptr<const velox::Type>(&type, [](auto*) {});
  if (velox::isJsonType(typePtr))
    return "json";
  if (velox::isUuidType(typePtr))
    return "uuid";
  if (velox::isTimestampWithTimeZoneType(typePtr))
    return "timestamptz";
  if (sdb::pg::IsVoid(typePtr))
    return "void";

  if (type.isDecimal()) {
    const auto [p, s] =
      type.isShortDecimal()
        ? std::make_pair((int)type.asShortDecimal().precision(),
                         (int)type.asShortDecimal().scale())
        : std::make_pair((int)type.asLongDecimal().precision(),
                         (int)type.asLongDecimal().scale());
    return fmt::format("numeric({},{})", p, s);
  }

  switch (type.kind()) {
    case velox::TypeKind::BOOLEAN:
      return "bool";
    case velox::TypeKind::TINYINT:
      return "char";
    case velox::TypeKind::SMALLINT:
      return "smallint";
    case velox::TypeKind::INTEGER:
      return "integer";
    case velox::TypeKind::BIGINT:
      return "bigint";
    case velox::TypeKind::HUGEINT:
      return "hugeint";
    case velox::TypeKind::REAL:
      return "real";
    case velox::TypeKind::DOUBLE:
      return "double precision";
    case velox::TypeKind::VARCHAR:
      return "text";
    case velox::TypeKind::VARBINARY:
      return "bytea";
    case velox::TypeKind::TIMESTAMP:
      return "timestamp";
    case velox::TypeKind::ARRAY: {
      return veloxTypeToSdbSqlName(
               *type.as<velox::TypeKind::ARRAY>().elementType()) +
             "[]";
    }
    default:
      VELOX_FAIL("No SDB SQL type-name mapping for Velox type {}",
                 type.toString());
  }
}

std::vector<std::string> makeTypeNames(const velox::UnionType& unionType) {
  std::vector<std::string> names;
  names.reserve(unionType.size());
  for (const auto& childType : unionType.children()) {
    names.push_back(veloxTypeToSdbSqlName(*childType));
  }
  return names;
}

uint8_t resolveUnionSelectorToTag(const velox::UnionType& unionType,
                                  std::string_view selector) {
  auto sel = canonicalizeTypeSelector(selector);
  VELOX_USER_CHECK(!sel.empty(),
                   "union_extract: type name selector must not be empty");

  for (uint32_t i = 0; i < unionType.size(); ++i) {
    const auto& child = *unionType.childAt(i);
    if (veloxTypeToSdbSqlName(child) == sel) {
      return static_cast<uint8_t>(i);
    }

    if (sel == "numeric" && child.isDecimal()) {
      return static_cast<uint8_t>(i);
    }
  }

  VELOX_USER_FAIL("union_extract: no variant named '{}' in {}", selector,
                  unionType.toString());
}

}  // namespace

class UnionTypeNameFunction : public velox::exec::VectorFunction {
 public:
  explicit UnionTypeNameFunction(std::vector<std::string> names)
    : typeNamesByTag_{std::move(names)} {}

  void apply(const velox::SelectivityVector& rows,
             std::vector<velox::VectorPtr>& args,
             const velox::TypePtr& /*outputType*/,
             velox::exec::EvalCtx& context,
             velox::VectorPtr& result) const override {
    VELOX_CHECK_EQ(args.size(), 1);
    auto& arg = args[0];

    auto baseNames = velox::BaseVector::create(
      velox::VARCHAR(), typeNamesByTag_.size(), context.pool());
    auto* flat = baseNames->asFlatVector<velox::StringView>();
    for (uint32_t tag = 0; tag < typeNamesByTag_.size(); ++tag) {
      flat->set(tag, velox::StringView(typeNamesByTag_[tag]));
    }

    if (arg->isConstantEncoding()) {
      auto* constVec = arg->wrappedVector()->as<velox::UnionVector>();
      VELOX_CHECK_NOT_NULL(constVec);
      auto constRow = arg->wrappedIndex(rows.begin());
      auto tag = constVec->tagAt(constRow);
      auto localResult =
        velox::BaseVector::wrapInConstant(rows.end(), tag, baseNames);
      context.moveOrCopyResult(localResult, rows, result);
      return;
    }

    auto* unionVec = arg->as<velox::UnionVector>();
    VELOX_CHECK_NOT_NULL(unionVec);

    auto indices = velox::AlignedBuffer::allocate<velox::vector_size_t>(
      rows.end(), context.pool(), 0);
    auto* rawIndices = indices->asMutable<velox::vector_size_t>();

    rows.applyToSelected([&](velox::vector_size_t row) {
      rawIndices[row] = unionVec->tagAt(row);
    });

    auto localResult = velox::BaseVector::wrapInDictionary(
      nullptr, indices, rows.end(), std::move(baseNames));
    context.moveOrCopyResult(localResult, rows, result);
  }

  static std::vector<std::shared_ptr<velox::exec::FunctionSignature>>
  signatures() {
    return {velox::exec::FunctionSignatureBuilder()
              .knownTypeVariable("U")
              .returnType("varchar")
              .argumentType("U")
              .build()};
  }

  static std::shared_ptr<velox::exec::VectorFunction> create(
    const std::string& /*name*/,
    const std::vector<velox::exec::VectorFunctionArg>& inputArgs,
    const velox::core::QueryConfig& /*config*/) {
    VELOX_CHECK_EQ(inputArgs.size(), 1);
    VELOX_USER_CHECK(inputArgs[0].type->isUnion(),
                     "union_type: argument must be a union column, got {}",
                     inputArgs[0].type->toString());
    return std::make_shared<UnionTypeNameFunction>(
      makeTypeNames(inputArgs[0].type->asUnion()));
  }

 private:
  const std::vector<std::string> typeNamesByTag_;
};

class UnionExtractFunction : public velox::exec::VectorFunction {
 public:
  explicit UnionExtractFunction(uint8_t targetTag) : targetTag_{targetTag} {}

  void apply(const velox::SelectivityVector& rows,
             std::vector<velox::VectorPtr>& args,
             const velox::TypePtr& outputType, velox::exec::EvalCtx& context,
             velox::VectorPtr& result) const override {
    VELOX_CHECK_EQ(args.size(), 3);
    context.ensureWritable(rows, outputType, result);

    velox::exec::LocalDecodedVector decoded(context, *args[0], rows);
    auto* unionVec = decoded->base()->as<velox::UnionVector>();
    VELOX_CHECK_NOT_NULL(unionVec);

    velox::SelectivityVector defaultRows(rows.end(), false);
    velox::SelectivityVector matchingRows(rows.end(), false);

    auto sourceRowsBuf = velox::AlignedBuffer::allocate<velox::vector_size_t>(
      rows.end(), context.pool());
    auto* sourceRows = sourceRowsBuf->asMutable<velox::vector_size_t>();

    rows.applyToSelected([&](velox::vector_size_t row) {
      if (decoded->isNullAt(row)) {
        defaultRows.setValid(row, true);
        return;
      }

      auto baseRow = decoded->index(row);
      bool useDefault =
        unionVec->isNullAt(baseRow) || unionVec->tagAt(baseRow) != targetTag_;

      if (useDefault) {
        defaultRows.setValid(row, true);
      } else {
        matchingRows.setValid(row, true);
        sourceRows[row] = unionVec->offsetAt(baseRow);
      }
    });

    defaultRows.updateBounds();
    matchingRows.updateBounds();

    if (defaultRows.hasSelections()) {
      result->copy(args[2].get(), defaultRows, nullptr);
    }
    if (matchingRows.hasSelections()) {
      auto* childVec = unionVec->childAt(targetTag_).get();
      VELOX_CHECK_NOT_NULL(childVec);
      result->copy(childVec, matchingRows, sourceRows);
    }
  }

  static std::vector<std::shared_ptr<velox::exec::FunctionSignature>>
  signatures() {
    return {velox::exec::FunctionSignatureBuilder()
              .knownTypeVariable("U")
              .typeVariable("R")
              .returnType("R")
              .argumentType("U")
              .constantArgumentType("varchar")
              .argumentType("R")
              .build()};
  }

  static std::shared_ptr<velox::exec::VectorFunction> create(
    const std::string& /*name*/,
    const std::vector<velox::exec::VectorFunctionArg>& inputArgs,
    const velox::core::QueryConfig& /*config*/) {
    VELOX_CHECK_EQ(inputArgs.size(), 3);
    VELOX_USER_CHECK(
      inputArgs[0].type->isUnion(),
      "union_extract: first argument must be a union column, got {}",
      inputArgs[0].type->toString());

    VELOX_USER_CHECK_NOT_NULL(
      inputArgs[1].constantValue,
      "union_extract: type selector must be a constant varchar");
    auto* selectorVec =
      inputArgs[1].constantValue->as<velox::SimpleVector<velox::StringView>>();
    VELOX_USER_CHECK_NOT_NULL(selectorVec);

    VELOX_USER_CHECK(!selectorVec->isNullAt(0),
                     "union_extract: type selector must not be NULL");

    const auto& unionType = inputArgs[0].type->asUnion();
    auto selector = selectorVec->valueAt(0);
    auto tag = resolveUnionSelectorToTag(
      unionType, std::string_view{selector.data(), selector.size()});

    const auto& childType = unionType.childAt(tag);
    VELOX_USER_CHECK(
      inputArgs[2].type->equivalent(*childType),
      "union_extract: default type {} doesn't match variant type {}",
      inputArgs[2].type->toString(), childType->toString());

    return std::make_shared<UnionExtractFunction>(tag);
  }

 private:
  const uint8_t targetTag_;
};

void RegisterUnionFunctions(const std::string& prefix) {
  velox::exec::registerStatefulVectorFunction(
    prefix + "union_type", UnionTypeNameFunction::signatures(),
    UnionTypeNameFunction::create);

  velox::exec::registerStatefulVectorFunction(
    prefix + "union_extract", UnionExtractFunction::signatures(),
    UnionExtractFunction::create,
    velox::exec::VectorFunctionMetadataBuilder()
      .defaultNullBehavior(false)
      .build());
}

}  // namespace sdb::pg::functions
