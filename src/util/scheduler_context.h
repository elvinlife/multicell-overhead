#ifndef CONTEXT_H_
#define CONTEXT_H_
#include "ue_context.h"
#include "slice_context.h"
#include <cstdint>

class schedulerContext
{
private:
    const int     nb_slices_;
    const int     ues_per_slice_;
    ueContext     **slice_user_[NB_RBGS];
    uint8_t       *slice_cqi_[NB_RBGS];
    bool          is_rbg_allocated_[NB_RBGS];
    sliceContext  *slices_[MAX_SLICES];
    int8_t        slice_rbgs_allocated_[MAX_SLICES];
    int8_t        slice_rbgs_quota_[MAX_SLICES];
    double        slice_rbgs_share_[MAX_SLICES];
    double        slice_rbgs_offset_[MAX_SLICES];

    void calculateRBGsQuota();
    void runEnterpriseSchedule(int rbg_id, int slice_id);
    void maxcellInterSchedule();
    void sequentialInterSchedule();

public:
    int         total_time_enterprise_;
    int         total_time_interslice_;
    schedulerContext(int nb_slices, int ues_per_slice);
    ~schedulerContext();
    void newTTI(unsigned int tti);
};

#endif