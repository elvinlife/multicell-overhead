#ifndef SCHEDULER_CONTEXT_H_
#define SCHEDULER_CONTEXT_H_
#include "cell_context.h"
#include "thread_pool.h"
#include "util.h"
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

class schedulerContext {
private:
  cellContext *all_cells[NB_CELLS];
  vector<vector<double>> cell_slice_cost;
  int nb_slices_;
  boost::asio::thread_pool boost_pool_;

public:
  schedulerContext(int nb_slices, int ues_per_slice);
  ~schedulerContext();
  void scheduleOneRBWithMute(int rbgid, int muteid, double *total_score);
  void newTTI(unsigned int tti);
  int finished_task = 0;
  std::mutex pending_task_mutex;
};

#endif