#include "components/forms/FormRow.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

FormRow::FormRow(const QString& label, QWidget* field, QWidget* parent)
    : QWidget(parent), m_field(field)
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(4);

    auto* row = new QHBoxLayout;
    row->setSpacing(12);

    m_label = new QLabel(label, this);
    m_label->setFixedWidth(140);
    m_label->setAlignment(Qt::AlignRight | Qt::AlignTop);
    m_label->setStyleSheet("color: #757575; padding-top: 6px;");

    row->addWidget(m_label);
    row->addWidget(m_field, 1);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color: #C62828; font-size: 12px; padding-left: 152px;");
    m_errorLabel->setVisible(false);
    m_errorLabel->setWordWrap(true);

    col->addLayout(row);
    col->addWidget(m_errorLabel);
}

void FormRow::setRequired(bool required)
{
    const QString base = m_label->text().remove(" *");
    m_label->setText(required ? base + " *" : base);
}

void FormRow::setError(const QString& message)
{
    m_errorLabel->setText(message);
    m_errorLabel->setVisible(!message.isEmpty());
    if (!message.isEmpty())
        m_field->setStyleSheet(m_field->styleSheet() + " border-color: #C62828;");
}

void FormRow::clearError()
{
    setError({});
    m_field->setStyleSheet({});
}
