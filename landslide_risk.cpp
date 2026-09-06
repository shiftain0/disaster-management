#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <limits>

// Generic scored-comparison prototype.
// score = S_H + S_slope*V_Rain + S_slope*V_GW + (1/S_cohesion)*V_S + (1/S_cohesion)*V_GW
// Names below are this prototype's example domain -- rename/resize per problem.
// Core idea unchanged from earlier versions: linear-combination score per
// case, then compare the current case's score to past cases' scores.
// Two additions on top of the original design:
//   1. Per-feature normalization (min-max, computed from the past dataset
//      itself) so features on different scales/units don't dominate the
//      comparison purely because their raw numbers are bigger.
//   2. Magnitude-aware similarity: cosine similarity multiplied by a
//      magnitude-agreement factor, so two vectors that only match in
//      direction but differ hugely in scale no longer score as "identical".

struct DynamicVectors {
    std::vector<double> V_Rain; // placeholder feature vector 1 -- rename/resize per problem
    std::vector<double> V_S;    // placeholder feature vector 2 -- rename/resize per problem
    std::vector<double> V_GW;   // placeholder feature vector 3 -- rename/resize per problem
};

struct StaticScalars {
    double S_H;         // placeholder fixed parameter -- rename per problem
    double S_slope;      // placeholder fixed parameter -- rename per problem
    double S_cohesion;   // placeholder fixed parameter -- rename per problem (must be non-zero)
};

const int N = 10; // number of past cases to compare against

std::vector<double> zeroPad(const std::vector<double>& v, size_t targetSize) {
    std::vector<double> out = v;
    while (out.size() < targetSize) out.push_back(0.0);
    return out;
}

std::vector<double> broadcastScalar(double s, size_t targetSize) {
    return std::vector<double>(targetSize, s);
}

std::vector<double> computeScore(const DynamicVectors& dyn, const StaticScalars& stat) {
    size_t n = dyn.V_Rain.size();
    std::vector<double> S_H_vec = broadcastScalar(stat.S_H, n);
    std::vector<double> V_S_vec = zeroPad(dyn.V_S, n);

    std::vector<double> score(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        score[i] = S_H_vec[i]
                 + stat.S_slope * dyn.V_Rain[i]
                 + stat.S_slope * dyn.V_GW[i]
                 + (1.0 / stat.S_cohesion) * V_S_vec[i]
                 + (1.0 / stat.S_cohesion) * dyn.V_GW[i];
    }
    return score;
}

// ---- Normalization: min-max per raw field, fit from the past dataset ----
struct FieldStats { double mn, mx; };

double normalize(double v, const FieldStats& s) {
    if (s.mx - s.mn < 1e-12) return 0.5; // all past values identical -> neutral midpoint
    return (v - s.mn) / (s.mx - s.mn);
}

// Only the compared FEATURE vectors get normalized (they mix units:
// rainfall mm, moisture m^3/m^3, pressure kPa/cm-H2O). S_H, S_slope,
// S_cohesion are coefficients/physical parameters, not compared features --
// normalizing S_cohesion would force the smallest historical value toward 0
// and blow up 1/S_cohesion toward infinity, so it is left in real units.
struct DatasetStats {
    FieldStats Rain0, Rain1, Sval, GW0, GW1;
};

DatasetStats fitStats(DynamicVectors data[], int n) {
    DatasetStats st;
    auto initStat = [](){ return FieldStats{ std::numeric_limits<double>::max(),
                                              std::numeric_limits<double>::lowest() }; };
    st.Rain0 = st.Rain1 = st.Sval = st.GW0 = st.GW1 = initStat();

    auto upd = [](FieldStats& f, double v){ f.mn = std::min(f.mn, v); f.mx = std::max(f.mx, v); };

    for (int i = 0; i < n; ++i) {
        upd(st.Rain0, data[i].V_Rain[0]);
        upd(st.Rain1, data[i].V_Rain[1]);
        upd(st.Sval,  data[i].V_S[0]);
        upd(st.GW0,   data[i].V_GW[0]);
        upd(st.GW1,   data[i].V_GW[1]);
    }
    return st;
}

void applyNormalize(DynamicVectors& d, const DatasetStats& st) {
    d.V_Rain[0] = normalize(d.V_Rain[0], st.Rain0);
    d.V_Rain[1] = normalize(d.V_Rain[1], st.Rain1);
    d.V_S[0]    = normalize(d.V_S[0], st.Sval);
    d.V_GW[0]   = normalize(d.V_GW[0], st.GW0);
    d.V_GW[1]   = normalize(d.V_GW[1], st.GW1);
}

// ---- Magnitude-aware similarity: cosine * magnitude-agreement factor ----
double magnitudeAwareSimilarity(const std::vector<double>& a, const std::vector<double>& b) {
    double dot = 0.0, normA = 0.0, normB = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot   += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    normA = std::sqrt(normA);
    normB = std::sqrt(normB);
    if (normA < 1e-12 || normB < 1e-12) return 0.0;

    double cosTheta   = dot / (normA * normB);
    double magFactor   = std::min(normA, normB) / std::max(normA, normB); // 1.0 if equal magnitude, ->0 if very different
    return cosTheta * magFactor;
}

int main() {
    // Past cases -- replace with real data, one row per index 0..9.
    StaticScalars S_H_data[N] = {
        {1.0, 0.6, 12.5}, {0.0, 0.45, 9.8},  {1.0, 0.55, 8.0},
        {0.0, 0.50, 10.2},{1.0, 0.65, 7.5},  {0.0, 0.40, 11.0},
        {1.0, 0.58, 9.0}, {0.0, 0.48, 10.8}, {1.0, 0.62, 8.4},
        {0.0, 0.52, 9.6}
    };
    DynamicVectors data[N] = {
        { {12.4, 85.0}, {0.34}, {40.0, 2.1} },
        { {5.0, 20.0},  {0.22}, {10.0, 0.8} },
        { {15.0, 90.0}, {0.38}, {45.0, 2.5} },
        { {8.0, 35.0},  {0.28}, {18.0, 1.2} },
        { {20.0, 110.0},{0.42}, {55.0, 3.0} },
        { {3.0, 12.0},  {0.18}, {8.0, 0.5}  },
        { {17.0, 95.0}, {0.36}, {48.0, 2.7} },
        { {6.0, 25.0},  {0.24}, {12.0, 0.9} },
        { {19.0, 105.0},{0.40}, {50.0, 2.9} },
        { {4.0, 15.0},  {0.20}, {9.0, 0.6}  }
    };

    // Current case -- left empty, fill in when available.
    StaticScalars realtime_S_H_data = { 0.0, 0.0, 1.0 };
    DynamicVectors realtime_data = { {0.0, 0.0}, {0.0}, {0.0, 0.0} };

    // Fit normalization stats from the past dataset's feature vectors only,
    // then apply the same scale to both past and realtime data (realtime
    // must be judged on the same scale the past cases were, not its own).
    DatasetStats stats = fitStats(data, N);

    DynamicVectors normD[N];
    for (int i = 0; i < N; ++i) {
        normD[i] = data[i];
        applyNormalize(normD[i], stats);
    }
    DynamicVectors normRealtimeD = realtime_data;
    applyNormalize(normRealtimeD, stats);

    // Scores computed on normalized feature vectors; scalars stay in real units.
    std::vector<std::vector<double>> pastScores(N);
    for (int i = 0; i < N; ++i) pastScores[i] = computeScore(normD[i], S_H_data[i]);
    std::vector<double> realtimeScore = computeScore(normRealtimeD, realtime_S_H_data);

    // Compare realtime to each past case with magnitude-aware similarity.
    std::vector<double> clampedSim(N);
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
        double sim = magnitudeAwareSimilarity(realtimeScore, pastScores[i]);
        clampedSim[i] = std::max(0.0, sim); // drop [-1, 0), keep [0, 1]
        sum += clampedSim[i];
    }
    double averageRisk = sum / N;

    std::cout << std::fixed << std::setprecision(4);
    for (int i = 0; i < N; ++i)
        std::cout << "pastScores[" << i << "] = ( " << pastScores[i][0] << " " << pastScores[i][1] << " )\n";
    std::cout << "realtimeScore    = ( " << realtimeScore[0] << " " << realtimeScore[1] << " )\n\n";

    for (int i = 0; i < N; ++i)
        std::cout << "similarity[" << i << "] = " << clampedSim[i] << "\n";
    std::cout << "\naverage similarity-weighted risk indicator (0-1) = " << averageRisk << "\n";

    return 0;
}
