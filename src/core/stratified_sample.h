#pragma once

#include <QVector>

namespace ur {

// Returns up to `sampleCount` indices evenly spaced across [0, total).
// The first and last indices of the range are always included when
// possible; intermediate values are evenly spaced and deduped (so a
// sampleCount larger than `total` collapses to {0, 1, ..., total-1}).
// Used to pick a representative set of pages for cheap header probing.
//
// MODULE: core (dependency-free utility).
QVector<int> stratifiedSample(int total, int sampleCount);

} // namespace ur
