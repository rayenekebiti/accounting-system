#include "components/tables/RowActionsDelegate.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFont>

RowActionsDelegate::RowActionsDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QRect RowActionsDelegate::editBtnRect(const QRect& cell)
{
    return { cell.right() - 104, cell.top() + (cell.height() - 22) / 2, 48, 22 };
}

QRect RowActionsDelegate::deleteBtnRect(const QRect& cell)
{
    return { cell.right() - 52,  cell.top() + (cell.height() - 22) / 2, 48, 22 };
}

void RowActionsDelegate::drawBtn(QPainter* p, const QRect& r, const QString& text, bool danger)
{
    p->save();
    const QColor bg = danger ? QColor(224, 85, 85, 170) : QColor(212, 96, 58, 160);
    QPainterPath path;
    path.addRoundedRect(QRectF(r), 4, 4);
    p->fillPath(path, bg);
    p->setPen(Qt::white);
    p->setFont(QFont("Segoe UI", 9, QFont::Medium));
    p->drawText(r, Qt::AlignCenter, text);
    p->restore();
}

void RowActionsDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt,
                               const QModelIndex& idx) const
{
    QStyledItemDelegate::paint(p, opt, idx);

    if (idx.row() != m_hoveredRow) return;

    const QRect& r = opt.rect;
    if (r.width() < 115) return;

    // Fade right edge of cell so buttons read cleanly
    const int fadeW = 115;
    QColor bg = opt.palette.base().color();
    if (opt.state & QStyle::State_Selected)
        bg = opt.palette.highlight().color();

    QLinearGradient fade(r.right() - fadeW, 0, r.right(), 0);
    fade.setColorAt(0.0, QColor(bg.red(), bg.green(), bg.blue(), 0));
    fade.setColorAt(0.45, QColor(bg.red(), bg.green(), bg.blue(), 210));
    fade.setColorAt(1.0,  QColor(bg.red(), bg.green(), bg.blue(), 255));
    p->fillRect(QRect(r.right() - fadeW, r.top(), fadeW, r.height()), fade);

    drawBtn(p, editBtnRect(r),   "Edit",   false);
    drawBtn(p, deleteBtnRect(r), "Delete", true);
}

bool RowActionsDelegate::editorEvent(QEvent* e, QAbstractItemModel*,
                                      const QStyleOptionViewItem& opt,
                                      const QModelIndex& idx)
{
    if (idx.row() != m_hoveredRow) return false;
    if (e->type() != QEvent::MouseButtonRelease) return false;

    auto* me = static_cast<QMouseEvent*>(e);
    if (me->button() != Qt::LeftButton) return false;

    if (editBtnRect(opt.rect).contains(me->pos())) {
        emit editClicked(idx.row());
        return true;
    }
    if (deleteBtnRect(opt.rect).contains(me->pos())) {
        emit deleteClicked(idx.row());
        return true;
    }
    return false;
}
