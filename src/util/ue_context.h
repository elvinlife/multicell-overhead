#ifndef UE_CONTEXT_H_
#define UE_CONTEXT_H_

#define MAX_SLICES 40
#define MAX_TRACE_TTIS 500
#define NB_RBGS 32
#define RBS_PER_RBG 4

#include <cstdint>
#include <vector>
using std::vector;

class ueContext
{
  static double beta_;
  static int cqi_report_period_;
  
  private:
    int             ue_id_;
    int             trace_ttis_;
    double          ewma_throughput_;
    uint8_t         subband_cqis_[NB_RBGS];
    uint8_t         subband_cqis_trace_[MAX_TRACE_TTIS][NB_RBGS];
    double          sched_metrics_[NB_RBGS];
    vector<int>     rbgs_allocated_;

  public:
    ueContext(int ue_id, int trace_id);
    ~ueContext();
    
    // at the beginning of every TTI, update the history throughput
    // based on the allocated RBGs in the previous TTI
    void updateThroughput(unsigned int tti);

    // calculate the user metric of all rbgs and store in the user context
    void calculateRankingMetric();

    inline void allocateRBG(int rbg_id) {
      rbgs_allocated_.push_back(rbg_id);
    }

    // return the user CQI of this rbg
    inline uint8_t getCQI(int rbg_id) {return subband_cqis_[rbg_id];}

    inline int getUserID() { return ue_id_; }

    // get the user metric of this rbg(without compute)
    inline double getRankingMetric(int rbg_id) {
      return sched_metrics_[rbg_id];
    }
};

#endif
