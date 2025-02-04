// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef TREEMODEL_H
#define TREEMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <memory>

class TreeNode;

class TreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    Q_DISABLE_COPY_MOVE(TreeModel)

    explicit TreeModel(const QString &data, QObject *parent = nullptr);
    ~TreeModel() override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;

    ////// CHANGES FOR DND
    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    ////// END CHANGES FOR DND

private:
    ////// CHANGES FOR DND
    TreeNode *nodeForIndex(const QModelIndex &index) const;
    QModelIndex indexForItem(TreeNode *node) const;
    std::unique_ptr<TreeNode> removeNode(TreeNode *node);
    ////// END CHANGES FOR DND

    static void setupModelData(const QList<QStringView> &lines, TreeNode *parent);

    std::unique_ptr<TreeNode> rootNode;
};

#endif // TREEMODEL_H
