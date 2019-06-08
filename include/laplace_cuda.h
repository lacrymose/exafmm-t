#ifndef laplace_cuda_h
#define laplace_cuda_h
#include "exafmm_t.h"
#include "geometry.h"
#include <cuda_runtime.h>
#include "cufft.h"
#include "cublas_v2.h"
#include <thrust/host_vector.h>
#include <thrust/device_vector.h>

namespace exafmm_t {
  void cuda_init_drivers();

  void fmmStepsGPU(Nodes& nodes, std::vector<int> &leafs_idx, std::vector<real_t> &bodies_coord, std::vector<real_t> &nodes_pt_src, std::vector<int> &nodes_pt_src_idx, int ncrit, RealVec &upward_equiv, std::vector<std::vector<int>> &nodes_by_level_idx, std::vector<std::vector<int>> &parent_by_level_idx, std::vector<std::vector<int>> &octant_by_level_idx, std::vector<real_t> &nodes_coord, std::vector<int> &M2Lsources_idx, std::vector<int> &M2Ltargets_idx, RealVec &dnward_equiv, std::vector<real_t> &nodes_trg, std::vector<int> &nodes_depth, std::vector<int> &nodes_idx);

  std::vector<real_t> M2LGPU(std::vector<int> &M2Ltargets_idx, std::vector<int> &M2LRelPos_offset, std::vector<int> &index_in_up_equiv_fft, std::vector<int> &M2LRelPoss, RealVec mat_M2L_Helper, std::vector<int> &M2Lsources_idx, std::vector<real_t> &upward_equiv, std::vector<int> &M2Llist_idx_offset, std::vector<int> &M2Llist_idx, int max);

/*  void L2PGPU(std::vector<real_t> &dnwd_equiv_surf, RealVec &dnward_equiv, std::vector<real_t> &bodies_coord, std::vector<real_t> &nodes_trg, std::vector<int> &leafs_idx, std::vector<int> &nodes_pt_src_idx);
*/
  
  void L2PGPU(RealVec &equivCoord, RealVec &dnward_equiv, std::vector<real_t> &bodies_coord, std::vector<real_t> &nodes_trg, std::vector<int> &leafs_idx, std::vector<int> &nodes_pt_src_idx, int THREADS);


  void L2LGPU(Nodes &nodes, RealVec &dnward_equiv, std::vector<std::vector<int>> &nodes_by_level_idx, std::vector<std::vector<int>> &parent_by_level_idx, std::vector<std::vector<int>> &octant_by_level_idx);
}

#endif
