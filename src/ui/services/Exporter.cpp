#include "services/Exporter.h"
#include <QAbstractItemModel>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QWidget>

bool Exporter::toCsv(QAbstractItemModel* model, QWidget* parent)
{
    const QString path = QFileDialog::getSaveFileName(
        parent, "Export to CSV", QString(), "CSV Files (*.csv)");
    if (path.isEmpty()) return false;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(parent, "Export Error", "Cannot write to: " + path);
        return false;
    }

    QTextStream out(&f);
    const int cols = model->columnCount();
    const int rows = model->rowCount();

    QStringList hdr;
    for (int c = 0; c < cols; ++c)
        hdr << model->headerData(c, Qt::Horizontal).toString();
    out << hdr.join(",") << "\n";

    for (int r = 0; r < rows; ++r) {
        QStringList row;
        for (int c = 0; c < cols; ++c) {
            QString cell = model->index(r, c).data().toString();
            if (cell.contains(',') || cell.contains('"') || cell.contains('\n'))
                cell = "\"" + cell.replace("\"", "\"\"") + "\"";
            row << cell;
        }
        out << row.join(",") << "\n";
    }

    return true;
}
