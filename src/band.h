// Copyright (c) 2013-2014 Anton Kozhevnikov, Thomas Schulthess
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that 
// the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the 
//    following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
//    and the following disclaimer in the documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED 
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/** \file band.h
 *   
 *   \brief Contains declaration and partial implementation of sirius::Band class.
 */

#ifndef __BAND_H__
#define __BAND_H__

#include "periodic_function.h"
#include "k_point.h"
#include "non_local_operator.h"
#include "hloc_operator.h"

namespace sirius
{

// TODO: Band problem is a mess and needs more formal organizaiton. We have different basis functions. 
//       We can do first- and second-variation or a full variation. We can do iterative or exact diagonalization.
//       This has to be organized. 

/// Setup and solve eigen value problem.
class Band
{
    private:

        /// Simulation context.
        Simulation_context& ctx_;

        /// Alias for the unit cell.
        Unit_cell& unit_cell_;

        /// BLACS grid for distributed linear algebra operations.
        BLACS_grid const& blacs_grid_;

        /// Non-zero Gaunt coefficients
        Gaunt_coefficients<double_complex>* gaunt_coefs_;
        
        /// Interface to a standard eigen-value solver.
        standard_evp* std_evp_solver_; 

        /// Interface to a generalized eigen-value solver.
        generalized_evp* gen_evp_solver_;

        /// Apply effective magentic field to the first-variational state.
        /** Must be called first because hpsi is overwritten with B|fv_j>. */
        void apply_magnetic_field(Wave_functions<true>& fv_states__,
                                  Gvec const& gkvec__,
                                  Periodic_function<double>* effective_magnetic_field__[3],
                                  std::vector<Wave_functions<true>*>& hpsi__);

        /// Apply SO correction to the first-variational states.
        /** Raising and lowering operators:
         *  \f[
         *      L_{\pm} Y_{\ell m}= (L_x \pm i L_y) Y_{\ell m}  = \sqrt{\ell(\ell+1) - m(m \pm 1)} Y_{\ell m \pm 1}
         *  \f]
         */
        void apply_so_correction(mdarray<double_complex, 2>& fv_states, mdarray<double_complex, 3>& hpsi);
        
        /// Apply UJ correction to scalar wave functions
        template <spin_block_t sblock>
        void apply_uj_correction(mdarray<double_complex, 2>& fv_states, mdarray<double_complex, 3>& hpsi);

        /// Add interstitial contribution to apw-apw block of Hamiltonian and overlap
        void set_fv_h_o_it(K_point* kp,
                           Periodic_function<double>* effective_potential, 
                           matrix<double_complex>& h,
                           matrix<double_complex>& o);

        void set_o_it(K_point* kp, mdarray<double_complex, 2>& o);

        template <spin_block_t sblock>
        void set_h_it(K_point* kp,
                      Periodic_function<double>* effective_potential, 
                      Periodic_function<double>* effective_magnetic_field[3],
                      matrix<double_complex>& h);
        
        /// Setup lo-lo block of Hamiltonian and overlap matrices
        void set_fv_h_o_lo_lo(K_point* kp, mdarray<double_complex, 2>& h, mdarray<double_complex, 2>& o);

        template <spin_block_t sblock>
        void set_h_lo_lo(K_point* kp, mdarray<double_complex, 2>& h);
        
        void set_o_lo_lo(K_point* kp, mdarray<double_complex, 2>& o);
       
        void set_o(K_point* kp, mdarray<double_complex, 2>& o);
    
        template <spin_block_t sblock> 
        void set_h(K_point* kp, Periodic_function<double>* effective_potential, 
                   Periodic_function<double>* effective_magnetic_field[3], mdarray<double_complex, 2>& h);
       
        /// Diagonalize a full-potential Hamiltonian.
        void diag_fv_full_potential(K_point* kp__,
                                    Periodic_function<double>* effective_potential__);

        /// Diagonalize a pseudo-potential Hamiltonian.
        void diag_pseudo_potential(K_point* kp__, 
                                   Periodic_function<double>* effective_potential__,
                                   Periodic_function<double>* effective_magnetic_field__[3]);

        /// Exact (not iterative) diagonalization of the Hamiltonian.
        void diag_pseudo_potential_exact(K_point* kp__,
                                         int ispn__,
                                         Hloc_operator& h_op__,
                                         D_operator& d_op__,
                                         Q_operator& q_op__);

        /// Iterative Davidson diagonalization.
        void diag_pseudo_potential_davidson(K_point* kp__,
                                            int ispn__,
                                            Hloc_operator& h_op__,
                                            D_operator& d_op__,
                                            Q_operator& q_op__);

        void diag_pseudo_potential_davidson_fast(K_point* kp__,
                                                 int ispn__,
                                                 Hloc_operator& h_op__,
                                                 D_operator& d_op__,
                                                 Q_operator& q_op__);

        //void diag_fv_pseudo_potential_rmm_diis_serial(K_point* kp__,
        //                                              double v0__,
        //                                              std::vector<double>& veff_it_coarse__);

        //void diag_fv_pseudo_potential_chebyshev_serial(K_point* kp__,
        //                                               std::vector<double> const& veff_it_coarse__);

        void apply_h_serial(K_point* kp__, 
                            std::vector<double> const& effective_potential__, 
                            std::vector<double> const& pw_ekin__, 
                            int N__,
                            int n__,
                            matrix<double_complex>& phi__,
                            matrix<double_complex>& hphi__,
                            mdarray<double_complex, 1>& kappa__,
                            mdarray<int, 1>& packed_mtrx_offset__,
                            mdarray<double_complex, 1>& d_mtrx_packed__);

        void diag_h_o(K_point* kp__,
                      int N__,
                      int num_bands__,
                      matrix<double_complex>& hmlt__,
                      matrix<double_complex>& ovlp__,
                      matrix<double_complex>& evec__,
                      dmatrix<double_complex>& hmlt_dist__,
                      dmatrix<double_complex>& ovlp_dist__,
                      dmatrix<double_complex>& evec_dist__,
                      std::vector<double>& eval__);

        void apply_h_o(K_point* kp__,
                       int ispn__, 
                       int N__,
                       int n__,
                       Wave_functions<false>& phi__,
                       Wave_functions<false>& hphi__,
                       Wave_functions<false>& ophi__,
                       Hloc_operator &h_op,
                       D_operator& d_op,
                       Q_operator& q_op);

        void set_h_o(K_point* kp__,
                     int N__,
                     int n__,
                     Wave_functions<false>& phi__,
                     Wave_functions<false>& hphi__,
                     Wave_functions<false>& ophi__,
                     matrix<double_complex>& h__,
                     matrix<double_complex>& o__,
                     matrix<double_complex>& h_old__,
                     matrix<double_complex>& o_old__);

        int residuals(K_point* kp__,
                      int ispn__,
                      int N__,
                      int num_bands__,
                      std::vector<double>& eval__,
                      std::vector<double>& eval_old__,
                      matrix<double_complex>& evec__,
                      Wave_functions<false>& hphi__,
                      Wave_functions<false>& ophi__,
                      Wave_functions<false>& hpsi__,
                      Wave_functions<false>& opsi__,
                      Wave_functions<false>& res__,
                      std::vector<double>& h_diag__,
                      std::vector<double>& o_diag__);

        void residuals_aux(K_point* kp__,
                           int num_bands__,
                           std::vector<double>& eval__,
                           Wave_functions<false>& hpsi__,
                           Wave_functions<false>& opsi__,
                           Wave_functions<false>& res__,
                           std::vector<double>& h_diag__,
                           std::vector<double>& o_diag__,
                           std::vector<double>& res_norm__);

        void add_nl_h_o_pw(K_point* kp__,
                           int n__,
                           matrix<double_complex>& phi__,
                           matrix<double_complex>& hphi__,
                           matrix<double_complex>& ophi__,
                           matrix<double_complex>& beta_gk__,
                           mdarray<int, 1>& packed_mtrx_offset__,
                           mdarray<double_complex, 1>& d_mtrx_packed__,
                           mdarray<double_complex, 1>& q_mtrx_packed__);

        void add_nl_h_o_rs(K_point* kp__,
                           int n__,
                           matrix<double_complex>& phi__,
                           matrix<double_complex>& hphi__,
                           matrix<double_complex>& ophi__,
                           mdarray<int, 1>& packed_mtrx_offset__,
                           mdarray<double_complex, 1>& d_mtrx_packed__,
                           mdarray<double_complex, 1>& q_mtrx_packed__,
                           mdarray<double_complex, 1>& kappa__);

    public:
        
        /// Constructor
        Band(Simulation_context& ctx__,
             BLACS_grid const& blacs_grid__) 
            : ctx_(ctx__),
              unit_cell_(ctx__.unit_cell()),
              blacs_grid_(blacs_grid__)
        {
            PROFILE();

            gaunt_coefs_ = new Gaunt_coefficients<double_complex>(ctx_.lmax_apw(), 
                                                                  ctx_.lmax_pot(), 
                                                                  ctx_.lmax_apw(),
                                                                  SHT::gaunt_hybrid);

            /* create standard eigen-value solver */
            switch (ctx_.std_evp_solver_type())
            {
                case ev_lapack:
                {
                    std_evp_solver_ = new standard_evp_lapack();
                    break;
                }
                case ev_scalapack:
                {
                    std_evp_solver_ = new standard_evp_scalapack(blacs_grid_, ctx_.cyclic_block_size(), ctx_.cyclic_block_size());
                    break;
                }
                case ev_plasma:
                {
                    std_evp_solver_ = new standard_evp_plasma();
                    break;
                }
                default:
                {
                    TERMINATE("wrong standard eigen-value solver");
                }
            }
            
            /* create generalized eign-value solver */
            switch (ctx_.gen_evp_solver_type())
            {
                case ev_lapack:
                {
                    gen_evp_solver_ = new generalized_evp_lapack(0.0);
                    break;
                }
                case ev_scalapack:
                {
                    gen_evp_solver_ = new generalized_evp_scalapack(blacs_grid_, 0.0, ctx_.cyclic_block_size(), ctx_.cyclic_block_size());
                    break;
                }
                case ev_elpa1:
                {
                    gen_evp_solver_ = new generalized_evp_elpa1(blacs_grid_, ctx_.cyclic_block_size());
                    break;
                }
                case ev_elpa2:
                {
                    gen_evp_solver_ = new generalized_evp_elpa2(blacs_grid_, ctx_.cyclic_block_size());
                    break;
                }
                case ev_magma:
                {
                    gen_evp_solver_ = new generalized_evp_magma();
                    break;
                }
                case ev_rs_gpu:
                {
                    gen_evp_solver_ = new generalized_evp_rs_gpu(blacs_grid_, ctx_.cyclic_block_size(), ctx_.cyclic_block_size());
                    break;
                }
                case ev_rs_cpu:
                {
                    gen_evp_solver_ = new generalized_evp_rs_cpu(blacs_grid_, ctx_.cyclic_block_size(), ctx_.cyclic_block_size());
                    break;
                }
                default:
                {
                    TERMINATE("wrong generalized eigen-value solver");
                }
            }

            if (std_evp_solver_->parallel() != gen_evp_solver_->parallel())
                TERMINATE("both eigen-value solvers must be serial or parallel");
        }

        ~Band()
        {
            PROFILE();

            delete gaunt_coefs_;
            delete std_evp_solver_;
            delete gen_evp_solver_;
        }

        /// Apply the muffin-tin part of the first-variational Hamiltonian to the apw basis function
        /** The following vector is computed:
         *  \f[
         *    b_{L_2 \nu_2}^{\alpha}({\bf G'}) = \sum_{L_1 \nu_1} \sum_{L_3} 
         *      a_{L_1\nu_1}^{\alpha*}({\bf G'}) 
         *      \langle u_{\ell_1\nu_1}^{\alpha} | h_{L3}^{\alpha} |  u_{\ell_2\nu_2}^{\alpha}  
         *      \rangle  \langle Y_{L_1} | R_{L_3} | Y_{L_2} \rangle +  
         *      \frac{1}{2} \sum_{\nu_1} a_{L_2\nu_1}^{\alpha *}({\bf G'})
         *      u_{\ell_2\nu_1}^{\alpha}(R_{\alpha})
         *      u_{\ell_2\nu_2}^{'\alpha}(R_{\alpha})R_{\alpha}^{2}
         *  \f] 
         */
        template <spin_block_t sblock>
        void apply_hmt_to_apw(int num_gkvec, int ia, mdarray<double_complex, 2>& alm, mdarray<double_complex, 2>& halm);
 
        //== template <spin_block_t sblock>
        //== void apply_hmt_to_apw(mdarray<double_complex, 2>& alm, mdarray<double_complex, 2>& halm);

        /// Setup apw-lo and lo-apw blocs of Hamiltonian and overlap matrices
        void set_fv_h_o_apw_lo(K_point* kp,
                               Atom_type const& type,
                               Atom const& atom,
                               int ia,
                               mdarray<double_complex, 2>& alm_row,
                               mdarray<double_complex, 2>& alm_col,
                               mdarray<double_complex, 2>& h,
                               mdarray<double_complex, 2>& o);
        
        template <spin_block_t sblock>
        void set_h_apw_lo(K_point* kp, Atom_type* type, Atom* atom, int ia, mdarray<double_complex, 2>& alm, 
                          mdarray<double_complex, 2>& h);
        
        /// Set APW-lo and lo-APW blocks of the overlap matrix.
        void set_o_apw_lo(K_point* kp, Atom_type* type, Atom* atom, int ia, mdarray<double_complex, 2>& alm, 
                          mdarray<double_complex, 2>& o);

        /// Setup the Hamiltonian and overlap matrices in APW+lo basis
        /** The Hamiltonian matrix has the following expression:
         *  \f[
         *      H_{\mu' \mu} = \langle \varphi_{\mu'} | \hat H | \varphi_{\mu} \rangle
         *  \f]
         *
         *  \f[
         *      H_{\mu' \mu}=\langle \varphi_{\mu' } | \hat H | \varphi_{\mu } \rangle  = 
         *      \left( \begin{array}{cc} 
         *         H_{\bf G'G} & H_{{\bf G'}j} \\
         *         H_{j'{\bf G}} & H_{j'j}
         *      \end{array} \right)
         *  \f]
         *  
         *  The overlap matrix has the following expression:
         *  \f[
         *      O_{\mu' \mu} = \langle \varphi_{\mu'} | \varphi_{\mu} \rangle
         *  \f]
         *  APW-APW block:
         *  \f[
         *      O_{{\bf G'} {\bf G}}^{\bf k} = \sum_{\alpha} \sum_{L\nu} a_{L\nu}^{\alpha *}({\bf G'+k}) 
         *      a_{L\nu}^{\alpha}({\bf G+k})
         *  \f]
         *  
         *  APW-lo block:
         *  \f[
         *      O_{{\bf G'} j}^{\bf k} = \sum_{\nu'} a_{\ell_j m_j \nu'}^{\alpha_j *}({\bf G'+k}) 
         *      \langle u_{\ell_j \nu'}^{\alpha_j} | \phi_{\ell_j}^{\zeta_j \alpha_j} \rangle
         *  \f]
         *
         *  lo-APW block:
         *  \f[
         *      O_{j' {\bf G}}^{\bf k} = 
         *      \sum_{\nu'} \langle \phi_{\ell_{j'}}^{\zeta_{j'} \alpha_{j'}} | u_{\ell_{j'} \nu'}^{\alpha_{j'}} \rangle
         *      a_{\ell_{j'} m_{j'} \nu'}^{\alpha_{j'}}({\bf G+k}) 
         *  \f]
         *
         *  lo-lo block:
         *  \f[
         *      O_{j' j}^{\bf k} = \langle \phi_{\ell_{j'}}^{\zeta_{j'} \alpha_{j'}} | 
         *      \phi_{\ell_{j}}^{\zeta_{j} \alpha_{j}} \rangle \delta_{\alpha_{j'} \alpha_j} 
         *      \delta_{\ell_{j'} \ell_j} \delta_{m_{j'} m_j}
         *  \f]
         *
         */
        template <processing_unit_t pu, electronic_structure_method_t basis>
        void set_fv_h_o(K_point* kp, Periodic_function<double>* effective_potential, dmatrix<double_complex>& h, 
                        dmatrix<double_complex>& o);

        /// Solve first-variational (non-magnetic) problem
        void solve_fv(K_point* kp__, Periodic_function<double>* effective_potential__);

        /// Solve second-variational problem
        void solve_sv(K_point* kp, Periodic_function<double>* effective_magnetic_field[3]);

        void solve_sv_pp(K_point* kp, Periodic_function<double>* effective_magnetic_field[3]);

        void solve_fd(K_point* kp, Periodic_function<double>* effective_potential, 
                      Periodic_function<double>* effective_magnetic_field[3]);

        inline standard_evp* std_evp_solver()
        {
            return std_evp_solver_;
        }

        inline generalized_evp* gen_evp_solver()
        {
            return gen_evp_solver_;
        }

        /// Get diagonal elements of Hamiltonian.
        std::vector<double> get_h_diag(K_point* kp__,
                                       int ispn__,
                                       double v0__,
                                       D_operator& d_op__);

        /// Get diagonal elements of overlap matrix.
        std::vector<double> get_o_diag(K_point* kp__,
                                       Q_operator& q_op__);

};

#include "band.hpp"

}

#endif // __BAND_H__
