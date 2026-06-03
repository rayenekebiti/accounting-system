#include "components/inputs/SearchBar.h"
#include <QLineEdit>
#include <QToolButton>
#include <QTimer>
#include <QHBoxLayout>

SearchBar::SearchBar(const QString& placeholder, QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(placeholder);
    m_edit->setMinimumWidth(220);
    m_edit->setStyleSheet("QLineEdit { padding-left: 28px; }");

    m_clearBtn = new QToolButton(this);
    m_clearBtn->setText("✕");
    m_clearBtn->setVisible(false);
    m_clearBtn->setStyleSheet("QToolButton { border: none; background: transparent; color: #9E9E9E; }");
    m_clearBtn->setFixedSize(24, 24);

    layout->addWidget(m_edit);
    layout->addWidget(m_clearBtn);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(300);

    connect(m_edit, &QLineEdit::textChanged, this, &SearchBar::onTextChanged);
    connect(m_debounce, &QTimer::timeout, this, &SearchBar::emitSearch);
    connect(m_clearBtn, &QToolButton::clicked, this, &SearchBar::clear);
}

QString SearchBar::text() const { return m_edit->text(); }

void SearchBar::clear()
{
    m_edit->clear();
}

void SearchBar::setDebounceMs(int ms)
{
    m_debounce->setInterval(ms);
}

void SearchBar::onTextChanged(const QString& text)
{
    m_clearBtn->setVisible(!text.isEmpty());
    m_debounce->start();
}

void SearchBar::emitSearch()
{
    emit searchChanged(m_edit->text());
}
