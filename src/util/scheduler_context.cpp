#include "scheduler_context.h"
#include "ue_context.h"
#include "util.h"
#include <cassert>
#include <chrono>
#include <mutex>
#include <ratio>
#include <unordered_set>

schedulerContext::schedulerContext(int nb_slices, int ues_per_slice)
    : nb_slices_(nb_slices) {
  for (int cid = 0; cid < NB_CELLS; cid++) {
    vector<double> slice_cost(nb_slices);
    cell_slice_cost.push_back(slice_cost);
    all_cells[cid] = new cellContext(nb_slices, ues_per_slice, cid);
  }
}

schedulerContext::~schedulerContext() {
  for (int cid = 0; cid < NB_CELLS; cid++) {
    delete all_cells[cid];
  }
}

void schedulerContext::scheduleOneRBWithMute(int rbgid, int muteid,
                                             muteScheduleResult *result) {
  vector<double> slice_metrics(nb_slices_);
  vector<double> slice_metrics_nomute(nb_slices_);
  vector<int> cell_to_slice(NB_CELLS);
  result->score = 0;
  for (int cid = 0; cid < NB_CELLS; cid++) {
    if (cid == muteid)
      continue;
    all_cells[cid]->muteOneRBG(rbgid, muteid);
    all_cells[cid]->assignOneRBG(rbgid, muteid);
    auto it = all_cells[cid]->addScheduleMetric(slice_metrics, rbgid, muteid);
    cell_to_slice[cid] = it.first;
    result->slices_benefit[cid] = it.first;
    result->ues[cid] = it.second;
  }
  // with the same slice constraint(under no muting)
  for (int cid = 0; cid < NB_CELLS; cid++) {
    all_cells[cid]->muteOneRBG(rbgid, -1);
    all_cells[cid]->assignOneRBG(rbgid, -1);
    int same_slice = cell_to_slice[cid];
    double metric =
        all_cells[cid]->getScheduleMetricGivenSid(same_slice, rbgid);
    slice_metrics_nomute[same_slice] += metric;
  }
  if (muteid == 0)
    fprintf(stderr, "metrics of muting cell0: ");
  for (int sid = 0; sid < nb_slices_; sid++) {
    if (muteid == 0)
      fprintf(stderr, "(%f, %f)", slice_metrics[sid],
              slice_metrics_nomute[sid]);
    if (slice_metrics[sid] > 0) {
      assert(slice_metrics_nomute[sid] > 0);
      double benefit = slice_metrics[sid] - slice_metrics_nomute[sid];
      double cost = cell_slice_cost[muteid][sid];
      result->score += benefit / cost;
    }
  }
}

void schedulerContext::newTTI(unsigned int tti) {
  auto t1 = std::chrono::high_resolution_clock::now();
  for (int cid = 0; cid < NB_CELLS; cid++) {
    all_cells[cid]->newTTI(tti);
    all_cells[cid]->getAvgCost(cell_slice_cost[cid]);
  }
  // recalculate the quota
  for (int cid = 0; cid < NB_CELLS; cid++) {
    all_cells[cid]->calculateRBGsQuota();
  }
  auto t2 = std::chrono::high_resolution_clock::now();
  total_time_t1_ +=
      std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
  std::vector<muteScheduleResult> mutecell_result(NB_CELLS);
  for (int rbgid = 0; rbgid < NB_RBGS; rbgid++) {
    fprintf(stderr, "alloc_rbg: %d\n", rbgid);
    auto t3 = std::chrono::high_resolution_clock::now();

    for (int muteid = 0; muteid < NB_CELLS; muteid++) {
      scheduleOneRBWithMute(rbgid, muteid, &mutecell_result[muteid]);
    }
    auto t4 = std::chrono::high_resolution_clock::now();
    total_time_t2_ +=
        std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
    int final_mute = -1;
    muteScheduleResult final_result;
    final_result.score = 0;
    double final_mute_benefit = 0;
    fprintf(stderr, "mute_scores: ");
    for (size_t i = 0; i < mutecell_result.size(); i++) {
      fprintf(stderr, " %f, ", mutecell_result[i].score);
      if (mutecell_result[i].score > final_result.score) {
        final_result = mutecell_result[i];
        final_mute = (int)i;
      }
    }
    fprintf(stderr, "\n");
    // do the final allocation with determined muted cell
    std::unordered_set<int> slices_benefit;
    for (int cid = 0; cid < NB_CELLS; cid++) {
      if (cid == final_mute) {
        continue;
      }
      ueContext *ue_alloc = final_result.ues[cid];
      int slice_alloc = final_result.slices_benefit[cid];
      slices_benefit.insert(slice_alloc);
      ue_alloc->allocateRBG(rbgid);
      all_cells[cid]->slice_rbgs_allocated_[slice_alloc] += 1;
    }
    fprintf(stderr, "final_mute: %d nb_benefits: %lu\n", final_mute,
            slices_benefit.size());
    // deduct the quota of benefited slices in the muted cell
    if (final_mute != -1)
      for (auto it = slices_benefit.begin(); it != slices_benefit.end(); it++) {
        all_cells[final_mute]->slice_rbgs_share_[*it] -=
            1.0 / slices_benefit.size();
      }
    auto t5 = std::chrono::high_resolution_clock::now();
    total_time_t3_ +=
        std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();
  }
}