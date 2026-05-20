#include "core/stratified_sample.h"

#include <algorithm>

namespace ur {

QVector<int> stratifiedSample(int total, int sampleCount)
{
    QVector<int> out;
    if (total <= 0 || sampleCount <= 0)
        return out;

    out.reserve(std::min(sampleCount, total));
    int last = -1;
    for (int i = 0; i < sampleCount; ++i) {
        const int idx = (total == 1)
                            ? 0
                            : (i * (total - 1)) / (sampleCount - 1);
        if (idx != last)                    // monotone non-decreasing -> dedup adj
            out.append(idx);
        last = idx;
    }
    return out;
}

} // namespace ur
