#include "components/feedback/ConfirmDialog.h"
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

ConfirmDialog::ConfirmDialog(const QString& title, const QString& message, Kind kind, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);
    setMinimumWidth(380);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 16);
    mainLayout->setSpacing(16);

    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setStyleSheet("font-size: 28px;");
    m_iconLabel->setAlignment(Qt::AlignTop);
    switch (kind) {
    case Kind::Question: m_iconLabel->setText("❓"); break;
    case Kind::Warning:  m_iconLabel->setText("⚠️");  break;
    case Kind::Danger:   m_iconLabel->setText("🗑️"); break;
    }

    m_messageLabel = new QLabel(message, this);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    contentRow->addWidget(m_iconLabel);
    contentRow->addWidget(m_messageLabel, 1);
    mainLayout->addLayout(contentRow);

    m_buttons = new QDialogButtonBox(this);
    auto* yesBtn = m_buttons->addButton(QDialogButtonBox::Yes);
    auto* noBtn  = m_buttons->addButton(QDialogButtonBox::No);

    if (kind == Kind::Danger) {
        yesBtn->setObjectName("danger");
        yesBtn->setText("Delete");
        noBtn->setText("Cancel");
    }

    mainLayout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool ConfirmDialog::ask(QWidget* parent, const QString& title, const QString& message, Kind kind)
{
    ConfirmDialog dlg(title, message, kind, parent);
    return dlg.exec() == QDialog::Accepted;
}
