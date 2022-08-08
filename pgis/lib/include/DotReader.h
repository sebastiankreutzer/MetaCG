/**
 * File: DotReader.h
 * License: Part of the metacg project. Licensed under BSD 3 clause license. See LICENSE.txt file at
 * https://github.com/tudasc/metacg/LICENSE.txt
 */

#include "../../../graph/include/Callgraph.h"
#include "MCGManager.h"

#include <fstream>
#include <string>

#ifndef DOTREADER_H_
#define DOTREADER_H_

/**
 * A not so robust reader for dot-files
 * \author roman
 */
namespace DOTCallgraphBuilder {

std::string extractBetween(const std::string &s, const std::string &pattern, size_t &start) {
  size_t first = s.find(pattern, start) + pattern.size();
  size_t second = s.find(pattern, first);

  start = second + pattern.size();
  return s.substr(first, second - first);
}

void build(std::string filePath, Config *c) {
  // PiraMCGProcessor *cg = new PiraMCGProcessor(c);
  auto &cg = metacg::graph::MCGManager::get();

  std::ifstream file(filePath);
  std::string line;

  while (std::getline(file, line)) {
    if (line.find('"') != 0) {  // does not start with "
      continue;
    }

    std::cout << line << std::endl;

    if (line.find("->") != std::string::npos) {
      if (line.find("dotted") != std::string::npos) {
        continue;
      }

      // edge
      size_t start = 0;
      std::string parent = extractBetween(line, "\"", start);
      std::string child = extractBetween(line, "\"", start);

      size_t numCallsStart = line.find("label=") + 6;
      unsigned long numCalls = stoul(line.substr(numCallsStart));

      // filename & line unknown; time already added with node
      //      cg.putEdge(parent, "", -1, child, numCalls, 0.0, 0, 0);
      cg.addEdge(parent, child);

    } else {
      // node
      size_t start = 0;
      std::string name = extractBetween(line, "\"", start);
      double time = stod(extractBetween(line, "\\n", start));

      cg.findOrCreateNode(name);
    }
  }
  file.close();
}
};  // namespace DOTCallgraphBuilder

#endif
