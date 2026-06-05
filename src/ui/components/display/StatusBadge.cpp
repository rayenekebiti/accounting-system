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
    case Status::Draft:     applyStyle("rgba(65,145,225,0.15)",  "#64B5F6"); setText("Draft");     break;
    case Status::Posted:    applyStyle("rgba(75,164,106,0.15)",  "#4BA46A"); setText("Posted");    break;
    case Status::Paid:      applyStyle("rgba(75,164,106,0.18)",  "#56C278"); setText("Paid");      break;
    case Status::Overdue:   applyStyle("rgba(212,96,58,0.18)",   "#D4603A"); setText("Overdue");   break;
    case Status::Void:      applyStyle("rgba(224,85,85,0.15)",   "#E05555"); setText("Void");      break;
    case Status::Active:    applyStyle("rgba(75,164,106,0.15)",  "#4BA46A"); setText("Active");    break;
    case Status::Inactive:  applyStyle("rgba(139,134,132,0.15)", "#8B8684"); setText("Inactive");  break;
    case Status::InStock:   applyStyle("rgba(75,164,106,0.15)",  "#4BA46A"); setText("In Stock");  break;
    case Status::LowStock:  applyStyle("rgba(212,96,58,0.18)",   "#D4603A"); setText("Low");       break;
    case Status::OutOfStock:applyStyle("rgba(224,85,85,0.15)",   "#E05555"); setText("Out");       break;
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
