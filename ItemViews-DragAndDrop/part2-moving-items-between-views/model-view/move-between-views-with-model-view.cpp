/*
  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: David Faure <david.faure@kdab.com>

  SPDX-License-Identifier: MIT
*/

#include <QAbstractItemModelTester>
#include <QAbstractTableModel>
#include <QApplication>
#include <QDebug>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIODevice>
#include <QLabel>
#include <QListView>
#include <QMimeData>
#include <QTableView>
#include <QTreeView>
#include <QVector>
#include <QWidget>
#include "check-index.h"

struct CountryData
{
    QString country;
    int population; // in millions
};
QDataStream &operator<<(QDataStream &stream, const CountryData &countryData)
{
    stream << countryData.country << countryData.population;
    return stream;
}
QDataStream &operator>>(QDataStream &stream, CountryData &countryData)
{
    stream >> countryData.country >> countryData.population;
    return stream;
}

static const char s_mimeType[] = "application/x-countrydata";

class CountryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit CountryModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent)
    {
#ifndef QT_NO_DEBUG
        // To catch errors during development
        new QAbstractItemModelTester(this, QAbstractItemModelTester::FailureReportingMode::Fatal, this);
#endif
    }

    enum Columns { Country, Population, COLUMNCOUNT };

    // Set the data for the model
    void setCountryData(const QVector<CountryData> &data)
    {
        beginResetModel();
        m_data = data;
        endResetModel();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        CHECK_rowCount(parent);
        if (parent.isValid())
            return 0; // flat model
        return m_data.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        CHECK_columnCount(parent);
        Q_UNUSED(parent);
        return COLUMNCOUNT;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        CHECK_data(index);
        if (!index.isValid() || role != Qt::DisplayRole)
            return QVariant();

        const CountryData &item = m_data.at(index.row());
        switch (index.column()) {
        case Columns::Country:
            return item.country;
        case Columns::Population:
            return item.population;
        default:
            break;
        }

        return QVariant();
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        CHECK_flags(index);
        if (!index.isValid())
            return Qt::ItemIsDropEnabled; // allow dropping between items
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled; // note: not ItemIsDropEnabled!
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        CHECK_headerData(section, orientation);
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
            if (section == 0)
                return QString("Country");
            else if (section == 1)
                return QString("Population (millions)");
        }
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    // the default is "copy only", change it
    Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }

    // the default is "return supportedDropActions()", let's be explicit
    Qt::DropActions supportedDragActions() const override { return Qt::MoveAction; }

    QStringList mimeTypes() const override { return {QString::fromLatin1(s_mimeType)}; }

    QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        // Serialize the actual data
        // If you only care for same-process DnD, a pointer to the underlying data would be enough
        // (see part1's treemodel for a sample implementation of that technique)

        QSet<int> seenRows;
        QByteArray encodedData;
        QDataStream stream(&encodedData, QIODevice::WriteOnly);
        for (const QModelIndex &index : indexes) {
            const int row = index.row();
            // Note that with QTreeView, this is called for every column => deduplicate
            if (!seenRows.contains(row)) {
                seenRows.insert(row);
                stream << m_data.at(row);
            }
        }

        QMimeData *mimeData = new QMimeData;
        mimeData->setData(s_mimeType, encodedData);
        return mimeData;
    }

    bool dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent) override
    {
        // qDebug() << "dropMimeData:" << mimeData->formats() << action << row << column << parent;
        // check if the format is supported
        if (!mimeData->hasFormat(s_mimeType))
            return false;
        // only drop between items (just to be safe, given that dropping onto items is forbidden by our flags() implementation)
        if (parent.isValid() && row == -1)
            return false;
        // drop into empty area = append
        if (row == -1)
            row = rowCount(parent);

        // decode data
        const QByteArray encodedData = mimeData->data(s_mimeType);
        QDataStream stream(encodedData);
        if (stream.atEnd())
            return false;

        QVector<CountryData> newCountries;
        while (!stream.atEnd()) {
            CountryData countryData;
            stream >> countryData;
            newCountries.append(countryData);
        }

        // insert new countries
        beginInsertRows(parent, row, row + newCountries.count() - 1);
        for (const CountryData &countryData : newCountries)
            m_data.insert(row++, countryData);
        endInsertRows();

        return true; // let the view handle deletion on the source side by calling removeRows there
    }

    bool removeRows(int position, int rows, const QModelIndex &parent) override
    {
        CHECK_removeRows(position, rows, parent);
        beginRemoveRows(parent, position, position + rows - 1);
        for (int row = 0; row < rows; ++row) {
            m_data.removeAt(position);
        }
        endRemoveRows();
        return true;
    }

private:
    QVector<CountryData> m_data;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CountryModel model1;
    CountryModel model2;

    const QVector<CountryData> data1 = {
        {"USA", 331}, {"China", 1439}, {"India", 1380}, {"Brazil", 213}, {"France", 67},
    };
    model1.setCountryData(data1);
    const QVector<CountryData> data2 = {
        {"Spain", 56},
    };
    model2.setCountryData(data2);
    auto topLevel = new QWidget(nullptr);
    auto layout = new QHBoxLayout(topLevel);

    const auto setupView = [&](QAbstractItemView *view, const QString &title) {
        auto vLayout = new QVBoxLayout;
        layout->addLayout(vLayout);
        vLayout->addWidget(new QLabel(title, topLevel));
        vLayout->addWidget(view);
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);

        view->setDefaultDropAction(Qt::MoveAction);
        // Note: this takes care of setDragEnabled(true) + setAcceptDrops(true)
        view->setDragDropMode(QAbstractItemView::DragDrop);
    };

    // Create the views
    QAbstractItemView *view = nullptr;
    const auto args = QCoreApplication::arguments();
    const QString viewType = args.size() > 1 ? args.at(1) : "list";
    if (viewType == "list") {
        topLevel->setWindowTitle("Moving between QListViews");
        auto listView1 = new QListView(topLevel);
        setupView(listView1, "Available");
        listView1->setModel(&model1);
        auto listView2 = new QListView(topLevel);
        setupView(listView2, "Selected");
        listView2->setModel(&model2);
    } else if (viewType == "table") {
        topLevel->setWindowTitle("Moving between QTableViews");
        auto tableView1 = new QTableView;
        setupView(tableView1, "Available");
        tableView1->setModel(&model1);
        auto tableView2 = new QTableView;
        setupView(tableView2, "Selected");
        tableView2->setModel(&model2);

        tableView1->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
        tableView2->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

        // Ensure QTableView calls removeRows when moving rows
        tableView1->setDragDropOverwriteMode(false);
        tableView2->setDragDropOverwriteMode(false);

    } else if (viewType == "tree") {
        topLevel->setWindowTitle("Moving between QTreeViews");
        auto treeView1 = new QTreeView;
        setupView(treeView1, "Available");
        treeView1->setModel(&model1);
        auto treeView2 = new QTreeView;
        setupView(treeView2, "Selected");
        treeView2->setModel(&model2);

        treeView1->header()->resizeSections(QHeaderView::ResizeToContents);
        treeView2->header()->resizeSections(QHeaderView::ResizeToContents);
    } else {
        return 1;
    }

    topLevel->resize(700, 400);
    topLevel->show();
    topLevel->setAttribute(Qt::WA_DeleteOnClose);

    return app.exec();
}

#include "move-between-views-with-model-view.moc"
