cmake_minimum_required(VERSION 3.16)

project(Recycle_Vending_Machine_LCD VERSION 0.61.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Qml SerialPort Sql Network Multimedia
    VirtualKeyboard Quick3D)

# Optional OpenCV — set USE_OPENCV=ON to enable real YOLO inference.
option(USE_OPENCV "Enable OpenCV for YOLO" OFF)
if (USE_OPENCV)
    find_package(OpenCV REQUIRED COMPONENTS dnn imgproc imgcodecs)
    add_compile_definitions(HAVE_OPENCV)
endif()

# libmosquitto — the kiosk's MQTT pipe. apt: libmosquitto-dev.
# OPTIONAL, like OpenCV above. When present (Pi / Ubuntu) the real MQTT
# pipe links in. When absent (e.g. a Windows dev build just to run the
# UI) MQTT compiles out via the HAVE_MOSQUITTO guard and telemetry is
# silently dropped — the kiosk still runs. This is why it's a warning,
# not a hard error: you can build the UI anywhere.
find_path(MOSQUITTO_INCLUDE_DIR mosquitto.h)
find_library(MOSQUITTO_LIBRARY NAMES mosquitto)
if (MOSQUITTO_INCLUDE_DIR AND MOSQUITTO_LIBRARY)
    set(HAVE_MOSQUITTO TRUE)
    message(STATUS "libmosquitto found — MQTT enabled (${MOSQUITTO_LIBRARY})")
else()
    set(HAVE_MOSQUITTO FALSE)
    message(WARNING
        "libmosquitto NOT found — building WITHOUT the MQTT pipe. "
        "Telemetry will be dropped. For the real pipe (Pi/Ubuntu) install "
        "it with: sudo apt install libmosquitto-dev")
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
        qml/RecycleSessionPage.qml
        qml/VendingPage.qml
        qml/VendingReceiptPage.qml
        qml/QRCodePage.qml
        qml/QrLoginPage.qml
        qml/StartPage.qml

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
        qml/admin/AdminMachinePage.qml
        qml/admin/AdminAnalyticsPage.qml
        qml/admin/AdminLogsPage.qml
        qml/admin/AdminFaultsPage.qml
        qml/admin/AdminInventoryPage.qml
        qml/admin/AdminAboutPage.qml
        qml/admin/CatalogPickerDialog.qml
        qml/admin/NewProductDialog.qml
        qml/registration/ConsentPage.qml
        qml/registration/RegistrationDetailsPage.qml
        qml/registration/FaceEnrollPage.qml
        qml/registration/RegistrationCompletePage.qml
        qml/registration/ClaimQrPage.qml


        components/PrimaryButton.qml

        components/FaceIdIcon.qml
        components/CameraPreview.qml
        components/RwgCoin.qml
        components/Coin3D.qml
        components/BounceOnPress.qml
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
        include/face_rec_sidecar.h      src/face_rec_sidecar.cpp
        include/analytics.h             src/analytics.cpp
        include/logs_viewer.h           src/logs_viewer.cpp
        include/mqtt_client.h           src/mqtt_client.cpp
        include/machine_link.h          src/machine_link.cpp
        include/idle_manager.h          src/idle_manager.cpp
        include/product_image_catalog.h src/product_image_catalog.cpp
        include/inventory_scanner.h     src/inventory_scanner.cpp
        include/product_catalog.h       src/product_catalog.cpp
        include/off_client.h            src/off_client.cpp
        include/update_checker.h        src/update_checker.cpp
        include/recycle_session.h       src/recycle_session.cpp
        include/recycle_classifier.h    src/recycle_classifier.cpp
        include/barcode_scanner.h       src/barcode_scanner.cpp
        include/face_preview_feeder.h   src/face_preview_feeder.cpp
        include/Serial_Connection.h     src/Serial_Connection.cpp
    RESOURCES
        resources/assets/vending.png
        resources/assets/Recycle.png
        resources/assets/ads.jpg
        resources/assets/soda-can.png
        resources/assets/plastic-bottle.png
        resources/assets/Recycle1.gif
        resources/assets/Vending1.gif
        resources/assets/rwg-coin.png
        resources/assets/coin3d/coin.mesh
        resources/assets/coin3d/coin.png
        resources/assets/NotoEmoji-Regular.ttf
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
    Qt6::Quick3D
)

# MQTT pipe — only link + flag it in when libmosquitto was found.
if (HAVE_MOSQUITTO)
    target_link_libraries(appRecycle_Vending_Machine_LCD PRIVATE
        ${MOSQUITTO_LIBRARY})
    target_include_directories(appRecycle_Vending_Machine_LCD PRIVATE
        ${MOSQUITTO_INCLUDE_DIR})
    target_compile_definitions(appRecycle_Vending_Machine_LCD PRIVATE
        HAVE_MOSQUITTO)
endif()

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

# The Flask backend is gone — we publish directly over MQTT now. We still
# ship the .env.example as a template at /opt/rewingo/share/ so postinst
# can drop it into /etc/rewingo/.env on a fresh install.
install(FILES backend/.env.example
    DESTINATION opt/rewingo/share
    RENAME      env.example)

install(PROGRAMS packaging/rewingo.sh
    DESTINATION usr/local/bin
    RENAME      rewingo)
install(PROGRAMS packaging/rewingo-update-helper.sh
    DESTINATION usr/local/bin
    RENAME      rewingo-update-helper)
install(FILES   packaging/sudoers.d/rewingo
    DESTINATION etc/sudoers.d
    PERMISSIONS OWNER_READ GROUP_READ)   # sudoers wants 0440 / root:root

# (No Flask systemd service any more — MQTT publishes from inside the
#  Qt process directly. The old rewingo-backend.service file is kept on
#  disk for reference but not installed.)
install(FILES   packaging/rewingo.desktop
    DESTINATION usr/share/applications)
install(FILES   resources/assets/rwg-coin.png
    DESTINATION usr/share/icons/hicolor/256x256/apps
    RENAME      rewingo.png)

# ── CPack: build a .deb ─────────────────────────────────────────────────
# Invoke with:    cmake --build build --target package
# Produces:       build/rewingo_<version>_arm64.deb
#
# IMPORTANT: install() rules above use relative DESTINATIONs (e.g.
#   "opt/rewingo/bin"). Without the override below, CMake prepends
#   CMAKE_INSTALL_PREFIX (usually "/usr"), so files would land at
#   /usr/opt/rewingo/bin instead of /opt/rewingo/bin. Setting the CPack
#   prefix to "/" makes relative destinations resolve from the filesystem
#   root — the standard Debian convention.
set(CPACK_PACKAGING_INSTALL_PREFIX        "/")

set(CPACK_GENERATOR                       "DEB")
set(CPACK_PACKAGE_NAME                    "rewingo")
set(CPACK_PACKAGE_VERSION                 "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT
    "ReWinGo Team <admin@rewingo.local>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "ReWinGo recycling + vending kiosk")
set(CPACK_DEBIAN_PACKAGE_SECTION          "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY         "optional")

# Architecture: ask dpkg what we're actually building for instead of
# hardcoding "arm64". This makes the .deb label match the real binary:
#   - native build on an arm64 host (Pi, arm64 Ubuntu)  → arm64
#   - native build on an amd64 host (x86 PC/laptop)      → amd64
#   - inside a `--platform linux/arm64` Docker container → arm64
# Hardcoding arm64 on an x86 host produces a mislabeled package that
# installs on the Pi then crashes with "Exec format error".
find_program(DPKG_CMD dpkg)
if(DPKG_CMD)
    execute_process(COMMAND ${DPKG_CMD} --print-architecture
                    OUTPUT_VARIABLE DEB_HOST_ARCH
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if(NOT DEB_HOST_ARCH)
    set(DEB_HOST_ARCH "arm64")   # sensible default for the Pi target
endif()
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE     "${DEB_HOST_ARCH}")
message(STATUS "Packaging .deb for architecture: ${DEB_HOST_ARCH}")

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
    qml6-module-qtquick3d
    libqt6quick3d6
    qt6-svg-plugins
    libopencv-dev
    libmosquitto1
    ca-certificates
)
string(JOIN ", " CPACK_DEBIAN_PACKAGE_DEPENDS ${REWINGO_DEB_DEPS})

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/packaging/debian/postinst;"
    "${CMAKE_SOURCE_DIR}/packaging/debian/prerm")

include(CPack)
