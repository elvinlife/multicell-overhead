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

int main()
{
  test_effective_sinr();
}
