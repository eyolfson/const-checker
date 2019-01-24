#ifndef CLANG_IMMUTABILITY_CHECK_WORKLIST_H
#define CLANG_IMMUTABILITY_CHECK_WORKLIST_H

#include <deque>
#include <clang/Analysis/CFG.h>
#include <llvm/ADT/BitVector.h>

namespace clang {
namespace immutability {

class Worklist {
  llvm::BitVector enqueuedBlocks;
  std::deque<const CFGBlock *> blocks;
public:
  Worklist(const CFG &cfg)
  : enqueuedBlocks(cfg.getNumBlockIDs()) {}
  void enqueueBlock(const CFGBlock *block) {
    if (!block) {
      return;
    }
    auto id = block->getBlockID();
    if (!enqueuedBlocks[id]) {
      enqueuedBlocks[id] = true;
      blocks.push_back(block);
    }
  }
  void enqueueSuccessors(const CFGBlock *block) {
    for (CFGBlock::const_succ_iterator I = block->succ_begin(),
         E = block->succ_end(); I != E; ++I) {
      enqueueBlock(*I);
    }
  }
  const CFGBlock *dequeue() {
    if (blocks.empty()) {
      return nullptr;
    }
    const CFGBlock *block = blocks.front();
    blocks.pop_front();
    enqueuedBlocks[block->getBlockID()] = false;
    return block;
  }
};

}
}

#endif
