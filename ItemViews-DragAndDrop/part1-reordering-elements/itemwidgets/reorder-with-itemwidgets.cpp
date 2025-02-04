/*
  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: David Faure <david.faure@kdab.com>

  SPDX-License-Identifier: MIT
*/

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

struct CountryData
{
    QString country;
    int population; // in millions
};

class TopLevelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TopLevelWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        m_layout = new QVBoxLayout(this);
        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &TopLevelWidget::okClicked);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &TopLevelWidget::close);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &TopLevelWidget::close);
        m_layout->addWidget(buttonBox);
    }
    void setView(QAbstractItemView *view) { m_layout->insertWidget(0, view); }

signals:
    void okClicked();

private:
    QVBoxLayout *m_layout;
};

#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
// QTableWidget::dropEvent implements InternalMove by moving data into cells (like Excel), not with insertion/removal
// So we want the default implementation from QAbstractItemView::dropEvent instead:
// it calls QTableWidget::dropMimeData which does the insertion, and setDragDropOverwriteMode(false) takes care of the removal after QDrag::exec() returns
// BUG: it loses header items though
//
// I fixed this bug in Qt itself: https://codereview.qt-project.org/c/qt/qtbase/+/582308 and https://codereview.qt-project.org/c/qt/qtbase/+/581090

class ReorderableTableWidget : public QTableWidget
{
public:
    using QTableWidget::QTableWidget;

protected:
    void dropEvent(QDropEvent *event) override { QAbstractItemView::dropEvent(event); }
};
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QVector<CountryData> data = {
        {"USA", 331}, {"China", 1439}, {"India", 1380}, {"Brazil", 213}, {"France", 67},
    };

    // DND CODE START

    // don't drop onto items
    auto setupTableItem = [](QTableWidgetItem *item) { item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled); };
    auto setupTreeItem = [](QTreeWidgetItem *item) { item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled); };

    auto setupWidgetForReorderingDnD = [](QAbstractItemView *view) 
    {
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);

        // Note: this takes care of setDragEnabled(true) + setAcceptDrops(true)
        // Also: InternalMove disables moving between different views, we don't need to test that
        view->setDragDropMode(QAbstractItemView::InternalMove);
    };

    auto setupTableWidgetForReorderingDnD = [&](QTableWidget *tableWidget)
    {
        setupWidgetForReorderingDnD(tableWidget);

        // We don't want Excel-like overwriting of data on drop, we want to move rows
        tableWidget->setDragDropOverwriteMode(false);

#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
        auto prototype = new QTableWidgetItem;
        setupTableItem(prototype);
        // After a drop, QTableWidget creates new items, ensure they don't support dropping onto them
        tableWidget->setItemPrototype(prototype); // takes ownership
#endif
    };

    // DND CODE END

    auto topLevel = new TopLevelWidget(nullptr);
    const auto args = QCoreApplication::arguments();
    const QString viewType = args.size() > 1 ? args.at(1) : "list";
    if (viewType == "list") {
        auto listWidget = new QListWidget;
        topLevel->setView(listWidget);
        topLevel->setWindowTitle("Reorderable QListWidget");

        for (const CountryData &countryData : data) {
            QListWidgetItem *item = new QListWidgetItem(countryData.country);
            listWidget->addItem(item);
        }

        QObject::connect(topLevel, &TopLevelWidget::okClicked, [&]() {
            // Use the new order - here we just print it out
            for (int i = 0; i < listWidget->count(); ++i) {
                qDebug() << listWidget->item(i)->text();
            }
        });

        setupWidgetForReorderingDnD(listWidget);

    } else if (viewType == "table") {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        qDebug() << "using upstream QTableWidget";
        auto tableWidget = new QTableWidget;
#else
        qDebug() << "using hackish ReorderableTableWidget";
        auto tableWidget = new ReorderableTableWidget;
#endif
        topLevel->setView(tableWidget);
        topLevel->setWindowTitle("Reorderable QTableWidget");
        tableWidget->setRowCount(data.count());
        tableWidget->setColumnCount(2);
        tableWidget->setHorizontalHeaderLabels({"Country", "Population (millions)"});

        for (int row = 0; row < data.count(); ++row) {
            const CountryData &countryData = data.at(row);
            auto countryItem = new QTableWidgetItem(countryData.country);
            setupTableItem(countryItem);
            tableWidget->setItem(row, 0, countryItem);
            auto populationItem = new QTableWidgetItem;
            setupTableItem(populationItem);
            populationItem->setData(Qt::DisplayRole, countryData.population);
            tableWidget->setItem(row, 1, populationItem);

            auto headerItem = new QTableWidgetItem();
            headerItem->setText(QStringLiteral("Country %1").arg(row + 1));
            tableWidget->setVerticalHeaderItem(row, headerItem);
        }
        setupTableWidgetForReorderingDnD(tableWidget);

        QObject::connect(topLevel, &TopLevelWidget::okClicked, [&]() {
            // Use the new order - here we just print it out
            for (int i = 0; i < tableWidget->rowCount(); ++i) {
                qDebug() << tableWidget->item(i, 0)->text();
            }
        });

    } else if (viewType == "tree") {
        auto treeWidget = new QTreeWidget;
        topLevel->setView(treeWidget);
        topLevel->setWindowTitle("Reorderable QTreeWidget");
        treeWidget->setColumnCount(2);
        treeWidget->setHeaderLabels({"Country", "Population (millions)"});

        for (const CountryData &countryData : data) {
            auto item = new QTreeWidgetItem;
            item->setData(0, Qt::DisplayRole, countryData.country);
            item->setData(1, Qt::DisplayRole, countryData.population);
            setupTreeItem(item);
            treeWidget->addTopLevelItem(item);
        }

        setupWidgetForReorderingDnD(treeWidget);

        QObject::connect(topLevel, &TopLevelWidget::okClicked, [&]() {
            // Use the new order - here we just print it out
            for (int i = 0; i < treeWidget->topLevelItemCount(); ++i) {
                qDebug() << treeWidget->topLevelItem(i)->text(0);
            }
        });

    } else {
        return 1;
    }

    topLevel->resize(300, 400);
    topLevel->show();
    topLevel->setAttribute(Qt::WA_DeleteOnClose);

    return app.exec();
}

#include "reorder-with-itemwidgets.moc"
