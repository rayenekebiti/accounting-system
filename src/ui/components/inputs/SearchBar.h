#pragma once
#include <QWidget>

class QLineEdit;
class QToolButton;
class QTimer;

class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(const QString& placeholder = "Search...", QWidget* parent = nullptr);

    QString text() const;
    void    clear();
    void    setDebounceMs(int ms);

signals:
    void searchChanged(const QString& text);

private slots:
    void onTextChanged(const QString& text);
    void emitSearch();

private:
    QLineEdit*   m_edit;
    QToolButton* m_clearBtn;
    QTimer*      m_debounce;
};
