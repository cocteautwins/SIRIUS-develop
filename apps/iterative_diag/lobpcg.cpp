#include <sirius.h>

using namespace sirius;

//== // N : subspace dimension
//== // n : required number of eigen pairs
//== // bs : block size (number of new basis functions)
//== // phi : basis functions
//== // hphi : H|phi>
//== void subspace_diag(Global& parameters, int N, int n, int bs, mdarray<double_complex, 2>& phi, mdarray<double_complex, 2>& hphi, 
//==                    mdarray<double_complex, 2>& res, double_complex v0, mdarray<double_complex, 2>& evec, std::vector<double>& eval)
//== {
//==     //== for (int i = 0; i < N; i++)
//==     //== {
//==     //==     for (int j = 0; j < N; j++)
//==     //==     {
//==     //==         double_complex z(0, 0);
//==     //==         for (int ig = 0; ig < parameters.num_gvec(); ig++)
//==     //==         {
//==     //==             z += conj(phi(ig, i)) * phi(ig, j);
//==     //==         }
//==     //==         if (i == j) z -= 1.0;
//==     //==         if (abs(z) > 1e-12) error_local(__FILE__, __LINE__, "basis is not orthogonal");
//==     //==     }
//==     //== }
//== 
//==     standard_evp* solver = new standard_evp_lapack();
//== 
//==     eval.resize(N);
//== 
//==     mdarray<double_complex, 2> hmlt(N, N);
//==     blas<cpu>::gemm(2, 0, N, N, parameters.num_gvec(), &phi(0, 0), phi.ld(), &hphi(0, 0), hphi.ld(), &hmlt(0, 0), hmlt.ld());
//==     //== for (int i = 0; i < N; i++)
//==     //== {
//==     //==     for (int j = 0; j < N; j++)
//==     //==     {
//==     //==         for (int ig = 0; ig < parameters.num_gvec(); ig++) hmlt(i, j) += conj(phi(ig, i)) * hphi(ig, j);
//==     //==     }
//==     //== }
//== 
//==     solver->solve(N, hmlt.get_ptr(), hmlt.ld(), &eval[0], evec.get_ptr(), evec.ld());
//== 
//==     delete solver;
//== 
//==     printf("\n");
//==     printf("Lowest eigen-values : \n");
//==     for (int i = 0; i < std::min(N, 10); i++)
//==     {
//==         printf("i : %i,  eval : %16.10f\n", i, eval[i]);
//==     }
//== 
//==     // compute residuals
//==     res.zero();
//==     for (int j = 0; j < bs; j++)
//==     {
//==         int i = j; //n - bs + j;
//==         for (int mu = 0; mu < N; mu++)
//==         {
//==             for (int ig = 0; ig < parameters.num_gvec(); ig++)
//==             {
//==                 res(ig, j) += (evec(mu, i) * hphi(ig, mu) - eval[i] * evec(mu, i) * phi(ig, mu));
//==             }
//==         }
//==         double norm = 0.0;
//==         for (int ig = 0; ig < parameters.num_gvec(); ig++) norm += real(conj(res(ig, j)) * res(ig, j));
//==         for (int ig = 0; ig < parameters.num_gvec(); ig++) res(ig, j) /= sqrt(norm);
//==     }
//==     
//==     // additional basis vectors
//==     for (int j = 0; j < bs; j++)
//==     {
//==         int i = j; //n - bs + j;
//==         for (int ig = 0; ig < parameters.num_gvec(); ig++)
//==         {
//==             double_complex t = pow(parameters.gvec_len(ig), 2) / 2.0 + v0 - eval[i];
//==             if (abs(t) < 1e-12) error_local(__FILE__, __LINE__, "problematic division");
//==             res(ig, j) /= t;
//==         }
//==     }
//== }
//== 

void apply_h(Global& parameters, K_point& kp, int n, std::vector<double_complex>& v_r, double_complex* phi__, double_complex* hphi__)
{
    mdarray<double_complex, 2> phi(phi__, kp.num_gkvec(), n);
    mdarray<double_complex, 2> hphi(hphi__, kp.num_gkvec(), n);
    auto fft = parameters.reciprocal_lattice()->fft();
    std::vector<double_complex> phi_r(fft->size());

    for (int i = 0; i < n; i++)
    {
        fft->input(kp.num_gkvec(), kp.fft_index(), &phi(0, i));
        fft->transform(1);
        fft->output(&phi_r[0]);

        for (int ir = 0; ir < fft->size(); ir++) phi_r[ir] *= v_r[ir];

        fft->input(&phi_r[0]);
        fft->transform(-1);
        fft->output(kp.num_gkvec(), kp.fft_index(), &hphi(0, i));

        for (int ig = 0; ig < kp.num_gkvec(); ig++) hphi(ig, i) += phi(ig, i) * pow(kp.gkvec_cart(ig).length(), 2) / 2.0;
    }
}

//==int diag_davidson(Global& parameters, int niter, int bs, int n, std::vector<double_complex>& v_pw, mdarray<double_complex, 2>& phi, 
//==                  mdarray<double_complex, 2>& evec)
//=={
//==    std::vector<double_complex> v_r(parameters.fft().size());
//==    parameters.fft().input(parameters.num_gvec(), parameters.fft_index(), &v_pw[0]);
//==    parameters.fft().transform(1);
//==    parameters.fft().output(&v_r[0]);
//==
//==    for (int ir = 0; ir < parameters.fft().size(); ir++)
//==    {
//==        if (fabs(imag(v_r[ir])) > 1e-14) error_local(__FILE__, __LINE__, "potential is complex");
//==    }
//==
//==    mdarray<double_complex, 2> hphi(parameters.num_gvec(), phi.size(1));
//==    mdarray<double_complex, 2> res(parameters.num_gvec(), bs);
//==    
//==    int N = n;
//==
//==    apply_h(parameters, N, v_r, &phi(0, 0), &hphi(0, 0));
//==
//==    std::vector<double> eval1;
//==    std::vector<double> eval2;
//==
//==    for (int iter = 0; iter < niter; iter++)
//==    {
//==        eval2 = eval1;
//==        subspace_diag(parameters, N, n, bs, phi, hphi, res, v_pw[0], evec, eval1);
//==        expand_subspace(parameters, N, bs, phi, res);
//==        apply_h(parameters, bs, v_r, &res(0, 0), &hphi(0, N));
//==
//==        if (iter)
//==        {
//==            double diff = 0;
//==            for (int i = 0; i < n; i++) diff += fabs(eval1[i] - eval2[i]);
//==            std::cout << "Eigen-value error : " << diff << std::endl;
//==        }
//==        
//==        N += bs;
//==    }
//==    return N - bs;
//==}

///void orthonormalize(mdarray<double_complex, 2>& f)
///{
///    std::vector<double_complex> v(f.size(0));
///    for (int j = 0; j < f.size(1); j++)
///    {
///        memcpy(&v[0], &f(0, j), f.size(0) * sizeof(double_complex));
///        for (int j1 = 0; j1 < j; j1++)
///        {
///            double_complex z(0, 0);
///            for (int ig = 0; ig < f.size(0); ig++) z += conj(f(ig, j1)) * v[ig];
///            for (int ig = 0; ig < f.size(0); ig++) v[ig] -= z * f(ig, j1);
///        }
///        double norm = 0;
///        for (int ig = 0; ig < f.size(0); ig++) norm += real(conj(v[ig]) * v[ig]);
///        for (int ig = 0; ig < f.size(0); ig++) f(ig, j) = v[ig] / sqrt(norm);
///    }
///}
///
///void check_orth(mdarray<double_complex, 2>& f)
///{
///    for (int i = 0; i < f.size(1); i++)
///    {
///        for (int j = 0; j < f.size(1); j++)
///        {
///            double_complex z(0, 0);
///            for (int ig = 0; ig < f.size(0); ig++)
///            {
///                z += conj(f(ig, i)) * f(ig, j);
///            }
///            if (i == j) z -= 1.0;
///            if (abs(z) > 1e-10)
///            {
///                std::stringstream s;
///                s << "basis is not orthonormal, error : " << abs(z);
///                error_local(__FILE__, __LINE__, s);
///            }
///        }
///    }
///}

void apply_p(K_point& kp, mdarray<double_complex, 2>& r)
{
    for (int i = 0; i < (int)r.size(1); i++)
    {
        // compute kinetic energy of the vector
        double ekin = 0;
        for (int ig = 0; ig < kp.num_gkvec(); ig++) ekin += real(conj(r(ig, i)) * r(ig, i)) * pow(kp.gkvec_cart(ig).length(), 2) / 2.0;

        // apply the preconditioner
        for (int ig = 0; ig < kp.num_gkvec(); ig++)
        {
            double x = pow(kp.gkvec_cart(ig).length(), 2) / 2 / 1.5 / ekin;
            r(ig, i) = r(ig, i) * (27 + 18 * x + 12 * x * x + 8 * x * x * x) / (27 + 18 * x + 12 * x * x + 8 * x * x * x + 16 * x * x * x * x);
        }
    }
}

void apply_p(K_point& kp, mdarray<double_complex, 2>& r, std::vector<double>& h_diag, std::vector<double>& eval)
{
    for (int i = 0; i < (int)r.size(1); i++)
    {
        for (int ig = 0; ig < kp.num_gkvec(); ig++)
        {
            r(ig, i) = r(ig, i) / (h_diag[ig] - eval[i]);
        }
    }
}

void diag_lobpcg(Global& parameters, K_point& kp, std::vector<double_complex>& v_pw, int num_bands)
{
    auto fft = parameters.reciprocal_lattice()->fft();
    std::vector<double_complex> v_r(fft->size());
    fft->input(parameters.reciprocal_lattice()->num_gvec(), parameters.reciprocal_lattice()->fft_index(), &v_pw[0]);
    fft->transform(1);
    fft->output(&v_r[0]);

    for (int ir = 0; ir < fft->size(); ir++)
    {
        if (fabs(imag(v_r[ir])) > 1e-10) error_local(__FILE__, __LINE__, "potential is complex");
    }

    // initial wave-functions
    mdarray<double_complex, 2> psi(kp.num_gkvec(), num_bands);
    psi.zero();
    for (int j = 0; j < num_bands; j++) psi(j, j) = complex_one;


    // initial basis functions
    mdarray<double_complex, 2> phi(kp.num_gkvec(), 3 * num_bands);
    phi.zero();
    for (int j = 0; j < num_bands; j++)
    {
        memcpy(&phi(0, j), &psi(0, j), kp.num_gkvec() * sizeof(double_complex));
    }

    mdarray<double_complex, 2> p(kp.num_gkvec(), num_bands);

    mdarray<double_complex, 2> hmlt(3 * num_bands, 3 * num_bands);
    mdarray<double_complex, 2> ovlp(3 * num_bands, 3 * num_bands);
    mdarray<double_complex, 2> evec(3 * num_bands, num_bands);

    std::vector<double> eval(num_bands);
    std::vector<double> eval_tmp(num_bands);
    std::vector<double> res_norm(num_bands);

    mdarray<double_complex, 2> hphi(kp.num_gkvec(), 3 * num_bands);

    std::vector<double> h_diag(kp.num_gkvec());
    for (int igk = 0; igk < kp.num_gkvec(); igk++)
        h_diag[igk] = std::pow(kp.gkvec_cart(igk).length(), 2) / 2 + real(v_pw[0]);

    generalized_evp* gevp = new generalized_evp_lapack(-1.0);

    for (int k = 0; k < 300; k++)
    {
        apply_h(parameters, kp, num_bands, v_r, &phi(0, 0), &hphi(0, 0));

        // Rayleigh–Ritz values
        for (int j = 0; j < num_bands; j++)
        {
            double a(0), b(0);
            for (int igk = 0; igk < kp.num_gkvec(); igk++)
            {
                a += real(conj(phi(igk, j)) * hphi(igk, j));
                b += real(conj(phi(igk, j)) * phi(igk, j));
            }
            eval[j] = a / b;
        }

        // res = H|phi> - E|phi>
        for (int j = 0; j < num_bands; j++)
        {
            double r = 0;
            for (int igk = 0; igk < kp.num_gkvec(); igk++) 
            {
                phi(igk, num_bands + j) = hphi(igk, j) - eval[j] * phi(igk, j);
                r += std::pow(std::abs(phi(igk, num_bands + j)), 2);
            }
            res_norm[j] = std::sqrt(r);
        }
        mdarray<double_complex, 2> res(&phi(0, num_bands), kp.num_gkvec(), num_bands);
        if (k > 2) apply_p(kp, res, h_diag, eval);

        std::cout << "Iteration : " << k << std::endl;
        for (int j = 0; j < num_bands; j++)
        {
            printf("band : %2i, residual : %12.8f, eval : %12.8f\n", j, res_norm[j], eval[j]);
        }

        int gevp_size = (k == 0) ? 2 * num_bands : 3 * num_bands;

        int num_happ = (k == 0) ? num_bands : 2 * num_bands;

        apply_h(parameters, kp, num_happ, v_r, &phi(0, num_bands), &hphi(0, num_bands));
        
        blas<cpu>::gemm(2, 0, gevp_size, gevp_size, kp.num_gkvec(), &phi(0, 0), phi.ld(), &hphi(0, 0), hphi.ld(), 
                        &hmlt(0, 0), hmlt.ld());
        
        blas<cpu>::gemm(2, 0, gevp_size, gevp_size, kp.num_gkvec(), &phi(0, 0), phi.ld(), &phi(0, 0), phi.ld(), 
                        &ovlp(0, 0), ovlp.ld());

        gevp->solve(gevp_size, gevp_size, gevp_size, num_bands, hmlt.ptr(), hmlt.ld(), ovlp.ptr(), ovlp.ld(), 
                    &eval_tmp[0], evec.ptr(), evec.ld());
            
        blas<cpu>::gemm(0, 0, kp.num_gkvec(), num_bands, gevp_size, &phi(0, 0), phi.ld(), 
                        &evec(0, 0), evec.ld(), &psi(0, 0), psi.ld());

        blas<cpu>::gemm(0, 0, kp.num_gkvec(), num_bands, num_happ, &phi(0, num_bands), phi.ld(), 
                        &evec(num_bands, 0), evec.ld(), &p(0, 0), p.ld());

        for (int j = 0; j < num_bands; j++)
        {
            memcpy(&phi(0, j), &psi(0, j), kp.num_gkvec() * sizeof(double_complex));
            memcpy(&phi(0, 2 * num_bands + j), &p(0, j), kp.num_gkvec() * sizeof(double_complex));
        }
    }




    //apply_h(parameters, kp, num_bands, v_r, &phi(0, 0), &hphi(0, 0));

    //mdarray<double_complex, 2> ovlp(3 * num_bands, 3 * num_bands);

    //mdarray<double_complex, 2> hmlt(3 * num_bands, 3 * num_bands);
    //blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &phi(0, 0), phi.ld(), &hphi(0, 0), hphi.ld(), &hmlt(0, 0), hmlt.ld());

    //std::vector<double> eval(3 * num_bands);
    //mdarray<double_complex, 2> evec(3 * num_bands, 3 * num_bands);
    //
    //standard_evp* solver = new standard_evp_lapack();
    //solver->solve(num_bands, hmlt.ptr(), hmlt.ld(), &eval[0], evec.ptr(), evec.ld());
    //delete solver;

    //mdarray<double_complex, 2> zm(kp.num_gkvec(), num_bands);
    //blas<cpu>::gemm(0, 0, kp.num_gkvec(), num_bands, num_bands, &phi(0, 0), phi.ld(), &evec(0, 0), evec.ld(), &zm(0, 0), zm.ld());
    //zm >> phi;

    //mdarray<double_complex, 2> res(kp.num_gkvec(), num_bands);
    //mdarray<double_complex, 2> hres(kp.num_gkvec(), num_bands);

    //mdarray<double_complex, 2> grad(kp.num_gkvec(), num_bands);
    //grad.zero();
    //mdarray<double_complex, 2> hgrad(kp.num_gkvec(), num_bands);
    //
    //generalized_evp* gevp = new generalized_evp_lapack(-1.0);

    //for (int k = 1; k < 300; k++)
    //{
    //    apply_h(parameters, kp, num_bands, v_r, &phi(0, 0), &hphi(0, 0));
    //    // res = H|phi> - E|phi>
    //    for (int i = 0; i < num_bands; i++)
    //    {
    //        for (int ig = 0; ig < kp.num_gkvec(); ig++) 
    //        {
    //            //double_complex t = pow(kp.gkvec_cart(ig).length(), 2) / 2.0 + v_pw[0] - eval[i];
    //            res(ig, i) = hphi(ig, i) - eval[i] * phi(ig, i);
    //            
    //            //if (abs(t) < 1e-12) error_local(__FILE__, __LINE__, "problematic division");
    //            //res(ig, i) /= t;
    //        }
    //    }

    //    std::cout << "Iteration : " << k << std::endl;
    //    for (int i = 0; i< num_bands; i++)
    //    {
    //        double r = 0;
    //        for (int ig = 0; ig < kp.num_gkvec(); ig++) r += real(conj(res(ig, i)) * res(ig, i));
    //        std::cout << "band : " << i << " residiual : " << r << " eigen-value : " << eval[i] << std::endl;
    //    }

    //    //apply_p(kp, res);

    //    //orthonormalize(res);
    //    apply_h(parameters, kp, num_bands, v_r, &res(0, 0), &hres(0, 0));

    //    hmlt.zero();
    //    ovlp.zero();
    //    for (int i = 0; i < 3 * num_bands; i++) ovlp(i, i) = double_complex(1, 0);

    //    // <phi|H|phi>
    //    blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &phi(0, 0), phi.ld(), &hphi(0, 0), hphi.ld(), 
    //                    &hmlt(0, 0), hmlt.ld());
    //    // <phi|H|res>
    //    blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &phi(0, 0), phi.ld(), &hres(0, 0), hres.ld(), 
    //                    &hmlt(0, num_bands), hmlt.ld());
    //    // <res|H|res>
    //    blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &res(0, 0), res.ld(), &hres(0, 0), hres.ld(), 
    //                    &hmlt(num_bands, num_bands), hmlt.ld());

    //    // <phi|res> 
    //    blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &phi(0, 0), phi.ld(), &res(0, 0), res.ld(), 
    //                    &ovlp(0, num_bands), ovlp.ld());
    //    
    //    // <res|res> 
    //    blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &res(0, 0), res.ld(), &res(0, 0), res.ld(), 
    //                    &ovlp(num_bands, num_bands), ovlp.ld());

    //    if (k == 1)
    //    {
    //        gevp->solve(2 * num_bands, 2 * num_bands, 2 * num_bands, num_bands, hmlt.ptr(), hmlt.ld(), ovlp.ptr(), ovlp.ld(), 
    //                    &eval[0], evec.ptr(), evec.ld());
    //    } 
    //    else
    //    {
    //        //orthonormalize(grad);
    //        apply_h(parameters, kp, num_bands, v_r, &grad(0, 0), &hgrad(0, 0));

    //        // <phi|H|grad>
    //        blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &phi(0, 0), phi.ld(), &hgrad(0, 0), hgrad.ld(), 
    //                        &hmlt(0, 2 * num_bands), hmlt.ld());
    //        // <res|H|grad>
    //        blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &res(0, 0), res.ld(), &hgrad(0, 0), hgrad.ld(), 
    //                        &hmlt(num_bands, 2 * num_bands), hmlt.ld());
    //        // <grad|H|grad>
    //        blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &grad(0, 0), grad.ld(), &hgrad(0, 0), hgrad.ld(), 
    //                        &hmlt(2 * num_bands, 2 * num_bands), hmlt.ld());
    //        
    //        // <phi|grad> 
    //        blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &phi(0, 0), phi.ld(), &grad(0, 0), grad.ld(), 
    //                        &ovlp(0, 2 * num_bands), ovlp.ld());
    //        // <res|grad> 
    //        blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &res(0, 0), res.ld(), &grad(0, 0), grad.ld(), 
    //                        &ovlp(num_bands, 2 * num_bands), ovlp.ld());
    //        // <grad|grad> 
    //        blas<cpu>::gemm(2, 0, num_bands, num_bands, kp.num_gkvec(), &grad(0, 0), grad.ld(), &grad(0, 0), grad.ld(), 
    //                        &ovlp(2 * num_bands, 2 * num_bands), ovlp.ld());
    //        
    //        gevp->solve(3 * num_bands, 3 * num_bands, 3 * num_bands, num_bands, hmlt.ptr(), hmlt.ld(), ovlp.ptr(), ovlp.ld(), 
    //                    &eval[0], evec.ptr(), evec.ld());
    //        
    //    }
    //    
    //    grad >> zm;
    //    // P^{k+1} = P^{k} * Z_{grad} + res^{k} * Z_{res}
    //    blas<cpu>::gemm(0, 0, kp.num_gkvec(), num_bands, num_bands, &res(0, 0), res.ld(), 
    //                    &evec(num_bands, 0), evec.ld(), &grad(0, 0), grad.ld());
    //    if (k > 1) 
    //    {
    //        blas<cpu>::gemm(0, 0, kp.num_gkvec(), num_bands, num_bands, double_complex(1, 0), &zm(0, 0), zm.ld(), 
    //                        &evec(2 * num_bands, 0), evec.ld(), double_complex(1, 0), &grad(0, 0), grad.ld());
    //    }

    //    // phi^{k+1} = phi^{k} * Z_{phi} + P^{k+1}
    //    phi >> zm;
    //    grad >> phi;
    //    blas<cpu>::gemm(0, 0, kp.num_gkvec(), num_bands, num_bands, double_complex(1, 0), &zm(0, 0), zm.ld(), 
    //                    &evec(0, 0), evec.ld(), double_complex(1, 0), &phi(0, 0), phi.ld());

    //    //check_orth(phi);    
    //}
    //

    delete gevp;

}

//== void expand_subspace(K_point& kp, int N, int num_bands, mdarray<double_complex, 2>& phi, mdarray<double_complex, 2>& res)
//== {
//==     // overlap between new addisional basis vectors and old basis vectors
//==     mdarray<double_complex, 2> ovlp(N, num_bands);
//==     ovlp.zero();
//==     for (int i = 0; i < N; i++)
//==     {
//==         for (int j = 0; j < num_bands; j++) 
//==         {
//==             for (int ig = 0; ig < kp.num_gkvec(); ig++) ovlp(i, j) += conj(phi(ig, i)) * res(ig, j);
//==         }
//==     }
//== 
//==     // project out the the old subspace
//==     for (int j = 0; j < num_bands; j++)
//==     {
//==         for (int i = 0; i < N; i++)
//==         {
//==             for (int ig = 0; ig < kp.num_gkvec(); ig++) res(ig, j) -= ovlp(i, j) * phi(ig, i);
//==         }
//==     }
//==     orthonormalize(res);
//== 
//==     for (int j = 0; j < num_bands; j++)
//==     {
//==         for (int ig = 0; ig < kp.num_gkvec(); ig++) phi(ig, N + j) = res(ig, j);
//==     }
//== }
//== 
//== void diag_davidson(Global& parameters, K_point& kp, std::vector<double_complex>& v_pw, int num_bands)
//== {
//==     std::vector<double_complex> v_r(parameters.fft().size());
//==     parameters.fft().input(parameters.num_gvec(), parameters.fft_index(), &v_pw[0]);
//==     parameters.fft().transform(1);
//==     parameters.fft().output(&v_r[0]);
//== 
//==     for (int ir = 0; ir < parameters.fft().size(); ir++)
//==     {
//==         if (fabs(imag(v_r[ir])) > 1e-10) error_local(__FILE__, __LINE__, "potential is complex");
//==     }
//== 
//==     int num_iter = 5;
//== 
//==     int num_big_iter = 5;
//== 
//==     // initial basis functions
//==     mdarray<double_complex, 2> phi(kp.num_gkvec(), num_bands * num_iter);
//==     phi.zero();
//==     for (int i = 0; i < num_bands; i++) phi(i, i) = 1.0;
//== 
//==     mdarray<double_complex, 2> hphi(kp.num_gkvec(), num_bands * num_iter);
//== 
//== 
//==     mdarray<double_complex, 2> hmlt(num_bands * num_iter, num_bands * num_iter);
//==     mdarray<double_complex, 2> evec(num_bands * num_iter, num_bands * num_iter);
//==     std::vector<double> eval(num_bands * num_iter);
//==     
//==     mdarray<double_complex, 2> res(kp.num_gkvec(), num_bands);
//== 
//==     standard_evp* solver = new standard_evp_lapack();
//== 
//==     for (int l = 0; l < num_big_iter; l++)
//==     {
//==         for (int k = 1; k <= num_iter; k++)
//==         {
//==             int N = k * num_bands;
//== 
//==             apply_h(parameters, kp, num_bands, v_r, &phi(0, (k - 1) * num_bands), &hphi(0, (k - 1) * num_bands));
//== 
//==             blas<cpu>::gemm(2, 0, N, N, kp.num_gkvec(), &phi(0, 0), phi.ld(), &hphi(0, 0), hphi.ld(), &hmlt(0, 0), hmlt.ld());
//== 
//==             solver->solve(N, hmlt.get_ptr(), hmlt.ld(), &eval[0], evec.get_ptr(), evec.ld());
//==             
//==             // compute residuals
//==             res.zero();
//==             for (int j = 0; j < num_bands; j++)
//==             {
//==                 for (int mu = 0; mu < N; mu++)
//==                 {
//==                     for (int ig = 0; ig < kp.num_gkvec(); ig++)
//==                     {
//==                         res(ig, j) += (evec(mu, j) * hphi(ig, mu) - eval[j] * evec(mu, j) * phi(ig, mu));
//==                     }
//==                 }
//==             }
//== 
//==             std::cout << "Iteration : " << k << std::endl;
//==             for (int i = 0; i < num_bands; i++)
//==             {
//==                 double r = 0;
//==                 for (int ig = 0; ig < kp.num_gkvec(); ig++) r += real(conj(res(ig, i)) * res(ig, i));
//==                 //for (int ig = 0; ig < kp.num_gkvec(); ig++) res(ig, i) /= sqrt(r);
//==                 std::cout << "band : " << i << " residiual : " << r << " eigen-value : " << eval[i] << std::endl;
//==             }
//== 
//==             //apply_p(kp, res);
//==             
//==             for (int j = 0; j < num_bands; j++)
//==             {
//==                 for (int ig = 0; ig < kp.num_gkvec(); ig++)
//==                 {
//==                     double_complex t = pow(kp.gkvec_cart(ig).length(), 2) / 2.0 + v_pw[0] - eval[j];
//==                     if (abs(t) < 1e-12) error_local(__FILE__, __LINE__, "problematic division");
//==                     res(ig, j) /= t;
//==                 }
//==             }
//== 
//==             if (k < num_iter) expand_subspace(kp, N, num_bands, phi, res);
//==         }
//== 
//==         mdarray<double_complex, 2> zm(kp.num_gkvec(), num_bands);
//==         blas<cpu>::gemm(0, 0, kp.num_gkvec(), num_bands, num_bands * num_iter, &phi(0, 0), phi.ld(), 
//==                         &evec(0, 0), evec.ld(), &zm(0, 0), zm.ld());
//==         //check_orth(zm);
//==         memcpy(phi.get_ptr(), zm.get_ptr(), num_bands * kp.num_gkvec() * sizeof(double_complex));
//==     }
//== 
//==     delete solver;
//== }


void test_lobpcg()
{
    Global parameters;

    double a0[] = {12.975 * 1.889726125, 0, 0};
    double a1[] = {0, 12.975 * 1.889726125, 0};
    double a2[] = {0, 0, 12.975 * 1.889726125};

    double Ekin = 3.0; // 40 Ry = 20 Ha

    parameters.unit_cell()->set_lattice_vectors(a0, a1, a2);
    parameters.set_pw_cutoff(2 * sqrt(2 * Ekin) + 0.5);
    parameters.initialize();
    parameters.print_info();
    
    double vk[] = {0, 0, 0};
    K_point kp(parameters, vk, 1.0);
    kp.generate_gkvec(sqrt(2 * Ekin));

    std::cout << "num_gkvec = " << kp.num_gkvec() << std::endl;

    // generate some potential in plane-wave domain
    std::vector<double_complex> v_pw(parameters.reciprocal_lattice()->num_gvec());
    for (int ig = 0; ig < parameters.reciprocal_lattice()->num_gvec(); ig++) v_pw[ig] = double_complex(1.0 / pow(parameters.reciprocal_lattice()->gvec_len(ig) + 1.0, 2), 0.0);

    //== // cook the Hamiltonian
    //== mdarray<double_complex, 2> hmlt(kp.num_gkvec(), kp.num_gkvec());
    //== hmlt.zero();
    //== for (int ig1 = 0; ig1 < kp.num_gkvec(); ig1++)
    //== {
    //==     for (int ig2 = 0; ig2 < kp.num_gkvec(); ig2++)
    //==     {
    //==         int ig = parameters.index_g12(kp.gvec_index(ig2), kp.gvec_index(ig1));
    //==         hmlt(ig2, ig1) = v_pw[ig];
    //==         if (ig1 == ig2) hmlt(ig2, ig1) += pow(kp.gkvec_cart(ig1).length(), 2) / 2.0;
    //==     }
    //== }

    //== standard_evp* solver = new standard_evp_lapack();

    //== std::vector<double> eval(kp.num_gkvec());
    //== mdarray<double_complex, 2> evec(kp.num_gkvec(), kp.num_gkvec());

    //== solver->solve(kp.num_gkvec(), hmlt.get_ptr(), hmlt.ld(), &eval[0], evec.get_ptr(), evec.ld());

    //== delete solver;
    
    int num_bands = 30;

    //== printf("\n");
    //== printf("Lowest eigen-values (exact): \n");
    //== for (int i = 0; i < num_bands; i++)
    //== {
    //==     printf("i : %i,  eval : %16.10f\n", i, eval[i]);
    //== }


    diag_lobpcg(parameters, kp, v_pw, num_bands);

    //diag_davidson(parameters, kp, v_pw, num_bands);

    
//    mdarray<double_complex, 2> evec(Nmax, Nmax);
//    mdarray<double_complex, 2> psi(parameters.num_gvec(), n);
//
//    // initial basis functions
//    mdarray<double_complex, 2> phi(parameters.num_gvec(), Nmax);
//    phi.zero();
//    for (int i = 0; i < n; i++) phi(i, i) = 1.0;
//
//    for (int k = 0; k < 2; k++)
//    {
//        int nphi = diag_davidson(parameters, niter, bs, n, v_pw, phi, evec);
//
//        psi.zero();
//        for (int j = 0; j < n; j++)
//        {
//            for (int mu = 0; mu < nphi; mu++)
//            {
//                for (int ig = 0; ig < parameters.num_gvec(); ig++) psi(ig, j) += evec(mu, j) * phi(ig, mu);
//            }
//        }
//        for (int j = 0; j < n; j++) memcpy(&phi(0, j), &psi(0, j), parameters.num_gvec() * sizeof(double_complex));
//    }
//





    
     

    parameters.clear();
}

int main(int argn, char** argv)
{
    Platform::initialize(true);

    test_lobpcg();

    Platform::finalize();
}
