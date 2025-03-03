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
#include <QListView>
#include <QMimeData>
#include <QStringListModel>
#include <QTableView>
#include <QTreeView>
#include <QVector>
#include <QWidget>
#include "check-index.h"

struct EmailFolder
{
    QString folderName;
    QStringList emails;
};

using EmailFolders = QVector<EmailFolder>;

static const char s_emailsMimeType[] = "application/x-emails-list";

// "Drag" model
class EmailsModel : public QAbstractListModel
{
public:
    // Like a QStringListModel, but with custom mimeData() so we can decode it
    explicit EmailsModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    void setEmails(EmailFolder *folder) {
        beginResetModel();
        m_emailFolder = folder;
        endResetModel();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        CHECK_rowCount(parent);
        if (parent.isValid())
            return 0; // flat model
        return m_emailFolder->emails.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        CHECK_data(index);
        if (!index.isValid() || role != Qt::DisplayRole)
            return QVariant();

        return m_emailFolder->emails.at(index.row());
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        CHECK_flags(index);
        if (!index.isValid())
            return {};
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        // CHECK_headerData(section, orientation); doesn't built, columnCount is private...
        if (orientation == Qt::Horizontal && section == 0)
            return "Emails";
        return {};
    }

    bool removeRows(int position, int rows, const QModelIndex &parent) override
    {
        CHECK_removeRows(position, rows, parent);
        beginRemoveRows(parent, position, position + rows - 1);
        for (int row = 0; row < rows; ++row) {
            m_emailFolder->emails.removeAt(position);
        }
        endRemoveRows();
        return true;
    }

    // the default is "copy only", change it
    Qt::DropActions supportedDragActions() const override { return Qt::MoveAction | Qt::CopyAction; }

    QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        QSet<int> seenRows;
        QByteArray encodedData;
        QDataStream stream(&encodedData, QIODevice::WriteOnly);

        // Serialize source folder name (to detect dropping onto the same folder)
        stream << m_emailFolder->folderName;

        // Serialize email contents
        for (const QModelIndex &index : indexes) {
            const int row = index.row();
            // Note that with QTreeView, this is called for every column => deduplicate
            if (!seenRows.contains(row)) {
                seenRows.insert(row);
                stream << m_emailFolder->emails.at(row);
            }
        }

        QMimeData *mimeData = new QMimeData;
        mimeData->setData(s_emailsMimeType, encodedData);
        return mimeData;
    }

private:
    EmailFolder *m_emailFolder = nullptr;
};

// "Drop" model
class FoldersModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    using QAbstractTableModel::QAbstractTableModel;

    void setEmailFolders(EmailFolders *emailFolders)
    {
        m_emailFolders = emailFolders;
#ifndef QT_NO_DEBUG
        // To catch errors during development
        new QAbstractItemModelTester(this, QAbstractItemModelTester::FailureReportingMode::Fatal, this);
#endif
    }

    enum Columns { Folder, NumEmails, COLUMNCOUNT };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        CHECK_rowCount(parent);
        if (parent.column() > 0)
            return 0;
        if (parent.isValid())
            return 0; // flat model
        return m_emailFolders->size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        CHECK_columnCount(parent);
        Q_UNUSED(parent);
        return COLUMNCOUNT;
    }

    EmailFolder *folderForIndex(const QModelIndex &index) { return &(*m_emailFolders)[index.row()]; }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        CHECK_data(index);
        if (!index.isValid() || role != Qt::DisplayRole)
            return QVariant();

        const auto &folder = m_emailFolders->at(index.row());
        switch (index.column()) {
            case Folder:
                return folder.folderName;
            case NumEmails:
                return folder.emails.size();
            default:
                break;
        }
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        CHECK_flags(index);
        if (!index.isValid())
            return {}; // do not allow dropping between items
        if (index.column() > 0)
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable; // don't drop on other columns
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        CHECK_headerData(section, orientation);
        if (orientation == Qt::Horizontal) {
            switch (section) {
                case Folder:
                    return "Folder Name";
                case NumEmails:
                    return "Count";
                default:
                    break;
            }
        }
        return {};
    }

    // the default is "copy only", change it
    Qt::DropActions supportedDropActions() const override { return Qt::MoveAction | Qt::CopyAction; }

    QStringList mimeTypes() const override { return {QString::fromLatin1(s_emailsMimeType)}; }

    bool dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent) override
    {
        // only drop onto items (just to be safe, given that dropping between items is forbidden by our flags() implementation)
        if (!parent.isValid())
            return false;

        EmailFolder *destFolder = folderForIndex(parent);

        // decode data
        const QByteArray encodedData = mimeData->data(s_emailsMimeType);
        QDataStream stream(encodedData);
        if (stream.atEnd())
            return false;
        QString sourceFolder;
        stream >> sourceFolder;
        // Dropping onto the same folder?
        if (sourceFolder == destFolder->folderName)
            return false;

        while (!stream.atEnd()) {
            QString email;
            stream >> email;
            destFolder->emails.append(email);
        }
        emit dataChanged(parent, parent); // update count

        return true; // let the view handle deletion on the source side by calling removeRows there
    }

private:
    EmailFolders *m_emailFolders = nullptr;
};

enum class ViewType { List, Table, Tree };
class TopLevel : public QWidget
{
public:
    explicit TopLevel(ViewType viewType);

private:
    // Application data
    EmailFolders m_emails = {
        {"Inbox", {"Call your mother", "Customer request", "Urgent", "Spam 1"}},
        {"Customers", {"Old customer"}},
        {"Archive", {"Old email 1", "Old email 2", "Old email 3", "Old email 4"}},
        {"Spam", {"Old spam"}},
        {"To do", {}},
        {"Will never be done", {"Clean the garage"}},
    };

    FoldersModel m_foldersModel;
    EmailsModel m_emailsModel;
};

TopLevel::TopLevel(ViewType viewType) : QWidget(nullptr)
{
    m_foldersModel.setEmailFolders(&m_emails);

    auto layout = new QHBoxLayout(this);

    // Drop side (left)
    const auto setupFoldersView = [&](QAbstractItemView *view) {
        layout->addWidget(view);
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        view->setModel(&m_foldersModel);

        // Note: this takes care of setAcceptDrops(true)
        view->setDragDropMode(QAbstractItemView::DropOnly);
        // Minor improvement: no forbidden cursor when moving the drag between folders
        view->setDragDropOverwriteMode(true);

        connect(view, &QAbstractItemView::clicked, view, [&](const QModelIndex &index) {
            m_emailsModel.setEmails(m_foldersModel.folderForIndex(index));
        });
        m_emailsModel.setEmails(&m_emails[0]);
    };

    // Drag side (right)
    const auto setupEmailsView = [&](QAbstractItemView *view) {
        layout->addWidget(view);
        view->setMaximumWidth(400);
        view->setModel(&m_emailsModel);
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);

        view->setDragDropMode(QAbstractItemView::DragOnly);
        // Don't be confused by the method name, this sets the default action on the drag side
        view->setDefaultDropAction(Qt::MoveAction);
        // When moving, delete the source, don't just clear it (QTableView)
        view->setDragDropOverwriteMode(false);
    };

    // Create the views
    switch (viewType) {
    case ViewType::List: {
        setWindowTitle("Dropping onto QListViews items");
        auto foldersListView = new QListView(this);
        setupFoldersView(foldersListView);

        auto emailsListView = new QListView(this);
        setupEmailsView(emailsListView);
        break;
    }
    case ViewType::Table: {
        setWindowTitle("Dropping onto QTableViews cells");
        auto foldersTableView = new QTableView(this);
        setupFoldersView(foldersTableView);

        auto emailsTableView = new QTableView(this);
        setupEmailsView(emailsTableView);

        foldersTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        foldersTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        emailsTableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
        break;
    }
    case ViewType::Tree: {
        setWindowTitle("Dropping onto QTreeView items");
        auto foldersTreeView = new QTreeView(this);
        setupFoldersView(foldersTreeView);

        auto emailsTreeView = new QTreeView(this);
        setupEmailsView(emailsTreeView);

        foldersTreeView->expandAll();
        foldersTreeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        foldersTreeView->header()->resizeSection(1, 80);
        foldersTreeView->header()->setStretchLastSection(false);
        emailsTreeView->header()->resizeSections(QHeaderView::ResizeToContents);
        break;
    }
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const auto args = QCoreApplication::arguments();
    const QString arg = args.size() > 1 ? args.at(1) : "list";
    ViewType viewType;
    if (arg == "list")
        viewType = ViewType::List;
    else if (arg == "table")
        viewType = ViewType::Table;
    else if (arg == "tree")
        viewType = ViewType::Tree;
    else
        return 1;

    auto topLevel = new TopLevel(viewType);
    topLevel->resize(700, 400);
    topLevel->show();
    topLevel->setAttribute(Qt::WA_DeleteOnClose);

    return app.exec();
}

#include "drop-onto-items-with-model-view.moc"
