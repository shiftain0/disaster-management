# Landslide Risk Scoring — Model Explanation & Workflow

## 1. Core Equation

$$R_L = S_H + S_{slope}\vec{V}_{Rain} + S_{slope}\vec{V}_{GW} + \frac{1}{S_{cohesion}}\vec{V}_S + \frac{1}{S_{cohesion}}\vec{V}_{GW}$$

$R_L$ is computed once per case (past or current) and comes out as a
2-component vector, since $\vec{V}_{Rain}$ and $\vec{V}_{GW}$ each have 2
components.

## 2. Variables

### Static scalars (fixed per site/case, change slowly)

| Symbol | Meaning | Role in equation |
|---|---|---|
| $S_H$ | Previous Landslide History | Added directly (baseline offset) |
| $S_{slope}$ | Slope Angle | Multiplies $\vec{V}_{Rain}$ and $\vec{V}_{GW}$ |
| $S_{cohesion}$ | Soil Shear Strength / Cohesion | Appears as $1/S_{cohesion}$, multiplies $\vec{V}_S$ and $\vec{V}_{GW}$ — must never be 0 |
| $S_{depth}$ | Soil Depth | Defined, not yet used in the equation |
| $S_{height}$ | Slope Height | Defined, not yet used in the equation |

### Dynamic vectors (change per reading/timestamp)

| Symbol | Components | Meaning |
|---|---|---|
| $\vec{V}_{Rain}$ | 2 | [Rainfall Intensity, Cumulative Rainfall] |
| $\vec{V}_S$ | 1 (zero-padded to 2 internally) | [Soil Moisture / Saturation, 0] |
| $\vec{V}_{GW}$ | 2 | [Antecedent Rainfall, Groundwater / Pore-Water Pressure] |

## 3. Workflow

```
                 PAST DATA (10 cases)                 REAL-TIME DATA (1 case)
        S_H[i], S_slope[i], S_cohesion[i]        S_H, S_slope, S_cohesion
        V_Rain[i], V_S[i], V_GW[i]               V_Rain, V_S, V_GW
                     │                                       │
                     ▼                                       ▼
        ┌─────────────────────────┐            ┌─────────────────────────┐
        │  Feature normalization  │            │  Feature normalization  │
        │  (min-max, fit on the   │───stats───▶│  (same min/max as the   │
        │   10 past cases)        │            │   past dataset — not    │
        └─────────────────────────┘            │   its own scale)        │
                     │                          └─────────────────────────┘
                     ▼                                       │
        ┌─────────────────────────┐                          ▼
        │   computeScore(R_L)     │            ┌─────────────────────────┐
        │   → pastScores[0..9]    │            │   computeScore(R_L)     │
        └─────────────────────────┘            │   → realtimeScore       │
                     │                          └─────────────────────────┘
                     │                                       │
                     └───────────────┬───────────────────────┘
                                     ▼
                  ┌───────────────────────────────────────┐
                  │  Magnitude-aware similarity, per pair  │
                  │  sim = cos(θ) × (min|a|,|b| / max|a|,|b|) │
                  │  clamp negative → 0                    │
                  └───────────────────────────────────────┘
                                     │
                                     ▼
                  ┌───────────────────────────────────────┐
                  │   Average of the 10 clamped values     │
                  │   → single risk indicator (0–1)        │
                  └───────────────────────────────────────┘
```

## 4. Why normalization was added

$\vec{V}_{Rain}$, $\vec{V}_S$, $\vec{V}_{GW}$ carry different units and
scales (rainfall in mm, moisture in m³/m³, pressure in kPa/cm H₂O). Without
normalization, whichever feature has the largest raw numbers dominates the
score and the similarity comparison, regardless of how meaningful that
feature actually is. Min-max normalization is fit **only on the 10 past
cases** and then applied to the real-time case too, so the real-time case
is judged on the same scale the past cases were — not rescaled on its own,
which would make it incomparable.

$S_H$, $S_{slope}$, $S_{cohesion}$ are **not** normalized: they act as
coefficients/physical parameters in the equation, not as directly-compared
features. $S_{cohesion}$ in particular is a divisor — normalizing it would
force the smallest historical value toward 0 and blow $1/S_{cohesion}$
toward infinity. They should be supplied in real physical units.

## 5. Why the similarity metric changed from plain cosine

Plain cosine similarity measures only the **direction** of a vector, not its
size. A vector 10× weaker in every component than a real landslide case
still scores a perfect 1.0 against it, because scaling a vector doesn't
change its direction — the exact signal the equation is built to capture
("higher magnitude = higher risk") gets thrown away.

The similarity used here is:

$$\text{similarity} = \cos(\theta) \times \frac{\min(|a|, |b|)}{\max(|a|, |b|)}$$

The first term keeps the original idea (does the current case point in the
same "shape" as a past landslide case). The second term penalizes cases
where the magnitudes disagree, so a weak or extreme case no longer scores
identically to a moderate one just because the ratio between features
matched.

## 6. What the output number actually means — and doesn't

The final average is a **similarity-weighted score in [0, 1]**, showing how
closely current conditions resemble the 10 stored past cases, adjusted for
both direction and magnitude. It is **not** a calibrated probability of a
landslide occurring. It has not been validated against confirmed
non-landslide periods, and 10 cases is a small sample. Treat the number as
a relative indicator ("more similar to past triggers" vs "less similar"),
not as a percentage chance.

