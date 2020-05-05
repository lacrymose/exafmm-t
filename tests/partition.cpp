#include "dataset.h"
#include "exafmm_t.h"
#include "partition.h"

using namespace exafmm_t;

int main(int argc, char **argv) {
  Args args(argc, argv);
  startMPI(argc, argv);

  int n = 1000;
  Bodies<real_t> sources = init_sources<real_t>(n, args.distribution, MPIRANK);
  Bodies<real_t> targets = init_targets<real_t>(n, args.distribution, MPIRANK+10);
  
  vec3 x0;
  real_t r0;
  allreduceBounds(sources, targets, x0, r0);
  std::vector<int> offset;
  partition(sources, targets, x0, r0, offset, args.maxlevel);
  std::cout << " sources size " << sources.size() 
            << " targets size " << targets.size()
            << " from proc " << MPIRANK << std::endl;

  writeBodies(sources);

  stopMPI();

  if (!MPIRANK) {
    std::cout << r0 << std::endl;
    std::cout << x0 << std::endl;
  }
}
