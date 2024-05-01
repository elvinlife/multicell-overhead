#ifndef SCHEDULER_CONTEXT_H_
#define SCHEDULER_CONTEXT_H_
#include "cell_context.h"
#include "thread_pool.h"
#include "ue_context.h"
#include "util.h"
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

struct muteScheduleResult {
  double score;
  ueContext *ues[NB_CELLS];
  int slices_benefit[NB_CELLS];
};

class schedulerContext {
private:
  cellContext *all_cells[NB_CELLS];
  vector<vector<double>> cell_slice_cost;
  int nb_slices_;
  boost::asio::thread_pool boost_pool_;

public:
  schedulerContext(int nb_slices, int ues_per_slice);
  ~schedulerContext();
  void scheduleOneRBWithMute(int rbgid, int muteid, muteScheduleResult *result);
  void newTTI(unsigned int tti);
  long long total_time_t1_ = 0;
  long long total_time_t2_ = 0;
  long long total_time_t3_ = 0;
};

#endif