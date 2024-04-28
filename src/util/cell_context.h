#ifndef CELL_CONTEXT_H_
#define CELL_CONTEXT_H_
#include "slice_context.h"
#include "ue_context.h"
// #include "ctpl.h"
#include "thread_pool.h"
#include <cstdint>
#include <vector>

class cellContext {
private:
  threadPool pool_;
  const int nb_slices_;
  const int ues_per_slice_;
  int cell_id_;
  ueContext **slice_user_[NB_RBGS];
  uint8_t *slice_cqi_[NB_RBGS];
  sliceContext *slices_[MAX_SLICES];
  int8_t slice_rbgs_allocated_[MAX_SLICES];
  double slice_rbgs_offset_[MAX_SLICES];
  void assignOneSlice(int slice_id);

public:
  double slice_rbgs_share_[MAX_SLICES];
  cellContext(int nb_slices, int ues_per_slice, int cell_id);
  ~cellContext();
  void calculateRBGsQuota();
  void muteOneRBG(int rbgid, int mute_cell);
  // for one rbg, check which user is scheduled for every slice
  void assignOneRBG(int rbg_id);
  // with the constraint that @sid must be given rbgid, get the metric
  double getScheduleMetricGivenSid(int sid, int rbgid);
  // add the metric after the enterprise schedulers finish, return the
  // @slice_id who gets the @rbgid
  int addScheduleMetric(vector<double> &, int rbgid);
  // do the real allocation, update quota, and return the @slice_id
  // who gets the @rbgid
  int doAllocation(int rbgid);
  void getAvgCost(vector<double> &);
  void newTTI(unsigned int tti);
};

#endif