#include "cell_context.h"
#include "util.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

cellContext::cellContext(int nb_slices, int ues_per_slice)
    : pool_(2), nb_slices_(nb_slices), ues_per_slice_(ues_per_slice) {
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

  total_time_t1_ = 0;
  total_time_t2_ = 0;
  total_time_t3_ = 0;
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
  auto t1 = std::chrono::high_resolution_clock::now();
  fprintf(stderr, "newTTI(%u) inter-slice scheduler\n", tti);
  for (int i = 0; i < nb_slices_; ++i) {
    slices_[i]->newTTI(tti);
  }
  calculateRBGsQuota();
  auto t2 = std::chrono::high_resolution_clock::now();
  sequentialInterSchedule();

  total_time_t1_ +=
      std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
}

void cellContext::calculateRBGsQuota() {
  int extra_rbgs = NB_RBGS;
  for (int i = 0; i < nb_slices_; i++) {
    slice_rbgs_share_[i] =
        slices_[i]->getWeight() * NB_RBGS + slice_rbgs_offset_[i];
    slice_rbgs_quota_[i] = std::floor(slice_rbgs_share_[i]);
    extra_rbgs -= slice_rbgs_quota_[i];
  }
  if (extra_rbgs > 0) {
    slice_rbgs_quota_[rand() % nb_slices_] += extra_rbgs % nb_slices_;
    for (int i = 0; i < nb_slices_; i++) {
      slice_rbgs_quota_[i] += extra_rbgs / nb_slices_;
    }
  }
  for (int i = 0; i < nb_slices_; i++) {
    slice_rbgs_offset_[i] = slice_rbgs_share_[i] - slice_rbgs_quota_[i];
  }

  for (int i = 0; i < nb_slices_; i++) {
    fprintf(stderr, "%d(%d); ", i, slice_rbgs_quota_[i]);
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

void cellContext::sequentialInterSchedule() {
  auto t1 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < nb_slices_; i++) {
    assignOneSlice(i);
  }
  auto t2 = std::chrono::high_resolution_clock::now();
  total_time_t3_ +=
      std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

  memset(slice_rbgs_allocated_, 0, nb_slices_ * sizeof(int8_t));

  for (int rbgid = 0; rbgid < NB_RBGS; rbgid++) {
    uint8_t max_cqi = 0;
    int max_rbgid = -1, max_sliceid = -1;
    for (int sliceid = 0; sliceid < nb_slices_; sliceid++) {
      if (slice_rbgs_allocated_[sliceid] >= slice_rbgs_quota_[sliceid])
        continue;
      if (slice_cqi_[rbgid][sliceid] > max_cqi) {
        max_cqi = slice_cqi_[rbgid][sliceid];
        max_rbgid = rbgid;
        max_sliceid = sliceid;
      }
    }
    assert(max_rbgid != -1);
    ueContext *ue = slice_user_[max_rbgid][max_sliceid];
    ue->allocateRBG(max_rbgid);
    slice_rbgs_allocated_[max_sliceid] += 1;

    // recomputation
    if (slice_rbgs_allocated_[max_sliceid] >= slice_rbgs_quota_[max_sliceid])
      continue;
    sliceContext *slice = slices_[max_sliceid];
    for (int rbgid = 0; rbgid < NB_RBGS; rbgid++) {
      ueContext *new_ue = slice->enterpriseSchedule(rbgid);
      slice_cqi_[rbgid][max_sliceid] = new_ue->getCQI(rbgid);
      slice_user_[rbgid][max_sliceid] = new_ue;
    }
  }
}
