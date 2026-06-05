#pragma once
#include <QWidget>
#include <QStringList>
#include <QList>

class QTableView;
class QAbstractItemModel;
class BusyOverlay;
class EmptyStateWidget;
class RowActionsDelegate;

class DataTableView : public QWidget {
    Q_OBJECT
public:
    explicit DataTableView(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model);
    void setColumns(const QStringList& headers);
    void setEmptyMessage(const QString& title, const QString& description = {});
    void setCompactMode(bool compact);
    void setMoneyColumns(const QList<int>& cols);
    void setStatusColumn(int col);
    void setDaysOverColumn(int col);
    void enableRowActions();

    void showBusy(bool busy);
    void showEmpty(bool empty);

    QTableView* tableView() const { return m_table; }

signals:
    void rowDoubleClicked(int row);
    void selectionChanged(int selectedRow);
    void rowEditRequested(int row);
    void rowDeleteRequested(int row);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void installActionsDelegate();

    QTableView*         m_table;
    BusyOverlay*        m_busyOverlay;
    EmptyStateWidget*   m_emptyState;
    RowActionsDelegate* m_actionsDelegate  = nullptr;
    bool                m_rowActionsEnabled = false;
};
