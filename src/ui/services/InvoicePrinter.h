#pragma once
class Invoice;
class Customer;
class QWidget;
class QString;

class InvoicePrinter {
public:
    static void print    (const Invoice& inv, const Customer& cust, QWidget* parent = nullptr);
    static void exportPdf(const Invoice& inv, const Customer& cust, QWidget* parent = nullptr);

private:
    static QString buildHtml(const Invoice& inv, const Customer& cust);
};
