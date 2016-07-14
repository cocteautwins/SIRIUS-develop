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

/** \file fft3d_cpu.cpp
 *   
 *  \brief Implementation
 */

#include "fft3d.h"

#ifdef __GPU
extern "C" void unpack_z_cols_gpu(cuDoubleComplex* z_cols_packed__,
                                  cuDoubleComplex* fft_buf__,
                                  int size_x__,
                                  int size_y__,
                                  int size_z__,
                                  int num_z_cols__,
                                  int const* z_columns_pos__,
                                  bool use_reduction,
                                  int stream_id__);

extern "C" void pack_z_cols_gpu(cuDoubleComplex* z_cols_packed__,
                                cuDoubleComplex* fft_buf__,
                                int size_x__,
                                int size_y__,
                                int size_z__,
                                int num_z_cols__,
                                int const* z_columns_pos__,
                                int stream_id__);
#endif

namespace sirius {

FFT3D::FFT3D(FFT3D_grid grid__,
             Communicator const& comm__,
             processing_unit_t pu__,
             double gpu_workload)
    : comm_(comm__),
      pu_(pu__),
      grid_(grid__),
      #ifdef __GPU
      cufft3d_(false),
      cufft_nbatch_(0),
      #endif
      ncall_(0),
      tcall_{0, 0, 0, 0, 0}
{
    PROFILE();

    /* split z-direction */
    spl_z_ = splindex<block>(grid_.size(2), comm_.size(), comm_.rank());
    local_size_z_ = spl_z_.local_size();
    offset_z_ = spl_z_.global_offset();

    /* allocate main buffer */
    fft_buffer_ = mdarray<double_complex, 1>(local_size(), "fft_buffer_");
    
    /* allocate 1d and 2d buffers */
    for (int i = 0; i < omp_get_max_threads(); i++)
    {
        fftw_buffer_z_.push_back((double_complex*)fftw_malloc(grid_.size(2) * sizeof(double_complex)));
        fftw_buffer_xy_.push_back((double_complex*)fftw_malloc(grid_.size(0) * grid_.size(1) * sizeof(double_complex)));
    }

    plan_forward_z_   = std::vector<fftw_plan>(omp_get_max_threads());
    plan_forward_xy_  = std::vector<fftw_plan>(omp_get_max_threads());
    plan_backward_z_  = std::vector<fftw_plan>(omp_get_max_threads());
    plan_backward_xy_ = std::vector<fftw_plan>(omp_get_max_threads());

    for (int i = 0; i < omp_get_max_threads(); i++)
    {
        plan_forward_z_[i] = fftw_plan_dft_1d(grid_.size(2), (fftw_complex*)fftw_buffer_z_[i], 
                                              (fftw_complex*)fftw_buffer_z_[i], FFTW_FORWARD, FFTW_ESTIMATE);

        plan_backward_z_[i] = fftw_plan_dft_1d(grid_.size(2), (fftw_complex*)fftw_buffer_z_[i], 
                                               (fftw_complex*)fftw_buffer_z_[i], FFTW_BACKWARD, FFTW_ESTIMATE);
        
        plan_forward_xy_[i] = fftw_plan_dft_2d(grid_.size(1), grid_.size(0), (fftw_complex*)fftw_buffer_xy_[i], 
                                               (fftw_complex*)fftw_buffer_xy_[i], FFTW_FORWARD, FFTW_ESTIMATE);

        plan_backward_xy_[i] = fftw_plan_dft_2d(grid_.size(1), grid_.size(0), (fftw_complex*)fftw_buffer_xy_[i], 
                                                (fftw_complex*)fftw_buffer_xy_[i], FFTW_BACKWARD, FFTW_ESTIMATE);
    }
    
    #ifdef __GPU
    if (pu_ == GPU)
    {
        int auto_alloc = 0;
        if (comm_.size() == 1 && cufft3d_)
        {
            cufft_nbatch_ = 1;

            int dims[] = {grid_.size(2), grid_.size(1), grid_.size(0)};
            cufft_create_plan_handle(&cufft_plan_);
            cufft_create_batch_plan(cufft_plan_, 3, dims, dims, 1, 1, cufft_nbatch_, auto_alloc);
        }
        else
        {
            /* GPU will take care of this number of xy-planes */
            cufft_nbatch_ = static_cast<int>(gpu_workload * local_size_z_);

            int dim_xy[] = {grid_.size(1), grid_.size(0)};
            int embed_xy[] = {grid_.size(1), grid_.size(0)};

            cufft_create_plan_handle(&cufft_plan_);
            cufft_create_batch_plan(cufft_plan_, 2, dim_xy, embed_xy, 1, grid_.size(0) * grid_.size(1), cufft_nbatch_, auto_alloc);
            cufft_set_stream(cufft_plan_, 0);
        }
    }
    #endif
}

FFT3D::~FFT3D()
{
    //== if (comm_.rank() == 0)
    //== {
    //==     printf("number of calls : %li\n", ncall());
    //==     printf("total transform time          : %f\n", tcall(0));
    //==     printf("transform xy time             : %f\n", tcall(1));
    //==     printf("transform z serial time       : %f\n", tcall(2));
    //==     printf("transform z parallel time     : %f\n", tcall(3));
    //==     printf("transform z parallel a2a time : %f\n", tcall(4));
    //== }
    for (int i = 0; i < omp_get_max_threads(); i++)
    {
        fftw_free(fftw_buffer_z_[i]);
        fftw_free(fftw_buffer_xy_[i]);

        fftw_destroy_plan(plan_forward_z_[i]);
        fftw_destroy_plan(plan_forward_xy_[i]);
        fftw_destroy_plan(plan_backward_z_[i]);
        fftw_destroy_plan(plan_backward_xy_[i]);
    }
    #ifdef __GPU
    if (pu_ == GPU) cufft_destroy_plan_handle(cufft_plan_);
    #endif
}

template <int direction, bool use_reduction>
void FFT3D::transform_xy(Gvec const& gvec__)
{
    comm_.barrier();
    TIMER("sirius::FFT3D::transform_xy");

    double t = omp_get_wtime();

    int size_xy = grid_.size(0) * grid_.size(1);
    int first_z = 0;

    #ifdef __GPU
    if (pu_ == GPU)
    {
        /* stream #0 will be doing cuFFT */
        switch (direction)
        {
            case 1:
            {
                /* srteam #0 copies packed columns to GPU */
                acc::copyin(fft_buffer_aux_.at<GPU>(), cufft_nbatch_, fft_buffer_aux_.at<CPU>(), local_size_z_,
                            cufft_nbatch_, static_cast<int>(gvec__.z_columns().size()), 0);

                /* srteam #0 unpacks z-columns into proper position of FFT buffer */
                unpack_z_cols_gpu(fft_buffer_aux_.at<GPU>(), fft_buffer_.at<GPU>(), grid_.size(0), grid_.size(1), 
                                  cufft_nbatch_, static_cast<int>(gvec__.z_columns().size()), gvec__.z_columns_pos().at<GPU>(),
                                  use_reduction, 0);
                /* stream #0 executes FFT */
                cufft_backward_transform(cufft_plan_, fft_buffer_.at<GPU>());
                break;
            }
            case -1:
            {
                /* stream #1 copies part of FFT buffer to CPU */
                acc::copyout(fft_buffer_.at<CPU>(cufft_nbatch_ * size_xy), fft_buffer_.at<GPU>(cufft_nbatch_ * size_xy),
                             size_xy * (local_size_z_ - cufft_nbatch_), 1);
                /* stream #0 executes FFT */
                cufft_forward_transform(cufft_plan_, fft_buffer_.at<GPU>());
                /* stream #0 packs z-columns */
                pack_z_cols_gpu(fft_buffer_aux_.at<GPU>(), fft_buffer_.at<GPU>(), grid_.size(0), grid_.size(1), 
                                cufft_nbatch_, static_cast<int>(gvec__.z_columns().size()), gvec__.z_columns_pos().at<GPU>(), 0);

                /* srteam #0 copies packed columns to CPU */
                acc::copyout(fft_buffer_aux_.at<CPU>(), local_size_z_, fft_buffer_aux_.at<GPU>(), cufft_nbatch_, 
                             cufft_nbatch_, static_cast<int>(gvec__.z_columns().size()), 0);
                /* stream #1 waits to complete memory copy */
                cuda_stream_synchronize(1);
                break;
            }
        }
        first_z = cufft_nbatch_;
    }
    #endif

    std::vector<int> z_col_pos(gvec__.z_columns().size());
    #pragma omp parallel for
    for (size_t i = 0; i < gvec__.z_columns().size(); i++)
    {
        int x = (gvec__.z_column(i).x + grid_.size(0)) % grid_.size(0);
        int y = (gvec__.z_column(i).y + grid_.size(1)) % grid_.size(1);
        z_col_pos[i] = x + y * grid_.size(0);
    }
    std::vector<int> z_col_ipos;
    if (use_reduction)
    {
        z_col_ipos = std::vector<int>(gvec__.z_columns().size());
        #pragma omp parallel for
        for (size_t i = 0; i < gvec__.z_columns().size(); i++)
        {
            /* x,y coordinates of inverse G-vectors */
            int x = (-gvec__.z_column(i).x + grid_.size(0)) % grid_.size(0);
            int y = (-gvec__.z_column(i).y + grid_.size(1)) % grid_.size(1);
            z_col_ipos[i] = x + y * grid_.size(0);
        }
    }

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        #pragma omp for
        for (int iz = first_z; iz < local_size_z_; iz++)
        {
            switch (direction)
            {
                case 1:
                {
                    /* clear xy-buffer */
                    std::fill(fftw_buffer_xy_[tid], fftw_buffer_xy_[tid] + size_xy, 0);
                    /* load z-columns into proper location */
                    for (size_t i = 0; i < gvec__.z_columns().size(); i++)
                    {
                        fftw_buffer_xy_[tid][z_col_pos[i]] = fft_buffer_aux_[iz + i * local_size_z_];

                        if (use_reduction && i)
                            fftw_buffer_xy_[tid][z_col_ipos[i]] = std::conj(fftw_buffer_xy_[tid][z_col_pos[i]]);
                    }
                    
                    /* execute local FFT transform */
                    fftw_execute(plan_backward_xy_[tid]);

                    /* copy xy plane to the main FFT buffer */
                    std::copy(fftw_buffer_xy_[tid], fftw_buffer_xy_[tid] + size_xy, &fft_buffer_[iz * size_xy]);
                    
                    break;
                }
                case -1:
                {
                    /* copy xy plane from the main FFT buffer */
                    std::copy(&fft_buffer_[iz * size_xy], &fft_buffer_[iz * size_xy] + size_xy, fftw_buffer_xy_[tid]);

                    /* execute local FFT transform */
                    fftw_execute(plan_forward_xy_[tid]);

                    /* get z-columns */
                    for (size_t i = 0; i < gvec__.z_columns().size(); i++)
                        fft_buffer_aux_[iz  + i * local_size_z_] = fftw_buffer_xy_[tid][z_col_pos[i]];

                    break;
                }
                default:
                {
                    TERMINATE("wrong direction");
                }
            }
        }
    }
        
    #ifdef __GPU
    if (pu_ == GPU)
    {
        if (direction == 1)
        {
            /* stream #1 copies data to GPU */
            acc::copyin(fft_buffer_.at<GPU>(cufft_nbatch_ * size_xy), fft_buffer_.at<CPU>(cufft_nbatch_ * size_xy),
                        size_xy * (local_size_z_ - cufft_nbatch_), 1);
            cuda_stream_synchronize(1);
        }
        /* wait for stram #0 */
        cuda_stream_synchronize(0);
    }
    #endif

    tcall_[1] += (omp_get_wtime() - t);
}

template <int direction, bool use_reduction>
void FFT3D::transform_z_serial(Gvec const& gvec__, double_complex* data__)
{
    TIMER("sirius::FFT3D::transform_z_serial");

    double t = omp_get_wtime();

    double norm = 1.0 / size();
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        #pragma omp for schedule(dynamic, 1)
        for (size_t i = 0; i < gvec__.z_columns().size(); i++)
        {
            int data_offset = gvec__.z_column(i).offset;

            switch (direction)
            {
                case 1:
                {
                    /* zero input FFT buffer */
                    std::fill(fftw_buffer_z_[tid], fftw_buffer_z_[tid] + grid_.size(2), 0);
                    /* load column into local FFT buffer */
                    for (size_t j = 0; j < gvec__.z_column(i).z.size(); j++)
                    {
                        /* coordinate inside FFT grid */
                        int z = (gvec__.z_column(i).z[j] + grid_.size(2)) % grid_.size(2);
                        fftw_buffer_z_[tid][z] = data__[data_offset + j];
                    }
                    /* column with {x,y} = {0,0} has only non-negative z components */
                    if (use_reduction && !i)
                    {
                        /* load remaining part of {0,0,z} column */
                        for (size_t j = 0; j < gvec__.z_column(i).z.size(); j++)
                        {
                            int z = (-gvec__.z_column(i).z[j] + grid_.size(2)) % grid_.size(2);
                            fftw_buffer_z_[tid][z] = std::conj(data__[data_offset + j]);
                        }
                    }

                    /* execute 1D transform of a z-column */
                    fftw_execute(plan_backward_z_[tid]);

                    /* load full column into auxiliary buffer */
                    std::copy(fftw_buffer_z_[tid], fftw_buffer_z_[tid] + grid_.size(2), &fft_buffer_aux_[i * grid_.size(2)]);

                    break;
                }
                case -1:
                {
                    /* load full column from auxiliary buffer */
                    std::copy(&fft_buffer_aux_[i * grid_.size(2)], &fft_buffer_aux_[i * grid_.size(2)] + grid_.size(2), fftw_buffer_z_[tid]);

                    /* execute 1D transform of a z-column */
                    fftw_execute(plan_forward_z_[tid]);

                    /* store PW coefficients */
                    for (size_t j = 0; j < gvec__.z_column(i).z.size(); j++)
                    {
                        int z = (gvec__.z_column(i).z[j] + grid_.size(2)) % grid_.size(2);
                        data__[data_offset + j] = fftw_buffer_z_[tid][z] * norm;
                    }
                    break;
                }
                default:
                {
                    TERMINATE("wrong direction");
                }
            }
        }
    }

    tcall_[2] += (omp_get_wtime() - t);
}

template <int direction, bool use_reduction>
void FFT3D::transform_z_parallel(Gvec const& gvec__, double_complex* data__)
{
    comm_.barrier();
    TIMER("sirius::FFT3D::transform_z_parallel");

    double t = omp_get_wtime();

    int rank = comm_.rank();
    int num_zcol_local = gvec__.zcol_fft_distr().counts[rank];
    double norm = 1.0 / size();

    if (direction == -1)
    {
        double t1 = omp_get_wtime();

        block_data_descriptor send(comm_.size());
        block_data_descriptor recv(comm_.size());
        for (int r = 0; r < comm_.size(); r++)
        {
            send.counts[r] = spl_z_.local_size(rank) * gvec__.zcol_fft_distr().counts[r];
            recv.counts[r] = spl_z_.local_size(r) * gvec__.zcol_fft_distr().counts[rank];
        }
        send.calc_offsets();
        recv.calc_offsets();

        std::copy(&fft_buffer_aux_[0], &fft_buffer_aux_[0] + gvec__.z_columns().size() * local_size_z_,
                  &fft_buffer_[0]);

        comm_.alltoall(&fft_buffer_[0], &send.counts[0], &send.offsets[0], &fft_buffer_aux_[0], &recv.counts[0], &recv.offsets[0]);

        tcall_[4] += (omp_get_wtime() - t1);
    }

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        #pragma omp for schedule(dynamic, 1)
        for (int i = 0; i < num_zcol_local; i++)
        {
            int icol = gvec__.zcol_fft_distr().offsets[rank] + i;
            int data_offset = gvec__.z_column(icol).offset;

            switch (direction)
            {
                case 1:
                {
                    /* clear z buffer */
                    std::fill(fftw_buffer_z_[tid], fftw_buffer_z_[tid] + grid_.size(2), 0);
                    /* load z column  of PW coefficients into buffer */
                    for (size_t j = 0; j < gvec__.z_column(icol).z.size(); j++)
                    {
                        int z = (gvec__.z_column(icol).z[j] + grid_.size(2)) % grid_.size(2);
                        fftw_buffer_z_[tid][z] = data__[data_offset + j];
                    }

                    /* column with {x,y} = {0,0} has only non-negative z components */
                    if (use_reduction && !icol)
                    {
                        /* load remaining part of {0,0,z} column */
                        for (size_t j = 0; j < gvec__.z_column(icol).z.size(); j++)
                        {
                            int z = (-gvec__.z_column(icol).z[j] + grid_.size(2)) % grid_.size(2);
                            fftw_buffer_z_[tid][z] = std::conj(data__[data_offset + j]);
                        }
                    }

                    /* perform local FFT transform of a column */
                    fftw_execute(plan_backward_z_[tid]);

                    /* redistribute z-column for a forthcoming all-to-all */ 
                    for (int r = 0; r < comm_.size(); r++)
                    {
                        int lsz = spl_z_.local_size(r);
                        int offs = spl_z_.global_offset(r);

                        std::copy(&fftw_buffer_z_[tid][offs], &fftw_buffer_z_[tid][offs + lsz], 
                                  &fft_buffer_aux_[offs * num_zcol_local + i * lsz]);
                    }

                    break;

                }
                case -1:
                {
                    /* collect full z-column */ 
                    for (int r = 0; r < comm_.size(); r++)
                    {
                        int lsz = spl_z_.local_size(r);
                        int offs = spl_z_.global_offset(r);
                        std::copy(&fft_buffer_aux_[offs * num_zcol_local + i * lsz],
                                  &fft_buffer_aux_[offs * num_zcol_local + i * lsz + lsz],
                                  &fftw_buffer_z_[tid][offs]);
                    }

                    /* perform local FFT transform of a column */
                    fftw_execute(plan_forward_z_[tid]);

                    /* save z column of PW coefficients*/
                    for (size_t j = 0; j < gvec__.z_column(icol).z.size(); j++)
                    {
                        int z = (gvec__.z_column(icol).z[j] + grid_.size(2)) % grid_.size(2);
                        data__[data_offset + j] = fftw_buffer_z_[tid][z] * norm;
                    }
                    break;

                }
                default:
                {
                    TERMINATE("wrong direction");
                }
            }
        }
    }

    /* scatter z-columns between slabs of FFT buffer */
    if (direction == 1)
    {
        double t1 = omp_get_wtime();

        block_data_descriptor send(comm_.size());
        block_data_descriptor recv(comm_.size());
        for (int r = 0; r < comm_.size(); r++)
        {
            send.counts[r] = spl_z_.local_size(r) * gvec__.zcol_fft_distr().counts[rank];
            recv.counts[r] = spl_z_.local_size(rank) * gvec__.zcol_fft_distr().counts[r];
        }
        send.calc_offsets();
        recv.calc_offsets();

        /* scatter z-columns */
        comm_.alltoall(&fft_buffer_aux_[0], &send.counts[0], &send.offsets[0], &fft_buffer_[0], &recv.counts[0], &recv.offsets[0]);

        /* copy local fractions of z-columns into auxiliary buffer */
        std::copy(&fft_buffer_[0], &fft_buffer_[0] + gvec__.z_columns().size() * local_size_z_,
                  &fft_buffer_aux_[0]);

        tcall_[4] += (omp_get_wtime() - t1);
    }
    comm_.barrier();

    tcall_[3] += (omp_get_wtime() - t);
}

template <int direction>
void FFT3D::transform(Gvec const& gvec__, double_complex* data__)
{
    TIMER("sirius::FFT3D::transform");

    ncall_++;
    double t = omp_get_wtime();

    /* reallocate auxiliary buffer if needed */
    size_t sz_max;
    if (comm_.size() > 1)
    {
        int rank = comm_.rank();
        int num_zcol_local = gvec__.zcol_fft_distr().counts[rank];
        /* we need this buffer for mpi_alltoall */
        sz_max = std::max(grid_.size(2) * num_zcol_local, local_size());
    }
    else
    {
        sz_max = grid_.size(2) * gvec__.z_columns().size();
    }
    if (sz_max > fft_buffer_aux_.size())
    {
        fft_buffer_aux_ = mdarray<double_complex, 1>(sz_max, "fft_buffer_aux_");
        #ifdef __GPU
        if (pu_ == GPU)
        {
            fft_buffer_aux_.pin_memory();
            fft_buffer_aux_.allocate_on_device();
        }
        #endif
    }

    /* single node FFT */
    if (comm_.size() == 1)
    {
        #ifdef __GPU
        if (pu_ == GPU && cufft3d_)
        {
            switch (direction)
            {
                case 1:
                {
                    cufft_backward_transform(cufft_plan_, fft_buffer_.at<GPU>());
                    break;
                }
                case -1:
                {
                    cufft_forward_transform(cufft_plan_, fft_buffer_.at<GPU>());
                    break;
                }
            }
        }
        #endif
        switch (direction)
        {
            case 1:
            {
                if (gvec__.reduced())
                {
                    transform_z_serial<1, true>(gvec__, data__);
                    transform_xy<1, true>(gvec__);
                }
                else
                {
                    transform_z_serial<1, false>(gvec__, data__);
                    transform_xy<1, false>(gvec__);
                }
                break;
            }
            case -1:
            {
                transform_xy<-1, false>(gvec__);
                transform_z_serial<-1, false>(gvec__, data__);
                break;
            }
            default:
            {
                TERMINATE("wrong direction");
            }
        }
    }
    else
    {
        switch (direction)
        {
            case 1:
            {
                if (gvec__.reduced())
                {
                    transform_z_parallel<1, true>(gvec__, data__);
                    transform_xy<1, true>(gvec__);
                }
                else
                {
                    transform_z_parallel<1, false>(gvec__, data__);
                    transform_xy<1, false>(gvec__);
                }
                break;
            }
            case -1:
            {
                transform_xy<-1, false>(gvec__);
                transform_z_parallel<-1, false>(gvec__, data__);
                break;
            }
            default:
            {
                TERMINATE("wrong direction");
            }
        }   
    }
    tcall_[0] += (omp_get_wtime() - t);
}

template void FFT3D::transform<1>(Gvec const& gvec__, double_complex* data__);
template void FFT3D::transform<-1>(Gvec const& gvec__, double_complex* data__);
        
};
