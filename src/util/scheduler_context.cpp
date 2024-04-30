#include "scheduler_context.h"
#include "ue_context.h"
#include "util.h"
#include <cassert>
#include <mutex>
#include <unordered_set>

schedulerContext::schedulerContext(int nb_slices, int ues_per_slice)
    : nb_slices_(nb_slices),
      boost_pool_(std::thread::hardware_concurrency() - 1) {
  for (int cid = 0; cid < NB_CELLS; cid++) {
    vector<double> slice_cost(nb_slices);
    cell_slice_cost.push_back(slice_cost);
    all_cells[cid] = new cellContext(nb_slices, ues_per_slice, cid);
  }
}

schedulerContext::~schedulerContext() {
  boost_pool_.join();
  for (int cid = 0; cid < NB_CELLS; cid++) {
    delete all_cells[cid];
  }
}

void schedulerContext::scheduleOneRBWithMute(int rbgid, int muteid,
                                             double *total_score) {
  *total_score = 0;
  vector<double> slice_metrics(nb_slices_);
  vector<int> cell_to_slice(NB_CELLS);
  for (int cid = 0; cid < NB_CELLS; cid++) {
    if (cid == muteid)
      continue;
    all_cells[cid]->muteOneRBG(rbgid, muteid);
    all_cells[cid]->assignOneRBG(rbgid, muteid);
    int slice_alloc =
        all_cells[cid]->addScheduleMetric(slice_metrics, rbgid, muteid);
    cell_to_slice[cid] = slice_alloc;
  }
  // with the same slice constraint(under no muting)
  vector<double> slice_metrics_nomute(nb_slices_);
  for (int cid = 0; cid < NB_CELLS; cid++) {
    all_cells[cid]->muteOneRBG(rbgid, -1);
    all_cells[cid]->assignOneRBG(rbgid, -1);
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
  if (muteid == 0)
    fprintf(stderr, "metrics of muting cell0: ");
  for (int sid = 0; sid < nb_slices_; sid++) {
    if (muteid == 0)
      fprintf(stderr, "(%f, %f)", slice_metrics[sid],
              slice_metrics_nomute[sid]);
    if (slice_metrics[sid] > 0) {
      assert(slice_metrics_nomute[sid] > 0);
      double benefit = slice_metrics[sid] - slice_metrics_nomute[sid];
      double cost = cell_slice_cost[muteid][sid] / nb_slices_benefit;
      *total_score += benefit / cost;
    }
  }
  if (muteid == 0)
    fprintf(stderr, " score: %f \n", *total_score);
  {
    std::unique_lock<std::mutex> lock(pending_task_mutex);
    finished_task += 1;
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
    std::vector<double> mutecell_score(NB_CELLS);
    for (int muteid = 0; muteid < NB_CELLS; muteid++) {
      boost::asio::post(
          boost_pool_, std::bind(&schedulerContext::scheduleOneRBWithMute, this,
                                 rbgid, muteid, &mutecell_score[muteid]));
    }
    // wait until all jobs finish
    while (true) {
      std::unique_lock<std::mutex> lock(pending_task_mutex);
      if (finished_task == NB_CELLS)
        break;
    }
    finished_task = 0;
    int final_mute = -1;
    double final_mute_benefit = 0;
    fprintf(stderr, "mute_scores: ");
    for (size_t i = 0; i < mutecell_score.size(); i++) {
      fprintf(stderr, " %f, ", mutecell_score[i]);
      if (mutecell_score[i] > final_mute_benefit) {
        final_mute_benefit = mutecell_score[i];
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
      all_cells[cid]->muteOneRBG(rbgid, final_mute);
      all_cells[cid]->assignOneRBG(rbgid, final_mute);
      int slice_alloc = all_cells[cid]->doAllocation(rbgid, final_mute);
      slices_benefit.insert(slice_alloc);
    }
    fprintf(stderr, "final_mute: %d nb_benefits: %lu\n", final_mute,
            slices_benefit.size());
    // deduct the quota of benefited slices in the muted cell
    if (final_mute != -1)
      for (auto it = slices_benefit.begin(); it != slices_benefit.end(); it++) {
        all_cells[final_mute]->slice_rbgs_share_[*it] -=
            1.0 / slices_benefit.size();
      }
  }
}