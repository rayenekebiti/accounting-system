#pragma once
class QAbstractItemModel;
class QWidget;

class Exporter {
public:
    static bool toCsv(QAbstractItemModel* model, QWidget* parent = nullptr);
};
