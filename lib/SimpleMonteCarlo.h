#pragma once

#include "lib/Strategy.h"
#include "lib/Annotator.h"

class KnowableState;

class SimpleMonteCarlo : public Strategy
{
public:
  virtual ~SimpleMonteCarlo();

  SimpleMonteCarlo(const StrategyPtr& intuition, const AnnotatorPtr& annotator=0, uint128_t maxAlternates=1000);

  virtual Card choosePlay(const KnowableState& state) const;

private:
  StrategyPtr mIntuition;
  const uint128_t kMaxAlternates;
};
