#include "scheduler_context.h"
#include "cell_context.h"
#include "ue_context.h"
#include <cassert>
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

void schedulerContext::newTTI(unsigned int tti) {
  for (int cid = 0; cid < NB_CELLS; cid++) {
    all_cells[cid]->newTTI(tti);
    all_cells[cid]->getAvgCost(cell_slice_cost[cid]);
  }
  // recalculate the quota
  for (int cid = 0; cid < NB_CELLS; cid++) {
    all_cells[cid]->calculateRBGsQuota();
  }
  for (int rbgid = 0; rbgid < NB_RBGS; rbgid++) {
    fprintf(stderr, "alloc_rbg: %d\n", rbgid);
    std::unordered_map<int, double> mutecell_benefits;
    for (int muteid = 0; muteid < NB_CELLS; muteid++) {
      fprintf(stderr, "mute_cell: %d\n", muteid);
      vector<double> slice_metrics(nb_slices_);
      vector<int> cell_to_slice(NB_CELLS);
      // mute the cell, update cqi, and calculate
      for (int cid = 0; cid < NB_CELLS; cid++) {
        if (cid == muteid)
          continue;
        all_cells[cid]->muteOneRBG(rbgid, muteid);
        all_cells[cid]->assignOneRBG(rbgid);
        int slice_alloc =
            all_cells[cid]->addScheduleMetric(slice_metrics, rbgid);
        cell_to_slice[cid] = slice_alloc;
      }
      // with the same slice constraint(under no muting)
      vector<double> slice_metrics_nomute(nb_slices_);
      for (int cid = 0; cid < NB_CELLS; cid++) {
        all_cells[cid]->muteOneRBG(rbgid, -1);
        all_cells[cid]->assignOneRBG(rbgid);
        int same_slice = cell_to_slice[cid];
        double metric =
            all_cells[cid]->getScheduleMetricGivenSid(same_slice, rbgid);
        slice_metrics_nomute[same_slice] += metric;
      }
      int nb_slices_benefit = 0;
      for (int i = 0; i < nb_slices_; i++) {
        if (slice_metrics[i] > 0) {
          nb_slices_benefit += 1;
        }
      }
      // we just check the complexity, so currently go with simple trade-off
      // approach. Every slice pays 1/n_benefits RBs in the muted cell
      fprintf(stderr, "metric, metric_nomute: ");
      for (int sid = 0; sid < nb_slices_; sid++) {
        fprintf(stderr, "(%f, %f)", slice_metrics[sid],
                slice_metrics_nomute[sid]);
        if (slice_metrics[sid] > 0) {
          assert(slice_metrics_nomute[sid] > 0);
          double benefit = slice_metrics[sid] - slice_metrics_nomute[sid];
          double cost = cell_slice_cost[muteid][sid] / nb_slices_benefit;
          mutecell_benefits[muteid] += benefit / cost;
        }
      }
      fprintf(stderr, "\n");
    }
    int final_mute = -1;
    double final_mute_benefit = 0;
    for (auto it = mutecell_benefits.begin(); it != mutecell_benefits.end();
         it++) {
      if (it->second > final_mute_benefit) {
        final_mute_benefit = it->second;
        final_mute = it->first;
      }
    }
    // do the final allocation with determined muted cell
    std::unordered_set<int> slices_benefit;
    // std::vector<int> slices_benefit;
    for (int cid = 0; cid < NB_CELLS; cid++) {
      if (cid == final_mute) {
        continue;
      }
      all_cells[cid]->muteOneRBG(rbgid, final_mute);
      all_cells[cid]->assignOneRBG(rbgid);
      int slice_alloc = all_cells[cid]->doAllocation(rbgid);
      slices_benefit.insert(slice_alloc);
    }
    fprintf(stderr, "final_mute: %d nb_benefits: %lu\n", final_mute,
            slices_benefit.size());
    for (auto it = slices_benefit.begin(); it != slices_benefit.end(); it++) {
      all_cells[final_mute]->slice_rbgs_share_[*it] -=
          1.0 / slices_benefit.size();
    }
  }
}