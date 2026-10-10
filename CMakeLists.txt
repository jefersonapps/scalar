cmake_minimum_required(VERSION 3.21)
project(Scalar VERSION 0.5.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
add_library(scalar_core src/documents/Document.cpp src/commands/History.cpp src/rendering/StrokeMesh.cpp src/persistence/ZipArchive.cpp src/geometry/Geometry.cpp src/recognition/ShapeRecognizer.cpp src/rendering/ShapeMesh.cpp src/tools/StrokeEraser.cpp src/rendering/BackgroundMesh.cpp)
target_include_directories(scalar_core PUBLIC src)
target_sources(scalar_core PRIVATE src/tools/GeometryTools.cpp)
if(BUILD_TESTING)
    add_executable(geometry_tools_tests tests/geometry_tools_tests.cpp)
    target_link_libraries(geometry_tools_tests PRIVATE scalar_core)
    add_test(NAME geometry_tools COMMAND geometry_tools_tests)
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE scalar_core)
    add_test(NAME core COMMAND core_tests)
    add_executable(recognition_tests tests/recognition_tests.cpp)
    target_link_libraries(recognition_tests PRIVATE scalar_core)
    add_test(NAME recognition COMMAND recognition_tests)
    add_executable(eraser_tests tests/eraser_tests.cpp)
    target_link_libraries(eraser_tests PRIVATE scalar_core)
    add_test(NAME eraser COMMAND eraser_tests)
endif()
option(SCALAR_BUILD_DESKTOP "Build the Qt desktop application" ON)
if(SCALAR_BUILD_DESKTOP)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Sql Concurrent Pdf Svg)
    qt_standard_project_setup()
    add_library(scalar_desktop STATIC src/app/AppController.cpp src/app/AppController.h
        src/canvas/CanvasItem.cpp src/canvas/CanvasItem.h src/canvas/CanvasSelection.cpp src/rendering/CanvasScene.cpp src/input/InputManager.cpp src/input/InputManager.h
        src/persistence/ProjectStore.cpp src/persistence/ProjectStore.h
        src/persistence/Library.cpp src/persistence/Library.h src/persistence/Thumbnail.cpp src/persistence/TrashStore.cpp src/clipboard/ImageImporter.cpp src/documents/Backgrounds.cpp src/math/MathRenderer.cpp src/math/SvgValidation.cpp src/rendering/TextRenderer.cpp src/rendering/MathMesh.cpp src/pdf/PdfService.cpp src/pdf/PdfRenderCache.cpp src/app/AppPages.cpp)
    target_link_libraries(scalar_desktop PUBLIC scalar_core Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Concurrent)
    target_compile_definitions(scalar_desktop PRIVATE SCALAR_MATH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/mathjax")
    target_link_libraries(scalar_desktop PUBLIC Qt6::Pdf)
    target_sources(scalar_desktop PRIVATE src/export/PdfExporter.cpp src/rendering/PageRenderer.cpp)
    target_sources(scalar_desktop PRIVATE src/canvas/CanvasGeometryTools.cpp)
    target_sources(scalar_desktop PRIVATE src/tools/RegionFill.cpp)
    target_sources(scalar_desktop PRIVATE src/app/Branding.cpp src/app/Branding.h)
    target_sources(scalar_desktop PRIVATE src/rendering/TextFormatting.cpp)
    target_sources(scalar_desktop PRIVATE src/rendering/ShapeRasterCache.cpp)
    target_compile_definitions(scalar_desktop PUBLIC SCALAR_HAVE_QT_PDF=1)
    find_package(Qt6 6.4 QUIET COMPONENTS Svg)
    if(TARGET Qt6::Svg)
        target_link_libraries(scalar_desktop PUBLIC Qt6::Svg)
        target_compile_definitions(scalar_desktop PUBLIC SCALAR_HAVE_QT_SVG=1)
    endif()
    # Optional fallback for installations without Qt's WEBP image-format plugin.
    find_path(SCALAR_WEBP_INCLUDE_DIR webp/decode.h)
    find_library(SCALAR_WEBP_LIBRARY NAMES webp)
    if(SCALAR_WEBP_INCLUDE_DIR AND SCALAR_WEBP_LIBRARY)
        target_include_directories(scalar_desktop PRIVATE ${SCALAR_WEBP_INCLUDE_DIR})
        target_link_libraries(scalar_desktop PRIVATE ${SCALAR_WEBP_LIBRARY})
        target_compile_definitions(scalar_desktop PRIVATE SCALAR_HAVE_WEBP=1)
    endif()
    qt_add_executable(scalar src/app/main.cpp)
    if(WIN32)
        enable_language(RC)
        configure_file(packaging/scalar.rc.in scalar.rc @ONLY)
        target_sources(scalar PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/scalar.rc")
        get_target_property(_scalar_qt_core_type Qt6::Core TYPE)
        if(_scalar_qt_core_type STREQUAL "SHARED_LIBRARY")
            # Shared QML plugins are deployed by windeployqt from qml/.
            # Avoid scanning the bundled MathJax JavaScript at configure time.
            set_target_properties(scalar PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
        endif()
    endif()
    qt_add_resources(scalar_desktop text_fonts PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
        assets/fonts/LobsterTwo-Regular.otf assets/fonts/LobsterTwo-Italic.otf
        assets/fonts/LobsterTwo-Bold.otf assets/fonts/LobsterTwo-OFL.txt)
    target_link_libraries(scalar PRIVATE scalar_desktop)
    file(GLOB_RECURSE SCALAR_QML CONFIGURE_DEPENDS qml/*.qml qml/qmldir qml/theme/qmldir)
    list(APPEND SCALAR_QML "${CMAKE_CURRENT_SOURCE_DIR}/icon.png")
    qt_add_resources(scalar ui PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SCALAR_QML})
    set_target_properties(scalar PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE FALSE)
    if(UNIX AND NOT APPLE)
        # Git ships /usr/bin/scalar. Keep the development target name while
        # installing the whiteboard under its own command, without replacing Git.
        install(PROGRAMS "$<TARGET_FILE:scalar>" DESTINATION bin RENAME scalar-whiteboard)
    else()
        install(TARGETS scalar RUNTIME DESTINATION bin)
    endif()
    install(FILES packaging/scalar.desktop DESTINATION share/applications)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/icon.png" DESTINATION share/pixmaps RENAME scalar.png)
    find_package(Python3 3.12 QUIET COMPONENTS Interpreter)
    if(Python3_FOUND)
        option(SCALAR_PREPARE_MATH "Automatically prepare offline math dependencies during development builds" ON)
        if(SCALAR_PREPARE_MATH)
            add_custom_target(prepare_math_default ALL
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/prepare_math.py" --allow-unavailable
                VERBATIM COMMENT "Automatically preparing local math dependencies (development build)")
        endif()
        add_custom_target(math_runtime
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/prepare_math.py"
            VERBATIM COMMENT "Preparing the bundled offline MathJax and Node runtime")
        configure_file(cmake/InstallMath.cmake.in InstallMath.cmake @ONLY)
        install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/InstallMath.cmake")
        install(DIRECTORY third_party/mathjax/ DESTINATION share/scalar/math USE_SOURCE_PERMISSIONS
            PATTERN "package-lock.json" EXCLUDE
            PATTERN "scalar-offline-math" EXCLUDE
            # MathJax's build sources are unused by the bundled JS runtime and
            # exceed NSIS's Windows path limit when staged inside the project.
            PATTERN "mathjax-full/components" EXCLUDE
            PATTERN "mathjax-full/ts" EXCLUDE
            # Scalar loads the CommonJS js/ modules, not browser bundles.
            PATTERN "mathjax-full/es5" EXCLUDE
            PATTERN "*.map" EXCLUDE
            PATTERN "*.d.ts" EXCLUDE)
    else()
        install(CODE "message(FATAL_ERROR \"Python 3.12 is required on the packaging machine to prepare the offline math runtime.\")")
    endif()
    # End users receive the offline MathJax/Node runtime below.  Native packages
    # also either resolve the platform runtimes (DEB) or deploy them beside the
    # executable (Windows), so no development dependencies are needed after install.
    if(WIN32)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
        set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
        include(InstallRequiredSystemLibraries)
        get_target_property(_scalar_qmake Qt6::qmake IMPORTED_LOCATION)
        if(_scalar_qmake)
            get_filename_component(_scalar_qt_bin "${_scalar_qmake}" DIRECTORY)
            find_program(SCALAR_WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS "${_scalar_qt_bin}")
        endif()
        if(NOT SCALAR_WINDEPLOYQT_EXECUTABLE)
            message(FATAL_ERROR "windeployqt from the selected Qt kit is required to package Scalar for Windows.")
        endif()
        configure_file(cmake/InstallWindowsQt.cmake.in InstallWindowsQt.cmake @ONLY)
        install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/InstallWindowsQt.cmake")
    endif()

    set(CPACK_PACKAGE_NAME "Scalar")
    # Preserve quotes and backslashes in the custom NSIS finish-page actions.
    set(CPACK_VERBATIM_VARIABLES TRUE)
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")
    set(CPACK_PACKAGE_VENDOR "Scalar")
    set(CPACK_PACKAGE_CONTACT "Scalar maintainers")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Offline whiteboard for notes, geometry, PDFs and LaTeX")
    set(CPACK_PACKAGE_EXECUTABLES "scalar;Scalar")
    if(WIN32)
        set(CPACK_GENERATOR "NSIS;ZIP")
        set(CPACK_NSIS_COMPRESSOR "/SOLID lzma")
        set(CPACK_NSIS_DISPLAY_NAME "Scalar")
        set(CPACK_NSIS_PACKAGE_NAME "Scalar")
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        set(CPACK_NSIS_MODIFY_PATH OFF)
        set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/scalar.exe")
        set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
        # NSIS shows a checked Run Scalar option on the final installer page.
        set(CPACK_NSIS_MUI_FINISHPAGE_RUN "scalar.exe")
        # Use Modern UI's second finish-page checkbox for the desktop shortcut.
        set(CPACK_NSIS_DEFINES [=[
!define MUI_FINISHPAGE_RUN_TEXT "Executar o Scalar"
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Criar atalho na area de trabalho"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION ScalarCreateDesktopShortcut
Function ScalarCreateDesktopShortcut
  CreateShortCut "$DESKTOP\Scalar.lnk" "$INSTDIR\bin\scalar.exe" "" "$INSTDIR\bin\scalar.exe" 0
FunctionEnd
]=])
        set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [=[
  Delete "$DESKTOP\Scalar.lnk"
]=])
    elseif(UNIX AND NOT APPLE)
        set(CPACK_GENERATOR "DEB;TGZ")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Scalar maintainers")
        set(CPACK_DEBIAN_PACKAGE_SECTION "education")
        set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        # QML imports and image codecs are loaded at runtime and are therefore
        # not visible to dpkg-shlibdeps; declare them explicitly for apt.
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6sql6-sqlite, qt6-image-formats-plugins, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-dialogs, qml6-module-qtquick-shapes, qml6-module-qtquick-templates, qml6-module-qtquick-window, qml6-module-qtqml, qml6-module-qtqml-workerscript")
        set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    else()
        set(CPACK_GENERATOR "TGZ")
    endif()
    include(CPack)
    if(BUILD_TESTING)
        find_package(Qt6 REQUIRED COMPONENTS Test)
        qt_add_executable(persistence_tests tests/persistence_tests.cpp src/persistence/ProjectStore.cpp src/persistence/Library.cpp src/documents/Backgrounds.cpp src/math/SvgValidation.cpp)
        target_link_libraries(persistence_tests PRIVATE scalar_core Qt6::Core Qt6::Gui Qt6::Sql Qt6::Test)
        add_test(NAME persistence COMMAND persistence_tests)
        qt_add_executable(desktop_tests tests/desktop_tests.cpp)
        if(WIN32 AND _scalar_qt_core_type STREQUAL "SHARED_LIBRARY")
            set_target_properties(desktop_tests PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
        endif()
        target_link_libraries(desktop_tests PRIVATE scalar_desktop Qt6::Test)
        qt_add_resources(desktop_tests ui PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SCALAR_QML})
        qt_add_executable(trash_tests tests/trash_tests.cpp src/persistence/TrashStore.cpp src/persistence/Library.cpp)
        target_include_directories(trash_tests PRIVATE src)
        target_link_libraries(trash_tests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
        add_test(NAME trash COMMAND trash_tests)
        qt_add_executable(math_tests tests/math_tests.cpp)
        target_link_libraries(math_tests PRIVATE scalar_desktop Qt6::Test)
        add_test(NAME math COMMAND math_tests)
        qt_add_executable(pdf_tests tests/pdf_tests.cpp)
        if(WIN32 AND _scalar_qt_core_type STREQUAL "SHARED_LIBRARY")
            set_target_properties(math_tests pdf_tests PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
        endif()
        target_link_libraries(pdf_tests PRIVATE scalar_desktop Qt6::Test)
        add_test(NAME pdf COMMAND pdf_tests)
        set_tests_properties(pdf PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
        set_tests_properties(math PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
        add_test(NAME desktop COMMAND desktop_tests)
        set_tests_properties(desktop PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data" TIMEOUT 60)
        foreach(screen IN ITEMS home editor)
            if(screen STREQUAL "editor")
                set(smoke_options --editor)
            else()
                set(smoke_options)
            endif()
            add_test(NAME smoke_${screen} COMMAND scalar --smoke-test ${smoke_options})
            set_tests_properties(smoke_${screen} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/smoke-${screen}" TIMEOUT 15)
        endforeach()
    endif()
endif()
