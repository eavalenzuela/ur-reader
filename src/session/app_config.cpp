#include "session/app_config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace ur {

namespace {

QString configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
         + QStringLiteral("/config.json");
}

} // namespace

AppConfig AppConfig::load()
{
    AppConfig cfg;   // defaults

    QFile file(configPath());
    if (!file.open(QIODevice::ReadOnly))
        return cfg;

    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    if (o.contains("backgroundColor"))
        cfg.backgroundColor = QColor(o.value("backgroundColor").toString());
    cfg.pageGap = o.value("pageGap").toInt(cfg.pageGap);
    if (o.value("defaultMode").toString() == QLatin1String("scroll"))
        cfg.defaultMode = ReadingMode::Scroll;
    if (o.contains("cacheBudgetBytes"))
        cfg.cacheBudgetBytes =
            static_cast<qint64>(o.value("cacheBudgetBytes").toDouble());
    cfg.prefetchAhead = o.value("prefetchAhead").toInt(cfg.prefetchAhead);
    cfg.prefetchBehind = o.value("prefetchBehind").toInt(cfg.prefetchBehind);
    return cfg;
}

bool AppConfig::save() const
{
    const QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject o;
    o["backgroundColor"] = backgroundColor.name();
    o["pageGap"] = pageGap;
    o["defaultMode"] =
        defaultMode == ReadingMode::Scroll ? QStringLiteral("scroll")
                                           : QStringLiteral("paged");
    o["cacheBudgetBytes"] = static_cast<double>(cacheBudgetBytes);
    o["prefetchAhead"] = prefetchAhead;
    o["prefetchBehind"] = prefetchBehind;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace ur
