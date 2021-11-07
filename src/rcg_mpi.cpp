// Riemannian conjugate gradient for parameter estimation.
//
// This file contains the rcg_optl_mat function for MPI calls.

#include "rcg.hpp"

#include <mpi.h>

#include <cmath>
#include <iostream>

#include "rcg_util.hpp"

Matrix<double> rcg_optl_mpi(Matrix<double> &logl_full, const std::vector<double> &log_times_observed_full, const std::vector<double> &alpha0, const double &tol, uint16_t maxiters) {
    int ntasks,rank;
    int rc = MPI_Comm_size(MPI_COMM_WORLD, &ntasks);
    rc = MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    uint16_t n_groups;
    uint32_t n_obs;
    if (rank == 0) {
	n_groups = logl_full.get_rows();
	n_obs = log_times_observed_full.size();
    }
    MPI_Bcast(&n_groups, 1, MPI_UINT16_T, 0, MPI_COMM_WORLD);
    MPI_Bcast(&n_obs, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);

    // Subdimensions for the processes
    uint16_t n_obs_per_task = n_obs/ntasks;
    uint32_t n_values_per_task = n_obs_per_task*n_groups;

    // Scatter the log likelihoods and log counts
    std::vector<double> log_times_observed(n_obs/ntasks);
    MPI_Scatter(&log_times_observed_full.front(), n_obs/ntasks, MPI_DOUBLE, &log_times_observed.front(), n_obs/ntasks, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    Matrix<double> logl(n_groups, n_obs_per_task, 0.0);
    MPI_Scatter(&logl_full.front(), n_values_per_task, MPI_DOUBLE, &logl.front(), n_values_per_task, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Initialize variables.
    Matrix<double> gamma_Z_partial = Matrix<double>(n_groups, n_obs_per_task, std::log(1.0/(double)n_groups));
    Matrix<double> step_partial(n_groups, n_obs_per_task, 0.0);

    // Oldstep, oldm, and oldnorm are needed to revert the gradient descent step in some special cases.
    Matrix<double> oldstep_partial(n_groups, n_obs_per_task, 0.0);
    std::vector<double> oldm(n_obs, 0.0);
    double oldnorm = 1.0;

    // ELBO variables
    long double bound = -100000.0;
    double bound_const;
    if (rank == 0) {
	bound_const = calc_bound_const(log_times_observed, alpha0);
    }
    MPI_Bcast(&bound_const, 1, MPI_LONG_DOUBLE, 0, MPI_COMM_WORLD);
    bool didreset = false;

    // // gamma_Z %*% exp(log_times_observed), store result in N_k.
    std::vector<double> N_k(alpha0.size());
    gamma_Z_partial.exp_right_multiply(log_times_observed, N_k);
    add_alpha0_to_Nk(alpha0, N_k);

    Matrix<double> gamma_Z;
    if (rank == 0) {
	gamma_Z = Matrix<double>(n_groups, n_obs, std::log(1.0/(double)n_groups)); // where gamma_Z is init at 1.0
    }

    double newnorm = 0.0;
    for (uint16_t k = 0; k < maxiters; ++k) {
	double newnorm_partial = mixt_negnatgrad(gamma_Z_partial, N_k, logl, step_partial);
	MPI_Allreduce(&newnorm_partial, &newnorm, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

	double beta_FR = newnorm/oldnorm;
	oldnorm = newnorm;
    
	if (didreset) {
	    oldstep_partial *= 0.0;
	} else if (beta_FR > 0) {
	    oldstep_partial *= beta_FR;
	    step_partial += oldstep_partial;
	}
	didreset = false;

	gamma_Z_partial += step_partial;

	// Logsumexp 1
	MPI_Gather(&gamma_Z_partial.front(), n_values_per_task, MPI_DOUBLE, &gamma_Z.front(), n_values_per_task, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	if (rank == 0) {
	    logsumexp(gamma_Z, oldm);
	}
	MPI_Scatter(&gamma_Z.front(), n_values_per_task, MPI_DOUBLE, &gamma_Z_partial.front(), n_values_per_task, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	MPI_Bcast(&oldm.front(), n_obs, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	gamma_Z_partial.exp_right_multiply(log_times_observed, N_k);
	add_alpha0_to_Nk(alpha0, N_k);
    
	long double oldbound = bound;
	long double bound_partial = 0.0;
  	ELBO_rcg_mat(logl, gamma_Z_partial, log_times_observed, alpha0, N_k, bound_partial);
	MPI_Allreduce(&bound_partial, &bound, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
	bound += bound_const;
    
	if (bound < oldbound) {
	    didreset = true;
	    revert_step(gamma_Z_partial, oldm);
	    if (beta_FR > 0) {
		gamma_Z_partial -= oldstep_partial;
	    }

	    // Logsumexp 2
	    MPI_Gather(&gamma_Z_partial.front(), n_values_per_task, MPI_DOUBLE, &gamma_Z.front(), n_values_per_task, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	    if (rank == 0) {
		logsumexp(gamma_Z, oldm);
	    }
	    MPI_Scatter(&gamma_Z.front(), n_values_per_task, MPI_DOUBLE, &gamma_Z_partial.front(), n_values_per_task, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	    MPI_Bcast(&oldm.front(), n_obs, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	    gamma_Z_partial.exp_right_multiply(log_times_observed, N_k);
	    add_alpha0_to_Nk(alpha0, N_k);

	    bound_partial = 0.0;
	    ELBO_rcg_mat(logl, gamma_Z_partial, log_times_observed, alpha0, N_k, bound_partial);
	    MPI_Allreduce(&bound_partial, &bound, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
	    bound += bound_const;
	} else {
	    oldstep_partial = step_partial;
	}
	if (k % 5 == 0 && rank == 0) {
	    std::cerr << "  " <<  "iter: " << k << ", bound: " << bound << ", |g|: " << newnorm << '\n';
	}
	if (bound - oldbound < tol && !didreset) {
	    // Logsumexp 3
	    MPI_Gather(&gamma_Z_partial.front(), n_values_per_task, MPI_DOUBLE, &gamma_Z.front(), n_values_per_task, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	    if (rank == 0) {
		logsumexp(gamma_Z, oldm);
	    }
	    std::cerr << std::endl;
	    return(gamma_Z);
	}
    }
    // Logsumexp 3
    MPI_Gather(&gamma_Z_partial.front(), n_values_per_task, MPI_DOUBLE, &gamma_Z.front(), n_values_per_task, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    if (rank == 0) {
	logsumexp(gamma_Z, oldm);
    }
    std::cerr << std::endl;
    return(gamma_Z);
}