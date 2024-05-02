#ifndef SCHEDULER_CONTEXT_H_
#define SCHEDULER_CONTEXT_H_
#include "cell_context.h"
#include "thread_pool.h"
#include "ue_context.h"
#include "util.h"
#include <thread>

struct muteScheduleResult {
  double score = -1;
  ueContext *ues_scheduled[NB_CELLS];
  int slices_benefit[NB_CELLS];
};

class schedulerContext {
private:
  int nb_slices_;
  cellContext *all_cells[NB_CELLS];
  vector<vector<double>> cell_slice_cost;
  threadPool threadpool_;

public:
  schedulerContext(int nb_slices, int ues_per_slice);
  ~schedulerContext();
  void scheduleOneRBNoMute(int rbgid, muteScheduleResult *result);
  void scheduleOneRBWithMute(int rbgid, int muteid, muteScheduleResult *result);
  void newTTI(unsigned int tti);

  long long total_time_t1_ = 0;
  long long total_time_t2_ = 0;
  long long total_time_t3_ = 0;
};

#endif