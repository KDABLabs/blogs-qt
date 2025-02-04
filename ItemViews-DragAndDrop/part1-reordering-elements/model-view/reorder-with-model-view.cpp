/*
  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: David Faure <david.faure@kdab.com>

  SPDX-License-Identifier: MIT
*/

#include <QAbstractItemModelTester>
#include <QAbstractTableModel>
#include <QApplication>
#include <QDebug>
#include <QHeaderView>
#include <QIODevice>
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

static const char s_mimeType[] = "application/x-countrydata-rownumber";

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

    // Only needed if you also implement dropMimeData
    // (not needed for QListView or -- when using Qt >= 6.8.0 -- for QTableView)
    QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        // Since we're *only* doing internal moves for now, we don't need to copy the complete data
        // All we need is row indexes

        QSet<int> seenRows;
        QByteArray encodedData;
        QDataStream stream(&encodedData, QIODevice::WriteOnly);
        for (const QModelIndex &index : indexes) {
            // Note that with QTreeView, this is called for every column => use a QSet to deduplicate
            seenRows.insert(index.row());
        }
        stream << seenRows;

        QMimeData *mimeData = new QMimeData;
        mimeData->setData(s_mimeType, encodedData);
        return mimeData;
    }

    // Since Qt 5.15.1, if you only care about QListView, you don't need to reimplement
    // dropMimeData, only moveRows(). This is because QListView::dropEvent() takes care of calling
    // moveRow[s]().

    // I implemented something similar in Qt 6.8.0 for QTableView.
    // https://codereview.qt-project.org/c/qt/qtbase/+/581090
    // So, all in all, with a recent Qt, you only need this dropMimeData() for QTreeView.

    // The default implementation in QAbstractTableModel::dropMimeData, when dropping between items,
    // is to encode the data with itemData(), decode it at destination, insert empty rows, and then
    // fill them in. This might be good enough for some use cases, but it forces modeling empty data
    // that gets fill in later (which doesn't always make sense), and it's much slower than just
    // moving the data on our own.
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

        QSet<int> rowsList;
        stream >> rowsList;
        if (rowsList.isEmpty())
            return false;

        // this assumes the selection is contiguous
        // See QListView::dropEvent for the complete code showing how to handle non-contiguous
        // selections (using QPersistentModelIndex so pending row numbers get updated)
        const int firstRow = *rowsList.begin();
        moveRows(parent, firstRow, rowsList.count(), parent, row);

        return false; // we handled the move, not just the insertion, so don't let the caller do
                      // the removal of the source rows
    }

    // Note that only QListView::dropEvent (and QTableView since Qt 6.8) calls moveRows() (when using InternalMove).
    // For other views, our own dropMimeData() here does it.
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild) override
    {
        CHECK_moveRows(sourceParent, sourceRow, count, destinationParent, destinationChild);
        // qDebug() << "moveRows" << sourceRow << count << "->" << destinationChild;

        if (!beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent, destinationChild))
            return false; // invalid move, e.g. no-op (move row 2 to row 2, or move row 2 to row 3)

        for (int i = 0; i < count; ++i) {
            m_data.move(sourceRow + i, destinationChild + (sourceRow > destinationChild ? 0 : -1));
        }

        endMoveRows();
        return true;
    }

private:
    QVector<CountryData> m_data;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CountryModel model;

    const QVector<CountryData> data = {
        {"USA", 331}, {"China", 1439}, {"India", 1380}, {"Brazil", 213}, {"France", 67},
    };
    model.setCountryData(data);

    // Create the view
    QAbstractItemView *view = nullptr;
    const auto args = QCoreApplication::arguments();
    const QString viewType = args.size() > 1 ? args.at(1) : "list";
    if (viewType == "list") {
        auto listView = new QListView;
        listView->setWindowTitle("Reorderable QListView");
        view = listView;
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    } else if (viewType == "table") {
        auto tableView = new QTableView;
        tableView->setWindowTitle("Reorderable QTableView");
        tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        view = tableView;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
#else
        view->setSelectionMode(QAbstractItemView::ContiguousSelection); // our dropMimeData is kinda limited
#endif
    } else if (viewType == "tree") {
        auto treeView = new QTreeView;
        treeView->setWindowTitle("Reorderable QTreeView");
        treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        view = treeView;
        view->setSelectionMode(QAbstractItemView::ContiguousSelection); // our dropMimeData is kinda limited
    } else {
        return 1;
    }

    view->setModel(&model);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Note: this takes care of setDragEnabled(true) + setAcceptDrops(true)
    // Also: InternalMove disables moving between different views, we don't need to test that
    view->setDragDropMode(QAbstractItemView::InternalMove);

    view->resize(300, 400);
    view->show();
    view->setAttribute(Qt::WA_DeleteOnClose);

    return app.exec();
}

#include "reorder-with-model-view.moc"
