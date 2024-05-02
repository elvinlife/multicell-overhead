#include "scheduler_context.h"
#include "ue_context.h"
#include "util.h"
#include <cassert>
#include <chrono>
#include <unordered_set>

schedulerContext::schedulerContext(int nb_slices, int ues_per_slice)
    : nb_slices_(nb_slices) {
  for (int cid = 0; cid < NB_CELLS; cid++) {
    vector<double> slice_cost(nb_slices, DEFAULT_COST);
    cell_slice_cost.push_back(slice_cost);
    all_cells[cid] = new cellContext(nb_slices, ues_per_slice, cid);
  }
}

schedulerContext::~schedulerContext() {
  for (int cid = 0; cid < NB_CELLS; cid++) {
    delete all_cells[cid];
  }
}

void schedulerContext::scheduleOneRBNoMute(int rbgid,
                                           muteScheduleResult *result) {
  double slice_metrics[MAX_SLICES];
  for (int cid = 0; cid < NB_CELLS; cid++) {
    all_cells[cid]->callEnterpriseSched(rbgid, -1);
    auto it = all_cells[cid]->callInterSliceSched(slice_metrics, rbgid, -1);
    result->slices_benefit[cid] = it.first;
    result->ues_scheduled[cid] = it.second;
  }
  result->score = 0;
}

void schedulerContext::scheduleOneRBWithMute(int rbgid, int muteid,
                                             muteScheduleResult *result) {
  double slice_metrics[MAX_SLICES];
  double slice_metrics_nomute[MAX_SLICES];
  int cell_to_slice[NB_CELLS];
  result->score = 0;
  for (int cid = 0; cid < NB_CELLS; cid++) {
    if (cid == muteid)
      continue;
    all_cells[cid]->callEnterpriseSched(rbgid, muteid);
    auto it = all_cells[cid]->callInterSliceSched(slice_metrics, rbgid, muteid);
    cell_to_slice[cid] = it.first;
    result->slices_benefit[cid] = it.first;
    result->ues_scheduled[cid] = it.second;
  }
  // with the same slice constraint(under no muting) this can be optimized, we
  // can calculate which user is scheduled in nomuting assumption
  for (int cid = 0; cid < NB_CELLS; cid++) {
    if (cid == muteid)
      continue;
    int same_slice = cell_to_slice[cid];
    slice_metrics_nomute[same_slice] +=
        all_cells[cid]->slice_nomute_metric[same_slice];
  }
#ifdef DEBUG_LOG
  if (muteid == 0)
    fprintf(stderr, "metrics of muting cell0: ");
#endif
  std::unordered_map<int, double> slices_benefit;
  for (int sid = 0; sid < nb_slices_; sid++) {
    double benefit = slice_metrics[sid] - slice_metrics_nomute[sid];
    if (benefit > 0.0001) {
      slices_benefit[sid] = benefit;
#ifdef DEBUG_LOG
      if (muteid == 0) {
        fprintf(stderr, "sid %d benefit %f cost %f ", sid, slices_benefit[sid],
                cell_slice_cost[muteid][sid]);
      }
#endif
    }
  }
#ifdef DEBUG_LOG
  if (muteid == 0) {
    fprintf(stderr, "\n");
  }
#endif
  while (slices_benefit.size() > 0) {
    bool allslices_work = true;
    for (auto it = slices_benefit.begin(); it != slices_benefit.end(); it++) {
      // remove if a slice cannot pay for his benefit
      if (it->second <
          cell_slice_cost[muteid][it->first] / slices_benefit.size()) {
        slices_benefit.erase(it->first);
        allslices_work = false;
        break;
      }
    }
    if (allslices_work)
      break;
  }
  for (auto it = slices_benefit.begin(); it != slices_benefit.end(); it++) {
    result->score += it->second / (cell_slice_cost[muteid][it->first] /
                                   slices_benefit.size());
  }
}

void schedulerContext::newTTI(unsigned int tti) {
#ifdef PROFILE_RUNTIME
  auto t1 = std::chrono::high_resolution_clock::now();
#endif
  for (int cid = 0; cid < NB_CELLS; cid++) {
    all_cells[cid]->newTTI(tti);
    all_cells[cid]->getAvgCost(std::ref(cell_slice_cost[cid]));
  }
  // recalculate the quota
  for (int cid = 0; cid < NB_CELLS; cid++) {
    all_cells[cid]->calculateRBGsQuota();
  }
#ifdef PROFILE_RUNTIME
  auto t2 = std::chrono::high_resolution_clock::now();
  total_time_t1_ +=
      std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
#endif
  std::vector<muteScheduleResult> mutecell_result(NB_CELLS + 1);
  for (int rbgid = 0; rbgid < NB_RBGS; rbgid++) {
#ifdef DEBUG_LOG
    fprintf(stderr, "alloc_rbg: %d\n", rbgid);
#endif
    // call scheduling without muting first, get the cell_to_slice pf metrics
    scheduleOneRBNoMute(rbgid, &mutecell_result[NB_CELLS]);

#ifdef PROFILE_RUNTIME
    auto t3 = std::chrono::high_resolution_clock::now();
#endif
    for (int muteid = 0; muteid < NB_CELLS; muteid++) {
      scheduleOneRBWithMute(rbgid, muteid, &mutecell_result[muteid]);
    }
#ifdef PROFILE_RUNTIME
    auto t4 = std::chrono::high_resolution_clock::now();
    total_time_t2_ +=
        std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
#endif
    // begin with no muting
    int final_mute = -1;
    muteScheduleResult final_result = mutecell_result[NB_CELLS];
#ifdef DEBUG_LOG
    fprintf(stderr, "mute_scores: ");
#endif
    for (size_t i = 0; i < NB_CELLS; i++) {
#ifdef DEBUG_LOG
      fprintf(stderr, " %f, ", mutecell_result[i].score);
#endif
      if (mutecell_result[i].score > final_result.score) {
        final_result = mutecell_result[i];
        final_mute = (int)i;
      }
    }
#ifdef DEBUG_LOG
    fprintf(stderr, "\n");
#endif
    // do the final allocation with determined muted cell
    std::unordered_set<int> slices_benefit;
    for (int cid = 0; cid < NB_CELLS; cid++) {
      if (cid == final_mute) {
        continue;
      }
      ueContext *ue_alloc = final_result.ues_scheduled[cid];
      int slice_alloc = final_result.slices_benefit[cid];
      slices_benefit.insert(slice_alloc);
      ue_alloc->allocateRBG(rbgid);
      all_cells[cid]->slice_rbgs_allocated_[slice_alloc] += 1;
    }
#ifdef DEBUG_LOG
    fprintf(stderr, "final_mute: %d nb_benefits: %lu\n", final_mute,
            slices_benefit.size());
#endif
    // deduct the quota of benefited slices in the muted cell
    if (final_mute != -1)
      for (auto it = slices_benefit.begin(); it != slices_benefit.end(); it++) {
        all_cells[final_mute]->slice_rbgs_share_[*it] -=
            1.0 / slices_benefit.size();
      }
#ifdef PROFILE_RUNTIME
    auto t5 = std::chrono::high_resolution_clock::now();
    total_time_t3_ +=
        std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();
#endif
  }
}