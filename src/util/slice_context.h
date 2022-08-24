#ifndef SLICE_CONTEXT_H_
#define SLICE_CONTEXT_H_
#include "ue_context.h"
#include <unordered_map>
using std::unordered_map;

class sliceContext
{
  private:
    int         slice_id_;
    float       weight_;
    float       rbgs_offset_;
    int         rbgs_quota_;
    unordered_map<int, ueContext *> ue_ctxs_;

  public:
    sliceContext(int slice_id, float weight);
    ~sliceContext();
    
    // append the user to the slice
    void appendUser(ueContext *ue);
    
    // update the average throughput for all ues
    // calculate metrics of all ues for all rbgs
    void newTTI(unsigned int tti);

    // compare the available metrics of all ues w.r.t the rbg_id
    // return the scheduled ue context
    ueContext* enterpriseSchedule(int rbg_id);

    inline int getSliceID() { return slice_id_; }
    inline float getWeight() { return weight_; }
};

#endif