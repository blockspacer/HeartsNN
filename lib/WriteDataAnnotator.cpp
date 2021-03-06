// lib/WriteDataAnnotator.cpp

#include "lib/WriteDataAnnotator.h"
#include "lib/GameState.h"
#include "lib/KnowableState.h"
#include "lib/PossibilityAnalyzer.h"
#include "lib/random.h"

#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>

WriteDataAnnotator::~WriteDataAnnotator()
{
  if (!mValidateMode) {
    for (int i=1; i<48; i++) {
      fclose(mFiles[i]);
    }
  }
}

WriteDataAnnotator::WriteDataAnnotator(bool validateMode)
: mHash(asHexString(RandomGenerator::Random128()))
, mValidateMode(validateMode)
{
  if (mValidateMode)
  {
    // Just write to stdout
    for (int i=1; i<48; i++) {
      mFiles[i] = stdout;
    }
  }
  else
  {
    const char* dataDirPath = "data";
    int err = mkdir(dataDirPath, 0777);
    if (err != 0 && errno != EEXIST) {
      const char* errmsg = strerror(errno);
      fprintf(stderr, "mkdir %s failed: %s\n", dataDirPath, errmsg);
      assert(err != 0);
    }
    for (int i=1; i<48; i++) {
      char path[128];
      sprintf(path, "%s/%s-%02d", dataDirPath, mHash.c_str(), i);
      mFiles[i] = fopen(path, "w");
    }
  }
}

void WriteDataAnnotator::On_DnnMonteCarlo_choosePlay(const KnowableState& state
                                  , PossibilityAnalyzer* analyzer
                                  , const float expectedScore[13], const float moonProb[13][3])
{
}

void WriteDataAnnotator::OnGameStateBeforePlay(const GameState& state)
{
}

const int kScale = 1000;

std::string FixedPointToString(int x)
{
  assert(x >= 0);
  assert(x <= kScale);
  if (x == kScale) {
    return std::string("1.000");
  } else {
    char buffer[6] = "0.000";
    sprintf(buffer+2, "%03d", x);
    return std::string(buffer);
  }
}

void WriteDataAnnotator::OnWriteData(const KnowableState& state, PossibilityAnalyzer* analyzer, const float expectedScore[13]
                          , const float moonProb[13][3], const float winsTrickProb[13])
{
  FILE* out = mFiles[state.PlayNumber()];
  fprintf(out, "%s\n", asHexString(state.dealIndex()).c_str());

  const CardHand choices = state.LegalPlays();
  fprintf(out, "Play %d, Player Leading %d, Current Player %d, Choices %d TrickSuit %s\n"
      , state.PlayNumber(), state.PlayerLeadingTrick(), state.CurrentPlayer(), choices.Size(), NameOfSuit(state.TrickSuit()));

  if (state.PlayInTrick() != 0)
    assert(SuitOf(state.GetTrickPlay(0)) == state.TrickSuit());
  fprintf(out, "TrickSoFar:"); // no \n
  for (int i=0; i<4; i++) {
    if (i < state.PlayInTrick())
      fprintf(out, "%3s  ", NameOf(state.GetTrickPlay(i)));
    else
      fprintf(out, ". ");
  }
  fprintf(out, "\n");

  fprintf(out, "PointsSoFar:");  // no \n
  for (int i=0; i<4; i++) {
    fprintf(out, "%d ", state.GetScoreFor(i));
  }
  fprintf(out, "\n");

  const uint128_t kPossibilities = analyzer->Possibilities();
  Distribution distribution;
  CardHands hands;
  state.PrepareHands(hands);
  analyzer->ExpectedDistribution(distribution, hands);
  distribution.DistributeRemainingToPlayer(state.CurrentPlayersHand(), state.CurrentPlayer(), kPossibilities);
  distribution.Validate(state.UnplayedCards(), kPossibilities);

  float prob[52][4];
  distribution.AsProbabilities(prob);
  Distribution::PrintProbabilities(prob, out);

  fprintf(out, "--\n");
  CardArray::iterator it(choices);
  for (unsigned i=0; i<choices.Size(); ++i) {
    Card card = it.next();

    const int kNumMoonProbClasses = 3;
    int f[kNumMoonProbClasses];
    int sum = 0;
    for (int j=0; j<kNumMoonProbClasses; j++) {
      int scaled = int(moonProb[i][j] * kScale + 0.5);
      sum += scaled;
      f[j] = scaled;
    }
    if (sum < kScale) {
      // We will add the difference to the first non-zero element less than 1/2
      const int kHalf = kScale/2;
      int delta = kScale - sum; // should be 1 nearly all the time
      sum += delta;
      for (int j=0; j<kNumMoonProbClasses; j++) {
        if (f[j]>0 && f[j]<kHalf) {
          f[j] += delta;
          break;
        }
      }
    } else if (sum > kScale) {
      int delta = kScale - sum; // should be -1 nearly all the time
      sum += delta;
      // We will reduce the largest value
      int imax = 0;
      int max = f[imax];
      for (int j=1; j<kNumMoonProbClasses; j++) {
        if (max < f[j]) {
          max = f[j];
          imax = j;
        }
      }
      f[imax] += delta;
    }

    assert(sum == kScale);

    // Expected score here should be the "boring" score.
    // We want to make it easy on our models to learn to predict appoximate number of points that can be taken.
    // We'll use the moonProb prediction to come the corrected score.
    const float kEpsilon = 0.001;  // a little fudge factor for inexact floating point.
    assert(expectedScore[i] >=  0.0 - kEpsilon);
    assert(expectedScore[i] <= 26.0 + kEpsilon);

    fprintf(out, "%3s  %5.4f %5.4f | %s %s %s\n", NameOf(card), expectedScore[i], winsTrickProb[i]
               , FixedPointToString(f[0]).c_str(), FixedPointToString(f[1]).c_str(), FixedPointToString(f[2]).c_str());
  }

  fprintf(out, "----\n");
}
