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

/** \file utils.h
 *   
 *  \brief Contains definition and partial implementation of sirius::Utils class.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <gsl/gsl_sf_erf.h>
#include <fstream>
#include <string>
#include <complex>
#include "sirius_internal.h"
#include "typedefs.h"
#include "constants.h"
#include "mdarray.h"
#include "vector3d.h"
#include "matrix3d.h"

/// Utility class.
class Utils
{
    public:
        
        /// Maximum number of \f$ \ell, m \f$ combinations for a given \f$ \ell_{max} \f$
        static inline int lmmax(int lmax)
        {
            return (lmax + 1) * (lmax + 1);
        }

        static inline int lm_by_l_m(int l, int m)
        {
            return (l * l + l + m);
        }

        static inline int lmax_by_lmmax(int lmmax__)
        {
            int lmax = int(std::sqrt(double(lmmax__)) + 1e-8) - 1;
            if (lmmax(lmax) != lmmax__) TERMINATE("wrong lmmax");
            return lmax;
        }

        static inline bool file_exists(const std::string file_name)
        {
            std::ifstream ifs(file_name.c_str());
            if (ifs.is_open()) return true;
            return false;
        }

        static inline double fermi_dirac_distribution(double e)
        {
            double kT = 0.001;
            if (e > 100 * kT) return 0.0;
            if (e < -100 * kT) return 1.0;
            return (1.0 / (exp(e / kT) + 1.0));
        }
        
        static inline double gaussian_smearing(double e, double delta)
        {
            return 0.5 * (1 - gsl_sf_erf(e / delta));
        }
        
        static inline double cold_smearing(double e)
        {
            double a = -0.5634;

            if (e < -10.0) return 1.0;
            if (e > 10.0) return 0.0;

            return 0.5 * (1 - gsl_sf_erf(e)) - 1 - 0.25 * exp(-e * e) * (a + 2 * e - 2 * a * e * e) / sqrt(pi);
        }

        static std::string double_to_string(double val, int precision = -1)
        {
            char buf[100];

            double abs_val = std::abs(val);

            if (precision == -1)
            {
                if (abs_val > 1.0) 
                {
                    precision = 6;
                }
                else if (abs_val > 1e-14)
                {
                    precision = int(-std::log(abs_val) / std::log(10.0)) + 7;
                }
                else
                {
                    return std::string("0.0");
                }
            }

            std::stringstream fmt;
            fmt << "%." << precision << "f";
        
            int len = snprintf(buf, 100, fmt.str().c_str(), val);
            for (int i = len - 1; i >= 1; i--) 
            {
                if (buf[i] == '0' && buf[i - 1] == '0') 
                {
                    buf[i] = 0;
                }
                else
                {
                    break;
                }
            }
            return std::string(buf);
        }

        static inline double phi_by_sin_cos(double sinp, double cosp)
        {
            double phi = std::atan2(sinp, cosp);
            if (phi < 0) phi += twopi;
            return phi;
        }

        static inline long double factorial(int n)
        {
            assert(n >= 0);

            long double result = 1.0L;
            for (int i = 1; i <= n; i++) result *= i;
            return result;
        }
        
        /// Simple hash function.
        /** Example: printf("hash: %16llX\n", hash()); */
        static uint64_t hash(void const* buff, size_t size, uint64_t h = 5381)
        {
            unsigned char const* p = static_cast<unsigned char const*>(buff);
            for(size_t i = 0; i < size; i++) h = ((h << 5) + h) + p[i];
            return h;
        }

        static void write_matrix(const std::string& fname, mdarray<double_complex, 2>& matrix, int nrow, int ncol,
                                 bool write_upper_only = true, bool write_abs_only = false, std::string fmt = "%18.12f");
        
        static void write_matrix(std::string const& fname, bool write_all, mdarray<double, 2>& matrix);

        static void write_matrix(std::string const& fname, bool write_all, matrix<double_complex> const& mtrx);

        static void check_hermitian(const std::string& name, mdarray<double_complex, 2>& mtrx, int n = -1);

        static double confined_polynomial(double r, double R, int p1, int p2, int dm);

        static std::vector<int> l_by_lm(int lmax);

        static std::pair< vector3d<double>, vector3d<int> > reduce_coordinates(vector3d<double> coord);

        static vector3d<int> find_translations(double radius__, matrix3d<double> const& lattice_vectors__);

        static std::vector< std::pair<int, int> > l_m_by_lm(int lmax)
        {
            std::vector< std::pair<int, int> > l_m(lmmax(lmax));
            for (int l = 0; l <= lmax; l++)
            {
                for (int m = -l; m <= l; m++)
                {
                    int lm = lm_by_l_m(l, m);
                    l_m[lm].first = l;
                    l_m[lm].second = m;
                }
            }
            return l_m;
        }

        inline static double round(double a__, int n__)
        {
            double a0 = std::floor(a__);
            double b = std::round((a__ - a0) * std::pow(10, n__)) / std::pow(10, n__);
            return a0 + b;
        }

        template <typename T>
        inline static int sign(T val)
        {
            return (T(0) < val) - (val < T(0));
        }
};

#endif

