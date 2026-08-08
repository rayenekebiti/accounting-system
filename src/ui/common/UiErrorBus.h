#pragma once
#include <QString>
#include <stdexcept>

class QWidget;

// Centralised error presentation. Every catch block in a page slot calls one of
// these instead of inlining QMessageBox::critical.
class UiErrorBus {
public:
    static void error(const QString& title, const QString& msg,
                      QWidget* parent = nullptr);

    // Wraps fn() in try/catch. Returns true on success, shows error + returns
    // false on any std::exception. Keeps pages down to one catch site per slot.
    template<typename F>
    static bool run(F&& fn, const QString& title, QWidget* parent = nullptr)
    {
        try {
            fn();
            return true;
        } catch (const std::exception& e) {
            error(title, QString::fromUtf8(e.what()), parent);
            return false;
        }
    }
};
