#include "simulation_context.h"

namespace sirius {

void Simulation_context::init_fft()
{
    auto rlv = unit_cell_.reciprocal_lattice_vectors();

    auto& comm = mpi_grid_->communicator(1 << _mpi_dim_k_row_ | 1 << _mpi_dim_k_col_);

    if (!(control_input_section_.fft_mode_ == "serial" || control_input_section_.fft_mode_ == "parallel")) {
        TERMINATE("wrong FFT mode");
    }

    if (full_potential()) {
        /* split bands between all ranks, use serial FFT */
        mpi_grid_fft_ = std::unique_ptr<MPI_grid>(new MPI_grid({1, comm.size()}, comm));
    } else {
        /* use parallel FFT for density and potential */
        mpi_grid_fft_ = std::unique_ptr<MPI_grid>(new MPI_grid({mpi_grid_->dimension_size(_mpi_dim_k_row_),
                                                                mpi_grid_->dimension_size(_mpi_dim_k_col_)}, comm));
        
        if (control_input_section_.fft_mode_ == "serial") {
            /* serial FFT in Hloc */
            mpi_grid_fft_vloc_ = std::unique_ptr<MPI_grid>(new MPI_grid({1, comm.size()}, comm));
        } else {
            mpi_grid_fft_vloc_ = std::unique_ptr<MPI_grid>(new MPI_grid({mpi_grid_->dimension_size(_mpi_dim_k_row_),
                                                                         mpi_grid_->dimension_size(_mpi_dim_k_col_)}, comm));
        }
    }

    /* create FFT driver for dense mesh (density and potential) */
    fft_ = std::unique_ptr<FFT3D>(new FFT3D(FFT3D_grid(pw_cutoff(), rlv), mpi_grid_fft_->communicator(1 << 0), processing_unit(), 0.9));

    /* create a list of G-vectors for dense FFT grid */
    gvec_ = Gvec(vector3d<double>(0, 0, 0), rlv, pw_cutoff(), fft_->grid(),
                 mpi_grid_fft_->dimension_size(0), true, control_input_section_.reduce_gvec_);

    gvec_fft_distr_ = std::unique_ptr<Gvec_FFT_distribution>(new Gvec_FFT_distribution(gvec_, mpi_grid_fft_->communicator(1 << 0)));

    if (!full_potential()) {
        /* create FFT driver for coarse mesh */
        fft_coarse_ = std::unique_ptr<FFT3D>(new FFT3D(FFT3D_grid(2 * gk_cutoff(), rlv), mpi_grid_fft_vloc_->communicator(1 << 0), processing_unit(), 0.9));

        /* create a list of G-vectors for corase FFT grid */
        gvec_coarse_ = Gvec(vector3d<double>(0, 0, 0), rlv, gk_cutoff() * 2, fft_coarse_->grid(),
                            mpi_grid_fft_vloc_->dimension_size(0), true, control_input_section_.reduce_gvec_);
        
        gvec_coarse_fft_distr_ = std::unique_ptr<Gvec_FFT_distribution>(new Gvec_FFT_distribution(gvec_coarse_, mpi_grid_fft_vloc_->communicator(1 << 0)));
    }
}

void Simulation_context::initialize()
{
    PROFILE();

    /* can't initialize twice */
    if (initialized_) {
        TERMINATE("Simulation context is already initialized.");
    }
    
    /* get processing unit */
    std::string pu = control_input_section_.processing_unit_;
    if (pu == "cpu") {
        processing_unit_ = CPU;
    } else {
        if (pu == "gpu") {
            processing_unit_ = GPU;
        } else {
            TERMINATE("wrong processing unit");
        }
    }

    /* check if we can use a GPU device */
    if (processing_unit() == GPU) {
        #ifndef __GPU
        TERMINATE_NO_GPU
        #endif
    }
    
    /* check MPI grid dimensions and set a default grid if needed */
    if (!control_input_section_.mpi_grid_dims_.size()) {
        control_input_section_.mpi_grid_dims_ = {comm_.size()};
    }
    
    /* can't use reduced G-vectors in LAPW code */
    if (full_potential()) {
        control_input_section_.reduce_gvec_ = false;
    }

    /* setup MPI grid */
    mpi_grid_ = std::unique_ptr<MPI_grid>(new MPI_grid(control_input_section_.mpi_grid_dims_, comm_));

    blacs_grid_ = std::unique_ptr<BLACS_grid>(new BLACS_grid(mpi_grid_->communicator(1 << _mpi_dim_k_row_ | 1 << _mpi_dim_k_col_), 
                                                             mpi_grid_->dimension_size(_mpi_dim_k_row_), mpi_grid_->dimension_size(_mpi_dim_k_col_)));
    
    blacs_grid_slice_ = std::unique_ptr<BLACS_grid>(new BLACS_grid(blacs_grid_->comm(), 1, blacs_grid_->comm().size()));

    /* initialize variables, related to the unit cell */
    unit_cell_.initialize();

    if (esm_type() == electronic_structure_method_t::paw_pseudopotential) {
        lmax_rho_ = unit_cell_.lmax() * 2;
        lmax_pot_ = unit_cell_.lmax() * 2;
    }

    /* initialize FFT interface */
    init_fft();

    #ifdef __PRINT_MEMORY_USAGE
    MEMORY_USAGE_INFO();
    #endif

    //if (comm_.rank() == 0)
    //{
    //    unit_cell_.write_cif();
    //    unit_cell_.write_json();
    //}

    if (unit_cell_.num_atoms() != 0) {
        unit_cell_.symmetry().check_gvec_symmetry(gvec_);
        if (!full_potential()) {
            unit_cell_.symmetry().check_gvec_symmetry(gvec_coarse_);
        }
    }

    auto& fft_grid = fft().grid();
    std::pair<int, int> limits(0, 0);
    for (int x: {0, 1, 2}) {
        limits.first = std::min(limits.first, fft_grid.limits(x).first); 
        limits.second = std::max(limits.second, fft_grid.limits(x).second); 
    }

    phase_factors_ = mdarray<double_complex, 3>(3, limits, unit_cell().num_atoms());

    #pragma omp parallel for
    for (int i = limits.first; i <= limits.second; i++)
    {
        for (int ia = 0; ia < unit_cell_.num_atoms(); ia++)
        {
            auto pos = unit_cell_.atom(ia).position();
            for (int x: {0, 1, 2}) phase_factors_(x, i, ia) = std::exp(double_complex(0.0, twopi * (i * pos[x])));
        }
    }
    
    if (full_potential()) {
        step_function_ = std::unique_ptr<Step_function>(new Step_function(unit_cell_, fft_.get(), *gvec_fft_distr_, comm_));
    }

    if (iterative_solver_input_section().real_space_prj_) 
    {
        STOP();
        //real_space_prj_ = new Real_space_prj(unit_cell_, comm_, iterative_solver_input_section().R_mask_scale_,
        //                                     iterative_solver_input_section().mask_alpha_,
        //                                     gk_cutoff(), num_fft_streams(), num_fft_workers());
    }

    /* take 10% of empty non-magnetic states */
    if (num_fv_states_ < 0) 
    {
        num_fv_states_ = static_cast<int>(1e-8 + unit_cell_.num_valence_electrons() / 2.0) +
                                          std::max(10, static_cast<int>(0.1 * unit_cell_.num_valence_electrons()));
    }
    
    if (num_fv_states() < int(unit_cell_.num_valence_electrons() / 2.0))
        TERMINATE("not enough first-variational states");
    
    std::string evsn[] = {std_evp_solver_name(), gen_evp_solver_name()};

    if (mpi_grid_->size(1 << _mpi_dim_k_row_ | 1 << _mpi_dim_k_col_) == 1)
    {
        if (evsn[0] == "") evsn[0] = "lapack";
        if (evsn[1] == "") evsn[1] = "lapack";
    }
    else
    {
        if (evsn[0] == "") evsn[0] = "scalapack";
        if (evsn[1] == "") evsn[1] = "elpa1";
    }

    ev_solver_t* evst[] = {&std_evp_solver_type_, &gen_evp_solver_type_};

    std::map<std::string, ev_solver_t> str_to_ev_solver_t;

    str_to_ev_solver_t["lapack"]    = ev_lapack;
    str_to_ev_solver_t["scalapack"] = ev_scalapack;
    str_to_ev_solver_t["elpa1"]     = ev_elpa1;
    str_to_ev_solver_t["elpa2"]     = ev_elpa2;
    str_to_ev_solver_t["magma"]     = ev_magma;
    str_to_ev_solver_t["plasma"]    = ev_plasma;
    str_to_ev_solver_t["rs_cpu"]    = ev_rs_cpu;
    str_to_ev_solver_t["rs_gpu"]    = ev_rs_gpu;

    for (int i: {0, 1})
    {
        auto name = evsn[i];

        if (str_to_ev_solver_t.count(name) == 0)
        {
            std::stringstream s;
            s << "wrong eigen value solver " << name;
            TERMINATE(s);
        }
        *evst[i] = str_to_ev_solver_t[name];
    }

    #if (__VERBOSITY > 0)
    if (comm_.rank() == 0) print_info();
    #endif

    if (esm_type() == ultrasoft_pseudopotential || esm_type() == paw_pseudopotential)
    {
        /* create augmentation operator Q_{xi,xi'}(G) here */
        for (int iat = 0; iat < unit_cell_.num_atom_types(); iat++) {
            augmentation_op_.push_back(std::move(Augmentation_operator(comm_, unit_cell_.atom_type(iat), gvec_, unit_cell_.omega())));
        }
    }
    
    time_active_ = -runtime::wtime();

    initialized_ = true;
}

void Simulation_context::print_info()
{
    printf("\n");
    printf("SIRIUS version : %2i.%02i\n", major_version, minor_version);
    printf("git hash       : %s\n", git_hash);
    printf("build date     : %s\n", build_date);
    printf("\n");
    printf("number of MPI ranks           : %i\n", comm_.size());
    printf("MPI grid                      :");
    for (int i = 0; i < mpi_grid_->num_dimensions(); i++) printf(" %i", mpi_grid_->dimension_size(i));
    printf("\n");
    printf("maximum number of OMP threads : %i\n", omp_get_max_threads());
    printf("number of independent FFTs    : %i\n", mpi_grid_fft_->dimension_size(1));
    printf("FFT comm size                 : %i\n", mpi_grid_fft_->dimension_size(0));

    printf("\n");
    printf("FFT context for density and potential\n");
    printf("=====================================\n");
    printf("  plane wave cutoff                     : %f\n", pw_cutoff());
    auto fft_grid = fft_->grid();
    printf("  grid size                             : %i %i %i   total : %i\n", fft_grid.size(0),
                                                                                fft_grid.size(1),
                                                                                fft_grid.size(2),
                                                                                fft_grid.size());
    printf("  grid limits                           : %i %i   %i %i   %i %i\n", fft_grid.limits(0).first,
                                                                                fft_grid.limits(0).second,
                                                                                fft_grid.limits(1).first,
                                                                                fft_grid.limits(1).second,
                                                                                fft_grid.limits(2).first,
                                                                                fft_grid.limits(2).second);
    printf("  number of G-vectors within the cutoff : %i\n", gvec_.num_gvec());
    printf("  number of G-shells                    : %i\n", gvec_.num_shells());
    printf("\n");
    if (!full_potential())
    {
        printf("\n");
        printf("FFT context for applying Hloc\n");
        printf("=============================\n");
        auto fft_grid = fft_coarse_->grid();
        printf("  grid size                             : %i %i %i   total : %i\n", fft_grid.size(0),
                                                                                    fft_grid.size(1),
                                                                                    fft_grid.size(2),
                                                                                    fft_grid.size());
        printf("  grid limits                           : %i %i   %i %i   %i %i\n", fft_grid.limits(0).first,
                                                                                    fft_grid.limits(0).second,
                                                                                    fft_grid.limits(1).first,
                                                                                    fft_grid.limits(1).second,
                                                                                    fft_grid.limits(2).first,
                                                                                    fft_grid.limits(2).second);
        printf("  number of G-vectors within the cutoff : %i\n", gvec_coarse_.num_gvec());
        printf("  number of G-shells                    : %i\n", gvec_coarse_.num_shells());
        printf("\n");
    }

    unit_cell_.print_info();
    for (int i = 0; i < unit_cell_.num_atom_types(); i++) unit_cell_.atom_type(i).print_info();

    printf("\n");
    printf("total number of aw basis functions : %i\n", unit_cell_.mt_aw_basis_size());
    printf("total number of lo basis functions : %i\n", unit_cell_.mt_lo_basis_size());
    printf("number of first-variational states : %i\n", num_fv_states());
    printf("number of bands                    : %i\n", num_bands());
    printf("number of spins                    : %i\n", num_spins());
    printf("number of magnetic dimensions      : %i\n", num_mag_dims());
    printf("lmax_apw                           : %i\n", lmax_apw());
    printf("lmax_pw                            : %i\n", lmax_pw());
    printf("lmax_rho                           : %i\n", lmax_rho());
    printf("lmax_pot                           : %i\n", lmax_pot());
    printf("lmax_rf                            : %i\n", unit_cell_.lmax());
    printf("smearing width                     : %f\n", smearing_width());
    printf("cyclic block size                  : %i\n", cyclic_block_size());

    std::string evsn[] = {"standard eigen-value solver        : ",
                          "generalized eigen-value solver     : "};

    ev_solver_t evst[] = {std_evp_solver_type_, gen_evp_solver_type_};
    for (int i = 0; i < 2; i++)
    {
        printf("%s", evsn[i].c_str());
        switch (evst[i])
        {
            case ev_lapack:
            {
                printf("LAPACK\n");
                break;
            }
            #ifdef __SCALAPACK
            case ev_scalapack:
            {
                printf("ScaLAPACK\n");
                break;
            }
            case ev_elpa1:
            {
                printf("ELPA1\n");
                break;
            }
            case ev_elpa2:
            {
                printf("ELPA2\n");
                break;
            }
            case ev_rs_gpu:
            {
                printf("RS_gpu\n");
                break;
            }
            case ev_rs_cpu:
            {
                printf("RS_cpu\n");
                break;
            }
            #endif
            case ev_magma:
            {
                printf("MAGMA\n");
                break;
            }
            case ev_plasma:
            {
                printf("PLASMA\n");
                break;
            }
            default:
            {
                TERMINATE("wrong eigen-value solver");
            }
        }
    }

    printf("\n");
    printf("processing unit : ");
    switch (processing_unit())
    {
        case CPU:
        {
            printf("CPU\n");
            break;
        }
        case GPU:
        {
            printf("GPU\n");
            break;
        }
    }
   
    int i = 1;
    printf("\n");
    printf("XC functionals\n");
    printf("==============\n");
    for (auto& xc_label: xc_functionals())
    {
        XC_functional xc(xc_label, num_spins());
        printf("%i) %s: %s\n", i, xc_label.c_str(), xc.name().c_str());
        printf("%s\n", xc.refs().c_str());
        i++;
    }
}

};
