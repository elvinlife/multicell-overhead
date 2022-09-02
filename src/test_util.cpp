#include "util/util.h"
#include <vector>
#include <cassert>
#include <iostream>

using std::vector;

void test_effective_sinr(){
  vector<double> rbgs_sinrs{7.53, 7.53, 7.53};
  double effective_sinr = get_effective_sinr(rbgs_sinrs);
  std::cerr << "effective_sinr: " << effective_sinr << std::endl;
  int mcs = get_mcs_from_cqi(get_cqi_from_sinr(effective_sinr));
  // cqi = 6
  assert(mcs == 10);
  int throughput = get_tbs_from_mcs(mcs, rbgs_sinrs.size());
  std::cerr << "throughput: " << throughput << std::endl;
  assert(throughput == 1632);
}

void test_cqi_sinr() {
    vector<double> rbgs_sinrs;
    double sinr = get_sinr_from_cqi(13);
    rbgs_sinrs.push_back(sinr);
    double eff_sinr = get_effective_sinr(rbgs_sinrs);
    int cqi = get_cqi_from_sinr(eff_sinr);
    assert( cqi == 13);
}

int main()
{
  test_effective_sinr();
  test_cqi_sinr();
}
