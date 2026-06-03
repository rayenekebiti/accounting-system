#pragma once
#include <QWidget>
#include <QStringList>

class QTableView;
class QAbstractItemModel;
class BusyOverlay;
class EmptyStateWidget;

class DataTableView : public QWidget {
    Q_OBJECT
public:
    explicit DataTableView(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model);
    void setColumns(const QStringList& headers);
    void setEmptyMessage(const QString& title, const QString& description = {});
    void setCompactMode(bool compact);

    void showBusy(bool busy);
    void showEmpty(bool empty);

    QTableView* tableView() const { return m_table; }

signals:
    void rowDoubleClicked(int row);
    void selectionChanged(int selectedRow);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QTableView*       m_table;
    BusyOverlay*      m_busyOverlay;
    EmptyStateWidget* m_emptyState;
};
