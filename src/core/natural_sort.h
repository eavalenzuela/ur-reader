#pragma once

#include <QString>

namespace ur {

// Compares two strings treating embedded digit runs as numbers, so
// "page2" < "page10". Case-insensitive for non-digit characters. Used to
// order archive entries — the archive's stored order is never trusted.
//
// MODULE: core (dependency-free utility).
bool naturalLess(const QString& a, const QString& b);

} // namespace ur
