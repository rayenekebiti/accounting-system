#ifndef QUICK_APP_CONTROLLER_H
#define QUICK_APP_CONTROLLER_H

#include <QObject>
#include <QString>
#include "InvoiceListModel.h"

class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int     invoiceCount          READ invoiceCount          NOTIFY changed)
    Q_PROPERTY(QString totalReceivablesText  READ totalReceivablesText  NOTIFY changed)
    Q_PROPERTY(QString dataPath              READ dataPath              CONSTANT)
    Q_PROPERTY(bool    rtl                   READ rtl                   WRITE setRtl  NOTIFY rtlChanged)

public:
    explicit AppController(const QString& dataPath, InvoiceListModel* model, QObject* parent = nullptr);

    int     invoiceCount()         const;
    QString totalReceivablesText() const;
    QString dataPath()             const { return dataPath_; }
    bool    rtl()                  const { return rtl_; }
    void    setRtl(bool v);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void toggleRtl();

signals:
    void changed();
    void rtlChanged();

private:
    void recomputeKpis();

    InvoiceListModel* model_;
    QString           dataPath_;
    bool              rtl_        = false;
    int               invoiceCount_        = 0;
    double            totalReceivables_    = 0.0;
};

#endif // QUICK_APP_CONTROLLER_H
