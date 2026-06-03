#include "components/tables/DataTableView.h"
#include "components/feedback/BusyOverlay.h"
#include "components/feedback/EmptyStateWidget.h"
#include <QTableView>
#include <QAbstractItemModel>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QStandardItemModel>

DataTableView::DataTableView(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_table = new QTableView(this);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFrameShape(QFrame::NoFrame);

    layout->addWidget(m_table);

    m_emptyState = new EmptyStateWidget(this);
    m_emptyState->setTitle("No records found");
    m_emptyState->setDescription("Try adjusting your search or filters.");
    m_emptyState->setVisible(false);

    m_busyOverlay = new BusyOverlay(this);

    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        emit rowDoubleClicked(idx.row());
    });
    connect(m_table->selectionModel()
                ? m_table->selectionModel() : nullptr,
            &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& cur, const QModelIndex&) {
                emit selectionChanged(cur.row());
            });
}

void DataTableView::setModel(QAbstractItemModel* model)
{
    m_table->setModel(model);
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& cur, const QModelIndex&) {
                emit selectionChanged(cur.row());
            });
}

void DataTableView::setColumns(const QStringList& headers)
{
    auto* model = qobject_cast<QStandardItemModel*>(m_table->model());
    if (!model) {
        model = new QStandardItemModel(0, headers.size(), this);
        m_table->setModel(model);
    }
    model->setHorizontalHeaderLabels(headers);
}

void DataTableView::setEmptyMessage(const QString& title, const QString& description)
{
    m_emptyState->setTitle(title);
    m_emptyState->setDescription(description);
}

void DataTableView::setCompactMode(bool compact)
{
    const int h = compact ? 28 : 36;
    m_table->verticalHeader()->setDefaultSectionSize(h);
}

void DataTableView::showBusy(bool busy)
{
    if (busy) {
        m_busyOverlay->show();
        m_emptyState->setVisible(false);
    } else {
        m_busyOverlay->hide();
    }
}

void DataTableView::showEmpty(bool empty)
{
    m_emptyState->setVisible(empty);
    m_table->setVisible(!empty);
}

void DataTableView::resizeEvent(QResizeEvent* event)
{
    m_busyOverlay->setGeometry(rect());
    const QRect center(width() / 4, height() / 4, width() / 2, height() / 2);
    m_emptyState->setGeometry(center);
    QWidget::resizeEvent(event);
}
