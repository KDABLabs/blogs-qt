# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(ReorderTreeModel LANGUAGES CXX)

set(PROJECT_SOURCES
    main.cpp
    treenode.cpp treenode.h
    treemodel.cpp treemodel.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ReorderTreeModel ${PROJECT_SOURCES})
else()
    qt_add_resources(PROJECT_SOURCES simpletreemodel.qrc)
    add_executable(ReorderTreeModel ${PROJECT_SOURCES})
endif()

set_target_properties(ReorderTreeModel PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# Qt::Test for QAbstractItemModelTester
target_link_libraries(ReorderTreeModel PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    set(ReorderTreeModel_resource_files
        "default.txt"
    )

    qt_add_resources(ReorderTreeModel "ReorderTreeModel"
        PREFIX "/"
        FILES ${ReorderTreeModel_resource_files}
    )
endif()