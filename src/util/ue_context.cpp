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
  std::string trace_fname =
      trace_dir + "ue" + std::to_string(trace_id) + ".log";
  std::ifstream ifs(trace_fname, std::ifstream::in);
  int cqi = 0;
  trace_ttis_ = 0;
  std::string line;
  while (std::getline(ifs, line)) {
    std::istringstream iss(line);
    for (int i = 0; i < 512; ++i) {
      iss >> cqi;
      if (i % (RBS_PER_RBG * 4) == 0) {
        subband_cqis_trace_[trace_ttis_][i / (RBS_PER_RBG * 4)] = (uint8_t)cqi;
      }
    }
    trace_ttis_ += 1;
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

void ueContext::calculateRankingMetric() {
  // default PF
  for (int i = 0; i < NB_RBGS; i++) {
    int tbs = get_tbs_from_mcs(get_mcs_from_cqi(subband_cqis_[i]), 1);
    sched_metrics_[i] = tbs / ewma_throughput_;
  }
}

/*
inline void ueContext::allocateRBG(int rbg_id) {
  // let's not update the metric currently
  // int bw_kbps = get_tbs_from_mcs(
  //   get_mcs_from_cqi( subband_cqis_[rbg_id] ), 1
  // );
  // ewma_throughput_ += beta_ * bw_kbps;
  // for (int i = 0; i < NB_RBGS; i++) {
  //   int tbs = get_tbs_from_mcs(
  //     get_mcs_from_cqi(subband_cqis_[i]), 1
  //   );
  //   sched_metrics_[i] = tbs / ewma_throughput_;
  // }

  rbgs_allocated_.push_back(rbg_id);
}
*/
