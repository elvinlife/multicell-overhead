#ifndef CELL_CONTEXT_H_
#define CELL_CONTEXT_H_
#include "slice_context.h"
#include "ue_context.h"
#include "util.h"
#include <cstdint>
#include <vector>

class cellContext {
private:
  const int nb_slices_;
  const int ues_per_slice_;
  int cell_id_;
  // with cell @i muted, the user scheduled for slice @j is
  // stored in slice_user_[j][i], and its cqi is stored in slice_cqi_[i][j]
  ueContext **slice_user_[NB_CELLS + 1];
  // with cell @i muted, the
  uint8_t *slice_cqi_[NB_CELLS + 1];
  sliceContext *slices_[MAX_SLICES];
  double slice_rbgs_offset_[MAX_SLICES];

public:
  int8_t slice_rbgs_allocated_[MAX_SLICES];
  double slice_rbgs_share_[MAX_SLICES];
  double slice_nomute_metric[MAX_SLICES];
  cellContext(int nb_slices, int ues_per_slice, int cell_id);
  ~cellContext();
  void calculateRBGsQuota();
  // add the metric after the enterprise schedulers finish, return the
  // @slice_id who gets the @rbgid
  std::pair<int, ueContext *> callInterSliceSched(double *, int rbgid,
                                                  int mute_cell);
  // for one rbg, check which user is scheduled for every slice
  void callEnterpriseSched(int rbg_id, int mute_cell);

  // do the real allocation, update quota, and return the @slice_id
  // who gets the @rbgid
  int doAllocation(int rbgid, int mute_cell);
  void getAvgCost(vector<double> &, int begin_rbg);
  void newTTI(unsigned int tti);
};

#endif