// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

/*
    treemodel.cpp

    Provides a simple tree model to show how to create and use hierarchical
    models.
*/

#include "treemodel.h"
#include "treenode.h"

#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QDebug>
#include <QIODevice>
#include <QMimeData>
#include <QStringList>

TreeModel::TreeModel(const QString &data, QObject *parent)
    : QAbstractItemModel(parent)
    , rootNode(std::make_unique<TreeNode>(QVariantList{tr("Title"), tr("Summary")}))
{
    setupModelData(QStringView{data}.split(u'\n'), rootNode.get());
    ////// CHANGES FOR DND
#ifndef QT_NO_DEBUG
    // To catch errors during development
    new QAbstractItemModelTester(this, QAbstractItemModelTester::FailureReportingMode::Fatal, this);
#endif
    ////// END CHANGES FOR DND
}

TreeModel::~TreeModel() = default;

int TreeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<TreeNode *>(parent.internalPointer())->columnCount();
    return rootNode->columnCount();
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return {};

    const auto *node = static_cast<const TreeNode *>(index.internalPointer());
    return node->data(index.column());
}

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
    ////// CHANGES FOR DND
    if (index.isValid())
        // Allow dragging nodes as well as dropping onto them
        return QAbstractItemModel::flags(index) | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    else
        // Allow dropping between nodes
        return Qt::ItemIsDropEnabled;
    ////// END CHANGES FOR DND
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    return orientation == Qt::Horizontal && role == Qt::DisplayRole
        ? rootNode->data(section) : QVariant{};
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    TreeNode *parentNode = parent.isValid()
        ? static_cast<TreeNode*>(parent.internalPointer())
        : rootNode.get();

    if (auto *childNode = parentNode->child(row))
        return createIndex(row, column, childNode);
    return {};
}

QModelIndex TreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};

    auto *childNode = static_cast<TreeNode *>(index.internalPointer());
    TreeNode *parentNode = childNode->parentNode();
    return indexForItem(parentNode);
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    const TreeNode *parentNode = parent.isValid()
        ? static_cast<const TreeNode*>(parent.internalPointer())
        : rootNode.get();

    return parentNode->childCount();
}

void TreeModel::setupModelData(const QList<QStringView> &lines, TreeNode *parent)
{
    struct ParentIndentation
    {
        TreeNode *parent;
        qsizetype indentation;
    };

    QList<ParentIndentation> state{{parent, 0}};

    for (const auto &line : lines) {
        qsizetype position = 0;
        for ( ; position < line.length() && line.at(position).isSpace(); ++position) {
        }

        const QStringView lineData = line.mid(position).trimmed();
        if (!lineData.isEmpty()) {
            // Read the column data from the rest of the line.
            const auto columnStrings = lineData.split(u'\t', Qt::SkipEmptyParts);
            QVariantList columnData;
            columnData.reserve(columnStrings.count());
            for (const auto &columnString : columnStrings)
                columnData << columnString.toString();

            if (position > state.constLast().indentation) {
                // The last child of the current parent is now the new parent
                // unless the current parent has no children.
                auto *lastParent = state.constLast().parent;
                if (lastParent->childCount() > 0)
                    state.append({lastParent->child(lastParent->childCount() - 1), position});
            } else {
                while (position < state.constLast().indentation && !state.isEmpty())
                    state.removeLast();
            }

            // Append a new node to the current parent's list of children.
            auto *lastParent = state.constLast().parent;
            lastParent->appendChild(std::make_unique<TreeNode>(columnData, lastParent));
        }
    }
}

////// CHANGES FOR DND

/*
  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: David Faure <david.faure@kdab.com>

  SPDX-License-Identifier: MIT
*/
static const char s_mimeType[] = "application/x-simpletreemodel-internalmove";

// the default is "copy only", change it
Qt::DropActions TreeModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}
// the default is "return supportedDropActions()", let's be explicit
Qt::DropActions TreeModel::supportedDragActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}
QStringList TreeModel::mimeTypes() const
{
    return {QString::fromLatin1(s_mimeType)};
}

QMimeData *TreeModel::mimeData(const QModelIndexList &indexes) const
{
    // Since we're *only* doing internal moves for now, we don't need to copy the complete data
    // All we need is node pointers.

    QList<TreeNode *> draggedNodes;
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    for (const QModelIndex &index : indexes) {
        auto node = static_cast<TreeNode *>(index.internalPointer());
        // Note that with QTreeView, this is called for every column => deduplicate
        if (!draggedNodes.contains(node)) {
            draggedNodes.append(node);
        }
    }

    stream << QCoreApplication::applicationPid();
    stream << draggedNodes.count();
    for (TreeNode *node : std::as_const(draggedNodes)) {
        stream << reinterpret_cast<qintptr>(node);
    }

    QMimeData *mimeData = new QMimeData;
    mimeData->setData(s_mimeType, encodedData);
    return mimeData;
}

// The default implementation in QAbstractTableModel::dropMimeData, when dropping between nodes,
// is to encode the data returned by itemData(), decode it at destination, insert empty rows, and then
// fill them in. This might be good enough for some use cases, but it forces modeling empty data
// that gets fill in later (which doesn't always make sense), and it's much slower than just
// moving the data on our own.
bool TreeModel::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    //qDebug() << "dropMimeData:" << mimeData->formats() << action << row << column << parent;
    // check if the format is supported
    if (!mimeData->hasFormat(s_mimeType))
        return false;

    // decode data
    const QByteArray encodedData = mimeData->data(s_mimeType);
    QDataStream stream(encodedData);
    qint64 senderPid;
    stream >> senderPid;
    if (senderPid != QCoreApplication::applicationPid()) {
        // Let's not cast pointers that come from another process...
        return false;
    }
    TreeNode *parentNode = nodeForIndex(parent);
    Q_ASSERT(parentNode);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    qsizetype count;
#else
    int count;
#endif
    stream >> count;

    if (row == -1) {
        // valid index means: drop onto node. I chose that this should insert
        // a child node, because this is the only way to create the first child of a node...
        // This explains why Qt calls it parent: unless you only support replacing, this
        // is really the future parent of the dropped nodes.
        if (parent.isValid())
            row = 0;
        else
            // invalid index means: append at bottom, after last toplevel
            row = rowCount();
    }

    for (int i = 0; i < count; ++i) {
        // Decode data from the QMimeData
        qlonglong nodePtr;
        stream >> nodePtr;
        auto node = reinterpret_cast<TreeNode *>(nodePtr);

        // Clone node into new position
        beginInsertRows(parent.siblingAtColumn(0), row, row);
        parentNode->insertChild(row, node->clone());
        endInsertRows();
        ++row;
    }
    return true;
}

bool TreeModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_ASSERT(checkIndex(parent));
    Q_ASSERT(row >= 0);
    auto parentNode = nodeForIndex(parent);
    Q_ASSERT(row <= parentNode->childCount() - count);
    beginRemoveRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        parentNode->takeChild(row);
    }
    endRemoveRows();
    return true;
}

TreeNode *TreeModel::nodeForIndex(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<TreeNode *>(index.internalPointer()) : rootNode.get();
}

QModelIndex TreeModel::indexForItem(TreeNode *node) const
{
    return node != rootNode.get() ? createIndex(node->row(), 0, node) : QModelIndex{};
}

std::unique_ptr<TreeNode> TreeModel::removeNode(TreeNode *node)
{
    const int row = node->row();
    QModelIndex idx = indexForItem(node);
    beginRemoveRows(idx.parent(), row, row);
    auto ownedNode = node->parentNode()->takeChild(row);
    endRemoveRows();
    return ownedNode;
}

////// END CHANGES FOR DND
