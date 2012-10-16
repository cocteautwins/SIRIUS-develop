namespace sirius
{

class xc_potential
{
    public:
        
        /*static void get(int size, double* rho, double* vxc, double* exc)
        {
            std::vector<double> tmp(size);
            memset(vxc, 0, size * sizeof(double)); 
            memset(exc, 0, size * sizeof(double));

            int xc_id[] = {1, 7};
            xc_func_type func;
    
            for (int i = 0; i < 2; i++)
            {
                if(xc_func_init(&func, xc_id[i], XC_UNPOLARIZED) != 0)
                    error(__FILE__, __LINE__, "functional is not found");
       
                xc_lda_vxc(&func, size, rho, &tmp[0]);

                for (int j = 0; j < size; j++)
                    vxc[j] += tmp[j];

                xc_lda_exc(&func, size, rho, &tmp[0]);

                for (int j = 0; j < size; j++)
                    exc[j] += tmp[j];
      
                xc_func_end(&func);
            }
        }*/

        static void get(int size, double* rho, double* vxc, double* exc)
        {
            memset(vxc, 0, size * sizeof(double)); 
            memset(exc, 0, size * sizeof(double));

            int xc_id[] = {1, 7};
            xc_func_type func;
    
            std::vector<double> vxc_tmp(size);
            std::vector<double> exc_tmp(size);

            // check rho
            for (int i = 0; i < size; i++)
                if (rho[i] < 0.0)
                    error(__FILE__, __LINE__, "rho is negative");
            
            for (int i = 0; i < 2; i++)
            {
                if(xc_func_init(&func, xc_id[i], XC_UNPOLARIZED) != 0)
                    error(__FILE__, __LINE__, "functional is not found");

                xc_lda_exc_vxc(&func, size, &rho[0], &exc_tmp[0], &vxc_tmp[0]);
       
                for (int j = 0; j < size; j++)
                {
                    vxc[j] += vxc_tmp[j];
                    exc[j] += exc_tmp[j];
                }

                xc_func_end(&func);
            }
        }

        static void get(int size, double* rho, double* mag, double* vxc, double* bxc, double* exc)
        {
            memset(vxc, 0, size * sizeof(double)); 
            memset(bxc, 0, size * sizeof(double)); 
            memset(exc, 0, size * sizeof(double));

            std::vector<double> rhoud(size * 2);
            for (int i = 0; i < size; i++)
            {
                rhoud[2 * i] = 0.5 * (rho[i] + mag[i]);
                rhoud[2 * i + 1] = 0.5 * (rho[i] - mag[i]);

                if (rhoud[2 * i] < 0.0)
                    error(__FILE__, __LINE__, "rho_up is negative");

                if (rhoud[2 * i + 1] < 0.0)
                {
                    if (fabs(rhoud[2 * i + 1]) > 1e-9)
                    {
                        std::stringstream s;
                        s << "rho_dn is negative : " << rhoud[2 * i + 1] << std::endl
                          << "  rho : " << rho[i] << "   mag : " << mag[i];
                        error(__FILE__, __LINE__, s);
                    }
                    else
                        rhoud[2 * i + 1] = 0.0;
                }
            }

            std::vector<double> vxc_tmp(size * 2);
            std::vector<double> exc_tmp(size);
            
            int xc_id[] = {1, 7};
            xc_func_type func;
    
            for (int i = 0; i < 2; i++)
            {
                if(xc_func_init(&func, xc_id[i], XC_POLARIZED) != 0)
                    error(__FILE__, __LINE__, "functional is not found");
       
                xc_lda_exc_vxc(&func, size, &rhoud[0], &exc_tmp[0], &vxc_tmp[0]);

                for (int j = 0; j < size; j++)
                {
                    exc[j] += exc_tmp[j];
                    vxc[j] += 0.5 * (vxc_tmp[2 * j] + vxc_tmp[2 * j + 1]);
                    bxc[j] += 0.5 * (vxc_tmp[2 * j] - vxc_tmp[2 * j + 1]);
                }

                xc_func_end(&func);
            }
        }
};

};