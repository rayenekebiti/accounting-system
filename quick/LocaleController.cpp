#include "LocaleController.h"

#include <QTranslator>
#include <QGuiApplication>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QCoreApplication>
#include <QVariantList>
#include <QVariantMap>

LocaleController::LocaleController(QQmlApplicationEngine* engine, QObject* parent)
    : QObject(parent)
    , engine_(engine)
{
    // Read saved language; default "en"
    QSettings settings;
    const QString saved = settings.value(QStringLiteral("ui/language"), QStringLiteral("en")).toString();

    // Apply language WITHOUT calling engine_->retranslate() yet
    // (engine has not loaded anything at ctor time — retranslate is deferred to reapply())
    qApp->removeTranslator(&translator_);
    if (translator_.load(QStringLiteral("app_") + saved, QStringLiteral(":/i18n")))
        qApp->installTranslator(&translator_);

    QGuiApplication::setLayoutDirection(saved == QLatin1String("ar") ? Qt::RightToLeft : Qt::LeftToRight);
    language_ = saved;
    // No retranslate here — nothing loaded yet. Caller must invoke reapply() after engine.load().
}

void LocaleController::reapply()
{
    // Force a retranslate pass now that the engine has loaded QML
    if (engine_)
        engine_->retranslate();
    emit languageChanged();
    initialApplied_ = true;
}

void LocaleController::setLanguage(const QString& code)
{
    if (code == language_ && initialApplied_)
        return;

    qApp->removeTranslator(&translator_);
    if (translator_.load(QStringLiteral("app_") + code, QStringLiteral(":/i18n")))
        qApp->installTranslator(&translator_);

    QGuiApplication::setLayoutDirection(code == QLatin1String("ar") ? Qt::RightToLeft : Qt::LeftToRight);
    QSettings().setValue(QStringLiteral("ui/language"), code);

    language_ = code;

    if (engine_ && initialApplied_)
        engine_->retranslate();

    emit languageChanged();
}

QVariantList LocaleController::languages() const
{
    // Native endonyms — conventionally NOT translated, so they read the same in
    // any UI language. rtl drives layout direction for that choice.
    QVariantList list;
    auto add = [&list](const char* code, const QString& label, bool rtl) {
        QVariantMap m;
        m[QStringLiteral("code")]  = QString::fromLatin1(code);
        m[QStringLiteral("label")] = label;
        m[QStringLiteral("rtl")]   = rtl;
        list.append(m);
    };
    add("en", QStringLiteral("English"),        false);
    add("fr", QString::fromUtf8("Français"),     false);
    add("ar", QString::fromUtf8("العربية"),       true);
    return list;
}
