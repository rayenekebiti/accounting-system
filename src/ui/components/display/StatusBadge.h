#pragma once
#include <QLabel>

class StatusBadge : public QLabel {
    Q_OBJECT
public:
    enum class Status { Draft, Posted, Paid, Overdue, Void, Active, Inactive, InStock, LowStock, OutOfStock };

    explicit StatusBadge(QWidget* parent = nullptr);
    explicit StatusBadge(Status status, QWidget* parent = nullptr);

    void setStatus(Status status);
    void setStatus(const QString& statusText);

private:
    void applyStyle(const QString& bg, const QString& fg);
};
