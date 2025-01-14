/**
 * File: MCGManagerTest.cpp
 * License: Part of the metacg project. Licensed under BSD 3 clause license. See LICENSE.txt file at
 * https://github.com/tudasc/metacg/LICENSE.txt
 */

#include "gtest/gtest.h"

#include "../../../pgis/test/unit/LoggerUtil.h"

#include "MCGManager.h"
#include "MetaDataHandler.h"

using namespace pira;

class MCGManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loggerutil::getLogger();
    auto &mcgm = metacg::graph::MCGManager::get();
    mcgm.resetManager();
    mcgm.addToManagedGraphs("emptyGraph",std::make_unique<metacg::Callgraph>());
  }
};

TEST_F(MCGManagerTest, EmptyCG) {

  auto &mcgm = metacg::graph::MCGManager::get();
  ASSERT_EQ(0, mcgm.size());

  auto graph = *mcgm.getCallgraph();

  ASSERT_TRUE(graph.isEmpty());
  ASSERT_EQ(false, graph.hasNode("main"));
  ASSERT_EQ(nullptr, graph.getMain());
  ASSERT_EQ(0, graph.size());
}

TEST_F(MCGManagerTest, OneNodeCG) {
  auto &mcgm = metacg::graph::MCGManager::get();
  mcgm.findOrCreateNode("main");
  auto nPtr = mcgm.findOrCreateNode("main");
  auto graph = *mcgm.getCallgraph();
  ASSERT_FALSE(graph.isEmpty());
  ASSERT_NE(nullptr, graph.getMain());
  ASSERT_EQ(nPtr, graph.getMain());
}

TEST_F(MCGManagerTest, TwoNodeCG) {
  auto &mcgm = metacg::graph::MCGManager::get();
  auto mainNode = mcgm.findOrCreateNode("main");
  auto childNode = mcgm.findOrCreateNode("child1");
  mcgm.addEdge("main", "child1");
  ASSERT_EQ(mainNode, mcgm.findOrCreateNode("main"));
  ASSERT_EQ(childNode, mcgm.findOrCreateNode("child1"));
  auto graph = *mcgm.getCallgraph();
  ASSERT_EQ(mainNode, graph.getMain());
  ASSERT_EQ(childNode, graph.getNode("child1"));
}

TEST_F(MCGManagerTest, ThreeNodeCG) {
  auto &mcgm = metacg::graph::MCGManager::get();
  auto mainNode = mcgm.findOrCreateNode("main");
  auto childNode = mcgm.findOrCreateNode("child1");
  mcgm.addEdge("main", "child1");
  ASSERT_EQ(mainNode, mcgm.findOrCreateNode("main"));
  ASSERT_EQ(childNode, mcgm.findOrCreateNode("child1"));
  auto childNode2 = mcgm.findOrCreateNode("child2");
  mcgm.addEdge("main", "child2");
  ASSERT_EQ(2, mainNode->getChildNodes().size());
  ASSERT_EQ(1, childNode->getParentNodes().size());
  ASSERT_EQ(1, childNode2->getParentNodes().size());
}

TEST_F(MCGManagerTest, OneMetaDataAttached) {
  auto &mcgm = metacg::graph::MCGManager::get();
  mcgm.addMetaHandler<metacg::io::retriever::TestHandler>();
  const auto &handlers = mcgm.getMetaHandlers();
  ASSERT_EQ(handlers.size(), 1);
}