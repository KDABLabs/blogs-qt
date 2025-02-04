// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "treemodel.h"

#include <QApplication>
#include <QFile>
#include <QScreen>
#include <QTreeView>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QFile file(":/default.txt");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    TreeModel model(QString::fromUtf8(file.readAll()));
    file.close();

    QTreeView view;

    ////// CHANGES FOR DND
    view.setDefaultDropAction(Qt::MoveAction);
    // Note: this takes care of setDragEnabled(true) + setAcceptDrops(true)
    // Also: InternalMove disables moving between different views, we don't need to test that
    view.setDragDropMode(QAbstractItemView::InternalMove);
    // This even works
    view.setSelectionMode(QAbstractItemView::ExtendedSelection);
    ////// END CHANGES FOR DND

    view.setModel(&model);
    view.setWindowTitle(TreeModel::tr("Reordering a Tree Model"));
    for (int c = 0; c < model.columnCount(); ++c)
        view.resizeColumnToContents(c);
    view.expandAll();
    const auto screenSize = view.screen()->availableSize();
    view.resize({screenSize.width() / 2, screenSize.height() * 2 / 3});
    view.show();
    return QCoreApplication::exec();
}
