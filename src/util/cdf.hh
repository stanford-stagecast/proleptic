#pragma once

#include <utility>
#include <vector>

void make_cdf( std::vector<float>& values,
               const unsigned int num_samples,
               std::vector<std::pair<float, float>>& cdf );
