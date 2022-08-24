#ifndef UTIL_H_
#define UTIL_H_
#include <vector>
#include <cassert>
#include <cstdint>

// times four since we're using 60khz subcarrier instead of 15khz
#define SUBCARRIER_MULTI 4

//int get_mcs_from_cqi(int cqi);
//int get_tbs_from_mcs(int mcs, int nb_rb);
//float get_sinr_from_cqi(int cqi);

extern int MapCQIToMCS[15];

extern int TransportBlockSize[27];

extern int McsToItbs[29];

extern double SINRForCQIIndex[15];

inline int get_mcs_from_cqi(uint8_t cqi) {
  // assert(cqi <= 15);
  // assert(cqi >= 1);
  return MapCQIToMCS [cqi-1];
}

inline int get_tbs_from_mcs(int mcs, int nb_rb) {
  // assert(mcs <= 28);
  // assert(mcs >= 0);
  int itbs = McsToItbs[mcs];
  return TransportBlockSize[itbs] * nb_rb * SUBCARRIER_MULTI;
}

inline float get_sinr_from_cqi(uint8_t cqi) {
  return SINRForCQIIndex[cqi-1];
}

uint8_t get_cqi_from_sinr(float sinr);
float get_effective_sinr(std::vector<float>&);

#endif
