#include "a11y.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QAccessible>
#include <QAccessibleInterface>
#include <QCoreApplication>
#include <QEventLoop>
#include <QString>
#include <QStringList>

#include <cstdio>

namespace {

void out(const QString& s) { std::fputs(s.toUtf8().constData(), stderr); std::fputc('\n', stderr); std::fflush(stderr); }

bool interactive(QAccessible::Role r)
{
    switch (r) {
    case QAccessible::Button:
    case QAccessible::CheckBox:
    case QAccessible::RadioButton:
    case QAccessible::EditableText:
    case QAccessible::ComboBox:
    case QAccessible::PageTab:
    case QAccessible::Link:
    case QAccessible::MenuItem:
    case QAccessible::SpinBox:
    case QAccessible::ListItem:
        return true;
    default:
        return false;
    }
}

const char* roleName(QAccessible::Role r)
{
    switch (r) {
    case QAccessible::Button:       return "Button";
    case QAccessible::CheckBox:     return "CheckBox";
    case QAccessible::EditableText: return "EditableText";
    case QAccessible::ComboBox:     return "ComboBox";
    case QAccessible::PageTab:      return "PageTab";
    case QAccessible::MenuItem:     return "MenuItem";
    case QAccessible::ListItem:     return "ListItem";
    case QAccessible::StaticText:   return "StaticText";
    default:                        return "Other";
    }
}

void walk(QAccessibleInterface* iface, int& total, int& named, QStringList& problems, QStringList& inventory)
{
    if (!iface || !iface->isValid()) return;

    const QAccessible::Role r = iface->role();
    const QString name = iface->text(QAccessible::Name).trimmed();

    // A hidden / offscreen control is NOT announced by a screen reader (Qt marks it Invisible and
    // NVDA/Narrator skip it), so it is not an accessibility defect and must not be counted.
    const QAccessible::State st = iface->state();
    if (interactive(r) && !st.invisible && !st.offscreen) {
        ++total;
        if (name.isEmpty()) {
            // Report enough to LOCATE the offender: its C++/QML class and the named accessible
            // ancestor it sits under (so the fix can be traced to a screen/component).
            QObject* o = iface->object();
            const QString cls = o ? QString::fromLatin1(o->metaObject()->className()) : QStringLiteral("?");
            const QString txt = o ? o->property("text").toString() : QString();
            const QString acc = o ? o->property("accessibleName").toString() : QString();
            const QString on  = o ? o->objectName() : QString();
            QString under;
            for (QAccessibleInterface* p = iface->parent(); p && p->isValid(); p = p->parent()) {
                const QString pn = p->text(QAccessible::Name).trimmed();
                if (!pn.isEmpty()) { under = pn; break; }
            }
            problems << QString("  UNNAMED %1 [%2] text='%3' accName='%4' objName='%5'%6")
                         .arg(roleName(r), cls, txt, acc, on,
                              under.isEmpty() ? QString() : QStringLiteral(" under \"") + under + "\"");
        }
        else { ++named; inventory << QString("  %1: \"%2\"").arg(roleName(r), name); }
    }

    const int n = iface->childCount();
    for (int i = 0; i < n; ++i)
        walk(iface->child(i), total, named, problems, inventory);
}

} // namespace

int runA11yAudit(QQmlApplicationEngine& engine)
{
    if (engine.rootObjects().isEmpty()) { out("A11Y: no root object"); return 2; }

    QObject* root = engine.rootObjects().first();
    QQuickWindow* win = qobject_cast<QQuickWindow*>(root);
    if (!win) { out("A11Y: root is not a window"); return 2; }

    // Show + pump events so the visible item tree (and thus its accessible
    // interfaces) is realized before we walk it.
    win->show();
    for (int i = 0; i < 15; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(win);
    if (!iface) { out("A11Y: no accessible interface for the window (bridge inactive?)"); return 2; }

    int total = 0, named = 0;
    QStringList problems, inventory;
    walk(iface, total, named, problems, inventory);

    out("== Accessibility tree audit (realized controls) ==");
    for (const QString& s : inventory) out(s);
    if (!problems.isEmpty()) {
        out("-- interactive controls MISSING an accessible name --");
        for (const QString& s : problems) out(s);
    }
    out(QString("A11Y: %1 interactive controls realized, %2 named, %3 unnamed")
            .arg(total).arg(named).arg(total - named));
    out("NOTE: NVDA/Narrator spoken-experience + dialog-internal controls are a manual pass.");

    // Pass = we found realized interactive controls and every one is named. (total==0
    // means the headless tree didn't realize — reported as inconclusive, not pass.)
    const bool pass = (total > 0) && (named == total);
    out(QString("  verdict: %1").arg(pass ? "PASS" : (total == 0 ? "INCONCLUSIVE (no realized controls)" : "FAIL")));
    return pass ? 0 : 1;
}
