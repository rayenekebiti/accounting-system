#pragma once
#include <QWidget>

class SectionHeader : public QWidget {
    Q_OBJECT
public:
    explicit SectionHeader(const QString& title, QWidget* parent = nullptr);
    void setTitle(const QString& title);
};
