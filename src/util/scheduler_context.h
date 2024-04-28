#ifndef SCHEDULER_CONTEXT_H_
#define SCHEDULER_CONTEXT_H_
#include "cell_context.h"
#define NB_CELLS 5

class schedulerContext {
private:
  cellContext *all_cells[NB_CELLS];
  vector<vector<double>> cell_slice_cost;
  int nb_slices_;

public:
  schedulerContext(int nb_slices, int ues_per_slice);
  ~schedulerContext();
  void newTTI(unsigned int tti);
};

#endif