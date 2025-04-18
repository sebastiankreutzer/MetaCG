/**
 * File: VersionTwoMCGReader.h
 * License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
 * https://github.com/tudasc/metacg/LICENSE.txt
 */

#ifndef METACG_VERSIONTWOMCGREADER_H
#define METACG_VERSIONTWOMCGREADER_H

#include "MCGReader.h"

/**
 * Class to read metacg files in file format v2.0.
 * The format contains the 'meta' field for tools to export information.
 */

namespace metacg::io {

class VersionTwoMetaCGReader : public metacg::io::MetaCGReader {
 public:
  explicit VersionTwoMetaCGReader(metacg::io::ReaderSource& source) : MetaCGReader(source) {}
  std::unique_ptr<Callgraph> read() override;
  static void upgradeV2FormatToV3Format(nlohmann::json& j);
};

}  // end namespace metacg::io

#endif  // METACG_VERSIONTWOMCGREADER_H