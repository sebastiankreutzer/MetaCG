/**
 * File: VersionTwoMCGWriter.h
 * License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
 * https://github.com/tudasc/metacg/LICENSE.txt
 */

#ifndef METACG_VERSIONTWOMCGWRITER_H
#define METACG_VERSIONTWOMCGWRITER_H

#include "MCGWriter.h"

namespace metacg::io {

class VersionTwoMCGWriter : public MCGWriter {
 public:
  explicit VersionTwoMCGWriter(
      metacg::MCGFileInfo fileInfo = metacg::getVersionTwoFileInfo(metacg::getCGCollectorGeneratorInfo()))
      : MCGWriter(std::move(fileInfo)) {}

  void write(const Callgraph* graph, JsonSink& js) override;

  static void downgradeV3FormatToV2Format(nlohmann::json& cg);
};
}  // namespace metacg::io

#endif  // METACG_VERSIONTWOMCGWRITER_H
