cmake_minimum_required(VERSION 3.16)
project(AccountingSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(UI_SOURCES
    src/ui/theme/ThemeManager.cpp
    src/ui/app/MainWindow.cpp
    src/ui/app/NavigationPanel.cpp
    src/ui/app/GlobalToolBar.cpp
    src/ui/app/PageHeader.cpp
    src/ui/app/PageRouter.cpp
    src/ui/pages/base/Page.cpp
    src/ui/pages/base/ListPage.cpp
    src/ui/pages/dashboard/DashboardPage.cpp
    src/ui/pages/customers/CustomersPage.cpp
    src/ui/pages/suppliers/SuppliersPage.cpp
    src/ui/pages/products/ProductsPage.cpp
    src/ui/pages/invoices/InvoicesPage.cpp
    src/ui/pages/payments/PaymentsPage.cpp
    src/ui/pages/reports/ReportsPage.cpp
    src/ui/pages/settings/SettingsPage.cpp
    src/ui/components/tables/DataTableView.cpp
    src/ui/components/tables/PaginationFooter.cpp
    src/ui/components/inputs/SearchBar.cpp
    src/ui/components/inputs/FilterBar.cpp
    src/ui/components/forms/FormRow.cpp
    src/ui/components/forms/SectionHeader.cpp
    src/ui/components/feedback/BusyOverlay.cpp
    src/ui/components/feedback/EmptyStateWidget.cpp
    src/ui/components/feedback/ConfirmDialog.cpp
    src/ui/components/display/KpiCard.cpp
    src/ui/components/display/StatusBadge.cpp
    src/ui/components/display/MoneyLabel.cpp
)

add_executable(AccountingSystem main.cpp ${UI_SOURCES})

target_include_directories(AccountingSystem PRIVATE src/ui)

target_link_libraries(AccountingSystem PRIVATE Qt6::Widgets)

if(WIN32)
    set_target_properties(AccountingSystem PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
