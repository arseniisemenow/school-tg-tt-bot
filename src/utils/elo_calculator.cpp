#include "utils/elo_calculator.h"

#include <cmath>

namespace utils {

EloCalculator::EloCalculator(int k_factor) : k_factor_(k_factor) {}

double EloCalculator::expectedScore(int elo1, int elo2) {
  return 1.0 / (1.0 + std::pow(10.0, (elo2 - elo1) / 400.0));
}

int EloCalculator::calculateChange(int elo, double expected_score, 
                                   double actual_score) {
  return static_cast<int>(k_factor_ * (actual_score - expected_score));
}

std::pair<int, int> EloCalculator::calculate(int elo1, int elo2,
                                             int score1, int score2) {
  double expected1 = expectedScore(elo1, elo2);
  double expected2 = 1.0 - expected1;
  
  // Determine actual scores (1.0 for win, 0.5 for draw, 0.0 for loss)
  double actual1 = 0.0;
  double actual2 = 0.0;
  
  if (score1 > score2) {
    actual1 = 1.0;
    actual2 = 0.0;
  } else if (score1 < score2) {
    actual1 = 0.0;
    actual2 = 1.0;
  } else {
    actual1 = 0.5;
    actual2 = 0.5;
  }
  
  int change1 = calculateChange(elo1, expected1, actual1);
  int change2 = calculateChange(elo2, expected2, actual2);
  
  int new_elo1 = elo1 + change1;
  int new_elo2 = elo2 + change2;
  
  return {new_elo1, new_elo2};
}

}  // namespace utils

