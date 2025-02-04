set(PROJECT_SOURCES
    reorder-with-model-view.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ReorderWithModelView
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(ReorderWithModelView
        ${PROJECT_SOURCES}
    )
endif()

# Qt::Test for QAbstractItemModelTester
target_link_libraries(ReorderWithModelView PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Test)

target_compile_features(ReorderWithModelView PRIVATE cxx_std_11)

set_target_properties(ReorderWithModelView PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ReorderWithModelView)
endif()

