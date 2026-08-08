#ifndef QUICK_LOCALE_CONTROLLER_H
#define QUICK_LOCALE_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QVariantList>

class QQmlApplicationEngine;

// Exposed to QML as the context property `i18n` (NOT `locale` — every QML Item
// has a built-in `locale` (QLocale) property that shadows a context property of
// the same name, which silently breaks all access inside Item scopes).
class LocaleController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool rtl READ rtl NOTIFY languageChanged)
    // Explicit, non-fragile language model: [{code, label(native endonym), rtl}].
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)

public:
    // engine may be null at construction (it only matters for deferred retranslate);
    // wire it later with setEngine(). This lets the controller be declared BEFORE the
    // engine so it outlives it — avoiding dangling-`i18n` binding warnings at shutdown.
    explicit LocaleController(QQmlApplicationEngine* engine = nullptr, QObject* parent = nullptr);

    void setEngine(QQmlApplicationEngine* engine) { engine_ = engine; }

    QString language() const { return language_; }
    bool rtl() const { return language_ == QLatin1String("ar"); }
    QVariantList languages() const;

    Q_INVOKABLE void setLanguage(const QString& code);

    // Called once after engine.load() to apply translator + retranslate
    void reapply();

signals:
    void languageChanged();

private:
    QTranslator            translator_;
    QString                language_;
    QQmlApplicationEngine* engine_;
    bool                   initialApplied_ = false;
};

#endif // QUICK_LOCALE_CONTROLLER_H
