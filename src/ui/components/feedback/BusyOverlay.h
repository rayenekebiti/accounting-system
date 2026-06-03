#pragma once
#include <QWidget>

class QLabel;

class BusyOverlay : public QWidget {
    Q_OBJECT
public:
    explicit BusyOverlay(QWidget* parent = nullptr);

    void show(const QString& message = "Loading...");
    void hide();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QLabel* m_messageLabel;
};
