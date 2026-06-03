#pragma once
#include <QWidget>

class QLabel;

class FormRow : public QWidget {
    Q_OBJECT
public:
    explicit FormRow(const QString& label, QWidget* field, QWidget* parent = nullptr);

    void setRequired(bool required);
    void setError(const QString& message);
    void clearError();

private:
    QLabel* m_label;
    QLabel* m_errorLabel;
    QWidget* m_field;
};
