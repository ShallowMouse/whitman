#include "CircuitSim.h"
#include <cmath>
#include <algorithm>

namespace whitman
{

int Circuit::inductor (int a, int b, double henries, double dcr)
{
    if (dcr > 0.0)
    {
        // the winding's resistance in series, through a node of its own
        const int mid = addNode();
        resistor (a, mid, dcr);
        return add ({ Kind::inductor, mid, b, 0, henries });
    }
    return add ({ Kind::inductor, a, b, 0, henries });
}

namespace
{
    /** Gaussian elimination with partial pivoting: solves A X = B in place (A n x n, B n x k, row-major). */
    template <typename T>
    bool solve (std::vector<T>& A, std::vector<T>& B, int n, int k)
    {
        for (int col = 0; col < n; ++col)
        {
            int piv = col;
            double best = std::abs (A[(size_t) (col * n + col)]);
            for (int r = col + 1; r < n; ++r)
                if (std::abs (A[(size_t) (r * n + col)]) > best) { best = std::abs (A[(size_t) (r * n + col)]); piv = r; }
            if (best < 1.0e-18) return false;
            if (piv != col)
            {
                for (int c = 0; c < n; ++c) std::swap (A[(size_t) (col * n + c)], A[(size_t) (piv * n + c)]);
                for (int c = 0; c < k; ++c) std::swap (B[(size_t) (col * k + c)], B[(size_t) (piv * k + c)]);
            }
            const T d = A[(size_t) (col * n + col)];
            for (int r = 0; r < n; ++r)
            {
                if (r == col) continue;
                const T f = A[(size_t) (r * n + col)] / d;
                if (f == T (0)) continue;
                for (int c = col; c < n; ++c) A[(size_t) (r * n + c)] -= f * A[(size_t) (col * n + c)];
                for (int c = 0; c < k; ++c)   B[(size_t) (r * k + c)] -= f * B[(size_t) (col * k + c)];
            }
        }
        for (int r = 0; r < n; ++r)
        {
            const T d = A[(size_t) (r * n + r)];
            for (int c = 0; c < k; ++c) B[(size_t) (r * k + c)] /= d;
        }
        return true;
    }

    /** The MNA layout: node voltages 1..N-1 at 0..N-2, then a current for the input source and for each amplifier. */
    struct Layout
    {
        int n = 0, inRow = 0;
        std::vector<int> ampRow;     // per element (-1 if not an amplifier)
        Layout (const Circuit& c, int nodes)
        {
            n = nodes - 1;
            inRow = n++;
            ampRow.assign (c.elements.size(), -1);
            for (size_t i = 0; i < c.elements.size(); ++i)
                if (c.elements[i].kind == Circuit::Kind::opamp || c.elements[i].kind == Circuit::Kind::vcvs)
                    ampRow[i] = n++;
        }
        static int idx (int node) { return node - 1; }   // -1 for ground
    };

    template <typename T>
    void stampConductance (std::vector<T>& A, int n, int a, int b, T g)
    {
        const int ia = Layout::idx (a), ib = Layout::idx (b);
        if (ia >= 0) A[(size_t) (ia * n + ia)] += g;
        if (ib >= 0) A[(size_t) (ib * n + ib)] += g;
        if (ia >= 0 && ib >= 0) { A[(size_t) (ia * n + ib)] -= g; A[(size_t) (ib * n + ia)] -= g; }
    }

    template <typename T>
    void stampAmplifiers (const Circuit& c, const Layout& L, std::vector<T>& A, int inNode)
    {
        const int n = L.n;
        // the input source
        const int ii = Layout::idx (inNode);
        A[(size_t) (ii * n + L.inRow)] += T (1);
        A[(size_t) (L.inRow * n + ii)] += T (1);
        for (size_t e = 0; e < c.elements.size(); ++e)
        {
            const auto& el = c.elements[e];
            const int k = L.ampRow[e];
            if (k < 0) continue;
            const int out = Layout::idx (el.c), p = Layout::idx (el.a), m = Layout::idx (el.b);
            if (out >= 0) A[(size_t) (out * n + k)] += T (1);                  // the amp's output current
            if (el.kind == Circuit::Kind::opamp)
            {
                if (p >= 0) A[(size_t) (k * n + p)] += T (1);                    // v+ - v- = 0
                if (m >= 0) A[(size_t) (k * n + m)] -= T (1);
            }
            else
            {
                if (out >= 0) A[(size_t) (k * n + out)] += T (1);                // vout - g (v+ - v-) = 0
                if (p >= 0) A[(size_t) (k * n + p)] -= T (el.value);
                if (m >= 0) A[(size_t) (k * n + m)] += T (el.value);
            }
        }
    }
}

std::complex<double> Circuit::responseAt (double hz) const
{
    using C = std::complex<double>;
    const Layout L (*this, nodes);
    const int n = L.n;
    std::vector<C> A ((size_t) (n * n), C (0)), B ((size_t) n, C (0));
    const C jw (0.0, 2.0 * 3.14159265358979323846 * hz);
    for (const auto& e : elements)
    {
        switch (e.kind)
        {
            case Kind::resistor:  stampConductance (A, n, e.a, e.b, C (1.0 / e.value)); break;
            case Kind::capacitor: stampConductance (A, n, e.a, e.b, jw * e.value); break;
            case Kind::inductor:  stampConductance (A, n, e.a, e.b, C (1.0) / (jw * e.value)); break;
            default: break;
        }
    }
    stampAmplifiers (*this, L, A, inNode);
    B[(size_t) L.inRow] = C (1.0);
    if (! solve (A, B, n, 1)) return C (0);
    return B[(size_t) Layout::idx (outNode)];
}

//==============================================================================
bool CircuitRunner::prepare (const Circuit& c, double sampleRate)
{
    const double T = 1.0 / sampleRate;
    std::vector<int> newShape;
    for (const auto& e : c.elements) { newShape.push_back ((int) e.kind); newShape.push_back (e.a); newShape.push_back (e.b); newShape.push_back (e.c); }
    newShape.push_back (c.nodes);
    const bool sameShape = newShape == shape;
    shape = newShape;

    const Layout L (c, c.nodes);
    const int n = L.n;
    std::vector<int> reactive;
    for (size_t e = 0; e < c.elements.size(); ++e)
        if (c.elements[e].kind == Circuit::Kind::capacitor || c.elements[e].kind == Circuit::Kind::inductor) reactive.push_back ((int) e);
    m = (int) reactive.size();

    G.assign ((size_t) m, 0.0);
    isInductor.assign ((size_t) m, 0);
    std::vector<double> A ((size_t) (n * n), 0.0);
    for (size_t e = 0; e < c.elements.size(); ++e)
    {
        const auto& el = c.elements[e];
        if (el.kind == Circuit::Kind::resistor) stampConductance (A, n, el.a, el.b, 1.0 / el.value);
    }
    for (int j = 0; j < m; ++j)
    {
        const auto& el = c.elements[(size_t) reactive[(size_t) j]];
        isInductor[(size_t) j] = el.kind == Circuit::Kind::inductor;
        G[(size_t) j] = isInductor[(size_t) j] ? T / (2.0 * el.value) : 2.0 * el.value / T;
        stampConductance (A, n, el.a, el.b, G[(size_t) j]);
    }
    stampAmplifiers (c, L, A, c.inNode);

    // right-hand sides: column 0 the input, column 1 + j reactive element j's history current (flowing a -> b)
    const int k = m + 1;
    std::vector<double> B ((size_t) (n * k), 0.0);
    B[(size_t) (L.inRow * k + 0)] = 1.0;
    for (int j = 0; j < m; ++j)
    {
        const auto& el = c.elements[(size_t) reactive[(size_t) j]];
        const int ia = Layout::idx (el.a), ib = Layout::idx (el.b);
        if (ia >= 0) B[(size_t) (ia * k + 1 + j)] -= 1.0;
        if (ib >= 0) B[(size_t) (ib * k + 1 + j)] += 1.0;
    }
    ok = solve (A, B, n, k);
    if (! ok) return false;

    V.assign ((size_t) (m * k), 0.0);
    for (int j = 0; j < m; ++j)
    {
        const auto& el = c.elements[(size_t) reactive[(size_t) j]];
        const int ia = Layout::idx (el.a), ib = Layout::idx (el.b);
        for (int col = 0; col < k; ++col)
            V[(size_t) (j * k + col)] = (ia >= 0 ? B[(size_t) (ia * k + col)] : 0.0) - (ib >= 0 ? B[(size_t) (ib * k + col)] : 0.0);
    }
    Y.assign ((size_t) k, 0.0);
    const int io = Layout::idx (c.outNode);
    for (int col = 0; col < k; ++col) Y[(size_t) col] = io >= 0 ? B[(size_t) (io * k + col)] : 0.0;

    if (! sameShape || (int) J.size() != m) J.assign ((size_t) m, 0.0);
    s.assign ((size_t) k, 0.0);
    v.assign ((size_t) m, 0.0);
    return true;
}

void CircuitRunner::reset()
{
    std::fill (J.begin(), J.end(), 0.0);
}

float CircuitRunner::processSample (float x) noexcept
{
    const int k = m + 1;
    s[0] = x;
    for (int j = 0; j < m; ++j) s[(size_t) j + 1] = J[(size_t) j];
    double y = 0.0;
    for (int col = 0; col < k; ++col) y += Y[(size_t) col] * s[(size_t) col];
    for (int j = 0; j < m; ++j)
    {
        const double* row = &V[(size_t) (j * k)];
        double vj = 0.0;
        for (int col = 0; col < k; ++col) vj += row[col] * s[(size_t) col];
        const double ij = G[(size_t) j] * vj + J[(size_t) j];
        // trapezoidal history: capacitor J' = -(G v + i), inductor J' = i + G v
        J[(size_t) j] = isInductor[(size_t) j] ? ij + G[(size_t) j] * vj : -(G[(size_t) j] * vj + ij);
    }
    return (float) y;
}

void CircuitRunner::process (float* x, int n) noexcept
{
    if (! ok) return;
    for (int i = 0; i < n; ++i) x[i] = processSample (x[i]);
}

} // namespace whitman
