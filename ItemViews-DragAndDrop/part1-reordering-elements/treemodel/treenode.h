// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef TREENODE_H
#define TREENODE_H

#include <QVariant>
#include <memory>
#include <vector>

class TreeNode
{
public:
    explicit TreeNode(QVariantList data, TreeNode *parentNode = nullptr);

    void appendChild(std::unique_ptr<TreeNode> &&child);
    ////// CHANGES FOR DND
    void insertChild(int pos, std::unique_ptr<TreeNode> &&child);
    std::unique_ptr<TreeNode> takeChild(int row);
    ////// END CHANGES FOR DND

    TreeNode *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    TreeNode *parentNode();

private:
    std::vector<std::unique_ptr<TreeNode>> m_childNodes;
    QVariantList m_itemData;
    TreeNode *m_parentNode;
};

#endif // TREENODE_H
