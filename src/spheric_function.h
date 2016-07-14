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

/** \file spheric_function.h
 *   
 *  \brief Contains declaration and implementation of sirius::Spheric_function and 
 *         sirius::Spheric_function_gradient classes.
 */

#ifndef __SPHERIC_FUNCTION_H__
#define __SPHERIC_FUNCTION_H__

#include <array>
#include <typeinfo>
#include "radial_grid.h"
#include "spline.h"

namespace sirius
{

/// Function in spherical harmonics or spherical coordinates representation.
template <function_domain_t domain_t, typename T = double_complex>
class Spheric_function
{
    private:

        /// Spheric function values.
        mdarray<T, 2> data_;
        
        /// Radial grid.
        Radial_grid const* radial_grid_;
        
        int angular_domain_size_;

        Spheric_function(Spheric_function<domain_t, T> const& src__) = delete;

        Spheric_function<domain_t, T>& operator=(Spheric_function<domain_t, T> const& src__) = delete;

    public:

        Spheric_function(Spheric_function<domain_t, T>&& src__)
        {
            data_ = std::move(src__.data_);
            radial_grid_ = src__.radial_grid_;
            angular_domain_size_ = src__.angular_domain_size_;
        }

        Spheric_function<domain_t, T>& operator=(Spheric_function<domain_t, T>&& src__)
        {
            if (this != &src__)
            {
                data_ = std::move(src__.data_);
                radial_grid_ = src__.radial_grid_;
                angular_domain_size_ = src__.angular_domain_size_;
            }
            return *this;
        }

        Spheric_function() : radial_grid_(nullptr)
        {
        }

        Spheric_function(int angular_domain_size__, Radial_grid const& radial_grid__) 
            : radial_grid_(&radial_grid__),
              angular_domain_size_(angular_domain_size__)
        {
            data_ = mdarray<T, 2>(angular_domain_size_, radial_grid_->num_points());
        }

        Spheric_function(T* ptr__, int angular_domain_size__, Radial_grid const& radial_grid__) 
            : radial_grid_(&radial_grid__),
              angular_domain_size_(angular_domain_size__)
        {
            data_ = mdarray<T, 2>(ptr__, angular_domain_size_, radial_grid_->num_points());
        }

        inline Spheric_function<domain_t, T>& operator+=(Spheric_function<domain_t, T>& rhs)
        {
            for (size_t i1 = 0; i1 < data_.size(1); i1++)
            {
                for (size_t i0 = 0; i0 < data_.size(0); i0++) data_(i0, i1) += rhs.data_(i0, i1);
            }
            
            return *this;
        }

        inline int angular_domain_size() const
        {
            return angular_domain_size_;
        }

        inline Radial_grid const& radial_grid() const
        {
            return *radial_grid_;
        }

        inline T& operator()(const int64_t i0, const int64_t i1) 
        {
            return data_(i0, i1);
        }

        inline T const& operator()(const int64_t i0, const int64_t i1) const 
        {
            return data_(i0, i1);
        }

        void zero()
        {
            data_.zero();
        }

        inline int size() const
        {
            return angular_domain_size_ * radial_grid_->num_points();
        }

        Spline<T> component(int lm__) const
        {
            if (domain_t != spectral) TERMINATE("function is not is spectral domain");

            Spline<T> s(radial_grid());
            for (int ir = 0; ir < radial_grid_->num_points(); ir++) s[ir] = data_(lm__, ir);
            return std::move(s.interpolate());
        }
};

template <function_domain_t domain_t, typename T>
Spheric_function<domain_t, T> operator+(Spheric_function<domain_t, T> const& a__, Spheric_function<domain_t, T> const& b__)
{
    if (a__.radial_grid().hash() != b__.radial_grid().hash()) TERMINATE("wrong radial grids");
    if (a__.angular_domain_size() != b__.angular_domain_size()) TERMINATE("wrong angular domain sizes");

    Spheric_function<domain_t, T> result(a__.angular_domain_size(), a__.radial_grid());

    for (int ir = 0; ir < a__.radial_grid().num_points(); ir++)
    {
        for (int i = 0; i < a__.angular_domain_size(); i++) result(i, ir) = a__(i, ir) + b__(i, ir);
    }

    return result;
}

/// Inner product of two spherical functions.
template <function_domain_t domain_t, typename T>
T inner(Spheric_function<domain_t, T> const& f1, Spheric_function<domain_t, T> const& f2)
{
    /* check radial grid */
    if (f1.radial_grid().hash() != f2.radial_grid().hash()) TERMINATE("radial grids don't match");

    Spline<T> s(f1.radial_grid());

    if (domain_t == spectral)
    {
        int lmmax = std::min(f1.angular_domain_size(), f2.angular_domain_size());
        for (int ir = 0; ir < s.num_points(); ir++)
        {
            for (int lm = 0; lm < lmmax; lm++)
                s[ir] += type_wrapper<T>::conjugate(f1(lm, ir)) * f2(lm, ir);
        }
    }
    else
    {
        STOP(); // and implement this
    }
    return s.interpolate().integrate(2);
}

/// Compute Laplacian of the spheric function.
/** Laplacian in spherical coordinates has the following expression:
 *  \f[
 *      \Delta = \frac{1}{r^2}\frac{\partial}{\partial r}\Big( r^2 \frac{\partial}{\partial r} \Big) + \frac{1}{r^2}\Delta_{\theta, \phi}
 *  \f]
 */
template <typename T>
Spheric_function<spectral, T> laplacian(Spheric_function<spectral, T>& f__)
{
    Spheric_function<spectral, T> g;
    auto& rgrid = f__.radial_grid();
    int lmmax = f__.angular_domain_size();
    int lmax = Utils::lmax_by_lmmax(lmmax);
    g = Spheric_function<spectral, T>(lmmax, rgrid);
    
    Spline<T> s(rgrid);
    for (int l = 0; l <= lmax; l++)
    {
        int ll = l * (l + 1);
        for (int m = -l; m <= l; m++)
        {
            int lm = Utils::lm_by_l_m(l, m);
            for (int ir = 0; ir < s.num_points(); ir++) s[ir] = f__(lm, ir);
            s.interpolate();
            
            for (int ir = 0; ir < s.num_points(); ir++) 
                g(lm, ir) = 2 * s.deriv(1, ir) * rgrid.x_inv(ir) + s.deriv(2, ir) - f__(lm, ir) * ll / std::pow(rgrid[ir], 2);
        }
    }

    return g;
}

/// Gradient of a spheric function.
template <function_domain_t domain_t, typename T = double_complex>
class Spheric_function_gradient
{
    private:

        Radial_grid const* radial_grid_;

        int angular_domain_size_;

        std::array<Spheric_function<domain_t, T>, 3> grad_;
    
    public:

        Spheric_function_gradient(int angular_domain_size__, Radial_grid const& radial_grid__) 
            : radial_grid_(&radial_grid__),
              angular_domain_size_(angular_domain_size__)
        {
        }

        inline Radial_grid const& radial_grid() const
        {
            return *radial_grid_;
        }

        inline int angular_domain_size() const
        {
            return angular_domain_size_;
        }

        inline Spheric_function<domain_t, T>& operator[](const int x)
        {
            assert(x >= 0 && x < 3);
            return grad_[x];
        }
};

/// Gradient of the function in complex spherical harmonics.
Spheric_function_gradient<spectral, double_complex> gradient(Spheric_function<spectral, double_complex>& f);

/// Gradient of the function in real spherical harmonics.
Spheric_function_gradient<spectral, double> gradient(Spheric_function<spectral, double>& f);

Spheric_function<spatial, double> operator*(Spheric_function_gradient<spatial, double>& f, 
                                            Spheric_function_gradient<spatial, double>& g);

}

#endif // __SPHERIC_FUNCTION_H__
