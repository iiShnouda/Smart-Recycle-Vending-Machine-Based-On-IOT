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

# Lowered from 6.11 to 6.4 so the project builds against whichever Qt 6
# ships with the current Debian / Pi OS release (Bookworm = 6.4,
# Trixie = 6.8, …). Everything we use exists in 6.4: QML_SINGLETON,
# Material style, TapHandler, QtVirtualKeyboard, etc.
qt_standard_project_setup(REQUIRES 6.4)

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
        qml/admin/AdminFaultsPage.qml
        qml/admin/AdminInventoryPage.qml
        qml/admin/AdminAboutPage.qml
        qml/admin/CatalogPickerDialog.qml
        qml/admin/NewProductDialog.qml
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
        include/inventory_scanner.h     src/inventory_scanner.cpp
        include/product_catalog.h       src/product_catalog.cpp
        include/off_client.h            src/off_client.cpp
        include/update_checker.h        src/update_checker.cpp
        include/Serial_Connection.h     src/Serial_Connection.cpp
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

# Bake the project version into the binary so the in-app "About" page +
# UpdateChecker can compare against the latest GitHub release. Reads
# `project(... VERSION X.Y.Z ...)` at the top of this file — bump that
# every time you cut a release.
target_compile_definitions(appRecycle_Vending_Machine_LCD PRIVATE
    REWINGO_VERSION="${PROJECT_VERSION}")

# ╔══════════════════════════════════════════════════════════════════════╗
# ║  LINUX / RASPBERRY PI PACKAGING                                      ║
# ║                                                                      ║
# ║  Only the .deb path uses these install() rules — Windows packaging   ║
# ║  is handled out-of-tree by Advanced Installer (see                   ║
# ║  docs/WINDOWS_INSTALLER.md). On other platforms `cmake --install`    ║
# ║  still works but lands in the default GNUInstallDirs locations.      ║
# ║                                                                      ║
# ║  Layout the .deb produces:                                           ║
# ║    /opt/rewingo/bin/rewingo            Qt binary                     ║
# ║    /opt/rewingo/backend/*              Python Flask                  ║
# ║    /usr/local/bin/rewingo              launcher (sets QT vars)       ║
# ║    /usr/local/bin/rewingo-backend      starts Flask in its venv      ║
# ║    /usr/share/applications/rewingo.desktop                           ║
# ║    /usr/share/icons/hicolor/256x256/apps/rewingo.png                 ║
# ║                                                                      ║
# ║  postinst creates at install-time:                                   ║
# ║    /var/lib/rewingo/                   SQLite + logs                 ║
# ║    /opt/rewingo/backend/venv/          Python virtualenv             ║
# ║    /etc/rewingo/.env                   editable credentials          ║
# ║                                                                      ║
# ╚══════════════════════════════════════════════════════════════════════╝

# Rename the on-disk binary to something friendly.
set_target_properties(appRecycle_Vending_Machine_LCD PROPERTIES
    OUTPUT_NAME "rewingo")

install(TARGETS appRecycle_Vending_Machine_LCD
    BUNDLE  DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION opt/rewingo/bin)

# Python backend — keep .env.example for the postinst template, drop
# the live .env (real credentials must never ship in a package).
install(DIRECTORY backend/
    DESTINATION opt/rewingo/backend
    REGEX   "/\\.env$"  EXCLUDE
    PATTERN "venv"      EXCLUDE
    PATTERN "__pycache__" EXCLUDE)

install(PROGRAMS packaging/rewingo.sh
    DESTINATION usr/local/bin
    RENAME      rewingo)
install(PROGRAMS packaging/rewingo-backend.sh
    DESTINATION usr/local/bin
    RENAME      rewingo-backend)
install(FILES   packaging/rewingo.desktop
    DESTINATION usr/share/applications)
install(FILES   resources/assets/vending.png
    DESTINATION usr/share/icons/hicolor/256x256/apps
    RENAME      rewingo.png)

# ── CPack: build a .deb ─────────────────────────────────────────────────
# Invoke with:    cmake --build build --target package
# Produces:       build/rewingo_<version>_arm64.deb
set(CPACK_GENERATOR                       "DEB")
set(CPACK_PACKAGE_NAME                    "rewingo")
set(CPACK_PACKAGE_VERSION                 "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT
    "ReWinGo Team <admin@rewingo.local>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "ReWinGo recycling + vending kiosk")
set(CPACK_DEBIAN_PACKAGE_SECTION          "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY         "optional")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE     "arm64")    # Pi 4 / Pi 5

# Use the Debian-standard file naming:  <name>_<version>_<arch>.deb
# (default would be the generic "<name>-<version>-Linux.deb").
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Runtime apt deps. Build the comma-separated string by joining a CMake
# list — `set(VAR "a" "b")` would have created a *list* (semicolons
# leak into the control file and apt fails with "parsing dependency N").
set(REWINGO_DEB_DEPS
    libqt6core6
    libqt6gui6
    libqt6qml6
    libqt6quick6
    libqt6quickcontrols2-6
    libqt6serialport6
    libqt6sql6-sqlite
    libqt6network6
    libqt6multimedia6
    qml6-module-qtquick
    qml6-module-qtquick-controls
    qml6-module-qtquick-layouts
    qml6-module-qtquick-templates
    qml6-module-qtquick-window
    qml6-module-qtmultimedia
    qml6-module-qt-labs-settings
    qml6-module-qtquick-virtualkeyboard
    libopencv-dev
    python3
    python3-venv
    python3-pip
)
string(JOIN ", " CPACK_DEBIAN_PACKAGE_DEPENDS ${REWINGO_DEB_DEPS})

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/packaging/debian/postinst;"
    "${CMAKE_SOURCE_DIR}/packaging/debian/prerm")

include(CPack)
