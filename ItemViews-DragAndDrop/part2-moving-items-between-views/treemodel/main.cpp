// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "treemodel.h"

#include <QApplication>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QScreen>
#include <QTreeView>
#include <QVBoxLayout>

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto topLayout = new QVBoxLayout(this);

        auto label = new QLabel("Training material for introductory course", this);
        topLayout->addWidget(label);

        auto view1 = new QTreeView(this);
        QFile file(":/default.txt");
        file.open(QIODevice::ReadOnly | QIODevice::Text);
        auto model1 = new TreeModel(QString::fromUtf8(file.readAll()), this);
        file.close();
        view1->setModel(model1);
        setupViewForDnD(view1);
        for (int c = 0; c < model1->columnCount(); ++c)
            view1->resizeColumnToContents(c);
        view1->expandAll();
        topLayout->addWidget(view1);

        auto labelAdvanced = new QLabel("Training material for advanced course", this);
        topLayout->addWidget(labelAdvanced);

        auto view2 = new QTreeView(this);
        auto model2 = new TreeModel(QString{}, this); // initially empty
        view2->setModel(model2);
        setupViewForDnD(view2);
        view2->header()->resizeSection(0, view1->header()->sectionSize(0));
        topLayout->addWidget(view2);
    }

private:
    void setupViewForDnD(QTreeView *view)
    {
        // Move by default. The user can press Control to copy instead.
        view->setDefaultDropAction(Qt::MoveAction);
        // Note: this takes care of setDragEnabled(true) + setAcceptDrops(true)
        view->setDragDropMode(QAbstractItemView::DragDrop);
        // Any selection mode is supported
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow mw;
    mw.setWindowTitle(TreeModel::tr("Move Between Tree Views"));
    mw.showMaximized();

    return QCoreApplication::exec();
}

#include "main.moc"