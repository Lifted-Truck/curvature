// Lowest-k nonzero modes of L phi = lambda M phi via Spectra shift-invert
// Lanczos. With diagonal M the generalized problem reduces to the standard
// one on A = M^{-1/2} L M^{-1/2}; eigenvectors map back M-orthonormal.
#pragma once

#include <Eigen/Dense>

#include "Laplacian.h"

namespace curv {

struct ModeSet {
    Eigen::VectorXd lambda;  // k smallest nonzero eigenvalues, ascending
    Eigen::MatrixXd phi;     // n x k, M-orthonormal
};

ModeSet solveModes(const LaplacianPair& lap, int k);

} // namespace curv
