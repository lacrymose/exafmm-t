#include "build_tree.h"
#include "dataset.h"
#include "interaction_list.h"
#include "laplace_c.h"
#include "precompute_c.h"
#include "profile.h"

using namespace exafmm_t;

int main(int argc, char **argv) {
  Args args(argc, argv);
  omp_set_num_threads(args.threads);
  size_t N = args.numBodies;
  MULTIPOLE_ORDER = args.P;
  NSURF = 6*(MULTIPOLE_ORDER-1)*(MULTIPOLE_ORDER-1) + 2;
  Profile::Enable(true);
  
#if TEST_P2P
  int n = 20;
  RealVec src_coord(3*n), trg_coord(3*n);
  ComplexVec src_value(n), trg_value(n, complex_t(0.,0.));
  srand48(10);

  for(int i=0; i<n; ++i) {
    for(int d=0; d<3; ++d) {
      src_coord[3*i+d] = drand48();
      trg_coord[3*i+d] = drand48();
    } 
    src_value[i] = complex_t(drand48(), drand48());
  }

  potentialP2P(src_coord, src_value, trg_coord, trg_value);
  for(int i=0; i<n; ++i) {
    std::cout << trg_value[i] << std::endl;
  }
#endif

  initRelCoord();    // initialize relative coords
  Precompute();

#if TEST_PRECOMP
  std::cout << mat_M2L.size() << std::endl;
  for(int i=0; i<mat_M2L.size(); ++i) {
    int len = mat_M2L[i].size();
    for(int j=0; j<len; j+=3) {
      std::cout << mat_M2L[i][j] << std::endl;
    }
  }
#endif

#if TEST_SCGEMM
  std::cout << M2M_U.size() << std::endl;
  ComplexVec q(NSURF), result(NSURF);
  for(int i=0; i<NSURF; ++i) {
    q[i] = complex_t(drand48(), drand48()); 
  }
  gemm(1, NSURF, NSURF, &q[0], &M2M_U[0], &result[0]);
  for(int i=0; i<NSURF; ++i) {
    std::cout << result[i] << std::endl;
  }
#endif
  return 0;
}
