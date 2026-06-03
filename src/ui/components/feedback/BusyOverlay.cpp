#include "components/feedback/BusyOverlay.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QResizeEvent>

BusyOverlay::BusyOverlay(QWidget* parent) : QWidget(parent)
{
    setObjectName("busyOverlay");
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setStyleSheet("QWidget#busyOverlay { background: rgba(255,255,255,0.75); }");
    hide();

    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    auto* spinner = new QLabel("⟳", this);
    spinner->setAlignment(Qt::AlignCenter);
    spinner->setStyleSheet("font-size: 32px; color: #1565C0;");

    m_messageLabel = new QLabel(this);
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setObjectName("muted");

    layout->addWidget(spinner);
    layout->addWidget(m_messageLabel);
}

void BusyOverlay::show(const QString& message)
{
    m_messageLabel->setText(message);
    raise();
    QWidget::show();
}

void BusyOverlay::hide()
{
    QWidget::hide();
}

void BusyOverlay::resizeEvent(QResizeEvent* event)
{
    if (parentWidget())
        setGeometry(parentWidget()->rect());
    QWidget::resizeEvent(event);
}
