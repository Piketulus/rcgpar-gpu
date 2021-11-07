#ifndef RCGMPI_TEST_UTIL_HPP
#define RCGMPI_TEST_UTIL_HPP

#include <vector>

#include "Matrix.hpp"

std::vector<double> mixture_components(const Matrix<double> &probs,
				       const std::vector<double> &log_times_observed,
				       const uint32_t n_times_total);

void read_test_data(Matrix<double> &log_lls, std::vector<double> &log_times_observed,
		    uint32_t &n_times_total);

#endif