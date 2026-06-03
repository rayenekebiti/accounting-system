#pragma once
#include <QWidget>
#include <QList>
#include <QString>

class QComboBox;
class QHBoxLayout;

class FilterBar : public QWidget {
    Q_OBJECT
public:
    explicit FilterBar(QWidget* parent = nullptr);

    void addFilter(const QString& label, const QStringList& options);
    QString filterValue(int index) const;
    void    reset();

signals:
    void filterChanged();

private:
    QHBoxLayout*        m_layout;
    QList<QComboBox*>   m_combos;
};
