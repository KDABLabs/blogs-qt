cmake_minimum_required(VERSION 3.10)

project(ModelViewDND-part3-dropping-onto-items VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Test REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} 5.15 COMPONENTS Widgets Test REQUIRED)

add_subdirectory(model-view)
add_subdirectory(qlistwidget)
add_subdirectory(qtablewidget)
add_subdirectory(qtreewidget)
add_subdirectory(treemodel)
