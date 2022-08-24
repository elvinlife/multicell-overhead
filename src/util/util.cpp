#include "util.h"
#include <math.h>
#include <cassert>

int MapCQIToMCS[15] = {
		0, 2, 4, 6, 8, 10, //QAM
		12, 14, 16, //4-QAM
		18, 20, 22, 24, 26, 28 //16QAM
};

int TransportBlockSize[27] = {
    16, 24, 32, 40, 56, 72, 88, 104, 120,
    136, 144, 176, 208, 224, 256, 280, 328, 336,
    376, 408, 440, 488, 520, 552, 584, 616, 712 };

int McsToItbs[29] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 10, 11, 12, 13, 14, 15, 15, 16, 17, 18,
    19, 20, 21, 22, 23, 24, 25, 26
};

double SINRForCQIIndex[15] = {
    -4.63, -2.6, -0.12, 2.26, 4.73,
    7.53, 8.67, 11.32,
    14.24, 15.21, 18.63, 21.32, 23.47, 28.49, 34.6
};

int get_cqi_from_sinr(float sinr) {
  int cqi = 1;
  while (SINRForCQIIndex[cqi] <= sinr && cqi <= 14) {
    cqi++;
  }
  return cqi;
}

float get_effective_sinr(std::vector<float>& sinrs) {
  assert(sinrs.size() > 0);
  double eff_sinr;
  double sum_I_sinr = 0;
  double beta = 1;
  for (auto it = sinrs.begin (); it != sinrs.end (); it++)
  {
	  //since sinr[] is expressed in dB we should convert it in natural unit!
	  double s = pow (10, ((*it)/10));
	  sum_I_sinr += exp (-s / beta);
  }

  eff_sinr = - beta * log (sum_I_sinr / sinrs.size ());
  eff_sinr = 10 * log10 (eff_sinr); //convert in dB
  return eff_sinr;
}
