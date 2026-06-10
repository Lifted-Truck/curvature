// Intrinsic cotan Laplacian: built from edge lengths only (law of cosines +
// Heron), mirroring prototype/laplacian.py. L symmetric PSD, M barycentric-
// lumped diagonal.
#pragma once

#include <Eigen/Sparse>

#include "Mesh.h"

namespace curv {

struct LaplacianPair {
    Eigen::SparseMatrix<double> L;
    Eigen::VectorXd massDiag;
};

LaplacianPair buildCotanLaplacian(const Mesh& mesh);

} // namespace curv
