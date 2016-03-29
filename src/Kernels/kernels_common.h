#include <cuComplex.h>
#include <map>
#include <vector>
#include <string>
#include <stdio.h>

cudaStream_t cuda_stream_by_id(int stream_id__);

const double twopi = 6.2831853071795864769;

inline __device__ size_t array2D_offset(int i0, int i1, int ld0)
{
    return i0 + i1 * ld0;
}

// TODO: can be optimized in terms of multiplication
inline __device__ size_t array3D_offset(int i0, int i1, int i2, int ld0, int ld1)
{
    return i0 + i1 * ld0 + i2 * ld0 * ld1;
}

// TODO: can be optimized in terms of multiplication
inline __device__ size_t array4D_offset(int i0, int i1, int i2, int i3, int ld0, int ld1, int ld2)
{
    return i0 + i1 * ld0 + i2 * ld0 * ld1 + i3 * ld0 * ld1 * ld2;
}

inline __host__ __device__ int num_blocks(int length, int block_size)
{
    return (length / block_size) + min(length % block_size, 1);
}

class CUDA_timers_wrapper
{
    private:

        std::map<std::string, std::vector<float> > cuda_timers_;

    public:

        void add_measurment(std::string const& label, float value)
        {
            cuda_timers_[label].push_back(value / 1000);
        }

        void print()
        {
            printf("\n");
            printf("CUDA timers \n");
            for (int i = 0; i < 115; i++) printf("-");
            printf("\n");
            printf("name                                                              count      total        min        max    average\n");
            for (int i = 0; i < 115; i++) printf("-");
            printf("\n");

            std::map<std::string, std::vector<float> >::iterator it;
            for (it = cuda_timers_.begin(); it != cuda_timers_.end(); it++)
            {
                int count = (int)it->second.size();
                double total = 0.0;
                float minval = 1e10;
                float maxval = 0.0;
                for (int i = 0; i < count; i++)
                {
                    total += it->second[i];
                    minval = std::min(minval, it->second[i]);
                    maxval = std::max(maxval, it->second[i]);
                }
                double average = (count == 0) ? 0.0 : total / count;
                if (count == 0) minval = 0.0;

                printf("%-60s :    %5i %10.4f %10.4f %10.4f %10.4f\n", it->first.c_str(), count, total, minval, maxval, average);
            }
        }
};

class CUDA_timer
{
    private:

        cudaEvent_t e_start_;
        cudaEvent_t e_stop_;
        bool active_;
        std::string label_;

        void start()
        {
            cudaEventCreate(&e_start_);
            cudaEventCreate(&e_stop_);
            cudaEventRecord(e_start_, 0);
        }

        void stop()
        {
            float time;
            cudaEventRecord(e_stop_, 0);
            cudaEventSynchronize(e_stop_);
            cudaEventElapsedTime(&time, e_start_, e_stop_);
            cudaEventDestroy(e_start_);
            cudaEventDestroy(e_stop_);
            cuda_timers_wrapper().add_measurment(label_, time);
            active_ = false;
        }

    public:

        CUDA_timer(std::string const& label__) : label_(label__), active_(false)
        {
            start();
        }

        ~CUDA_timer()
        {
            stop();
        }

        static CUDA_timers_wrapper& cuda_timers_wrapper()
        {
            static CUDA_timers_wrapper cuda_timers_wrapper_;
            return cuda_timers_wrapper_;
        }
};
