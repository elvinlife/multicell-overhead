#include "ue_context.h"
#include "util.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using std::string;

// function definition of ueContext
double ueContext::beta_ = 0.01;
int ueContext::cqi_report_period_ = 40;

ueContext::ueContext(int ue_id, int trace_id)
    : ue_id_(ue_id), trace_ttis_(0), ewma_throughput_(0) {
  trace_ttis_ = MAX_TRACE_TTIS;
  for (int i = 0; i < MAX_TRACE_TTIS; i++) {
    for (int j = 0; j < NB_RBGS; j++) {
      subband_cqis_trace_[i][j] = 6;
    }
  }
}

ueContext::~ueContext() {}

void ueContext::updateThroughput(unsigned int tti) {
  // periodically cqi report
  if (tti % cqi_report_period_ == 0) {
    memcpy(subband_cqis_, subband_cqis_trace_[tti % trace_ttis_],
           sizeof(uint8_t) * NB_RBGS);
  }
  if (rbgs_allocated_.size() == 0) {
    ewma_throughput_ = (1 - beta_) * ewma_throughput_;
  } else {
    vector<double> rbgs_sinrs;
    for (size_t i = 0; i < rbgs_allocated_.size(); i++) {
      rbgs_sinrs.push_back(
          get_sinr_from_cqi(subband_cqis_[rbgs_allocated_[i]]));
    }
    double effective_sinr = get_effective_sinr(rbgs_sinrs);
    int mcs = get_mcs_from_cqi(get_cqi_from_sinr(effective_sinr));
    int throughput = get_tbs_from_mcs(mcs, rbgs_allocated_.size());
    ewma_throughput_ = beta_ * throughput + (1 - beta_) * ewma_throughput_;
  }
  if (ewma_throughput_ < 0.1)
    ewma_throughput_ = 0.1;
  rbgs_allocated_.clear();
}

void ueContext::calcPFMetricAll() {
  // default PF
  for (int i = 0; i < NB_RBGS; i++) {
    int tbs = get_tbs_from_mcs(get_mcs_from_cqi(subband_cqis_[i]), 1);
    sched_metrics_[0][i] = tbs / ewma_throughput_;
  }
}

void ueContext::calcPFMetricOneRB(int rbgid, int mute_cell) {
  uint8_t cqi = subband_cqis_[rbgid];
  if (mute_cell != -1) {
    cqi += 2;
    if (cqi >= 15)
      cqi = 15;
  }
  int tbs = get_tbs_from_mcs(get_mcs_from_cqi(cqi), 1);
  sched_metrics_[mute_cell + 1][rbgid] = tbs / ewma_throughput_;
}