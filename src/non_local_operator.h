// Copyright (c) 2013-2016 Anton Kozhevnikov, Thomas Schulthess
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

/** \file non_local_operator.h
 *   
 *  \brief Contains declaration and implementation of sirius::Non_local_operator class.
 */

#ifndef __NON_LOCAL_OPERATOR_H__
#define __NON_LOCAL_OPERATOR_H__

#include "beta_projectors.h"
#include "simulation_context.h"

namespace sirius {

template <typename T>
class Non_local_operator
{
    protected:

        Beta_projectors& beta_;

        processing_unit_t pu_;
        
        int packed_mtrx_size_;

        mdarray<int, 1> packed_mtrx_offset_;

        /// Non-local operator matrix.
        mdarray<T, 2> op_;

        mdarray<T, 1> work_;

        bool is_null_;

        Non_local_operator& operator=(Non_local_operator const& src) = delete;
        Non_local_operator(Non_local_operator const& src) = delete;

    public:

        Non_local_operator(Beta_projectors& beta__, processing_unit_t pu__) : beta_(beta__), pu_(pu__), is_null_(false)
        {
            PROFILE();

            auto& uc = beta_.unit_cell();
            packed_mtrx_offset_ = mdarray<int, 1>(uc.num_atoms());
            packed_mtrx_size_ = 0;
            for (int ia = 0; ia < uc.num_atoms(); ia++)
            {   
                int nbf = uc.atom(ia).mt_basis_size();
                packed_mtrx_offset_(ia) = packed_mtrx_size_;
                packed_mtrx_size_ += nbf * nbf;
            }

            #ifdef __GPU
            if (pu_ == GPU)
            {
                packed_mtrx_offset_.allocate_on_device();
                packed_mtrx_offset_.copy_to_device();
            }
            #endif
        }

        ~Non_local_operator()
        {
        }
        
        void apply(int chunk__, int ispn__, Wave_functions<false>& op_phi__, int idx0__, int n__);

        inline T operator()(int xi1__, int xi2__, int ia__)
        {
            return (*this)(xi1__, xi2__, 0, ia__);
        }

        inline T operator()(int xi1__, int xi2__, int ispn__, int ia__)
        {
            int nbf = beta_.unit_cell().atom(ia__).mt_basis_size();
            return op_(packed_mtrx_offset_(ia__) + xi2__ * nbf + xi1__, ispn__);
        }
};

template <typename T>
class D_operator: public Non_local_operator<T>
{
    public:

        D_operator(Simulation_context const& ctx__, Beta_projectors& beta__) : Non_local_operator<T>(beta__, ctx__.processing_unit())
        {
            this->op_ = mdarray<T, 2>(this->packed_mtrx_size_, ctx__.num_mag_dims() + 1);
            this->op_.zero();

            auto& uc = this->beta_.unit_cell();

            for (int j = 0; j < ctx__.num_mag_dims() + 1; j++)
            {
                for (int ia = 0; ia < uc.num_atoms(); ia++)
                {
                    int nbf = uc.atom(ia).mt_basis_size();
                    for (int xi2 = 0; xi2 < nbf; xi2++)
                    {
                        for (int xi1 = 0; xi1 < nbf; xi1++)
                        {
                            assert(uc.atom(ia).d_mtrx(xi1, xi2, j).imag() < 1e-10);
                            this->op_(this->packed_mtrx_offset_(ia) + xi2 * nbf + xi1, j) = uc.atom(ia).d_mtrx(xi1, xi2, j).real();
                        }
                    }
                }
            }
            if (ctx__.num_mag_dims())
            {
                for (int ia = 0; ia < uc.num_atoms(); ia++)
                {
                    int nbf = uc.atom(ia).mt_basis_size();
                    for (int xi2 = 0; xi2 < nbf; xi2++)
                    {
                        for (int xi1 = 0; xi1 < nbf; xi1++)
                        {
                            auto v0 = this->op_(this->packed_mtrx_offset_(ia) + xi2 * nbf + xi1, 0); 
                            auto v1 = this->op_(this->packed_mtrx_offset_(ia) + xi2 * nbf + xi1, 1); 
                            this->op_(this->packed_mtrx_offset_(ia) + xi2 * nbf + xi1, 0) = std::real(v0 + v1);
                            this->op_(this->packed_mtrx_offset_(ia) + xi2 * nbf + xi1, 1) = std::real(v0 - v1);
                        }
                    }
                }
            }
            #ifdef __GPU
            if (this->pu_ == GPU)
            {
                this->op_.allocate_on_device();
                this->op_.copy_to_device();
            }
            #endif
        }
};

template <typename T>
class Q_operator: public Non_local_operator<T>
{
    public:
        
        Q_operator(Simulation_context const& ctx__, Beta_projectors& beta__) : Non_local_operator<T>(beta__, ctx__.processing_unit())
        {
            if (ctx__.esm_type() == ultrasoft_pseudopotential)
            {
                /* Q-operator is independent of spin */
                this->op_ = mdarray<T, 2>(this->packed_mtrx_size_, 1);
                this->op_.zero();

                auto& uc = this->beta_.unit_cell();
                for (int ia = 0; ia < uc.num_atoms(); ia++)
                {
                    int iat = uc.atom(ia).type().id();
                    int nbf = uc.atom(ia).mt_basis_size();
                    for (int xi2 = 0; xi2 < nbf; xi2++)
                    {
                        for (int xi1 = 0; xi1 < nbf; xi1++)
                        {
                            assert(ctx__.augmentation_op(iat).q_mtrx(xi1, xi2).imag() < 1e-10);
                            this->op_(this->packed_mtrx_offset_(ia) + xi2 * nbf + xi1, 0) = ctx__.augmentation_op(iat).q_mtrx(xi1, xi2).real();
                        }
                    }
                }
                #ifdef __GPU
                if (this->pu_ == GPU)
                {
                    this->op_.allocate_on_device();
                    this->op_.copy_to_device();
                }
                #endif
            }
            else
            {
                this->is_null_ = true;
            }
        }
};

}

#endif