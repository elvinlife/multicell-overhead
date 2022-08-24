#include "scheduler_context.h"
#include "util.h"
#include <cmath>
#include <cstring>
#include <string>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <chrono>

schedulerContext::schedulerContext(int nb_slices, int ues_per_slice)
  :nb_slices_(nb_slices), ues_per_slice_(ues_per_slice) {
  vector<int> user_trace_mapping;
  std::string fname = "/home/alvin/Research/RadioSaber/pbecc-traces-noise0/mapping0.config";
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
      ueContext* ue = new ueContext(ue_id, user_trace_mapping[ue_id]);
      slices_[i]->appendUser(ue);
      ue_id += 1;
    }
  }
  for (int i = 0; i < NB_RBGS; ++i) {
    slice_user_[i] = new ueContext* [nb_slices_];
    slice_cqi_[i] = new uint8_t[nb_slices_];
  }

  total_time_enterprise_ = 0;
  total_time_interslice_ = 0;
}

schedulerContext::~schedulerContext() {
  for (int i = 0; i < nb_slices_; ++i) {
    delete slices_[i];
  }
  for (int i = 0; i < NB_RBGS; ++i) {
    delete [] slice_user_[i];
    delete [] slice_cqi_[i];
  }
}

void schedulerContext::newTTI(unsigned int tti) {
  auto t1 = std::chrono::high_resolution_clock::now();
  fprintf(stderr, "newTTI(%u) inter-slice scheduler\n", tti);
  for (int i = 0; i < nb_slices_; ++i) {
    slices_[i]->newTTI(tti);
  }
  calculateRBGsQuota();
  auto t2 = std::chrono::high_resolution_clock::now();
  //maxcellInterSchedule();
  sequentialInterSchedule();
  auto t3 = std::chrono::high_resolution_clock::now();
  total_time_enterprise_ += std::chrono::duration_cast<std::chrono::microseconds>(
        t2 - t1).count();
  total_time_interslice_ += std::chrono::duration_cast<std::chrono::microseconds>(
        t3 - t2).count();
}

void schedulerContext::calculateRBGsQuota() {
  int extra_rbgs = NB_RBGS;
  for (int i = 0; i < nb_slices_; i++) {
    slice_rbgs_share_[i] = slices_[i]->getWeight() * NB_RBGS \
        + slice_rbgs_offset_[i];
    slice_rbgs_quota_[i] = std::floor(slice_rbgs_share_[i]);
    extra_rbgs -= slice_rbgs_quota_[i];
  }
  int rand_begin_idx = rand();
  bool is_first_slice = true;
  for (int i = 0; i < nb_slices_; i++) {
    int k = (i + rand_begin_idx) % nb_slices_;
    slice_rbgs_quota_[k] += extra_rbgs / nb_slices_;
    if (is_first_slice) {
      slice_rbgs_quota_[k] += extra_rbgs % nb_slices_;
      is_first_slice = false;
    }
  }
  for (int i = 0; i < nb_slices_; i++) {
    slice_rbgs_offset_[i] = slice_rbgs_share_[i] - slice_rbgs_quota_[i];
  }

  for (int i = 0; i < nb_slices_; i++) {
    fprintf(stderr, "%d(%u); ", i, slice_rbgs_quota_[i]);
  }
  fprintf(stderr, "\n");
}

void schedulerContext::sequentialInterSchedule() {
  for (int i = 0; i < NB_RBGS; i++) {
    for (int j = 0; j < nb_slices_; j++) {
      ueContext* ue = slices_[j]->enterpriseSchedule(i);
      slice_cqi_[i][j] = ue->getCQI(i);
      slice_user_[i][j] = ue;
    }
  }
  memset(slice_rbgs_allocated_, 0, nb_slices_ * sizeof(int8_t));
  memset(is_rbg_allocated_, 0, NB_RBGS * sizeof(bool));

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
    ueContext* ue = slice_user_[max_rbgid][max_sliceid];
    is_rbg_allocated_[max_rbgid] = true;
    ue->allocateRBG(max_rbgid);
    slice_rbgs_allocated_[max_sliceid] += 1;
  }
}

void schedulerContext::maxcellInterSchedule() {
  for (int i = 0; i < NB_RBGS; i++) {
    for (int j = 0; j < nb_slices_; j++) {
      ueContext* ue = slices_[j]->enterpriseSchedule(i);
      slice_cqi_[i][j] = ue->getCQI(i);
      slice_user_[i][j] = ue;
    }
  }

  memset(slice_rbgs_allocated_, 0, nb_slices_ * sizeof(uint8_t));
  memset(is_rbg_allocated_, 0, NB_RBGS * sizeof(bool));
  
  for (int k = 0; k < NB_RBGS; k++) {
    uint8_t max_cqi = 0;
    int max_rbgid = -1, max_sliceid = -1;
    for (int rbgid = 0; rbgid < NB_RBGS; rbgid++ ) {
      // the rbg has been allocated
      if (is_rbg_allocated_[rbgid])
        continue;
      for (int sliceid = 0; sliceid < nb_slices_; sliceid++) {
        // the slice has been allocated quota rbgs
        if (slice_rbgs_allocated_[sliceid] >= slice_rbgs_quota_[sliceid])
          continue;
        if (slice_cqi_[rbgid][sliceid] > max_cqi) {
          max_cqi = slice_cqi_[rbgid][sliceid];
          max_rbgid = rbgid;
          max_sliceid = sliceid;
        }
      }
    }

    fprintf(stderr, "rbg: %d allocated to slice: %d left: %d\n",
        max_rbgid, max_sliceid, NB_RBGS - k - 1);
    assert(max_rbgid != -1);
    ueContext* ue = slice_user_[max_rbgid][max_sliceid];
    
    is_rbg_allocated_[max_rbgid] = true;
    ue->allocateRBG(max_rbgid);
    slice_rbgs_allocated_[max_sliceid] += 1;

    // recomputation
    // if (slice_rbgs_allocated[max_sliceid] >= slice_rbgs_quota[max_sliceid])
    //   continue;
    // sliceContext* slice = slices_[max_sliceid];
    // for (int rbgid = 0; rbgid < NB_RBGS; rbgid++) {
    //   ueContext* new_ue = slice->enterpriseSchedule(rbgid);
    //   slice_cqi_[rbgid][max_sliceid] = new_ue->getCQI(rbgid);
    //   slice_user_[rbgid][max_sliceid] = new_ue;
    // }
  }
}