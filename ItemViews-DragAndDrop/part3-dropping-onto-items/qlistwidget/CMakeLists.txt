set(PROJECT_SOURCES
    drop-onto-qlistwidgetitems.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(DropOntoQListWidgetItems
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(DropOntoQListWidgetItems
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(DropOntoQListWidgetItems PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

target_compile_features(DropOntoQListWidgetItems PRIVATE cxx_std_11)

set_target_properties(DropOntoQListWidgetItems PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(DropOntoQListWidgetItems)
endif()

