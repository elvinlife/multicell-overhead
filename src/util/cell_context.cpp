#include "cell_context.h"
#include "ue_context.h"
#include "util.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

cellContext::cellContext(int nb_slices, int ues_per_slice, int cell_id)
    : pool_(0), nb_slices_(nb_slices), ues_per_slice_(ues_per_slice),
      cell_id_(cell_id) {
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
  for (int i = 0; i < NB_RBGS; ++i) {
    slice_user_[i] = new ueContext *[nb_slices_];
    slice_cqi_[i] = new uint8_t[nb_slices_];
  }
}

cellContext::~cellContext() {
  for (int i = 0; i < nb_slices_; ++i) {
    delete slices_[i];
  }
  for (int i = 0; i < NB_RBGS; ++i) {
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

void cellContext::assignOneRBG(int rbg_id) {
  for (int i = 0; i < nb_slices_; i++) {
    ueContext *ue = slices_[i]->enterpriseSchedule(rbg_id);
    slice_cqi_[rbg_id][i] = ue->getCQI(rbg_id);
    slice_user_[rbg_id][i] = ue;
  }
}

void cellContext::assignOneSlice(int slice_id) {
  sliceContext *slice = slices_[slice_id];
  for (int i = 0; i < NB_RBGS; i++) {
    ueContext *ue = slice->enterpriseSchedule(i);
    slice_cqi_[i][slice_id] = ue->getCQI(i);
    slice_user_[i][slice_id] = ue;
  }
}

int cellContext::addScheduleMetric(vector<double> &slice_metric, int rbgid) {
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
    if (slice_cqi_[rbgid][sid] >= max_cqi) {
      max_cqi = slice_cqi_[rbgid][sid];
      max_sliceid = sid;
    }
  }
  if (cell_id_ == 0)
    fprintf(stderr, " final_slice: %d \n", max_sliceid);
  assert(max_sliceid != -1);
  ueContext *ue = slice_user_[rbgid][max_sliceid];
  slice_metric[max_sliceid] += ue->getRankingMetric(rbgid);
  return max_sliceid;
}

double cellContext::getScheduleMetricGivenSid(int sid, int rbgid) {
  ueContext *ue = slices_[sid]->enterpriseSchedule(rbgid);
  return ue->getRankingMetric(rbgid);
}

int cellContext::doAllocation(int rbgid) {
  uint8_t max_cqi = 0;
  int max_sliceid = -1;
  for (int sid = 0; sid < nb_slices_; sid++) {
    if ((double)slice_rbgs_allocated_[sid] >= slice_rbgs_share_[sid])
      continue;
    if (slice_cqi_[rbgid][sid] >= max_cqi) {
      max_cqi = slice_cqi_[rbgid][sid];
      max_sliceid = sid;
    }
  }
  ueContext *ue = slice_user_[rbgid][max_sliceid];
  ue->allocateRBG(rbgid);
  slice_rbgs_allocated_[max_sliceid] += 1;
  // fprintf(stderr, "cell: %d final_slice: %d\n", cell_id_, max_sliceid);
  return max_sliceid;
}

void cellContext::getAvgCost(vector<double> &cell_slice_cost) {
  for (int i = 0; i < nb_slices_; i++) {
    slice_rbgs_share_[i] = slices_[i]->getWeight() * NB_RBGS;
  }
  for (int i = 0; i < nb_slices_; i++) {
    assignOneSlice(i);
  }
  memset(slice_rbgs_allocated_, 0, nb_slices_ * sizeof(int8_t));

  for (int rbgid = 0; rbgid < NB_RBGS; rbgid++) {
    uint8_t max_cqi = 0;
    int max_sliceid = -1;
    for (int sliceid = 0; sliceid < nb_slices_; sliceid++) {
      if ((double)slice_rbgs_allocated_[sliceid] >= slice_rbgs_share_[sliceid])
        continue;
      if (slice_cqi_[rbgid][sliceid] > max_cqi) {
        max_cqi = slice_cqi_[rbgid][sliceid];
        max_sliceid = sliceid;
      }
    }
    ueContext *ue = slice_user_[rbgid][max_sliceid];
    slice_rbgs_allocated_[max_sliceid] += 1;
    cell_slice_cost[max_sliceid] += ue->getRankingMetric(rbgid);
  }
  for (size_t sid = 0; sid < cell_slice_cost.size(); sid++) {
    cell_slice_cost[sid] /= (double)slice_rbgs_allocated_[sid];
  }
}
