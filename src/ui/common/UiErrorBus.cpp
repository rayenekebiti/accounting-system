#include "common/UiErrorBus.h"
#include <QMessageBox>

void UiErrorBus::error(const QString& title, const QString& msg, QWidget* parent)
{
    QMessageBox::critical(parent, title, msg);
}
