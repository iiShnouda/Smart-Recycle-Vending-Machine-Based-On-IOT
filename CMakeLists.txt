cmake_minimum_required(VERSION 3.16)

project(Recycle_Vending_Machine_LCD VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Qml SerialPort Sql Network Multimedia
    VirtualKeyboard)

# Optional OpenCV — set USE_OPENCV=ON to enable real YOLO inference.
option(USE_OPENCV "Enable OpenCV for YOLO" OFF)
if (USE_OPENCV)
    find_package(OpenCV REQUIRED COMPONENTS dnn imgproc imgcodecs)
    add_compile_definitions(HAVE_OPENCV)
endif()

qt_standard_project_setup(REQUIRES 6.11)

qt_add_executable(appRecycle_Vending_Machine_LCD src/main.cpp)

qt_add_qml_module(appRecycle_Vending_Machine_LCD
    URI Recycle_Vending_Machine_LCD
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/MainPage.qml
        qml/RecycleWaitingPage.qml
        qml/VendingPage.qml
        qml/QRCodePage.qml
        qml/SleepMode.qml
        qml/RecycleCountingPage.qml
        qml/LanguageSelectionPage.qml
        qml/Theme.qml
        qml/AdvertisePage.qml
        qml/AuthChoicePage.qml
        qml/FaceDetectionPage.qml
        qml/RecycleSummaryPage.qml
        qml/admin/AdminGatePage.qml
        qml/admin/AdminMainPage.qml
        qml/admin/AdminDiagnosticsPage.qml
        qml/admin/AdminProductsPage.qml
        qml/admin/AdminAnalyticsPage.qml
        qml/admin/AdminLogsPage.qml
        qml/registration/ConsentPage.qml
        qml/registration/FaceEnrollPage.qml
        qml/registration/RegistrationCompletePage.qml


        components/PrimaryButton.qml

        components/FaceIdIcon.qml
    SOURCES
        include/applicationmanager.h    src/applicationmanager.cpp
        include/translationmanager.h    src/translationmanager.cpp
        include/logger.h                src/logger.cpp
        include/database.h              src/database.cpp
        include/admin_auth.h            src/admin_auth.cpp
        include/reed_monitor.h          src/reed_monitor.cpp
        include/products_model.h        src/products_model.cpp
        include/diagnostics_runner.h    src/diagnostics_runner.cpp
        include/yolo_runner.h           src/yolo_runner.cpp
        include/face_service.h          src/face_service.cpp
        include/analytics.h             src/analytics.cpp
        include/logs_viewer.h           src/logs_viewer.cpp
        include/mongo_client.h          src/mongo_client.cpp
        include/idle_manager.h          src/idle_manager.cpp
        include/product_image_catalog.h src/product_image_catalog.cpp
        src/Serial_Connection.h         src/Serial_Connection.cpp
    RESOURCES
        resources/assets/vending.png
        resources/assets/Recycle.png
        resources/assets/ads.jpg
        resources/assets/soda-can.png
        resources/assets/plastic-bottle.png
        resources/assets/Recycle1.gif
        resources/assets/Vending1.gif
        resources/translations/Recycle_Vending_Machine_LCD_en_US.qm
        resources/translations/Recycle_Vending_Machine_LCD_ar_EG.qm
)

target_include_directories(appRecycle_Vending_Machine_LCD PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(appRecycle_Vending_Machine_LCD PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appRecycle_Vending_Machine_LCD PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::SerialPort
    Qt6::Sql
    Qt6::Network
    Qt6::Multimedia
    Qt6::VirtualKeyboard
)

if (USE_OPENCV)
    target_link_libraries(appRecycle_Vending_Machine_LCD PRIVATE
        ${OpenCV_LIBS})
    target_include_directories(appRecycle_Vending_Machine_LCD PRIVATE
        ${OpenCV_INCLUDE_DIRS})
endif()

include(GNUInstallDirs)
install(TARGETS appRecycle_Vending_Machine_LCD
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
