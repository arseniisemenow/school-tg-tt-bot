#ifndef UTILS_ELO_CALCULATOR_H
#define UTILS_ELO_CALCULATOR_H

#include <utility>

namespace utils {

class EloCalculator {
 public:
  EloCalculator(int k_factor = 32);
  
  // Calculate new ELO ratings after a match
  // Returns: (new_elo1, new_elo2)
  std::pair<int, int> calculate(int elo1, int elo2, 
                                int score1, int score2);
  
  // Calculate expected score for player1
  double expectedScore(int elo1, int elo2);
  
  // Calculate ELO change for a player
  int calculateChange(int elo, double expected_score, double actual_score);

 private:
  int k_factor_;
};

}  // namespace utils

#endif  // UTILS_ELO_CALCULATOR_H

