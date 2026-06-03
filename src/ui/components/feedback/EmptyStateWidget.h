#pragma once
#include <QWidget>

class QLabel;
class QPushButton;

class EmptyStateWidget : public QWidget {
    Q_OBJECT
public:
    explicit EmptyStateWidget(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setDescription(const QString& description);
    void setActionText(const QString& text);

signals:
    void actionClicked();

private:
    QLabel*      m_iconLabel;
    QLabel*      m_titleLabel;
    QLabel*      m_descLabel;
    QPushButton* m_actionBtn;
};
