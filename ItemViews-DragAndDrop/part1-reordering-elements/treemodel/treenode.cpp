// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

/*
    treeitem.cpp

    A container for nodes of data supplied by the simple tree model.
*/

#include "treenode.h"

TreeNode::TreeNode(QVariantList data, TreeNode *parent)
    : m_itemData(std::move(data))
    , m_parentNode(parent)
{
}

void TreeNode::appendChild(std::unique_ptr<TreeNode> &&child)
{
    m_childNodes.push_back(std::move(child));
}

////// CHANGES FOR DND
void TreeNode::insertChild(int pos, std::unique_ptr<TreeNode> &&child)
{
    child->m_parentNode = this;
    const auto it = m_childNodes.begin() + pos;
    m_childNodes.insert(it, std::move(child));
}

std::unique_ptr<TreeNode> TreeNode::takeChild(int row)
{
    const auto it = m_childNodes.begin() + row;
    auto ret = std::move(*it);
    m_childNodes.erase(it);
    return ret;
}
////// END CHANGES FOR DND

TreeNode *TreeNode::child(int row)
{
    return row >= 0 && row < childCount() ? m_childNodes.at(row).get() : nullptr;
}

int TreeNode::childCount() const
{
    return int(m_childNodes.size());
}

int TreeNode::columnCount() const
{
    return int(m_itemData.count());
}

QVariant TreeNode::data(int column) const
{
    return m_itemData.value(column);
}

TreeNode *TreeNode::parentNode()
{
    return m_parentNode;
}

int TreeNode::row() const
{
    if (m_parentNode == nullptr)
        return 0;
    const auto it = std::find_if(m_parentNode->m_childNodes.cbegin(), m_parentNode->m_childNodes.cend(),
                                 [this](const std::unique_ptr<TreeNode> &treeNode) {
                                     return treeNode.get() == this;
                                 });

    if (it != m_parentNode->m_childNodes.cend())
        return std::distance(m_parentNode->m_childNodes.cbegin(), it);
    Q_ASSERT(false); // should not happen
    return -1;
}
