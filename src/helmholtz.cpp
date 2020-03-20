#include <cstring>  // std::memset()
#include <fstream>  // std::ifstream
#include <set>      // std::set
#include "helmholtz.h"
#include "math_wrapper.h"

namespace exafmm_t {
#if 0
  // non-simd P2P
  void potential_P2P(RealVec& src_coord, ComplexVec& src_value, RealVec& trg_coord, ComplexVec& trg_value) {
    complex_t I(0, 1);
    //complex_t wavek = complex_t(1,.1) / real_t(2*PI);
    real_t wavek = 20*PI;
    int src_cnt = src_coord.size() / 3;
    int trg_cnt = trg_coord.size() / 3;
    for (int i=0; i<trg_cnt; i++) {
      complex_t p = complex_t(0., 0.);
      cvec3 F = complex_t(0., 0.);
      for (int j=0; j<src_cnt; j++) {
        vec3 dX;
        for (int d=0; d<3; d++) dX[d] = trg_coord[3*i+d] - src_coord[3*j+d];
        real_t R2 = norm(dX);
        if (R2 != 0) {
          // real_t R = std::sqrt(R2);
          // newton iteration
          real_t invR = 1 / std::sqrt(R2);  // initial guess
          invR *= 3 - R2*invR*invR;
          invR *= 12 - R2*invR*invR;        // two iterations
          invR /= 16;                       // add back the coefficient in Newton iterations
          real_t R = 1 / invR;
          complex_t pij = std::exp(I * R * wavek) * src_value[j] / R;
          complex_t coef = (1/R2 - I*wavek/R) * pij;
          p += pij;
        }
      }
      trg_value[i] += p / (4*PI);
    }
  }

  void gradient_P2P(RealVec& src_coord, ComplexVec& src_value, RealVec& trg_coord, ComplexVec& trg_value) {
    complex_t I(0, 1);
    //complex_t wavek = complex_t(1,.1) / real_t(2*PI);
    real_t wavek = 20*PI;
    int src_cnt = src_coord.size() / 3;
    int trg_cnt = trg_coord.size() / 3;
    for (int i=0; i<trg_cnt; i++) {
      complex_t p = complex_t(0., 0.);
      cvec3 F = complex_t(0., 0.);
      for (int j=0; j<src_cnt; j++) {
        vec3 dX;
        for (int d=0; d<3; d++) dX[d] = trg_coord[3*i+d] - src_coord[3*j+d];
        real_t R2 = norm(dX);
        if (R2 != 0) {
          // real_t R = std::sqrt(R2);
          // newton iteration
          real_t invR = 1 / std::sqrt(R2);  // initial guess
          invR *= 3 - R2*invR*invR;
          invR *= 12 - R2*invR*invR;        // two iterations
          invR /= 16;                       // add back the coefficient in Newton iterations
          real_t R = 1 / invR;
          complex_t pij = std::exp(I * R * wavek) * src_value[j] / R;
          complex_t coef = (1/R2 - I*wavek/R) * pij;
          p += pij;
          for (int d=0; d<3; d++) {
            F[d] += coef * dX[d];
          }
        }
      }
      trg_value[4*i+0] += p / (4*PI);
      trg_value[4*i+1] += F[0] / (4*PI);
      trg_value[4*i+2] += F[1] / (4*PI);
      trg_value[4*i+3] += F[2] / (4*PI);
    }
  }
#endif 

  void HelmholtzFMM::initialize_matrix() {
    matrix_UC2E_V.resize(depth+1);
    matrix_UC2E_U.resize(depth+1);
    matrix_DC2E_V.resize(depth+1);
    matrix_DC2E_U.resize(depth+1);
    matrix_M2M.resize(depth+1);
    matrix_L2L.resize(depth+1);
    for(int level = 0; level <= depth; level++) {
      matrix_UC2E_V[level].resize(nsurf*nsurf);
      matrix_UC2E_U[level].resize(nsurf*nsurf);
      matrix_DC2E_V[level].resize(nsurf*nsurf);
      matrix_DC2E_U[level].resize(nsurf*nsurf);
      matrix_M2M[level].resize(REL_COORD[M2M_Type].size(), ComplexVec(nsurf*nsurf));
      matrix_L2L[level].resize(REL_COORD[L2L_Type].size(), ComplexVec(nsurf*nsurf));
    }
  }

  void HelmholtzFMM::precompute_check2equiv() {
    real_t c[3] = {0, 0, 0};
    for(int level = 0; level <= depth; level++) {
      // caculate matrix_UC2E_U and matrix_UC2E_V
      RealVec up_check_surf = surface(p, r0, level, c, 2.95);
      RealVec up_equiv_surf = surface(p, r0, level, c, 1.05);
      ComplexVec matrix_c2e(nsurf*nsurf);
      kernel_matrix(&up_check_surf[0], nsurf, &up_equiv_surf[0], nsurf, &matrix_c2e[0]);
      RealVec S(nsurf*nsurf);
      ComplexVec S_(nsurf*nsurf);
      ComplexVec U(nsurf*nsurf), VH(nsurf*nsurf);
      svd(nsurf, nsurf, &matrix_c2e[0], &S[0], &U[0], &VH[0]);
      // inverse S
      real_t max_S = 0;
      for(int i=0; i<nsurf; i++) {
        max_S = fabs(S[i*nsurf+i])>max_S ? fabs(S[i*nsurf+i]) : max_S;
      }
      for(int i=0; i<nsurf; i++) {
        S[i*nsurf+i] = S[i*nsurf+i]>EPS*max_S*4 ? 1.0/S[i*nsurf+i] : 0.0;
      }
      for(size_t i=0; i<S.size(); ++i) {
        S_[i] = S[i];
      }
      // save matrix
      ComplexVec V = conjugate_transpose(VH, nsurf, nsurf);
      ComplexVec UH = conjugate_transpose(U, nsurf, nsurf);
      matrix_UC2E_U[level] = UH;
      gemm(nsurf, nsurf, nsurf, &V[0], &S_[0], &(matrix_UC2E_V[level][0]));

      matrix_DC2E_U[level] = transpose(V, nsurf, nsurf);
      ComplexVec UTH = transpose(UH, nsurf, nsurf);
      gemm(nsurf, nsurf, nsurf, &UTH[0], &S_[0], &(matrix_DC2E_V[level][0]));
    }
  }

  void HelmholtzFMM::precompute_M2M() {
    real_t parent_coord[3] = {0, 0, 0};
    for(int level = 0; level <= depth; level++) {
      RealVec parent_up_check_surf = surface(p, r0, level, parent_coord, 2.95);
      real_t s = r0 * powf(0.5, level+1);
      int num_coords = REL_COORD[M2M_Type].size();
#pragma omp parallel for
      for(int i=0; i<num_coords; i++) {
        ivec3& coord = REL_COORD[M2M_Type][i];
        real_t child_coord[3] = {parent_coord[0] + coord[0]*s,
                                 parent_coord[1] + coord[1]*s,
                                 parent_coord[2] + coord[2]*s};
        RealVec child_up_equiv_surf = surface(p, r0, level+1, child_coord, 1.05);
        ComplexVec matrix_pc2ce(nsurf*nsurf);
        kernel_matrix(&parent_up_check_surf[0], nsurf, &child_up_equiv_surf[0], nsurf, &matrix_pc2ce[0]);
        // M2M: child's upward_equiv to parent's check
        ComplexVec buffer(nsurf*nsurf);
        gemm(nsurf, nsurf, nsurf, &(matrix_UC2E_U[level][0]), &matrix_pc2ce[0], &buffer[0]);
        gemm(nsurf, nsurf, nsurf, &(matrix_UC2E_V[level][0]), &buffer[0], &(matrix_M2M[level][i][0]));
        // L2L: parent's dnward_equiv to child's check, reuse surface coordinates
        matrix_pc2ce = transpose(matrix_pc2ce, nsurf, nsurf);
        gemm(nsurf, nsurf, nsurf, &matrix_pc2ce[0], &(matrix_DC2E_V[level][0]), &buffer[0]);
        gemm(nsurf, nsurf, nsurf, &buffer[0], &(matrix_DC2E_U[level][0]), &(matrix_L2L[level][i][0]));
      }
    }
  }

  void HelmholtzFMM::precompute_M2L(std::ofstream& file, std::vector<std::vector<int>>& parent2child) {
    int n1 = p * 2;
    int n3 = n1 * n1 * n1;
    int fft_size = 2 * n3 * NCHILD * NCHILD;
    std::vector<RealVec> matrix_M2L_Helper(REL_COORD[M2L_Helper_Type].size(), RealVec(2*n3));
    std::vector<AlignedVec> matrix_M2L(REL_COORD[M2L_Type].size(), AlignedVec(fft_size));
    // create fftw plan
    ComplexVec fftw_in(n3);
    RealVec fftw_out(2*n3);
    int dim[3] = {2*p, 2*p, 2*p};
    fft_plan plan = fft_plan_dft(3, dim,
                                reinterpret_cast<fft_complex*>(fftw_in.data()), 
                                reinterpret_cast<fft_complex*>(fftw_out.data()),
                                FFTW_FORWARD, FFTW_ESTIMATE);
    // Precompute M2L matrix
    RealVec trg_coord(3,0);
    for(int l=1; l<depth+1; ++l) {
      // compute DFT of potentials at convolution grids
#pragma omp parallel for
      for(size_t i=0; i<REL_COORD[M2L_Helper_Type].size(); i++) {
        real_t coord[3];
        for(int d=0; d<3; d++) {
          coord[d] = REL_COORD[M2L_Helper_Type][i][d] * r0 * powf(0.5, l-1);
        }
        RealVec conv_coord = convolution_grid(p, r0, l, coord);   // convolution grid
        ComplexVec conv_p(n3);   // potentials on convolution grid
        kernel_matrix(conv_coord.data(), n3, trg_coord.data(), 1, conv_p.data());
        fft_execute_dft(plan, reinterpret_cast<fft_complex*>(conv_p.data()),
                              reinterpret_cast<fft_complex*>(matrix_M2L_Helper[i].data()));
      }
      // convert M2L_Helper to M2L, reorder to improve data locality
#pragma omp parallel for
      for(size_t i=0; i<REL_COORD[M2L_Type].size(); ++i) {
        for(int j=0; j<NCHILD*NCHILD; j++) {   // loop over child's relative positions
          int child_rel_idx = parent2child[i][j];
          if (child_rel_idx != -1) {
            for(int k=0; k<n3; k++) {   // loop over frequencies
              int new_idx = k*(2*NCHILD*NCHILD) + 2*j;
              matrix_M2L[i][new_idx+0] = matrix_M2L_Helper[child_rel_idx][k*2+0] / n3;   // real
              matrix_M2L[i][new_idx+1] = matrix_M2L_Helper[child_rel_idx][k*2+1] / n3;   // imag
            }
          }
        }
      }
      // write to file
      for(auto& vec : matrix_M2L) {
        file.write(reinterpret_cast<char*>(vec.data()), fft_size*sizeof(real_t));
      }
    }
    // destroy fftw plan
    fft_destroy_plan(plan);
  }

  bool HelmholtzFMM::load_matrix() {
    std::ifstream file(filename, std::ifstream::binary);
    int n1 = p * 2;
    int n3 = n1 * n1 * n1;
    size_t fft_size = n3 * 2 * NCHILD * NCHILD;
    size_t file_size = (2*REL_COORD[M2M_Type].size()+4) * nsurf * nsurf * (depth+1) * sizeof(complex_t)
                     +  REL_COORD[M2L_Type].size() * fft_size * depth * sizeof(real_t) + 2 * sizeof(real_t);    // 2 denotes r0 and wavek
    if (file.good()) {     // if file exists
      file.seekg(0, file.end);
      if (size_t(file.tellg()) == file_size) {   // if file size is correct
        file.seekg(0, file.beg);         // move the position back to the beginning
        // check whether r0 matches
        real_t r0_;
        file.read(reinterpret_cast<char*>(&r0_), sizeof(real_t));
        if (r0 != r0_) {
          std::cout << r0 << " " << r0_ << std::endl;
          return false;
        }
        // check whether wavek matches
        real_t wavek_;
        file.read(reinterpret_cast<char*>(&wavek_), sizeof(real_t));
        if (wavek != wavek_) {
          std::cout << wavek << " " << wavek_ << std::endl;
          return false;
        }
        size_t size = nsurf * nsurf;
        for(int level = 0; level <= depth; level++) {
          // UC2E, DC2E
          file.read(reinterpret_cast<char*>(&matrix_UC2E_U[level][0]), size*sizeof(complex_t));
          file.read(reinterpret_cast<char*>(&matrix_UC2E_V[level][0]), size*sizeof(complex_t));
          file.read(reinterpret_cast<char*>(&matrix_DC2E_U[level][0]), size*sizeof(complex_t));
          file.read(reinterpret_cast<char*>(&matrix_DC2E_V[level][0]), size*sizeof(complex_t));
          // M2M, L2L
          for(auto & vec : matrix_M2M[level]) {
            file.read(reinterpret_cast<char*>(&vec[0]), size*sizeof(complex_t));
          }
          for(auto & vec : matrix_L2L[level]) {
            file.read(reinterpret_cast<char*>(&vec[0]), size*sizeof(complex_t));
          }
        }
        file.close();
        return true;
      } else {
        return false;
      }
    } else {
      return false;
    }
  }

  void HelmholtzFMM::save_matrix(std::ofstream& file) {
    // root radius r0
    file.write(reinterpret_cast<char*>(&r0), sizeof(real_t));
    // wavenumber wavek
    file.write(reinterpret_cast<char*>(&wavek), sizeof(real_t));
    size_t size = nsurf*nsurf;
    for(int level = 0; level <= depth; level++) {
      // save UC2E, DC2E precompute data
      file.write(reinterpret_cast<char*>(&matrix_UC2E_U[level][0]), size*sizeof(complex_t));
      file.write(reinterpret_cast<char*>(&matrix_UC2E_V[level][0]), size*sizeof(complex_t));
      file.write(reinterpret_cast<char*>(&matrix_DC2E_U[level][0]), size*sizeof(complex_t));
      file.write(reinterpret_cast<char*>(&matrix_DC2E_V[level][0]), size*sizeof(complex_t));
      // M2M, M2L precompute data
      for(auto & vec : matrix_M2M[level]) {
        file.write(reinterpret_cast<char*>(&vec[0]), size*sizeof(complex_t));
      }
      for(auto & vec : matrix_L2L[level]) {
        file.write(reinterpret_cast<char*>(&vec[0]), size*sizeof(complex_t));
      }
    }
  }

  void HelmholtzFMM::precompute() {
    // if matrix binary file exists
    filename = "helmholtz";
    filename += "_" + std::string(sizeof(real_t)==4 ? "f":"d") + "_" + "p" + std::to_string(p) + "_" + "l" + std::to_string(depth);
    filename += ".dat";
    initialize_matrix();
    if (load_matrix()) {
      is_precomputed = false;
      return;
    } else {
      precompute_check2equiv();
      precompute_M2M();
      std::remove(filename.c_str());
      std::ofstream file(filename, std::ofstream::binary);
      save_matrix(file);
      auto parent2child = map_matrix_index();
      precompute_M2L(file, parent2child);
      file.close();
    }
  }

  void HelmholtzFMM::M2L_setup(NodePtrs_t& nonleafs) {
    int n1 = p * 2;
    int n3 = n1 * n1 * n1;
    size_t mat_cnt = REL_COORD[M2L_Type].size();
    // initialize m2ldata
    m2ldata.resize(depth);
    // construct M2L target nodes for each level
    std::vector<NodePtrs_t> nodes_out(depth);
    for(size_t i=0; i<nonleafs.size(); i++) {
      nodes_out[nonleafs[i]->level].push_back(nonleafs[i]);
    }
    // prepare for m2ldata for each level
    for(int l=0; l<depth; l++) {
      // construct M2L source nodes for current level
      std::set<Node_t*> nodes_in_;
      for(size_t i=0; i<nodes_out[l].size(); i++) {
        NodePtrs_t& M2L_list = nodes_out[l][i]->M2L_list;
        for(size_t k=0; k<mat_cnt; k++) {
          if(M2L_list[k])
            nodes_in_.insert(M2L_list[k]);
        }
      }
      NodePtrs_t nodes_in;
      for(std::set<Node_t*>::iterator node=nodes_in_.begin(); node!=nodes_in_.end(); node++) {
        nodes_in.push_back(*node);
      }
      // prepare fft displ
      std::vector<size_t> fft_offset(nodes_in.size());       // displacement in all_up_equiv
      std::vector<size_t> ifft_offset(nodes_out[l].size());  // displacement in all_dn_equiv
      for(size_t i=0; i<nodes_in.size(); i++) {
        fft_offset[i] = nodes_in[i]->children[0]->idx * nsurf;
      }
      for(size_t i=0; i<nodes_out[l].size(); i++) {
        ifft_offset[i] = nodes_out[l][i]->children[0]->idx * nsurf;
      }
      // calculate interaction_offset_f & interaction_count_offset
      std::vector<size_t> interaction_offset_f;
      std::vector<size_t> interaction_count_offset;
      for(size_t i=0; i<nodes_in.size(); i++) {
        nodes_in[i]->idx_M2L = i;  // node_id: node's index in nodes_in list
      }
      size_t n_blk1 = nodes_out[l].size() * sizeof(real_t) / CACHE_SIZE;
      if(n_blk1==0) n_blk1 = 1;
      size_t interaction_count_offset_ = 0;
      size_t fftsize = 2 * 8 * n3;
      for(size_t blk1=0; blk1<n_blk1; blk1++) {
        size_t blk1_start=(nodes_out[l].size()* blk1   )/n_blk1;
        size_t blk1_end  =(nodes_out[l].size()*(blk1+1))/n_blk1;
        for(size_t k=0; k<mat_cnt; k++) {
          for(size_t i=blk1_start; i<blk1_end; i++) {
            NodePtrs_t& M2L_list = nodes_out[l][i]->M2L_list;
            if(M2L_list[k]) {
              interaction_offset_f.push_back(M2L_list[k]->idx_M2L * fftsize);   // node_in's displacement in fft_in
              interaction_offset_f.push_back(        i           * fftsize);   // node_out's displacement in fft_out
              interaction_count_offset_++;
            }
          }
          interaction_count_offset.push_back(interaction_count_offset_);
        }
      }
      m2ldata[l].fft_offset     = fft_offset;
      m2ldata[l].ifft_offset    = ifft_offset;
      m2ldata[l].interaction_offset_f = interaction_offset_f;
      m2ldata[l].interaction_count_offset = interaction_count_offset;
    }
  }

  void HelmholtzFMM::hadamard_product(std::vector<size_t>& interaction_count_offset, std::vector<size_t>& interaction_offset_f,
                       AlignedVec& fft_in, AlignedVec& fft_out, std::vector<AlignedVec>& matrix_M2L) {
    int n1 = p * 2;
    int n3 = n1 * n1 * n1;
    size_t fftsize = 2 * NCHILD * n3;
    AlignedVec zero_vec0(fftsize, 0.);
    AlignedVec zero_vec1(fftsize, 0.);

    size_t mat_cnt = matrix_M2L.size();
    size_t blk1_cnt = interaction_count_offset.size()/mat_cnt;
    int BLOCK_SIZE = CACHE_SIZE * 2 / sizeof(real_t);
    std::vector<real_t*> IN_(BLOCK_SIZE*blk1_cnt*mat_cnt);
    std::vector<real_t*> OUT_(BLOCK_SIZE*blk1_cnt*mat_cnt);

    // initialize fft_out with zero
    #pragma omp parallel for
    for(size_t i=0; i<fft_out.capacity()/fftsize; ++i) {
      std::memset(fft_out.data()+i*fftsize, 0, fftsize*sizeof(real_t));
    }
    
    #pragma omp parallel for
    for(size_t interac_blk1=0; interac_blk1<blk1_cnt*mat_cnt; interac_blk1++) {
      size_t interaction_count_offset0 = (interac_blk1==0?0:interaction_count_offset[interac_blk1-1]);
      size_t interaction_count_offset1 =                    interaction_count_offset[interac_blk1  ] ;
      size_t interac_cnt  = interaction_count_offset1-interaction_count_offset0;
      for(size_t j=0; j<interac_cnt; j++) {
        IN_ [BLOCK_SIZE*interac_blk1 +j] = &fft_in[interaction_offset_f[(interaction_count_offset0+j)*2+0]];
        OUT_[BLOCK_SIZE*interac_blk1 +j] = &fft_out[interaction_offset_f[(interaction_count_offset0+j)*2+1]];
      }
      IN_ [BLOCK_SIZE*interac_blk1 +interac_cnt] = &zero_vec0[0];
      OUT_[BLOCK_SIZE*interac_blk1 +interac_cnt] = &zero_vec1[0];
    }

    for(size_t blk1=0; blk1<blk1_cnt; blk1++) {
    #pragma omp parallel for
      for(int k=0; k<n3; k++) {
        for(size_t mat_indx=0; mat_indx< mat_cnt; mat_indx++) {
          size_t interac_blk1 = blk1*mat_cnt+mat_indx;
          size_t interaction_count_offset0 = (interac_blk1==0?0:interaction_count_offset[interac_blk1-1]);
          size_t interaction_count_offset1 =                    interaction_count_offset[interac_blk1  ] ;
          size_t interac_cnt  = interaction_count_offset1-interaction_count_offset0;
          real_t** IN = &IN_[BLOCK_SIZE*interac_blk1];
          real_t** OUT= &OUT_[BLOCK_SIZE*interac_blk1];
          real_t* M = &matrix_M2L[mat_indx][k*2*NCHILD*NCHILD]; // k-th freq's (row) offset in matrix_M2L
          for(size_t j=0; j<interac_cnt; j+=2) {
            real_t* M_   = M;
            real_t* IN0  = IN [j+0] + k*NCHILD*2;   // go to k-th freq chunk
            real_t* IN1  = IN [j+1] + k*NCHILD*2;
            real_t* OUT0 = OUT[j+0] + k*NCHILD*2;
            real_t* OUT1 = OUT[j+1] + k*NCHILD*2;
            matmult_8x8x2(M_, IN0, IN1, OUT0, OUT1);
          }
        }
      }
    }
  }

  void HelmholtzFMM::fft_up_equiv(std::vector<size_t>& fft_offset, ComplexVec& all_up_equiv, AlignedVec& fft_in) {
    int n1 = p * 2;
    int n3 = n1 * n1 * n1;
    std::vector<size_t> map(nsurf);
    real_t c[3]= {0.5, 0.5, 0.5};
    for(int d=0; d<3; d++) c[d] += 0.5*(p-2);
    RealVec surf = surface(p, r0, 0, c, (real_t)(p-1), true);
    for(size_t i=0; i<map.size(); i++) {
      map[i] = ((size_t)(p-1-surf[i*3]+0.5))
             + ((size_t)(p-1-surf[i*3+1]+0.5)) * n1
             + ((size_t)(p-1-surf[i*3+2]+0.5)) * n1 * n1;
    }

    size_t fftsize = 2 * NCHILD * n3;
    ComplexVec fftw_in(n3*NCHILD);
    AlignedVec fftw_out(fftsize);
    int dim[3] = {n1, n1, n1};

    fft_plan plan = fft_plan_many_dft(3, dim, NCHILD, reinterpret_cast<fft_complex*>(&fftw_in[0]),
                                      nullptr, 1, n3, (fft_complex*)(&fftw_out[0]), nullptr, 1, n3, 
                                      FFTW_FORWARD, FFTW_ESTIMATE);

    #pragma omp parallel for
    for(size_t node_idx=0; node_idx<fft_offset.size(); node_idx++) {
      RealVec buffer(fftsize, 0);
      ComplexVec equiv_t(NCHILD*n3, complex_t(0.,0.));

      complex_t* up_equiv = &all_up_equiv[fft_offset[node_idx]];  // offset ptr of node's 8 child's up_equiv in all_up_equiv, size=8*nsurf
      real_t* up_equiv_f = &fft_in[fftsize*node_idx];   // offset ptr of node_idx in fft_in vector, size=fftsize

      for(int k=0; k<nsurf; k++) {
        size_t idx = map[k];
        for(int j0=0; j0<NCHILD; j0++)
          equiv_t[idx+j0*n3] = up_equiv[j0*nsurf+k];
      }
      fft_execute_dft(plan, reinterpret_cast<fft_complex*>(&equiv_t[0]), (fft_complex*)&buffer[0]);
      for(int j=0; j<n3; j++) {
        for(int k=0; k<NCHILD; k++) {
          up_equiv_f[2*(NCHILD*j+k)+0] = buffer[2*(n3*k+j)+0];
          up_equiv_f[2*(NCHILD*j+k)+1] = buffer[2*(n3*k+j)+1];
        }
      }
    }
    fft_destroy_plan(plan);
  }

  void HelmholtzFMM::ifft_dn_check(std::vector<size_t>& ifft_offset, AlignedVec& fft_out, ComplexVec& all_dn_equiv) {
    int n1 = p * 2;
    int n3 = n1 * n1 * n1;
    std::vector<size_t> map(nsurf);
    real_t c[3]= {0.5, 0.5, 0.5};
    for(int d=0; d<3; d++) c[d] += 0.5*(p-2);
    RealVec surf = surface(p, r0, 0, c, (real_t)(p-1), true);
    for(size_t i=0; i<map.size(); i++) {
      map[i] = ((size_t)(p*2-0.5-surf[i*3]))
             + ((size_t)(p*2-0.5-surf[i*3+1])) * n1
             + ((size_t)(p*2-0.5-surf[i*3+2])) * n1 * n1;
    }

    size_t fftsize = 2 * NCHILD * n3;
    AlignedVec fftw_in(fftsize);
    ComplexVec fftw_out(n3*NCHILD);
    int dim[3] = {n1, n1, n1};

    fft_plan plan = fft_plan_many_dft(3, dim, NCHILD, (fft_complex*)(&fftw_in[0]), nullptr, 1, n3, 
                                      reinterpret_cast<fft_complex*>(&fftw_out[0]), nullptr, 1, n3, 
                                      FFTW_BACKWARD, FFTW_ESTIMATE);

    #pragma omp parallel for
    for(size_t node_idx=0; node_idx<ifft_offset.size(); node_idx++) {
      RealVec buffer0(fftsize, 0);
      ComplexVec buffer1(NCHILD*n3, 0);
      real_t* dn_check_f = &fft_out[fftsize*node_idx];  // offset ptr for node_idx in fft_out vector, size=fftsize
      complex_t* dn_equiv = &all_dn_equiv[ifft_offset[node_idx]];  // offset ptr for node_idx's child's dn_equiv in all_dn_equiv, size=numChilds * nsurf
      for(int j=0; j<n3; j++)
        for(int k=0; k<NCHILD; k++) {
          buffer0[2*(n3*k+j)+0] = dn_check_f[2*(NCHILD*j+k)+0];
          buffer0[2*(n3*k+j)+1] = dn_check_f[2*(NCHILD*j+k)+1];
        }
      fft_execute_dft(plan, (fft_complex*)&buffer0[0], reinterpret_cast<fft_complex*>(&buffer1[0]));
      for(int k=0; k<nsurf; k++) {
        size_t idx = map[k];
        for(int j0=0; j0<NCHILD; j0++)
          dn_equiv[nsurf*j0+k]+=buffer1[idx+j0*n3];
      }
    }
    fft_destroy_plan(plan);
  }
  
  void HelmholtzFMM::M2L(Nodes_t& nodes) {
    int n1 = p * 2;
    int n3 = n1 * n1 * n1;
    int fft_size = 2 * 8 * n3;
    int num_nodes = nodes.size();
    int num_coords = REL_COORD[M2L_Type].size();   // number of relative coords for M2L_Type

    ComplexVec all_up_equiv, all_dn_equiv;
    all_up_equiv.reserve(num_nodes*nsurf);
    all_dn_equiv.reserve(num_nodes*nsurf);
    std::vector<AlignedVec> matrix_M2L(num_coords, AlignedVec(fft_size*NCHILD, 0));

    // setup ifstream of M2L precomputation matrix
    std::string fname = "helmholtz";   // precomputation matrix file name
    fname += "_" + std::string(sizeof(real_t)==4 ? "f":"d") + "_" + "p" + std::to_string(p) + "_" + "l" + std::to_string(depth);
    fname += ".dat";
    std::ifstream ifile(fname, std::ifstream::binary);
    ifile.seekg(0, ifile.end);
    size_t fsize = ifile.tellg();   // file size in bytes
    size_t msize = NCHILD * NCHILD * n3 * 2 * sizeof(real_t);   // size in bytes for each M2L matrix
    ifile.seekg(fsize - depth*num_coords*msize, ifile.beg);   // go to the start of M2L section
    
    // collect all upward equivalent charges
    #pragma omp parallel for collapse(2)
    for(int i=0; i<num_nodes; ++i) {
      for(int j=0; j<nsurf; ++j) {
        all_up_equiv[i*nsurf+j] = nodes[i].up_equiv[j];
        all_dn_equiv[i*nsurf+j] = nodes[i].dn_equiv[j];
      }
    }
    // FFT-accelerate M2L
    for(int l=0; l<depth; ++l) {
      // load M2L matrix for current level
      for(int i=0; i<num_coords; ++i) {
        ifile.read(reinterpret_cast<char*>(matrix_M2L[i].data()), msize);
      }
      AlignedVec fft_in, fft_out;
      fft_in.reserve(m2ldata[l].fft_offset.size()*fft_size);
      fft_out.reserve(m2ldata[l].ifft_offset.size()*fft_size);
      fft_up_equiv(m2ldata[l].fft_offset, all_up_equiv, fft_in);
      hadamard_product(m2ldata[l].interaction_count_offset, 
                       m2ldata[l].interaction_offset_f, 
                       fft_in, fft_out, matrix_M2L);
      ifft_dn_check(m2ldata[l].ifft_offset, fft_out, all_dn_equiv);
    }
    // update all downward check potentials
    #pragma omp parallel for collapse(2)
    for(int i=0; i<num_nodes; ++i) {
      for(int j=0; j<nsurf; ++j) {
        // nodes[i].up_equiv[j] = all_up_equiv[i*nsurf+j];
        nodes[i].dn_equiv[j] = all_dn_equiv[i*nsurf+j];
      }
    }
    ifile.close();   // close ifstream
  }

}  // end namespace exafmm_t
