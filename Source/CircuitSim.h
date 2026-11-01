#pragma once

#include <vector>
#include <complex>
#include <cstddef>

namespace whitman
{

//==============================================================================
/**
    A small linear circuit simulator, for running a desk's EQ as its actual network of
    resistors, capacitors, inductors and amplifiers (from the schematic) rather than as
    textbook filter curves. That keeps everything the specs leave out: how close bands
    push on each other, the real shape of each curve at each setting, the inductors' Q.

    Elements: resistors, capacitors, inductors (with series resistance), ideal op-amps
    (nullors: the output does whatever keeps the inputs equal), voltage-controlled
    voltage sources (an amplifier stage of fixed gain), and one input voltage source.
    Node 0 is ground.

    Real time: each capacitor and inductor becomes its trapezoidal (bilinear) companion
    model - a conductance and a history current - so the network is a fixed linear map
    from [input, history currents] to every element's voltage and the output. That map is
    worked out once whenever the circuit's values change (prepare), then each sample costs
    about (reactive elements + 1)^2 multiplies.

    The nonlinear parts (valves, transformer iron, clipping) stay in AmpModel; this is
    only the linear network between them.
*/
class Circuit
{
public:
    enum class Kind { resistor, capacitor, inductor, opamp, vcvs };

    struct Element
    {
        Kind kind;
        int a, b;             // two-terminal: a to b. opamp: a = +in, b = -in, c = out. vcvs: out = gain * (v(a) - v(b)), at c
        int c = 0;
        double value = 0;     // ohms, farads, henries; vcvs: gain
        double seriesR = 0;   // an inductor's winding resistance (ohms)
    };

    explicit Circuit (int numNodes = 1) : nodes (numNodes) {}

    int  addNode() { return nodes++; }
    int  numNodes() const { return nodes; }
    void setInput (int node)  { inNode = node; }    // driven by the input voltage
    void setOutput (int node) { outNode = node; }

    int resistor  (int a, int b, double ohms)               { return add ({ Kind::resistor, a, b, 0, ohms }); }
    int capacitor (int a, int b, double farads)             { return add ({ Kind::capacitor, a, b, 0, farads }); }
    int inductor  (int a, int b, double henries, double dcr = 0);
    int opamp     (int plus, int minus, int out)            { return add ({ Kind::opamp, plus, minus, out, 0 }); }
    int vcvs      (int plus, int minus, int out, double g)  { return add ({ Kind::vcvs, plus, minus, out, g }); }

    /** Change an element's value (a pot moved, a switch changed a component) - call prepare after. */
    void setValue (int element, double v)  { elements[(size_t) element].value = v; }

    std::vector<Element> elements;

    /** The exact (continuous-time) response at hz, by solving the circuit with complex impedances. For tests. */
    std::complex<double> responseAt (double hz) const;

private:
    int nodes = 1, inNode = 1, outNode = 1;
    int add (Element e) { elements.push_back (e); return (int) elements.size() - 1; }
    friend class CircuitRunner;
};

//==============================================================================
/** Runs a Circuit sample by sample. */
class CircuitRunner
{
public:
    /** (Re)build the per-sample map; the reactive elements keep their state when the circuit
        keeps its shape (only values changed), so moving a control doesn't click. Returns
        false if the circuit couldn't be solved (a floating node, say). */
    bool prepare (const Circuit& c, double sampleRate);
    void reset();
    void process (float* x, int n) noexcept;
    float processSample (float x) noexcept;
    bool ready() const noexcept { return ok; }

private:
    bool ok = false;
    int m = 0;                                // reactive elements
    std::vector<double> V;                    // m x (m+1): each reactive element's voltage from [u, J]
    std::vector<double> Y;                    // m+1: the output from [u, J]
    std::vector<double> G;                    // each reactive element's companion conductance
    std::vector<int> isInductor;
    std::vector<double> J, s, v;              // history currents, [u, J], element voltages
    std::vector<int> shape;                   // to tell whether the circuit's shape changed
};

} // namespace whitman
