#include "cell_context.h"
#include "ue_context.h"
#include "util.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

cellContext::cellContext(int nb_slices, int ues_per_slice, int cell_id)
    : nb_slices_(nb_slices), ues_per_slice_(ues_per_slice), cell_id_(cell_id) {
  vector<int> user_trace_mapping;
  std::string fname = trace_dir + "mapping0.config";
  std::ifstream ifs(fname, std::ifstream::in);
  int ue_id, trace_id;
  while (ifs >> ue_id >> trace_id) {
    user_trace_mapping.push_back(trace_id);
  }
  ue_id = 0;

  // construct the slices and users context
  for (int i = 0; i < nb_slices_; ++i) {
    slices_[i] = new sliceContext(i, 1.0 / nb_slices_);
    for (int j = 0; j < ues_per_slice; ++j) {
      int trace_id = user_trace_mapping[ue_id % user_trace_mapping.size()];
      ueContext *ue = new ueContext(ue_id, trace_id);
      slices_[i]->appendUser(ue);
      ue_id += 1;
    }
  }
  for (int i = 0; i < (NB_CELLS + 1); ++i) {
    slice_user_[i] = new ueContext *[nb_slices_];
    slice_cqi_[i] = new uint8_t[nb_slices_];
  }
}

cellContext::~cellContext() {
  for (int i = 0; i < nb_slices_; ++i) {
    delete slices_[i];
  }
  for (int i = 0; i < (NB_CELLS + 1); ++i) {
    delete[] slice_user_[i];
    delete[] slice_cqi_[i];
  }
}

void cellContext::newTTI(unsigned int tti) {
  for (int i = 0; i < nb_slices_; ++i) {
    slices_[i]->newTTI(tti);
  }
}

void cellContext::calculateRBGsQuota() {
  for (int i = 0; i < nb_slices_; i++) {
    slice_rbgs_share_[i] =
        slices_[i]->getWeight() * NB_RBGS + slice_rbgs_offset_[i];
  }
  memset(slice_rbgs_allocated_, 0, nb_slices_ * sizeof(int8_t));
#ifdef DEBUG_LOG
  fprintf(stderr, "slice_share: ");
  for (int i = 0; i < nb_slices_; i++) {
    fprintf(stderr, "%d(%f); ", i, slice_rbgs_share_[i]);
  }
  fprintf(stderr, "\n");
#endif
}

void cellContext::callEnterpriseSched(int rbg_id, int mute_cell) {
  for (int sid = 0; sid < nb_slices_; sid++) {
    // if a slice's quota is met, don't recompute pf-metric, etc
    if ((double)slice_rbgs_allocated_[sid] > slice_rbgs_share_[sid]) {
      continue;
    }
    slices_[sid]->calcPFMetricOneRBG(rbg_id, mute_cell);
    ueContext *ue = slices_[sid]->enterpriseSchedule(rbg_id, mute_cell);
    slice_cqi_[mute_cell + 1][sid] = ue->getCQI(rbg_id);
    slice_user_[mute_cell + 1][sid] = ue;
    if (mute_cell == -1) {
      slice_nomute_metric[sid] = ue->getRankingMetric(rbg_id, -1);
    }
  }
}

std::pair<int, ueContext *>
cellContext::callInterSliceSched(double *slice_metric, int rbgid,
                                 int mute_cell) {
  // how do we deal with quota?
  uint8_t max_cqi = 0;
  int max_sliceid = -1;
#ifdef DEBUG_LOG
  if (cell_id_ == 0)
    fprintf(stderr, "(%d)rbgs_allocated, rbgs_share: ", cell_id_);
#endif
  for (int sid = 0; sid < nb_slices_; sid++) {
#ifdef DEBUG_LOG
    if (cell_id_ == 0)
      fprintf(stderr, "(%u %f) ", slice_rbgs_allocated_[sid],
              slice_rbgs_share_[sid]);
#endif
    if ((double)slice_rbgs_allocated_[sid] >= slice_rbgs_share_[sid])
      continue;
    if (slice_cqi_[mute_cell + 1][sid] >= max_cqi) {
      max_cqi = slice_cqi_[mute_cell + 1][sid];
      max_sliceid = sid;
    }
  }
#ifdef DEBUG_LOG
  if (cell_id_ == 0)
    fprintf(stderr, " final_slice: %d \n", max_sliceid);
#endif
  assert(max_sliceid != -1);
  ueContext *ue = slice_user_[mute_cell + 1][max_sliceid];
  slice_metric[max_sliceid] += ue->getRankingMetric(rbgid, mute_cell);
  return std::pair<int, ueContext *>(max_sliceid, ue);
}

int cellContext::doAllocation(int rbgid, int mute_cell) {
  uint8_t max_cqi = 0;
  int max_sliceid = -1;
  for (int sid = 0; sid < nb_slices_; sid++) {
    if ((double)slice_rbgs_allocated_[sid] >= slice_rbgs_share_[sid])
      continue;
    if (slice_cqi_[mute_cell + 1][sid] >= max_cqi) {
      max_cqi = slice_cqi_[mute_cell + 1][sid];
      max_sliceid = sid;
    }
  }
  ueContext *ue = slice_user_[mute_cell + 1][max_sliceid];
  ue->allocateRBG(rbgid);
  slice_rbgs_allocated_[max_sliceid] += 1;
  return max_sliceid;
}

void cellContext::getSumMetric(vector<double> &cell_slice_metric, int begin_rbg,
                               bool is_avg, int mute_cell) {
  if (begin_rbg >= NB_RBGS) {
    return;
  }
  int8_t rbgs_alloc_backup[MAX_SLICES];
  memcpy(rbgs_alloc_backup, slice_rbgs_allocated_, nb_slices_ * sizeof(int8_t));
  std::fill(cell_slice_metric.begin(), cell_slice_metric.end(), 0);

  for (int rbgid = begin_rbg; rbgid < NB_RBGS; rbgid++) {
    for (int sid = 0; sid < nb_slices_; sid++) {
      if (rbgid == begin_rbg && mute_cell != -1) {
        ueContext *ue = slices_[sid]->enterpriseSchedule(rbgid, mute_cell);
        slice_cqi_[0][sid] = ue->getCQI(rbgid);
        slice_user_[0][sid] = ue;
      } else {
        ueContext *ue = slices_[sid]->enterpriseSchedule(rbgid);
        slice_cqi_[0][sid] = ue->getCQI(rbgid);
        slice_user_[0][sid] = ue;
      }
    }

    uint8_t max_cqi = 0;
    int max_sid = -1;
    for (int sid = 0; sid < nb_slices_; sid++) {
      if ((double)slice_rbgs_allocated_[sid] >= slice_rbgs_share_[sid])
        continue;
      if (slice_cqi_[0][sid] > max_cqi) {
        max_cqi = slice_cqi_[0][sid];
        max_sid = sid;
      }
    }
    assert(max_sid != -1);
    ueContext *ue = slice_user_[0][max_sid];
    slice_rbgs_allocated_[max_sid] += 1;
    cell_slice_metric[max_sid] += ue->getRankingMetric(rbgid);
  }
  if (is_avg) {
    for (size_t sid = 0; sid < cell_slice_metric.size(); sid++) {
      int rbgs_alloc = slice_rbgs_allocated_[sid] - rbgs_alloc_backup[sid];
      if (rbgs_alloc > 0) {
        cell_slice_metric[sid] /= (double)rbgs_alloc;
      }
    }
  }

  memcpy(slice_rbgs_allocated_, rbgs_alloc_backup, nb_slices_ * sizeof(int8_t));
}
