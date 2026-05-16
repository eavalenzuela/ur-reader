#include "core/natural_sort.h"

namespace ur {

namespace {

bool isAsciiDigit(QChar c)
{
    return c >= QLatin1Char('0') && c <= QLatin1Char('9');
}

} // namespace

bool naturalLess(const QString& a, const QString& b)
{
    const int na = a.size();
    const int nb = b.size();
    int i = 0;
    int j = 0;

    while (i < na && j < nb) {
        const QChar ca = a.at(i);
        const QChar cb = b.at(j);

        if (isAsciiDigit(ca) && isAsciiDigit(cb)) {
            // Compare two runs of digits by numeric value.
            const int si = i;
            const int sj = j;
            while (i < na && isAsciiDigit(a.at(i))) ++i;
            while (j < nb && isAsciiDigit(b.at(j))) ++j;

            // Strip leading zeros (keep at least one digit).
            int zi = si;
            while (zi < i - 1 && a.at(zi) == QLatin1Char('0')) ++zi;
            int zj = sj;
            while (zj < j - 1 && b.at(zj) == QLatin1Char('0')) ++zj;

            const int li = i - zi;   // significant digit count
            const int lj = j - zj;
            if (li != lj)
                return li < lj;      // fewer significant digits => smaller
            for (int k = 0; k < li; ++k) {
                if (a.at(zi + k) != b.at(zj + k))
                    return a.at(zi + k) < b.at(zj + k);
            }
            // Equal value: order the run with fewer leading zeros first,
            // so ordering stays deterministic.
            if ((i - si) != (j - sj))
                return (i - si) < (j - sj);
        } else {
            const QChar la = ca.toLower();
            const QChar lb = cb.toLower();
            if (la != lb)
                return la < lb;
            ++i;
            ++j;
        }
    }

    // Shared prefix exhausted: the shorter remainder sorts first.
    return (na - i) < (nb - j);
}

} // namespace ur
