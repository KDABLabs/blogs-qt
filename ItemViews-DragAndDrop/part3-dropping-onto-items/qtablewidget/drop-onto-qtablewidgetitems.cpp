/*
  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: David Faure <david.faure@kdab.com>

  SPDX-License-Identifier: MIT
*/

#include <QApplication>
#include <QDebug>
#include <QIODevice>
#include <QHeaderView>
#include <QMimeData>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

struct EmailFolder
{
    QString folderName;
    QStringList emails;
};
Q_DECLARE_METATYPE(EmailFolder *); // for Qt5, not needed with Qt6 (but doesn't hurt)

using EmailFolders = QVector<EmailFolder>;

static const char s_emailsMimeType[] = "application/x-emails-list";

class FoldersTableWidget : public QTableWidget
{
public:
    using QTableWidget::QTableWidget;

    void setEmailFolders(EmailFolders *folders);

protected:
    void updateCounts();
    QStringList mimeTypes() const override { return {QString::fromLatin1(s_emailsMimeType)}; }
    bool dropMimeData(int row, int column, const QMimeData *mimeData, Qt::DropAction action) override;

private:
    EmailFolders *m_folders = nullptr;
};

void FoldersTableWidget::setEmailFolders(EmailFolders *folders)
{
    m_folders = folders; 
    setRowCount(m_folders->size());
    setColumnCount(2);
    setHorizontalHeaderLabels({"Folder", "Count"});
    verticalHeader()->hide();
    for (int row = 0; row < m_folders->size(); ++row) {
        EmailFolder &folder = (*m_folders)[row];
        auto item = new QTableWidgetItem(folder.folderName);
        // QTableWidgetItem has both drag and drop enabled by default - remove drag here
        item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
        item->setData(Qt::UserRole, QVariant::fromValue(&folder));
        setItem(row, 0, item);

        auto countItem = new QTableWidgetItem;
        countItem->setFlags(countItem->flags() & ~Qt::ItemIsDragEnabled);
        countItem->setData(Qt::UserRole, QVariant::fromValue(&folder));
        setItem(row, 1, countItem);
    }
    updateCounts();
}

void FoldersTableWidget::updateCounts()
{
    for (int row = 0; row < m_folders->size(); ++row) {
        item(row, 1)->setData(Qt::DisplayRole, m_folders->at(row).emails.size());
    }
}

bool FoldersTableWidget::dropMimeData(int row, int column, const QMimeData *mimeData, Qt::DropAction action)
{
    auto destFolder = item(row, column)->data(Qt::UserRole).value<EmailFolder *>();
    Q_ASSERT(destFolder);

    // decode data
    const QByteArray encodedData = mimeData->data(s_emailsMimeType);
    QDataStream stream(encodedData);
    if (stream.atEnd())
        return false;

    qint64 senderPid;
    stream >> senderPid;
    if (senderPid != QCoreApplication::applicationPid()) {
        // Let's not cast pointers that come from another process...
        return false;
    }

    quintptr sourceFolderPtr;
    stream >> sourceFolderPtr;
    auto sourceFolder = reinterpret_cast<EmailFolder *>(sourceFolderPtr);
    // Dropping onto the same folder?
    if (sourceFolder == destFolder)
        return false;

    QList<qintptr> emailItemPointers;
    stream >> emailItemPointers;
    for (qintptr emailItemPointer : std::as_const(emailItemPointers))
    {
        const auto emailItem = reinterpret_cast<QTableWidgetItem *>(emailItemPointer);
        // Add to data structure
        destFolder->emails.append(emailItem->text());
        // (no need to add to UI, that folder is never visible)

        // We have to handle deletion of the source, in case of a move, instead of just
        // letting QTableWidget do it by returning true.
        // This is because if we let QTableWidget delete the source item, it won't notify us and we won't
        // be able to update EmailFolder::emails. So it's all done here and we return false at the end.
        if (action == Qt::MoveAction) {
            const int sourceRow = emailItem->tableWidget()->row(emailItem);
            // Remove from data structure
            sourceFolder->emails.removeAt(sourceRow);
            // Remove from UI
            emailItem->tableWidget()->removeRow(sourceRow);
        }
    }

    updateCounts();

    return false;
}

class EmailsTableWidget : public QTableWidget
{
public:
    using QTableWidget::QTableWidget;

    void fillEmailsList(EmailFolder &folder);

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QMimeData *mimeData(const QList<QTableWidgetItem *> &items) const override;
#else
    QMimeData *mimeData(const QList<QTableWidgetItem *> items) const override;
#endif

private:
    EmailFolder * m_folder = nullptr;
};

void EmailsTableWidget::fillEmailsList(EmailFolder &folder)
{
    m_folder = &folder;
    clear();
    setRowCount(folder.emails.size());
    setColumnCount(1);
    setHorizontalHeaderLabels({"Emails"});
    for (int row = 0; row < folder.emails.size(); ++row) {
        // QTableWidgetItem has ItemIsDragEnabled and Qt::ItemIsDropEnabled set by default!
        setItem(row, 0, new QTableWidgetItem(folder.emails.at(row)));
    }
    horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QMimeData *EmailsTableWidget::mimeData(const QList<QTableWidgetItem *> &items) const
#else
QMimeData *EmailsTableWidget::mimeData(const QList<QTableWidgetItem *> items) const
#endif
{
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    // We stream pointers, so ensure drag and drop happen in the same process
    stream << QCoreApplication::applicationPid();

    // Serialize source folder (to detect dropping onto the same folder, and to handle moves)
    stream << quintptr(m_folder);

    // Serialize item pointers
    // In this example it's the simplest solution, because we want a move
    // to delete both the item and the underlying email.
    QList<quintptr> itemPointers;
    itemPointers.reserve(items.size());
    std::transform(items.begin(), items.end(), std::back_inserter(itemPointers), [this](auto item) { return quintptr(item); });
    stream << itemPointers;

    QMimeData *mimeData = new QMimeData;
    mimeData->setData(s_emailsMimeType, encodedData);
    return mimeData;
}

class TopLevel : public QWidget
{
public:
    TopLevel();

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
};

TopLevel::TopLevel()
    : QWidget(nullptr)
{
    auto layout = new QHBoxLayout(this);

    // Drop side (left)
    const auto setupFoldersWidget = [&](QAbstractItemView *view) {
        layout->addWidget(view);
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);

        // Note: this takes care of setAcceptDrops(true)
        view->setDragDropMode(QAbstractItemView::DropOnly);
        // Minor improvement: no forbidden cursor when moving the drag between folders
        view->setDragDropOverwriteMode(true);
    };

    // Drag side (right)
    const auto setupEmailsWidget = [&](QAbstractItemView *view) {
        layout->addWidget(view);
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        view->setMaximumWidth(400);

        view->setDragDropMode(QAbstractItemView::DragOnly);
        // Don't be confused by the method name, this sets the default action on the drag side
        view->setDefaultDropAction(Qt::MoveAction);
        // When moving, delete the source, don't just clear it (well, we're not using that feature)
        view->setDragDropOverwriteMode(false);
    };

    setWindowTitle("Dropping onto QTableWidgetItems");

    // Drop side (left)
    auto foldersTableWidget = new FoldersTableWidget(this);
    setupFoldersWidget(foldersTableWidget);
    foldersTableWidget->setEmailFolders(&m_emails);
    foldersTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    foldersTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    // Drag side (right)
    auto emailsTableWidget = new EmailsTableWidget(this);
    setupEmailsWidget(emailsTableWidget);
    emailsTableWidget->fillEmailsList(m_emails[0]);

    connect(foldersTableWidget, &QTableWidget::itemClicked, this, [=](const QTableWidgetItem *folderItem) {
            auto folder = folderItem->data(Qt::UserRole).value<EmailFolder *>();
            Q_ASSERT(folder);
            emailsTableWidget->fillEmailsList(*folder);
    });
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    auto topLevel = new TopLevel();
    topLevel->resize(700, 400);
    topLevel->show();
    topLevel->setAttribute(Qt::WA_DeleteOnClose);

    return app.exec();
}

