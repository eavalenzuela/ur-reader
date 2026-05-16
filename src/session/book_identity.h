#pragma once

#include <QString>

namespace ur {

// Computes the partial-hash book id that keys the progress file:
//
//   "p1:" + hex( SHA-256( fileSizeBytes ++ first64KB ++ last64KB ) )
//
// The "p1:" prefix tags the partial-hash algorithm version, so a future
// algorithm change cannot silently collide with old ids. Near-instant on
// large archives; survives the file being renamed or moved.
//
// Returns an empty string if the file cannot be read.
//
// MODULE: session/progress.
QString computeBookId(const QString& archivePath);

} // namespace ur
