#include "components/display/StatusBadge.h"

StatusBadge::StatusBadge(QWidget* parent) : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setContentsMargins(8, 3, 8, 3);
}

StatusBadge::StatusBadge(Status status, QWidget* parent) : StatusBadge(parent)
{
    setStatus(status);
}

void StatusBadge::setStatus(Status status)
{
    switch (status) {
    case Status::Draft:     applyStyle("#E3F2FD", "#1565C0"); setText("Draft");     break;
    case Status::Posted:    applyStyle("#E8F5E9", "#2E7D32"); setText("Posted");    break;
    case Status::Paid:      applyStyle("#E8F5E9", "#1B5E20"); setText("Paid");      break;
    case Status::Overdue:   applyStyle("#FFF3E0", "#E65100"); setText("Overdue");   break;
    case Status::Void:      applyStyle("#FFEBEE", "#B71C1C"); setText("Void");      break;
    case Status::Active:    applyStyle("#E8F5E9", "#2E7D32"); setText("Active");    break;
    case Status::Inactive:  applyStyle("#F5F5F5", "#616161"); setText("Inactive");  break;
    case Status::InStock:   applyStyle("#E8F5E9", "#2E7D32"); setText("In Stock");  break;
    case Status::LowStock:  applyStyle("#FFF3E0", "#E65100"); setText("Low");       break;
    case Status::OutOfStock:applyStyle("#FFEBEE", "#B71C1C"); setText("Out");       break;
    }
}

void StatusBadge::setStatus(const QString& statusText)
{
    static const QMap<QString, Status> map = {
        {"Draft",    Status::Draft},    {"Posted",   Status::Posted},
        {"Paid",     Status::Paid},     {"Overdue",  Status::Overdue},
        {"Void",     Status::Void},     {"Active",   Status::Active},
        {"Inactive", Status::Inactive}, {"InStock",  Status::InStock},
        {"LowStock", Status::LowStock}, {"OutOfStock",Status::OutOfStock},
    };
    if (map.contains(statusText))
        setStatus(map[statusText]);
    else {
        setText(statusText);
        applyStyle("#F5F5F5", "#616161");
    }
}

void StatusBadge::applyStyle(const QString& bg, const QString& fg)
{
    setStyleSheet(QString("QLabel { background: %1; color: %2; border-radius: 10px; "
                          "font-size: 11px; font-weight: 600; padding: 3px 8px; }").arg(bg, fg));
}
