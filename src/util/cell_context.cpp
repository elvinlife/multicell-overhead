#include "cell_context.h"
#include "ue_context.h"
#include "util.h"
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
  fprintf(stderr, "cell(%d), newTTI(%u) inter-slice scheduler\n", cell_id_,
          tti);
  for (int i = 0; i < nb_slices_; ++i) {
    slices_[i]->newTTI(tti);
  }
}

void cellContext::muteOneRBG(int rbgid, int mute_cell) {
  for (int sid = 0; sid < nb_slices_; sid++) {
    slices_[sid]->calcPFMetricOneRBG(rbgid, mute_cell);
  }
}

void cellContext::calculateRBGsQuota() {
  for (int i = 0; i < nb_slices_; i++) {
    slice_rbgs_share_[i] =
        slices_[i]->getWeight() * NB_RBGS + slice_rbgs_offset_[i];
  }
  memset(slice_rbgs_allocated_, 0, nb_slices_ * sizeof(int8_t));
  fprintf(stderr, "slice_share: ");
  for (int i = 0; i < nb_slices_; i++) {
    fprintf(stderr, "%d(%f); ", i, slice_rbgs_share_[i]);
  }
  fprintf(stderr, "\n");
}

void cellContext::assignOneRBG(int rbg_id, int mute_cell) {
  for (int sid = 0; sid < nb_slices_; sid++) {
    ueContext *ue = slices_[sid]->enterpriseSchedule(rbg_id, mute_cell);
    slice_cqi_[mute_cell + 1][sid] = ue->getCQI(rbg_id);
    slice_user_[mute_cell + 1][sid] = ue;
  }
}

int cellContext::addScheduleMetric(vector<double> &slice_metric, int rbgid,
                                   int mute_cell) {
  // how do we deal with quota?
  uint8_t max_cqi = 0;
  int max_sliceid = -1;
  if (cell_id_ == 0)
    fprintf(stderr, "(%d)rbgs_allocated, rbgs_share: ", cell_id_);
  for (int sid = 0; sid < nb_slices_; sid++) {
    if (cell_id_ == 0)
      fprintf(stderr, "(%u %f) ", slice_rbgs_allocated_[sid],
              slice_rbgs_share_[sid]);
    if ((double)slice_rbgs_allocated_[sid] >= slice_rbgs_share_[sid])
      continue;
    if (slice_cqi_[mute_cell + 1][sid] >= max_cqi) {
      max_cqi = slice_cqi_[mute_cell + 1][sid];
      max_sliceid = sid;
    }
  }
  if (cell_id_ == 0)
    fprintf(stderr, " final_slice: %d \n", max_sliceid);
  assert(max_sliceid != -1);
  ueContext *ue = slice_user_[mute_cell + 1][max_sliceid];
  slice_metric[max_sliceid] += ue->getRankingMetric(rbgid, mute_cell);
  return max_sliceid;
}

double cellContext::getScheduleMetricGivenSid(int sid, int rbgid) {
  ueContext *ue = slices_[sid]->enterpriseSchedule(rbgid);
  return ue->getRankingMetric(rbgid);
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
  // fprintf(stderr, "cell: %d final_slice: %d\n", cell_id_, max_sliceid);
  return max_sliceid;
}

void cellContext::getAvgCost(vector<double> &cell_slice_cost) {
  for (int i = 0; i < nb_slices_; i++) {
    slice_rbgs_share_[i] = slices_[i]->getWeight() * NB_RBGS;
  }
  memset(slice_rbgs_allocated_, 0, nb_slices_ * sizeof(int8_t));

  for (int rbgid = 0; rbgid < NB_RBGS; rbgid++) {
    for (int sid = 0; sid < nb_slices_; sid++) {
      ueContext *ue = slices_[sid]->enterpriseSchedule(rbgid);
      slice_cqi_[0][sid] = ue->getCQI(rbgid);
      slice_user_[0][sid] = ue;
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
    ueContext *ue = slice_user_[0][max_sid];
    slice_rbgs_allocated_[max_sid] += 1;
    cell_slice_cost[max_sid] += ue->getRankingMetric(rbgid);
  }
  for (size_t sid = 0; sid < cell_slice_cost.size(); sid++) {
    cell_slice_cost[sid] /= (double)slice_rbgs_allocated_[sid];
  }
}
