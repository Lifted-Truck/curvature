#include "EigenSolver.h"

#include <Spectra/MatOp/SparseSymShiftSolve.h>
#include <Spectra/SymEigsShiftSolver.h>

#include <stdexcept>

namespace curv {

ModeSet solveModes(const LaplacianPair& lap, int k)
{
    const int n = static_cast<int>(lap.massDiag.size());
    if (k + 1 >= n)
        throw std::runtime_error("requested more modes than the mesh supports");

    const Eigen::VectorXd dInvSqrt = lap.massDiag.cwiseSqrt().cwiseInverse();
    Eigen::SparseMatrix<double> A =
        dInvSqrt.asDiagonal() * lap.L * dInvSqrt.asDiagonal();

    const double sigma = -1e-3;  // L PSD => A - sigma I positive definite
    const int nev = k + 1;       // include the trivial mode, drop it below
    const int ncv = std::min(n, std::max(2 * nev + 1, 40));

    Spectra::SparseSymShiftSolve<double> op(A);
    Spectra::SymEigsShiftSolver<Spectra::SparseSymShiftSolve<double>> eigs(op, nev, ncv, sigma);
    eigs.init();
    eigs.compute(Spectra::SortRule::LargestMagn, 2000, 1e-12);
    if (eigs.info() != Spectra::CompInfo::Successful)
        throw std::runtime_error("Spectra eigensolve failed");

    Eigen::VectorXd lam = eigs.eigenvalues();
    Eigen::MatrixXd vec = eigs.eigenvectors();

    // sort ascending
    std::vector<int> order(static_cast<size_t>(lam.size()));
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = static_cast<int>(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return lam[a] < lam[b]; });

    const double scale = std::max(lam[order.back()], 1.0);
    if (lam[order.front()] > 1e-8 * scale)
        throw std::runtime_error("expected trivial lambda_0 ~ 0 mode");

    ModeSet out;
    out.lambda.resize(k);
    out.phi.resize(n, k);
    for (int i = 0; i < k; ++i) {
        const int src = order[(size_t) i + 1];  // skip trivial mode
        out.lambda[i] = lam[src];
        out.phi.col(i) = dInvSqrt.asDiagonal() * vec.col(src);  // M-orthonormal
    }
    return out;
}

} // namespace curv
