#include "slice_context.h"

sliceContext::sliceContext(int slice_id, double weight)
 : slice_id_(slice_id), weight_(weight) {
}

sliceContext::~sliceContext() {
  for (auto it = ue_ctxs_.begin(); it != ue_ctxs_.end(); ++it) {
    delete it->second;
  }
}

void sliceContext::appendUser(ueContext *ue) {
  ue_ctxs_[ue->getUserID()] = ue;
}

// function definition of sliceContext
void sliceContext::newTTI(unsigned int tti) {
  fprintf(stderr, "newTTI(%u) enterprise scheduler: %d\n",
      tti, slice_id_);
  for(auto it = ue_ctxs_.begin(); it != ue_ctxs_.end(); ++it) {
    // we update it after allocating every rbg
    it->second->updateThroughput(tti);
    it->second->calculateRankingMetric();
  }
}

ueContext* sliceContext::enterpriseSchedule(int rbg_id) {
  ueContext* ue = NULL;
  double metric_max = -1;
  for (auto it = ue_ctxs_.begin(); it != ue_ctxs_.end(); ++it) {
    double metric = it->second->getRankingMetric( rbg_id );
    if ( metric > metric_max ) {
      metric_max = metric;
      ue = it->second;
    }
  }
  return ue;
}