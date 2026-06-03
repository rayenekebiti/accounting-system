#pragma once
#include <QDialog>

class QLabel;
class QDialogButtonBox;

class ConfirmDialog : public QDialog {
    Q_OBJECT
public:
    enum class Kind { Question, Warning, Danger };

    explicit ConfirmDialog(const QString& title,
                           const QString& message,
                           Kind kind = Kind::Question,
                           QWidget* parent = nullptr);

    static bool ask(QWidget* parent,
                    const QString& title,
                    const QString& message,
                    Kind kind = Kind::Question);

private:
    QLabel*          m_iconLabel;
    QLabel*          m_messageLabel;
    QDialogButtonBox* m_buttons;
};
