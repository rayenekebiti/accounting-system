#pragma once
#include <QStyledItemDelegate>

class RowActionsDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit RowActionsDelegate(QObject* parent = nullptr);

    void setHoveredRow(int row) { m_hoveredRow = row; }
    int  hoveredRow()     const { return m_hoveredRow; }

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override;
    bool editorEvent(QEvent* e, QAbstractItemModel* model,
                     const QStyleOptionViewItem& opt,
                     const QModelIndex& idx) override;

signals:
    void editClicked(int row);
    void deleteClicked(int row);

private:
    int m_hoveredRow = -1;

    static QRect editBtnRect(const QRect& cell);
    static QRect deleteBtnRect(const QRect& cell);
    static void  drawBtn(QPainter* p, const QRect& r, const QString& text, bool danger);
};
