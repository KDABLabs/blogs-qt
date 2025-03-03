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
#include <QTreeWidget>
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

class FoldersTreeWidget : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;
    void setEmailFolders(EmailFolders *folders) { m_folders = folders; updateCounts(); }

protected:
    void updateCounts();
    QStringList mimeTypes() const override { return {QString::fromLatin1(s_emailsMimeType)}; }
    bool dropMimeData(QTreeWidgetItem *destItem, int index, const QMimeData *mimeData, Qt::DropAction action) override;

private:
    EmailFolders *m_folders = nullptr;
};

void FoldersTreeWidget::updateCounts()
{
    QTreeWidgetItemIterator it(this);
     while (*it) {
         auto folder = (*it)->data(0, Qt::UserRole).value<EmailFolder *>();
         (*it)->setData(1, Qt::DisplayRole, folder->emails.size());
         ++it;
     }
}

bool FoldersTreeWidget::dropMimeData(QTreeWidgetItem *destItem, int index, const QMimeData *mimeData, Qt::DropAction action)
{
    Q_UNUSED(index); // We're dropping onto "destItem", index is unused
    auto destFolder = destItem->data(0, Qt::UserRole).value<EmailFolder *>();
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
        const auto emailItem = reinterpret_cast<QTreeWidgetItem *>(emailItemPointer);
        // Add to data structure
        destFolder->emails.append(emailItem->text(0));
        // (no need to add to UI, that folder is never visible)

        // We have to handle deletion of the source, in case of a move, instead of just
        // letting QTreeWidget do it by returning true.
        // This is because if we let QTreeWidget delete the source item, it won't notify us and we won't
        // be able to update EmailFolder::emails. So it's all done here and we return false at the end.
        if (action == Qt::MoveAction) {
            auto parent = emailItem->parent() ? emailItem->parent() : emailItem->treeWidget()->invisibleRootItem();
            const int index = parent->indexOfChild(emailItem);
            // Remove from data structure
            sourceFolder->emails.removeAt(index);
            // Remove from UI
            delete emailItem;
        }
    }

    updateCounts();

    return false;
}

class EmailsTreeWidget : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;

    void fillEmailsList(EmailFolder &folder);

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override;
#else
    QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const override;
#endif

private:
    EmailFolder * m_folder = nullptr;
};

void EmailsTreeWidget::fillEmailsList(EmailFolder &folder)
{
    m_folder = &folder;
    clear();
    for (const QString &email : folder.emails) {
        // QTreeWidgetItem has ItemIsDragEnabled and ItemIsDropEnabled set by default
        addTopLevelItem(new QTreeWidgetItem({email}));
    }
    header()->resizeSections(QHeaderView::ResizeToContents);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QMimeData *EmailsTreeWidget::mimeData(const QList<QTreeWidgetItem *> &items) const
#else
QMimeData *EmailsTreeWidget::mimeData(const QList<QTreeWidgetItem *> items) const
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
    void fillFoldersTreeWidget(QTreeWidget *foldersTreeWidget);

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

void TopLevel::fillFoldersTreeWidget(QTreeWidget *foldersTreeWidget)
{
    for (EmailFolder &folder : m_emails) {
        QTreeWidgetItem *item = new QTreeWidgetItem({folder.folderName});
        // QTreeWidgetItem has ItemIsDragEnabled and ItemIsDropEnabled set by default
        item->setData(0, Qt::UserRole, QVariant::fromValue(&folder));
        foldersTreeWidget->addTopLevelItem(item);
    }
}

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
    };

    setWindowTitle("Dropping onto QTreeWidgetItems");

    // Drop side (left)
    auto foldersTreeWidget = new FoldersTreeWidget(this);
    setupFoldersWidget(foldersTreeWidget);
    fillFoldersTreeWidget(foldersTreeWidget);
    foldersTreeWidget->setEmailFolders(&m_emails);

    foldersTreeWidget->setHeaderLabels({"Folder", "Count"});
    foldersTreeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    foldersTreeWidget->header()->resizeSection(1, 80);
    foldersTreeWidget->header()->setStretchLastSection(false);

    // Drag side (right)
    auto emailsTreeWidget = new EmailsTreeWidget(this);
    setupEmailsWidget(emailsTreeWidget);
    emailsTreeWidget->fillEmailsList(m_emails[0]);
    emailsTreeWidget->setHeaderLabels({"Emails"});

    connect(foldersTreeWidget, &QTreeWidget::itemClicked, this, [=](const QTreeWidgetItem *folderItem) {
            auto folder = folderItem->data(0, Qt::UserRole).value<EmailFolder *>();
            Q_ASSERT(folder);
            emailsTreeWidget->fillEmailsList(*folder);
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

