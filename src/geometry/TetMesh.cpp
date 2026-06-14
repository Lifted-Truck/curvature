#include "TetMesh.h"

#include <cmath>
#include <map>

#include <Eigen/Dense>

namespace curv {

namespace {

// Freudenthal 6-tet split of a cube cell: 000 -> unit steps along each
// permutation of the axes -> 111 (mirrors prototype/manifold4d.py).
const int kStep[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
const int kPerms[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 },
                           { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };

} // namespace

TetMesh makeFlatTorus3(int nx, int ny, int nz, double a, double b, double c)
{
    TetMesh m;
    m.nx = nx; m.ny = ny; m.nz = nz; m.a = a; m.b = b; m.c = c;
    const double dx = a / nx, dy = b / ny, dz = c / nz;

    auto vid = [&](int i, int j, int k) {
        return (((i % nx + nx) % nx) * ny + ((j % ny + ny) % ny)) * nz + ((k % nz + nz) % nz);
    };

    m.V.resize((size_t) nx * ny * nz);
    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < ny; ++j)
            for (int k = 0; k < nz; ++k)
                m.V[(size_t) vid(i, j, k)] = { i * dx, j * dy, k * dz };

    std::map<std::pair<int, int>, int> edgeIndex;
    auto addEdge = [&](int u, int v) {
        const auto key = std::pair { std::min(u, v), std::max(u, v) };
        if (edgeIndex.emplace(key, (int) m.edges.size()).second)
            m.edges.push_back({ key.first, key.second });
    };

    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < ny; ++j)
            for (int k = 0; k < nz; ++k)
                for (const auto& perm : kPerms) {
                    int corner[3] = { i, j, k };
                    std::array<int, 4> tet;
                    tet[0] = vid(corner[0], corner[1], corner[2]);
                    for (int s = 0; s < 3; ++s) {
                        const int ax = perm[s];
                        corner[0] += kStep[ax][0];
                        corner[1] += kStep[ax][1];
                        corner[2] += kStep[ax][2];
                        tet[s + 1] = vid(corner[0], corner[1], corner[2]);
                    }
                    m.tets.push_back(tet);
                    for (int x = 0; x < 4; ++x)
                        for (int y = x + 1; y < 4; ++y)
                            addEdge(tet[x], tet[y]);
                }

    m.adjacency.assign(m.V.size(), {});
    for (const auto& e : m.edges) {
        m.adjacency[(size_t) e[0]].push_back(e[1]);
        m.adjacency[(size_t) e[1]].push_back(e[0]);
    }
    return m;
}

LaplacianPair buildTetLaplacian(const TetMesh& mesh, const Eigen::VectorXd& u)
{
    const int n = mesh.numVertices();
    const double box[3] = { mesh.a, mesh.b, mesh.c };
    const bool conformal = u.size() == n;

    std::vector<Eigen::Triplet<double>> trips;
    trips.reserve(mesh.tets.size() * 16);
    Eigen::VectorXd mass = Eigen::VectorXd::Zero(n);

    for (const auto& tet : mesh.tets) {
        const auto& p0 = mesh.V[(size_t) tet[0]];
        Eigen::Matrix3d M3;  // columns = edge vectors from p0 (minimal image)
        for (int s = 0; s < 3; ++s) {
            const auto& ps = mesh.V[(size_t) tet[s + 1]];
            for (int d = 0; d < 3; ++d) {
                double e = ps[d] - p0[d];
                e -= box[d] * std::round(e / box[d]);  // periodic minimal image
                M3(d, s) = e;
            }
        }
        const double det = M3.determinant();
        const double vol = std::abs(det) / 6.0;
        if (vol < 1e-15)
            continue;

        // conformal scaling: per-tet similarity s = exp(mean u). Under scale s,
        // stiffness ~ s, mass ~ s^3, so eigenvalues ~ 1/s^2 (frequencies track
        // local size) — the same conformal behaviour as the 2D metric.
        double sStiff = 1.0, sMass = 1.0;
        if (conformal) {
            const double um = 0.25 * (u[tet[0]] + u[tet[1]] + u[tet[2]] + u[tet[3]]);
            const double s = std::exp(um);
            sStiff = s;
            sMass = s * s * s;
        }

        const Eigen::Matrix3d Ginv = M3.inverse();  // rows = grad of b1,b2,b3
        Eigen::Matrix<double, 4, 3> grads;
        grads.row(1) = Ginv.row(0);
        grads.row(2) = Ginv.row(1);
        grads.row(3) = Ginv.row(2);
        grads.row(0) = -(grads.row(1) + grads.row(2) + grads.row(3));

        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y)
                trips.emplace_back(tet[x], tet[y],
                                   sStiff * vol * grads.row(x).dot(grads.row(y)));
            mass[tet[x]] += sMass * vol / 4.0;
        }
    }

    LaplacianPair out;
    out.L.resize(n, n);
    out.L.setFromTriplets(trips.begin(), trips.end());
    out.massDiag = std::move(mass);
    return out;
}

} // namespace curv
