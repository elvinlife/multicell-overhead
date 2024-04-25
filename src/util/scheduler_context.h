#ifndef CONTEXT_H_
#define CONTEXT_H_
#include "slice_context.h"
#include "ue_context.h"
// #include "ctpl.h"
#include "thread_pool.h"
#include <cstdint>

class schedulerContext {
private:
  threadPool pool_;
  const int nb_slices_;
  const int ues_per_slice_;
  ueContext **slice_user_[NB_RBGS];
  uint8_t *slice_cqi_[NB_RBGS];
  bool is_rbg_allocated_[NB_RBGS];
  sliceContext *slices_[MAX_SLICES];
  int8_t slice_rbgs_allocated_[MAX_SLICES];
  int8_t slice_rbgs_quota_[MAX_SLICES];
  double slice_rbgs_share_[MAX_SLICES];
  double slice_rbgs_offset_[MAX_SLICES];

  void calculateRBGsQuota();
  void assignOneRBG(int rbg_id);
  void assignOneSlice(int slice_id);
  void maxcellInterSchedule();
  void sequentialInterSchedule();

public:
  int total_time_t1_;
  int total_time_t2_;
  int total_time_t3_;
  schedulerContext(int nb_slices, int ues_per_slice);
  ~schedulerContext();
  void newTTI(unsigned int tti);
};

#endif