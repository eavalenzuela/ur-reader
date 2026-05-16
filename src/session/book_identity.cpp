#include "session/book_identity.h"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QFile>

namespace ur {

QString computeBookId(const QString& archivePath)
{
    QFile file(archivePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    constexpr qint64 kChunk = 64 * 1024;
    const qint64 size = file.size();

    QCryptographicHash hash(QCryptographicHash::Sha256);

    // File size as 8 little-endian bytes — distinguishes same-prefix files.
    char sizeBytes[8];
    for (int i = 0; i < 8; ++i)
        sizeBytes[i] = static_cast<char>((size >> (8 * i)) & 0xFF);
    hash.addData(QByteArrayView(sizeBytes, 8));

    // First 64 KB.
    hash.addData(file.read(kChunk));

    // Last 64 KB. For files <= one chunk the head already covers everything.
    if (size > kChunk) {
        file.seek(size - kChunk);
        hash.addData(file.read(kChunk));
    }

    return QStringLiteral("p1:") + QString::fromLatin1(hash.result().toHex());
}

} // namespace ur
