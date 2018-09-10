#ifndef traverse_h
#define traverse_h
#include "exafmm_t.h"
#include "profile.h"

namespace exafmm_t {
  void upwardPass(Nodes& nodes, std::vector<Node*>& leafs) {
    Profile::Tic("P2M", false, 5);
    P2M(leafs);
    Profile::Toc();
    Profile::Tic("M2M", false, 5);
    #pragma omp parallel
    #pragma omp single nowait
    M2M(&nodes[0]);
    Profile::Toc();
  }

  void downwardPass(Nodes& nodes, std::vector<Node*>& leafs) {
    Profile::Tic("P2L", false, 5);
    P2L(nodes);
    Profile::Toc();
    Profile::Tic("M2P", false, 5);
    M2P(leafs);
    Profile::Toc();
    Profile::Tic("P2P", false, 5);
    P2P(leafs);
    Profile::Toc();
    Profile::Tic("M2L", false, 5);
    M2L(M2Ldata, nodes);
    Profile::Toc();
    Profile::Tic("L2L", false, 5);
    #pragma omp parallel
    #pragma omp single nowait
    L2L(&nodes[0]);
    Profile::Toc();
    Profile::Tic("L2P", false, 5);
    L2P(leafs);
    Profile::Toc();
  }

  RealVec verify(std::vector<Node*>& leafs) {
    int numTargets = 10;
    int stride = leafs.size() / numTargets;
    Nodes targets;
    for(size_t i=0; i<numTargets; i++) {
      targets.push_back(*(leafs[i*stride]));
    }
    Nodes targets2 = targets;    // used for direct summation
#pragma omp parallel for
    for(size_t i=0; i<targets2.size(); i++) {
      Node *target = &targets2[i];
      std::fill(target->pt_trg.begin(), target->pt_trg.end(), 0.);
      for(size_t j=0; j<leafs.size(); j++) {
        gradientP2P(leafs[j]->pt_coord, leafs[j]->pt_src, target->pt_coord, target->pt_trg);
      }
    }
    real_t p_diff = 0, p_norm = 0, g_diff = 0, g_norm = 0;
    for(size_t i=0; i<targets.size(); i++) {
      p_norm += targets2[i].pt_trg[0] * targets2[i].pt_trg[0];
      p_diff += (targets2[i].pt_trg[0] - targets[i].pt_trg[0]) * (targets2[i].pt_trg[0] - targets[i].pt_trg[0]);
      for(int d=1; d<4; d++) {
        g_diff += (targets2[i].pt_trg[d] - targets[i].pt_trg[d]) * (targets2[i].pt_trg[d] - targets[i].pt_trg[d]);
        g_norm += targets2[i].pt_trg[d] * targets2[i].pt_trg[d];
      }
    }
    RealVec l2_error(2);
    l2_error[0] = sqrt(p_diff/p_norm);
    l2_error[1] = sqrt(g_diff/g_norm);
    return l2_error;
  }
}//end namespace
#endif