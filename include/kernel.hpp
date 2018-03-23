#ifndef _PVFMM_FMM_KERNEL_HPP_
#define _PVFMM_FMM_KERNEL_HPP_
#include "intrinsics.h"
#include "vec.h"
#include "pvfmm.h"
namespace pvfmm {
struct Kernel{
  public:
  typedef void (*Ker_t)(real_t* r_src, int src_cnt, real_t* v_src,
                        real_t* r_trg, int trg_cnt, real_t* k_out);

  int ker_dim[2];
  std::string ker_name;
  Ker_t ker_poten;

  mutable bool init;
  mutable std::vector<real_t> src_scal;
  mutable std::vector<real_t> trg_scal;
  mutable std::vector<Permutation<real_t> > perm_vec;

  mutable const Kernel* k_s2m;
  mutable const Kernel* k_s2l;
  mutable const Kernel* k_s2t;
  mutable const Kernel* k_m2m;
  mutable const Kernel* k_m2l;
  mutable const Kernel* k_m2t;
  mutable const Kernel* k_l2l;
  mutable const Kernel* k_l2t;

  Kernel(Ker_t poten, const char* name, std::pair<int,int> k_dim) {
    ker_dim[0]=k_dim.first;
    ker_dim[1]=k_dim.second;
    ker_name=std::string(name);
    ker_poten=poten;
 
    init=false;
    src_scal.resize(ker_dim[0], 0.); 
    trg_scal.resize(ker_dim[1], 0.); 
    perm_vec.resize(Perm_Count);
    std::fill(perm_vec.begin(), perm_vec.begin()+C_Perm, Permutation<real_t>(ker_dim[0]));
    std::fill(perm_vec.begin()+C_Perm, perm_vec.end(), Permutation<real_t>(ker_dim[1]));

    k_s2m=NULL;
    k_s2l=NULL;
    k_s2t=NULL;
    k_m2m=NULL;
    k_m2l=NULL;
    k_m2t=NULL;
    k_l2l=NULL;
    k_l2t=NULL;
  }

  void Initialize() const{
    if(init) return;
    init = true;
    // hardcoded src & trg scal here, since they are constants for Laplace based on the original code
    if (ker_dim[1] == 3) std::fill(trg_scal.begin(), trg_scal.end(), 2.);
    else std::fill(trg_scal.begin(), trg_scal.end(), 1.);

    if(!k_s2m) k_s2m=this;
    if(!k_s2l) k_s2l=this;
    if(!k_s2t) k_s2t=this;
    if(!k_m2m) k_m2m=this;
    if(!k_m2l) k_m2l=this;
    if(!k_m2t) k_m2t=this;
    if(!k_l2l) k_l2l=this;
    if(!k_l2t) k_l2t=this;
    assert(k_s2t->ker_dim[0]==ker_dim[0]);
    assert(k_s2m->ker_dim[0]==k_s2l->ker_dim[0]);
    assert(k_s2m->ker_dim[0]==k_s2t->ker_dim[0]);
    assert(k_m2m->ker_dim[0]==k_m2l->ker_dim[0]);
    assert(k_m2m->ker_dim[0]==k_m2t->ker_dim[0]);
    assert(k_l2l->ker_dim[0]==k_l2t->ker_dim[0]);
    assert(k_s2t->ker_dim[1]==ker_dim[1]);
    assert(k_s2m->ker_dim[1]==k_m2m->ker_dim[1]);
    assert(k_s2l->ker_dim[1]==k_l2l->ker_dim[1]);
    assert(k_m2l->ker_dim[1]==k_l2l->ker_dim[1]);
    assert(k_s2t->ker_dim[1]==k_m2t->ker_dim[1]);
    assert(k_s2t->ker_dim[1]==k_l2t->ker_dim[1]);
    k_s2m->Initialize();
    k_s2l->Initialize();
    k_s2t->Initialize();
    k_m2m->Initialize();
    k_m2l->Initialize();
    k_m2t->Initialize();
    k_l2l->Initialize();
    k_l2t->Initialize();
  }

  //! Laplace P2P save pairwise contributions to k_out (not aggregate over each target)
  // For Laplace: ker_dim[0] = 1, j = 0; Force a unit charge (q=1)
  // r_src layout: [x1, y1, z1, x2, y2, z2, ...] 
  // k_out layout (potential): [p11, p12, p13, ..., p21, p22, ...]  (1st digit: src_idx; 2nd: trg_idx)
  // k_out layout (gradient) : [Fx11, Fy11, Fz11, Fx12, Fy12, Fz13, ... Fx1n, Fy1n, Fz1n, ...
  //                            Fx21, Fy21, Fz21, Fx22, Fy22, Fz22, ... Fx2n, Fy2n, Fz2n, ...
  //                            ...]
  void BuildMatrix(real_t* r_src, int src_cnt, real_t* r_trg, int trg_cnt, real_t* k_out) const{
    memset(k_out, 0, src_cnt*ker_dim[0]*trg_cnt*ker_dim[1]*sizeof(real_t));
    for(int i=0;i<src_cnt;i++)
      for(int j=0;j<ker_dim[0];j++){
	std::vector<real_t> v_src(ker_dim[0],0);
	v_src[j]=1.0;
        // do P2P: i-th source
	ker_poten(&r_src[i*3], 1, &v_src[0], r_trg, trg_cnt,
		  &k_out[(i*ker_dim[0]+j)*trg_cnt*ker_dim[1]]);
      }
  }
};

template<void (*A)(real_t*, int, real_t*, real_t*, int, real_t*)>
Kernel BuildKernel(const char* name, std::pair<int,int> k_dim, 
                   const Kernel* k_s2m=NULL, const Kernel* k_s2l=NULL, const Kernel* k_s2t=NULL, const Kernel* k_m2m=NULL, 
                   const Kernel* k_m2l=NULL, const Kernel* k_m2t=NULL, const Kernel* k_l2l=NULL, const Kernel* k_l2t=NULL) {
  Kernel K(A, name, k_dim);
  K.k_s2m=k_s2m;
  K.k_s2l=k_s2l;
  K.k_s2t=k_s2t;
  K.k_m2m=k_m2m;
  K.k_m2l=k_m2l;
  K.k_m2t=k_m2t;
  K.k_l2l=k_l2l;
  K.k_l2t=k_l2t;
  return K;
}

//! Laplace potential P2P 1/(4*pi*|r|) with matrix interface, potentials saved in trg_value matrix
// source & target coord matrix size: 3 by N
void potentialP2P(Matrix<real_t>& src_coord, Matrix<real_t>& src_value, Matrix<real_t>& trg_coord, Matrix<real_t>& trg_value){
  simdvec zero((real_t)0);
  const real_t OOFP = 1.0/(16*4*M_PI);   // factor 16 comes from the simd rsqrt function
  simdvec oofp(OOFP);
  int src_cnt = src_coord.Dim(1);
  int trg_cnt = trg_coord.Dim(1);
  for(int t=0; t<trg_cnt; t+=NSIMD){
    simdvec tx(&trg_coord[0][t], (int)sizeof(real_t));
    simdvec ty(&trg_coord[1][t], (int)sizeof(real_t));
    simdvec tz(&trg_coord[2][t], (int)sizeof(real_t));
    simdvec tv(zero);
    for(int s=0; s<src_cnt; s++){
      simdvec sx(src_coord[0][s]);
      sx = sx - tx;
      simdvec sy(src_coord[1][s]);
      sy = sy - ty;
      simdvec sz(src_coord[2][s]);
      sz = sz - tz;
      simdvec sv(src_value[0][s]);
      simdvec r2(zero);
      r2 = r2 + sx*sx;
      r2 = r2 + sy*sy;
      r2 = r2 + sz*sz;
      simdvec invR = rsqrt(r2);
      tv = tv + invR*sv;
    }
    tv = tv * oofp;
    for(int k=0; k<NSIMD && t+k<trg_cnt; k++) {
      trg_value[0][t+k] = tv[k];
    }
  }
  Profile::Add_FLOP((long long)trg_cnt*(long long)src_cnt*20);
}

//! Laplace gradient P2P -r/(4*pi*|r|^3) with matrix interface, gradients saved in trg_value matrix
// source & target coord matrix size: 3 by N
void gradientP2P(Matrix<real_t>& src_coord, Matrix<real_t>& src_value, Matrix<real_t>& trg_coord, Matrix<real_t>& trg_value){
  simdvec zero((real_t)0);
  const real_t OOFP = -1.0/(4*16*16*16*M_PI);
  simdvec oofp(OOFP);
  int src_cnt = src_coord.Dim(1);
  int trg_cnt = trg_coord.Dim(1);
  for(int t=0; t<trg_cnt; t+=NSIMD){
    simdvec tx(&trg_coord[0][t], (int)sizeof(real_t));
    simdvec ty(&trg_coord[1][t], (int)sizeof(real_t));
    simdvec tz(&trg_coord[2][t], (int)sizeof(real_t));
    simdvec tv0(zero);
    simdvec tv1(zero);
    simdvec tv2(zero);
    for(int s=0; s<src_cnt; s++){
      simdvec sx(src_coord[0][s]);
      sx = tx - sx;
      simdvec sy(src_coord[1][s]);
      sy = ty - sy;
      simdvec sz(src_coord[2][s]);
      sz = tz - sz;
      simdvec r2(zero);
      r2 = r2 + sx*sx;
      r2 = r2 + sy*sy;
      r2 = r2 + sz*sz;
      simdvec invR = rsqrt(r2);
      simdvec invR3 = (invR*invR) * invR;
      simdvec sv(src_value[0][s]);
      sv = invR3 * sv;
      tv0 = tv0 + sv*sx;
      tv1 = tv1 + sv*sy;
      tv2 = tv2 + sv*sz;
    }
    tv0 = tv0 * oofp;
    tv1 = tv1 * oofp;
    tv2 = tv2 * oofp;
    for(int k=0; k<NSIMD && t+k<trg_cnt; k++) {
      trg_value[0][t+k] = tv0[k];
      trg_value[1][t+k] = tv1[k];
      trg_value[2][t+k] = tv2[k];
    }
  }
  Profile::Add_FLOP((long long)trg_cnt*(long long)src_cnt*27);
}

//! Wrap around the above P2P functions with matrix interface to provide array interface
//! Evaluate potential / gradient based on the argument grad
// r_src & r_trg coordinate array: [x1, y1, z1, x2, y2, z2, ...]
void laplaceP2P(real_t* r_src, int src_cnt, real_t* v_src, real_t* r_trg, int trg_cnt, real_t* v_trg, bool grad=false){
int SRC_DIM = 1;
int TRG_DIM = (grad) ? 3 : 1;

  real_t* buff=NULL;
  Matrix<real_t> src_coord;
  Matrix<real_t> src_value;
  Matrix<real_t> trg_coord;
  Matrix<real_t> trg_value;
  {
    size_t buff_size = src_cnt*(3+SRC_DIM) + trg_cnt*(3+TRG_DIM);
    int err = posix_memalign((void**)&buff, MEM_ALIGN, buff_size*sizeof(real_t));
real_t* buff_ptr = buff;

    src_coord.ReInit(3, src_cnt,buff_ptr,false);  buff_ptr+=3*src_cnt;
    src_value.ReInit(  SRC_DIM, src_cnt,buff_ptr,false);  buff_ptr+=  SRC_DIM*src_cnt;
    trg_coord.ReInit(3, trg_cnt,buff_ptr,false);  buff_ptr+=3*trg_cnt;
    trg_value.ReInit(  TRG_DIM, trg_cnt,buff_ptr,false);
    for(int i=0;i<src_cnt ;i++){
      for(size_t j=0;j<3;j++){
        src_coord[j][i]=r_src[i*3+j];
      }
    }
    for(int i=0;i<src_cnt ;i++){
      for(size_t j=0;j<SRC_DIM;j++){
        src_value[j][i]=v_src[i*SRC_DIM+j];
      }
    }
    for(int i=0;i<trg_cnt ;i++){
      for(size_t j=0;j<3;j++){
        trg_coord[j][i]=r_trg[i*3+j];
      }
    }
    for(int i=0;i<trg_cnt;i++){
      for(size_t j=0;j<TRG_DIM;j++){
        trg_value[j][i]=0;
      }
    }
  }
  if (grad) gradientP2P(src_coord,src_value,trg_coord,trg_value);
  else potentialP2P(src_coord,src_value,trg_coord,trg_value);
  {
    for(size_t i=0;i<trg_cnt ;i++){
      for(size_t j=0;j<TRG_DIM;j++){
        v_trg[i*TRG_DIM+j]+=trg_value[j][i];
      }
    }
  }
  if(buff){
    free(buff);
//std::cout << "use buff" << std::endl;
  }
}

//! Laplace potential P2P with array interface
void potentialP2P(real_t* r_src, int src_cnt, real_t* v_src, real_t* r_trg, int trg_cnt, real_t* v_trg){
  laplaceP2P(r_src, src_cnt, v_src,  r_trg, trg_cnt, v_trg, false);
}
//! Laplace gradient P2P with array interface
void gradientP2P(real_t* r_src, int src_cnt, real_t* v_src,  real_t* r_trg, int trg_cnt, real_t* v_trg){
  laplaceP2P(r_src, src_cnt, v_src, r_trg, trg_cnt, v_trg, true);
}

}//end namespace

#endif //_PVFMM_FMM_KERNEL_HPP_
