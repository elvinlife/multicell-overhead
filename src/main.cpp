#include "util.h"
#include "scheduler_context.h"
#include <cstring>
#include <chrono>
#include <iostream>

int main() {
  schedulerContext scheduler(10, 20);
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