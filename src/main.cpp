#include "cell_context.h"
#include "util.h"
#include "util/cell_context.h"
#include "util/scheduler_context.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    throw std::runtime_error("./radiosaber [num_slices] [num_users_per_slice]");
  }
  int num_slices = atoi(argv[1]);
  if (num_slices > 40) {
    throw std::runtime_error("Maximal number of slices is 40");
  }
  schedulerContext scheduler(num_slices, atoi(argv[2]));
  int total_tti = 10000;
  auto start = std::chrono::high_resolution_clock::now();
  for (int tti = 0; tti < total_tti; tti++) {
    std::cout << "TTI: " << tti << std::endl;
    scheduler.newTTI(tti);
  }
  auto elapsed = std::chrono::high_resolution_clock::now() - start;

  long long microseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
  std::cout << "avg_time(us): " << microseconds / total_tti << std::endl;
  // std::cout << "time_t1(us): " << scheduler.total_time_t1_ / total_tti
  //           << std::endl;
  // std::cout << "time_t2(us): " << scheduler.total_time_t2_ / total_tti
  //           << std::endl;
  // std::cout << "time_t3(us): " << scheduler.total_time_t3_ / total_tti
  //           << std::endl;
}
