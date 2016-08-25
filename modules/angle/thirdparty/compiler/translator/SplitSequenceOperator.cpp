//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SplitSequenceOperator is an AST traverser that detects sequence operator expressions that
// go through further AST transformations that generate statements, and splits them so that
// possible side effects of earlier parts of the sequence operator expression are guaranteed to be
// evaluated before the latter parts of the sequence operator expression are evaluated.
//

#include "compiler/translator/SplitSequenceOperator.h"

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/IntermNodePatternMatcher.h"

namespace
{

class SplitSequenceOperatorTraverser : public TLValueTrackingTraverser
{
  public:
    SplitSequenceOperatorTraverser(unsigned int patternsToSplitMask,
                                   const TSymbolTable &symbolTable,
                                   int shaderVersion);

    bool visitBinary(Visit visit, TIntermBinary *node) override;
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    bool visitSelection(Visit visit, TIntermSelection *node) override;

    void nextIteration();
    bool foundExpressionToSplit() const { return mFoundExpressionToSplit; }

  protected:
    // Marked to true once an operation that needs to be hoisted out of the expression has been
    // found. After that, no more AST updates are performed on that traversal.
    bool mFoundExpressionToSplit;
    int mInsideSequenceOperator;

    IntermNodePatternMatcher mPatternToSplitMatcher;
};

SplitSequenceOperatorTraverser::SplitSequenceOperatorTraverser(unsigned int patternsToSplitMask,
                                                               const TSymbolTable &symbolTable,
                                                               int shaderVersion)
    : TLValueTrackingTraverser(true, false, true, symbolTable, shaderVersion),
      mFoundExpressionToSplit(false),
      mInsideSequenceOperator(0),
      mPatternToSplitMatcher(patternsToSplitMask)
{
}

void SplitSequenceOperatorTraverser::nextIteration()
{
    mFoundExpressionToSplit = false;
    mInsideSequenceOperator = 0;
    nextTemporaryIndex();
}

bool SplitSequenceOperatorTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    if (mFoundExpressionToSplit)
        return false;

    if (mInsideSequenceOperator > 0 && visit == PreVisit)
    {
        // Detect expressions that need to be simplified
        mFoundExpressionToSplit =
            mPatternToSplitMatcher.match(node, getParentNode(), isLValueRequiredHere());
        return !mFoundExpressionToSplit;
    }

    return true;
}

bool SplitSequenceOperatorTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    if (node->getOp() == EOpComma)
    {
        if (visit == PreVisit)
        {
            if (mFoundExpressionToSplit)
            {
                return false;
            }
            mInsideSequenceOperator++;
        }
        else if (visit == PostVisit)
        {
            if (mFoundExpressionToSplit)
            {
                // Move all operands of the sequence operation except the last one into separate
                // statements in the parent block.
                TIntermSequence insertions;
                for (auto *sequenceChild : *node->getSequence())
                {
                    if (sequenceChild != node->getSequence()->back())
                    {
                        insertions.push_back(sequenceChild);
                    }
                }
                insertStatementsInParentBlock(insertions);
                // Replace the sequence with its last operand
                queueReplacement(node, node->getSequence()->back(), OriginalNode::IS_DROPPED);
            }
            mInsideSequenceOperator--;
        }
        return true;
    }

    if (mFoundExpressionToSplit)
        return false;

    if (mInsideSequenceOperator > 0 && visit == PreVisit)
    {
        // Detect expressions that need to be simplified
        mFoundExpressionToSplit = mPatternToSplitMatcher.match(node, getParentNode());
        return !mFoundExpressionToSplit;
    }

    return true;
}

bool SplitSequenceOperatorTraverser::visitSelection(Visit visit, TIntermSelection *node)
{
    if (mFoundExpressionToSplit)
        return false;

    if (mInsideSequenceOperator > 0 && visit == PreVisit)
    {
        // Detect expressions that need to be simplified
        mFoundExpressionToSplit = mPatternToSplitMatcher.match(node);
        return !mFoundExpressionToSplit;
    }

    return true;
}

}  // namespace

void SplitSequenceOperator(TIntermNode *root,
                           int patternsToSplitMask,
                           unsigned int *temporaryIndex,
                           const TSymbolTable &symbolTable,
                           int shaderVersion)
{
    SplitSequenceOperatorTraverser traverser(patternsToSplitMask, symbolTable, shaderVersion);
    ASSERT(temporaryIndex != nullptr);
    traverser.useTemporaryIndex(temporaryIndex);
    // Separate one expression at a time, and reset the traverser between iterations.
    do
    {
        traverser.nextIteration();
        root->traverse(&traverser);
        if (traverser.foundExpressionToSplit())
            traverser.updateTree();
    } while (traverser.foundExpressionToSplit());
}
