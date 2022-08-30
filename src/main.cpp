#include "util.h"
#include "scheduler_context.h"
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <iostream>

int main(int argc, char* argv[]) {
  if (argc != 3) {
    throw std::runtime_error("./radiosaber [num_slices] [num_users_per_slice]");
  }
  int num_slices = atoi(argv[1]);
  if (num_slices > 30) {
    throw std::runtime_error("Maximal number of slices is 30");
  }
  schedulerContext scheduler(num_slices, atoi(argv[2]));
  int global_tti = 0;
  int total_tti = 10000;
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < total_tti; i++) {
    scheduler.newTTI(global_tti);
    global_tti += 1;
  }
  auto elapsed = std::chrono::high_resolution_clock::now() - start;

  long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        elapsed).count();
  std::cout << "avg_time(us): " << microseconds / total_tti << std::endl;
  std::cout << "enterprise_time(us): " \
      << scheduler.total_time_enterprise_ / total_tti << std::endl;
  std::cout << "interslice_time(us): " \
      << scheduler.total_time_interslice_ / total_tti << std::endl;
}