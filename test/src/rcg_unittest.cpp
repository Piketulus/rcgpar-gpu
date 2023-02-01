// rcgpar: parallel estimation of mixture model components
// https://github.com/tmaklin/rcgpar
//
// Copyright (C) 2021 Tommi Mäklin (tommi@maklin.fi)
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
// USA
//
#include "rcg_unittest.hpp"

#include <fstream>

#include "gtest/gtest.h"

#include "rcg.hpp"

// Test digamma()
TEST_F(DigammaTest, ResultCorrect) {
    for (uint16_t i = 0; i < tests.size(); ++i) {
	SCOPED_TRACE(i);
	double got = rcgpar::digamma(tests[i]);
	EXPECT_NEAR(expects[i], got, 1e-6);
    }
}

// Test mixt_negnatgrad()
TEST_F(MixtNegnatgradTest, NewnormCorrect) {
    newnorm_got = rcgpar::mixt_negnatgrad(gamma_Z, N_k, logl, step_got);
    EXPECT_NEAR(expected_newnorm, newnorm_got, 1e-4);
}

// Test mixt_negnatgrad()
TEST_F(MixtNegnatgradTest, dL_dPhiCorrect) {
    rcgpar::mixt_negnatgrad(gamma_Z, N_k, logl, step_got);
    for (size_t i = 0; i < expected_step.get_rows(); ++i) {
	for (size_t j = 0; j < expected_step.get_cols(); ++j) {
	    EXPECT_NEAR(expected_step(i, j), step_got(i, j), 1e-4);
	}
    }
}

// Test update_N_k()
TEST_F(UpdateNkTest, NkCorrect) {
    rcgpar::update_N_k(expected_gamma_Z, log_times_observed, alpha0, got);
    for (uint16_t i = 0; i < got.size(); ++i) {
	SCOPED_TRACE(i);
	EXPECT_NEAR(expected_N_k[i], got[i], 1e-2);
    }
}

// Test logsumexp()
TEST_F(LogsumexpTest, GammaZCorrect) {
    rcgpar::logsumexp(gamma_Z_got);
    for (size_t i = 0; i < expected_gamma_Z.get_rows(); ++i) {
	for (size_t j = 0; j < expected_gamma_Z.get_cols(); ++j) {
	    EXPECT_NEAR(expected_gamma_Z(i, j), gamma_Z_got(i, j), 1e-4);
	}
    }
}

// Test logsumexp()
TEST_F(LogsumexpTest, GammaZCorrectInReturnOldM) {
    rcgpar::logsumexp(gamma_Z_got, oldm_got);
    for (size_t i = 0; i < expected_gamma_Z.get_rows(); ++i) {
	for (size_t j = 0; j < expected_gamma_Z.get_cols(); ++j) {
	    EXPECT_NEAR(expected_gamma_Z(i, j), gamma_Z_got(i, j), 1e-4);
	}
    }
}

// Test logsumexp()
TEST_F(LogsumexpTest, OldMCorrect) {
    rcgpar::logsumexp(gamma_Z_got, oldm_got);
    for (uint32_t i = 0; i < oldm_got.size(); ++i) {
 	SCOPED_TRACE(i);
	EXPECT_NEAR(expected_oldm[i], oldm_got[i], 1e-4);
    }
}

// Test ELBO_rcg_mat()
TEST_F(ElboRcgMatTest, BoundCorrect) {
    bound_got = rcgpar::ELBO_rcg_mat(logl, expected_gamma_Z, log_times_observed, expected_N_k, expected_bound_const);
    EXPECT_NEAR(expected_bound, bound_got, 1e-1);
}

// Test revert_step
TEST_F(RevertStepTest, RevertedGammaZCorrect) {
    rcgpar::revert_step(gamma_Z_got, expected_oldm);
    for (size_t i = 0; i < expected_reverted_gamma_Z.get_rows(); ++i) {
	for (size_t j = 0; j < expected_reverted_gamma_Z.get_cols(); ++j) {
	    EXPECT_NEAR(expected_reverted_gamma_Z(i, j), gamma_Z_got(i, j), 1e-4);
	}
    }
}

// Test calc_bound_const()
TEST_F(CalcBoundConstTest, BoundConstCorrect) {
    bound_const_got = rcgpar::calc_bound_const(log_times_observed, alpha0);
    EXPECT_NEAR(expected_bound_const, bound_const_got, 1e-2);
}

// Test rcg_optl_mat()
TEST_F(RcgOptlMatTest, FinalGammaZCorrect) {
    std::ofstream empty;
    rcgpar::rcg_optl_mat(logl, log_times_observed, alpha0,
		 expected_bound_const, 1e-8, 5000,
		 false, final_gamma_Z_got, empty);
    std::cerr << final_gamma_Z_got.get_rows() << std::endl;
    for (size_t i = 0; i < final_gamma_Z.get_rows(); ++i) {
	for (size_t j = 0; j < final_gamma_Z.get_cols(); ++j) {
	    EXPECT_NEAR(final_gamma_Z(i, j), final_gamma_Z_got(i, j), 1e-4);
	}
    }
}
