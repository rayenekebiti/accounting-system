#include "components/forms/SectionHeader.h"
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>

SectionHeader::SectionHeader(const QString& title, QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(12);

    auto* label = new QLabel(title, this);
    label->setObjectName("sectionTitle");

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setStyleSheet("color: #E0E0E0;");

    layout->addWidget(label);
    layout->addWidget(line, 1);
}

void SectionHeader::setTitle(const QString& title)
{
    if (auto* label = findChild<QLabel*>())
        label->setText(title);
}
