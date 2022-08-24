#ifndef CONTEXT_H_
#define CONTEXT_H_
#include "ue_context.h"
#include "slice_context.h"

class schedulerContext
{
  public:
    const int     nb_slices_;
    const int     ues_per_slice_;
    sliceContext  *slices_[MAX_SLICES];
    ueContext     **slice_user_[NB_RBGS];
    int           *slice_cqi_[NB_RBGS];
    bool          is_rbg_allocated_[NB_RBGS];
    int           slice_rbgs_quota[MAX_SLICES];
    float         slice_rbgs_share[MAX_SLICES];
    float         slice_rbgs_offset[MAX_SLICES];

    schedulerContext(int nb_slices, int ues_per_slice);
    ~schedulerContext();

    void newTTI(unsigned int tti);
    void calculateRBGsQuota();
    void maxcellInterSchedule();
};

#endif