#include "components/tables/DataTableView.h"
#include "components/tables/RowActionsDelegate.h"
#include "components/feedback/BusyOverlay.h"
#include "components/feedback/EmptyStateWidget.h"
#include <QTableView>
#include <QAbstractItemModel>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QStandardItemModel>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QStyle>

// ── Internal delegates ────────────────────────────────────────────────────────

class MoneyDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override
    {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, idx);
        o.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
        QStyledItemDelegate::paint(p, o, idx);
    }
};

class DaysOverDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override
    {
        const int days = idx.data().toString().toInt();
        QString color;
        if      (days <= 30) color = "#BA7B2A";  // amber — caution
        else if (days <= 60) color = "#D06030";  // orange — escalate
        else                 color = "#C83434";  // red — critical

        QStyleOptionViewItem o = opt;
        initStyleOption(&o, idx);
        o.palette.setColor(QPalette::Text, QColor(color));
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &o, p);
    }
};

class StatusBadgeDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override
    {
        // Draw the row chrome WITHOUT text. We call drawControl directly
        // (not QStyledItemDelegate::paint) so the base class can't re-init
        // and re-populate option.text from the model.
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, idx);
        o.text.clear();
        o.features &= ~QStyleOptionViewItem::HasDisplay;
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &o, p);

        const QString status = idx.data().toString();
        if (status.isEmpty()) return;

        struct Badge { QColor bg; QColor fg; };
        static const QMap<QString, Badge> map = {
            {"Draft",    { QColor( 26,111,224, 38), QColor("#5B9BFF") }},
            {"Posted",   { QColor( 38,140, 90, 40), QColor("#3FB174") }},
            {"Paid",     { QColor( 38,140, 90, 50), QColor("#3FB174") }},
            {"Overdue",  { QColor(200, 52, 52, 50), QColor("#E55A5A") }},
            {"Void",     { QColor(200, 52, 52, 38), QColor("#D04545") }},
            {"Active",   { QColor( 38,140, 90, 40), QColor("#3FB174") }},
            {"Inactive", { QColor(107,116,133, 32), QColor("#7B8497") }},
            {"In Stock", { QColor( 38,140, 90, 40), QColor("#3FB174") }},
            {"Low",      { QColor(186,123, 42, 50), QColor("#D69138") }},
            {"Out",      { QColor(200, 52, 52, 50), QColor("#E55A5A") }},
        };

        const Badge badge = map.value(status, { QColor(107,116,133,32), QColor("#7B8497") });

        QFont f("Segoe UI", 9, QFont::DemiBold);
        const QFontMetrics fm(f);
        const int textW = fm.horizontalAdvance(status);
        const int pw    = textW + 16;
        const int ph    = 18;
        const QRect& r  = opt.rect;
        const QRect pill(r.left() + 8, r.top() + (r.height() - ph) / 2, pw, ph);

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(pill), ph / 2.0, ph / 2.0);
        p->fillPath(path, badge.bg);
        p->setPen(badge.fg);
        p->setFont(f);
        p->drawText(pill, Qt::AlignCenter, status);
        p->restore();
    }
};

// ── DataTableView ────────────────────────────────────────────────────────────

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
    m_table->setShowGrid(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(36);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSortIndicatorShown(false);
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
}

void DataTableView::setModel(QAbstractItemModel* model)
{
    m_table->setModel(model);
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& cur, const QModelIndex&) {
                emit selectionChanged(cur.row());
            });
    if (m_rowActionsEnabled && !m_actionsDelegate)
        installActionsDelegate();
}

void DataTableView::setColumns(const QStringList& headers)
{
    auto* model = qobject_cast<QStandardItemModel*>(m_table->model());
    if (!model) {
        model = new QStandardItemModel(0, headers.size(), this);
        m_table->setModel(model);
    }
    model->setHorizontalHeaderLabels(headers);

    if (m_rowActionsEnabled && !m_actionsDelegate)
        installActionsDelegate();
}

void DataTableView::setMoneyColumns(const QList<int>& cols)
{
    for (int col : cols)
        m_table->setItemDelegateForColumn(col, new MoneyDelegate(this));
}

void DataTableView::setStatusColumn(int col)
{
    m_table->setItemDelegateForColumn(col, new StatusBadgeDelegate(this));
}

void DataTableView::setDaysOverColumn(int col)
{
    m_table->setItemDelegateForColumn(col, new DaysOverDelegate(this));
}

void DataTableView::enableRowActions()
{
    m_rowActionsEnabled = true;
    if (m_table->model() && m_table->model()->columnCount() > 0 && !m_actionsDelegate)
        installActionsDelegate();
}

void DataTableView::installActionsDelegate()
{
    const int lastCol = m_table->model()->columnCount() - 1;
    if (lastCol < 0) return;

    m_actionsDelegate = new RowActionsDelegate(this);
    m_table->setItemDelegateForColumn(lastCol, m_actionsDelegate);

    m_table->setMouseTracking(true);
    m_table->viewport()->setMouseTracking(true);
    m_table->viewport()->installEventFilter(this);

    connect(m_table, &QAbstractItemView::entered, this, [this](const QModelIndex& idx) {
        if (m_actionsDelegate->hoveredRow() != idx.row()) {
            m_actionsDelegate->setHoveredRow(idx.row());
            m_table->viewport()->update();
        }
    });
    connect(m_actionsDelegate, &RowActionsDelegate::editClicked,
            this, &DataTableView::rowEditRequested);
    connect(m_actionsDelegate, &RowActionsDelegate::deleteClicked,
            this, &DataTableView::rowDeleteRequested);
}

void DataTableView::setEmptyMessage(const QString& title, const QString& description)
{
    m_emptyState->setTitle(title);
    m_emptyState->setDescription(description);
}

void DataTableView::setCompactMode(bool compact)
{
    m_table->verticalHeader()->setDefaultSectionSize(compact ? 30 : 36);
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

bool DataTableView::eventFilter(QObject* obj, QEvent* event)
{
    if (m_actionsDelegate && obj == m_table->viewport()
        && event->type() == QEvent::Leave) {
        m_actionsDelegate->setHoveredRow(-1);
        m_table->viewport()->update();
    }
    return QWidget::eventFilter(obj, event);
}
