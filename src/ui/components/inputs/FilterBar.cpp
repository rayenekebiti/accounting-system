#include "components/inputs/FilterBar.h"
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>

FilterBar::FilterBar(QWidget* parent) : QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(8);
}

void FilterBar::addFilter(const QString& label, const QStringList& options)
{
    auto* lbl = new QLabel(label + ":", this);
    lbl->setObjectName("muted");
    m_layout->addWidget(lbl);

    auto* combo = new QComboBox(this);
    combo->addItem("All");
    combo->addItems(options);
    combo->setMinimumWidth(120);
    m_combos.append(combo);
    m_layout->addWidget(combo);

    connect(combo, &QComboBox::currentIndexChanged, this, &FilterBar::filterChanged);
}

QString FilterBar::filterValue(int index) const
{
    if (index < 0 || index >= m_combos.size()) return {};
    const int i = m_combos[index]->currentIndex();
    return i == 0 ? QString{} : m_combos[index]->currentText();
}

void FilterBar::reset()
{
    for (auto* combo : m_combos)
        combo->setCurrentIndex(0);
}
