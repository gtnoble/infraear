#include <stdio.h>
#include <math.h>
#include <stdlib.h>

const double fxtal = 40E6;

double dividend(int sdm0, int sdm1, int sdm2);
double divisor(int odiv);

int main(int arcg, char *argv[]) {
  double desired_frequency = atof(argv[1]);

  double closest_frequency;
  int closest_sdm0;
  int closest_sdm1;
  int closest_sdm2;
  int closest_odiv;

  for (int sdm0 = 0; sdm0 <= 255; sdm0++) {
    for (int sdm1 = 0; sdm1 <= 255; sdm1++) {
      for (int sdm2 = 0; sdm2 <= 63; sdm2++) {
        for (int odiv = 0; odiv <= 31; odiv++) {
          double candidate_dividend = dividend(sdm0, sdm1, sdm2);
          if (candidate_dividend >= 350E6 && candidate_dividend <= 500E6) {
            double candidate_frequency = candidate_dividend / divisor(odiv);
            if (fabs(candidate_frequency - desired_frequency) < fabs(closest_frequency - desired_frequency)) {
              closest_frequency = candidate_frequency;
              closest_sdm0 = sdm0;
              closest_sdm1 = sdm1;
              closest_sdm2 = sdm2;
              closest_odiv = odiv;
            }
          }
        }
      }
    }
  }

  printf(
      "Desired Frequency: %f\n"
      "Closest Frequency: %f\n"
      "sdm0: %d, sdm1: %d, sdm2: %d, odiv: %d",
      desired_frequency,
      closest_frequency,
      closest_sdm0,
      closest_sdm1,
      closest_sdm2,
      closest_odiv
  );
}

double dividend(int sdm0, int sdm1, int sdm2) {
  return fxtal * 
    (sdm2 + sdm1 / (1 << 8) + sdm0 / (1 << 16) + 4);
}

double divisor(int odiv) {
  return (2 * (odiv + 2));
}
