/*
  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: David Faure <david.faure@kdab.com>

  SPDX-License-Identifier: MIT
*/

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QLabel>
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
    TopLevelWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto mainLayout = new QVBoxLayout(this);
        m_layout = new QHBoxLayout;
        mainLayout->addLayout(m_layout);
        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &TopLevelWidget::okClicked);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &TopLevelWidget::close);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &TopLevelWidget::close);
        mainLayout->addWidget(buttonBox);
    }

    void addView(QAbstractItemView *view, const QString &title) {
        auto vLayout = new QVBoxLayout;
        m_layout->addLayout(vLayout);
        vLayout->addWidget(new QLabel(title, this));
        vLayout->addWidget(view);

        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);

        view->setDefaultDropAction(Qt::MoveAction);
        // Note: this takes care of setDragEnabled(true) + setAcceptDrops(true)
        view->setDragDropMode(QAbstractItemView::DragDrop);
    }

signals:
    void okClicked();

private:
    QHBoxLayout *m_layout;
};

class MoveOnlyListWidget : public QListWidget
{
public:
    using QListWidget::QListWidget;

protected:
     Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }
};

class MoveOnlyTableWidget : public QTableWidget
{
public:
    using QTableWidget::QTableWidget;

protected:
     Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QVector<CountryData> availableData = {
        {"USA", 331}, {"China", 1439}, {"India", 1380}, {"Brazil", 213}, {"France", 67},  {"Spain", 56},
    };
    QVector<CountryData> selectedData;
    selectedData.reserve(availableData.size());

    auto topLevel = new TopLevelWidget(nullptr);
    const auto args = QCoreApplication::arguments();
    const QString viewType = args.size() > 1 ? args.at(1) : "list";
    if (viewType == "list") {
        topLevel->setWindowTitle("Moving between QListWidgets");
        auto listWidget1 = new MoveOnlyListWidget;
        topLevel->addView(listWidget1, "Available");
        auto listWidget2 = new MoveOnlyListWidget;
        topLevel->addView(listWidget2, "Selected");

        for (const CountryData &countryData : availableData) {
            QListWidgetItem *item = new QListWidgetItem(countryData.country);
            listWidget1->addItem(item);
        }

        QObject::connect(topLevel, &TopLevelWidget::okClicked, [&]() {
            // Use the new order - here we just print it out
            for (int i = 0; i < listWidget2->count(); ++i) {
                qDebug() << listWidget2->item(i)->text();
            }
        });

    } else if (viewType == "table") {
        topLevel->setWindowTitle("Moving between QTableWidgets");
        auto tableWidget1 = new MoveOnlyTableWidget;
        tableWidget1->setRowCount(availableData.count());
        tableWidget1->setColumnCount(2);
        tableWidget1->setHorizontalHeaderLabels({"Country", "Population (millions)"});
        topLevel->addView(tableWidget1, "Available");

        auto tableWidget2 = new MoveOnlyTableWidget;
        tableWidget2->setColumnCount(2);
        tableWidget2->setHorizontalHeaderLabels({"Country", "Population (millions)"});
        topLevel->addView(tableWidget2, "Selected");

        // We don't want Excel-like overwriting of data on drop, we want to move rows
        tableWidget1->setDragDropOverwriteMode(false);
        tableWidget2->setDragDropOverwriteMode(false);

        auto setupTableItem = [](QTableWidgetItem *item) { item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled); };

        for (int row = 0; row < availableData.count(); ++row) {
            const CountryData &countryData = availableData.at(row);
            auto countryItem = new QTableWidgetItem(countryData.country);
            setupTableItem(countryItem);
            tableWidget1->setItem(row, 0, countryItem);
            auto populationItem = new QTableWidgetItem;
            setupTableItem(populationItem);
            populationItem->setData(Qt::DisplayRole, countryData.population);
            tableWidget1->setItem(row, 1, populationItem);

            // Not supported for DnD
            // auto headerItem = new QTableWidgetItem();
            // headerItem->setText(QStringLiteral("Country %1").arg(row + 1));
            // tableWidget1->setVerticalHeaderItem(row, headerItem);
        }

        auto prototype = new QTableWidgetItem;
        setupTableItem(prototype);
        // After a drop, QTableWidget creates new items, ensure they don't support dropping onto them
        tableWidget2->setItemPrototype(prototype); // takes ownership

        // Same for tableWidget1, when dropping back to the left
        auto prototype1 = new QTableWidgetItem;
        setupTableItem(prototype1);
        tableWidget1->setItemPrototype(prototype1); // takes ownership

        QObject::connect(topLevel, &TopLevelWidget::okClicked, [&]() {
            // Use the new order - here we just print it out
            for (int i = 0; i < tableWidget2->rowCount(); ++i) {
                qDebug() << tableWidget2->item(i, 0)->text();
            }
        });

    } else if (viewType == "tree") {
        topLevel->setWindowTitle("Moving between QTreeWidgets");
        auto treeWidget1 = new QTreeWidget;
        treeWidget1->setColumnCount(2);
        treeWidget1->setHeaderLabels({"Country", "Population (millions)"});
        topLevel->addView(treeWidget1, "Available");
        auto treeWidget2 = new QTreeWidget;
        treeWidget2->setHeaderLabels({"Country", "Population (millions)"});
        topLevel->addView(treeWidget2, "Selected");

        for (const CountryData &countryData : availableData) {
            auto item = new QTreeWidgetItem;
            item->setData(0, Qt::DisplayRole, countryData.country);
            item->setData(1, Qt::DisplayRole, countryData.population);
            // Not supported: the flag would be set on items created by dropping -- not configurable
            // QTreeWidgetItem doesn't have a prototype feature like QTableWidgetItem
            //item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled); // don't drop onto items
            treeWidget1->addTopLevelItem(item);
        }

        QObject::connect(topLevel, &TopLevelWidget::okClicked, [&]() {
            // Use the new order - here we just print it out
            for (int i = 0; i < treeWidget1->topLevelItemCount(); ++i) {
                qDebug() << treeWidget1->topLevelItem(i)->text(0);
            }
        });
    } else {
        return 1;
    }

    topLevel->resize(700, 400);
    topLevel->show();
    topLevel->setAttribute(Qt::WA_DeleteOnClose);

    return app.exec();
}

#include "move-between-views-with-itemwidgets.moc"
