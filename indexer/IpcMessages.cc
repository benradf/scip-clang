#include <compare>
#include <type_traits>

#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"

#include "llvm/Support/JSON.h"

#include "indexer/Comparison.h"
#include "indexer/Derive.h"
#include "indexer/IpcMessages.h"

namespace clang::tooling {

llvm::json::Value toJSON(const clang::tooling::CompileCommand &cc) {
  return llvm::json::Object{{"directory", cc.Directory},
                            {"file", cc.Filename},
                            {"output", cc.Output},
                            {"arguments", cc.CommandLine}};
}

bool fromJSON(const llvm::json::Value &jsonValue,
              clang::tooling::CompileCommand &cc, llvm::json::Path path) {
  llvm::json::ObjectMapper mapper(jsonValue, path);
  return mapper && mapper.map("directory", cc.Directory)
         && mapper.map("file", cc.Filename)
         && mapper.mapOptional("output", cc.Output)
         && mapper.map("arguments", cc.CommandLine);
}

} // namespace clang::tooling

namespace scip_clang {

std::string driverToWorkerQueueName(std::string_view driverId,
                                    WorkerId workerId) {
  return fmt::format("scip-clang-{}-worker-{}-recv", driverId, workerId);
}

std::string workerToDriverQueueName(std::string_view driverId) {
  return fmt::format("scip-clang-{}-worker-send", driverId);
}

llvm::json::Value JobId::toJSON(const JobId &jobId) {
  return llvm::json::Value(jobId.to64Bit());
}
llvm::json::Value toJSON(const JobId &jobId) {
  return JobId::toJSON(jobId);
}
bool JobId::fromJSON(const llvm::json::Value &value, JobId &jobId,
                     llvm::json::Path path) {
  if (auto uint = value.getAsUINT64()) {
    jobId = JobId::from64Bit(uint.value());
    return true;
  }
  path.report("expected uint64_t for job");
  return false;
}
bool fromJSON(const llvm::json::Value &value, JobId &jobId,
              llvm::json::Path path) {
  return JobId::fromJSON(value, jobId, path);
}

std::string JobId::debugString() const {
  return fmt::format("{}.{}", this->_taskId, this->subtaskId);
}

llvm::json::Value toJSON(const IndexJob::Kind &kind) {
  switch (kind) {
  case IndexJob::Kind::SemanticAnalysis:
    return llvm::json::Value("SemanticAnalysis");
  case IndexJob::Kind::EmitIndex:
    return llvm::json::Value("EmitIndex");
  }
}

bool fromJSON(const llvm::json::Value &jsonValue, IndexJob::Kind &t,
              llvm::json::Path path) {
  if (auto s = jsonValue.getAsString()) {
    if (s.value() == "SemanticAnalysis") {
      t = IndexJob::Kind::SemanticAnalysis;
      return true;
    } else if (s.value() == "EmitIndex") {
      t = IndexJob::Kind::EmitIndex;
      return true;
    }
  }
  path.report("expected SemanticAnalysis or EmitIndex for IndexJob::Kind");
  return false;
}

template <typename IJ> llvm::json::Value toJSONIndexJob(const IJ &job) {
  llvm::json::Value details("");
  switch (job.kind) {
  case IndexJob::Kind::SemanticAnalysis:
    details = toJSON(job.semanticAnalysis);
    break;
  case IndexJob::Kind::EmitIndex:
    details = toJSON(job.emitIndex);
    break;
  }
  return llvm::json::Object{{"kind", toJSON(job.kind)}, {"details", details}};
}
template <typename IJ>
bool fromJSONIndexJob(const llvm::json::Value &jsonValue, IJ &t,
                      llvm::json::Path path) {
  llvm::json::ObjectMapper mapper(jsonValue, path);
  bool ret = mapper && mapper.map("kind", t.kind);
  if (ret) {
    switch (t.kind) {
    case IndexJob::Kind::SemanticAnalysis:
      ret = mapper.map("details", t.semanticAnalysis);
      break;
    case IndexJob::Kind::EmitIndex:
      ret = mapper.map("details", t.emitIndex);
      break;
    }
  }
  return ret;
}

llvm::json::Value toJSON(const IndexJob &job) {
  return toJSONIndexJob(job);
}
bool fromJSON(const llvm::json::Value &jsonValue, IndexJob &job,
              llvm::json::Path path) {
  return fromJSONIndexJob(jsonValue, job, path);
}
llvm::json::Value toJSON(const IndexJobResult &job) {
  return toJSONIndexJob(job);
}
bool fromJSON(const llvm::json::Value &jsonValue, IndexJobResult &job,
              llvm::json::Path path) {
  return fromJSONIndexJob(jsonValue, job, path);
}

llvm::json::Value toJSON(const HashValue &h) {
  return llvm::json::Value(h.rawValue);
}
bool fromJSON(const llvm::json::Value &jsonValue, HashValue &h,
              llvm::json::Path path) {
  if (auto v = jsonValue.getAsUINT64()) {
    h.rawValue = v.value();
    return true;
  }
  path.report("expected uint64_t for HashValue");
  return false;
}

DERIVE_SERIALIZE_1_NEWTYPE(scip_clang::IndexingStatistics, totalTimeMicros)
DERIVE_SERIALIZE_1_NEWTYPE(scip_clang::EmitIndexJobDetails, filesToBeIndexed)
DERIVE_SERIALIZE_1_NEWTYPE(scip_clang::IpcTestMessage, content)
DERIVE_SERIALIZE_1_NEWTYPE(scip_clang::SemanticAnalysisJobDetails, command)

DERIVE_SERIALIZE_2(scip_clang::ShardPaths, docsAndExternals, forwardDecls)
DERIVE_SERIALIZE_2(scip_clang::EmitIndexJobResult, statistics, shardPaths)
DERIVE_SERIALIZE_2(scip_clang::PreprocessedFileInfo, path, hashValue)
DERIVE_SERIALIZE_2(scip_clang::PreprocessedFileInfoMulti, path, hashValues)
DERIVE_SERIALIZE_2(scip_clang::IndexJobRequest, id, job)
DERIVE_SERIALIZE_2(scip_clang::SemanticAnalysisJobResult, wellBehavedFiles,
                   illBehavedFiles)

std::strong_ordering operator<=>(const PreprocessedFileInfo &lhs,
                                 const PreprocessedFileInfo &rhs) {
  CMP_EXPR(lhs.hashValue, rhs.hashValue);
  CMP_EXPR(lhs.path, rhs.path);
  return std::strong_ordering::equal;
}

std::strong_ordering operator<=>(const PreprocessedFileInfoMulti &lhs,
                                 const PreprocessedFileInfoMulti &rhs) {
  CMP_EXPR(lhs.path, rhs.path);
  CMP_RANGE(lhs.hashValues, rhs.hashValues);
  return std::strong_ordering::equal;
}

llvm::json::Value toJSON(const IndexJobResponse &r) {
  return llvm::json::Object{
      {"workerId", r.workerId}, {"jobId", r.jobId}, {"result", r.result}};
}

bool fromJSON(const llvm::json::Value &value, IndexJobResponse &r,
              llvm::json::Path path) {
  llvm::json::ObjectMapper mapper(value, path);
  return mapper && mapper.map("workerId", r.workerId)
         && mapper.map("jobId", r.jobId) && mapper.map("result", r.result);
}

} // namespace scip_clang
