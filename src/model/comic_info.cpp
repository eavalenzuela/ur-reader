#include "model/comic_info.h"

#include <QLatin1String>
#include <QXmlStreamReader>

namespace ur {

namespace {

MangaHint parseMangaValue(const QString& v)
{
    if (v.compare(QLatin1String("YesAndRightToLeft"), Qt::CaseInsensitive) == 0)
        return MangaHint::YesAndRTL;
    if (v.compare(QLatin1String("Yes"), Qt::CaseInsensitive) == 0)
        return MangaHint::Yes;
    if (v.compare(QLatin1String("No"), Qt::CaseInsensitive) == 0)
        return MangaHint::No;
    return MangaHint::Unknown;
}

bool parseBoolAttr(QStringView v)
{
    return v.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0
        || v == QLatin1String("1");
}

} // namespace

ComicInfoHints ComicInfoHints::parse(const QByteArray& xml)
{
    ComicInfoHints out;
    if (xml.isEmpty())
        return out;

    QXmlStreamReader r(xml);
    while (!r.atEnd() && !r.hasError()) {
        r.readNext();
        if (!r.isStartElement())
            continue;

        const QStringView name = r.name();
        if (name == QLatin1String("Manga")) {
            out.manga = parseMangaValue(r.readElementText());
        } else if (name == QLatin1String("Page")) {
            const QXmlStreamAttributes attrs = r.attributes();
            if (!attrs.hasAttribute(QLatin1String("Image")))
                continue;
            bool ok = false;
            const int idx = attrs.value(QLatin1String("Image")).toInt(&ok);
            if (!ok || idx < 0)
                continue;
            ComicInfoPage p;
            if (attrs.hasAttribute(QLatin1String("DoublePage")))
                p.doublePage = parseBoolAttr(attrs.value(QLatin1String("DoublePage")));
            if (attrs.hasAttribute(QLatin1String("Type")))
                p.type = attrs.value(QLatin1String("Type")).toString();
            out.pages.insert(idx, p);
        }
    }
    return out;
}

std::optional<ReadingDirection> ComicInfoHints::readingDirection() const
{
    switch (manga) {
    case MangaHint::YesAndRTL:
        return ReadingDirection::RightToLeft;
    case MangaHint::No:
        return ReadingDirection::LeftToRight;
    case MangaHint::Yes:
    case MangaHint::Unknown:
        return std::nullopt;          // ambiguous — let other layers decide
    }
    return std::nullopt;
}

} // namespace ur
