#ifndef SCIP_CLANG_SCIP_EXTRAS_H
#define SCIP_CLANG_SCIP_EXTRAS_H

#include <compare>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "spdlog/fmt/fmt.h"

#include "scip/scip.pb.h"

#include "llvm/ADT/PointerUnion.h"

#include "indexer/Comparison.h"
#include "indexer/Derive.h"
#include "indexer/Enforce.h"
#include "indexer/RAII.h"

namespace scip {

std::strong_ordering compareRelationships(const scip::Relationship &lhs,
                                          const scip::Relationship &rhs);

struct RelationshipExt {
  scip::Relationship rel;

  friend std::strong_ordering operator<=>(const RelationshipExt &lhs,
                                          const RelationshipExt &rhs);
  DERIVE_EQ_VIA_CMP(RelationshipExt) // absl::flat_hash_set needs explicit ==

  template <typename H>
  friend H AbslHashValue(H h, const RelationshipExt &self) {
    auto &r = self.rel;
    return H::combine(std::move(h), r.symbol(), r.is_definition(),
                      r.is_reference(), r.is_type_definition(),
                      r.is_implementation());
  }
};

std::strong_ordering
compareScipRange(const google::protobuf::RepeatedField<int32_t> &lhs,
                 const google::protobuf::RepeatedField<int32_t> &rhs);

std::strong_ordering compareOccurrences(const scip::Occurrence &lhs,
                                        const scip::Occurrence &rhs);

struct OccurrenceExt {
  scip::Occurrence occ;

  friend std::strong_ordering operator<=>(const OccurrenceExt &lhs,
                                          const OccurrenceExt &rhs);
  DERIVE_EQ_VIA_CMP(OccurrenceExt) // absl::flat_hash_set needs ==

  template <typename H> friend H AbslHashValue(H h, const OccurrenceExt &self) {
    for (auto &i : self.occ.range()) {
      h = H::combine(std::move(h), i);
    }
    h = H::combine(std::move(h), self.occ.symbol());
    h = H::combine(std::move(h), self.occ.symbol_roles());
    for (auto &d : self.occ.override_documentation()) {
      h = H::combine(std::move(h), d);
    }
    h = H::combine(std::move(h), self.occ.syntax_kind());
    for (auto &d : self.occ.diagnostics()) {
      h = H::combine(std::move(h), d.severity(), d.code(), d.message(),
                     d.source());
      for (auto &t : d.tags()) {
        h = H::combine(std::move(h), t);
      }
    }
    return h;
  }
};

class SymbolInformationBuilder final {
  std::vector<std::string> documentation;
  absl::flat_hash_set<RelationshipExt> relationships;
  scip_clang::Bomb _bomb;

public:
  template <typename C1, typename C2>
  SymbolInformationBuilder(std::string_view name, C1 &&docs, C2 &&rels)
      : documentation(), relationships(),
        _bomb(
            BOMB_INIT(fmt::format("SymbolInformationBuilder for '{}'", name))) {
    (void)name;
    this->setDocumentation(std::move(docs));
    this->mergeRelationships(std::move(rels));
  }
  bool hasDocumentation() const {
    return !this->documentation.empty();
  }
  template <typename C> void setDocumentation(C &&newDocumentation) {
    ENFORCE(!this->hasDocumentation());
    absl::c_move(std::move(newDocumentation),
                 std::back_inserter(this->documentation));
  };
  template <typename C> void mergeRelationships(C &&newRelationships) {
    for (auto &rel : newRelationships) {
      this->relationships.insert({std::move(rel)});
    }
  }
  void discard() {
    this->_bomb.defuse();
  }
  void finish(bool deterministic, scip::SymbolInformation &out);
};

class SymbolName {
  std::string value;

  // The implicitly synthesized copy constructor is important as this is
  // used a map key, which are required to be copy-constructible.
public:
  SymbolName(std::string &&value) : value(std::move(value)) {
    ENFORCE(!this->value.empty());
  }
  const std::string &asStringRef() const {
    return this->value;
  }
  std::string &asStringRefMut() {
    return this->value;
  }
  DERIVE_HASH_CMP_NEWTYPE(SymbolName, value, CMP_STR)
};

using SymbolToInfoMap = absl::flat_hash_map<
    std::string_view,
    llvm::PointerUnion<SymbolInformation *, SymbolInformationBuilder *>>;

class DocumentBuilder final {
  scip::Document soFar;
  scip_clang::Bomb _bomb;

  absl::flat_hash_set<OccurrenceExt> occurrences;

  // Keyed by the symbol name. The SymbolInformationBuilder value
  // doesn't carry the name to avoid redundant allocations.
  absl::flat_hash_map<SymbolName, SymbolInformationBuilder> symbolInfos;

public:
  DocumentBuilder(scip::Document &&document);
  void merge(scip::Document &&doc);
  void populateSymbolToInfoMap(SymbolToInfoMap &);
  void finish(bool deterministic, scip::Document &out);
};

// This type is currently in ScipExtras.h instead of Path.h because this
// type currently only needs to be used in IndexBuilder.
class RootRelativePath {
  std::string value; // non-empty, but allow default constructor for avoiding
                     // PITA as a hashmap key
public:
  RootRelativePath(std::string &&value);

  DERIVE_HASH_CMP_NEWTYPE(RootRelativePath, value, CMP_STR)
};

class IndexBuilder final {
  scip::Index &fullIndex;
  // The key is deliberately the path only, not the path+hash, so that we can
  // aggregate information across different hashes into a single Document.
  absl::flat_hash_map<RootRelativePath, std::unique_ptr<DocumentBuilder>>
      multiplyIndexed;
  absl::flat_hash_map<SymbolName, std::unique_ptr<SymbolInformationBuilder>>
      externalSymbols;

  scip_clang::Bomb _bomb;

public:
  IndexBuilder(scip::Index &fullIndex);
  void addDocument(scip::Document &&doc, bool isMultiplyIndexed);
  void addExternalSymbol(scip::SymbolInformation &&extSym);

  // The map contains interior references into IndexBuilder's state.
  std::unique_ptr<SymbolToInfoMap> populateSymbolToInfoMap();
  void addForwardDeclaration(const SymbolToInfoMap &,
                             scip::SymbolInformation &&forwardDeclSym);

  void finish(bool deterministic);

private:
  void addExternalSymbolUnchecked(SymbolName &&,
                                  scip::SymbolInformation &&symWithoutName);
};

} // namespace scip

#endif // SCIP_CLANG_SCIP_EXTRAS_H
