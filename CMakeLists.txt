cmake_minimum_required(VERSION 3.21)
project(ur-reader CXX)

# --- Build --------------------------------------------------------------------
# Each module is a static library; the app module links them into the
# ur-reader executable. All modules are implemented: core, archive, model,
# decode, session, view, app. Per-module smoke tests are built alongside.
# -------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive)

# --- core: shared utilities, zero project dependencies -----------------------
add_library(ur-core STATIC
    src/core/natural_sort.cpp
)
target_include_directories(ur-core PUBLIC src)
target_link_libraries(ur-core PUBLIC Qt6::Core)

# --- archive: libarchive wrapper, no GUI -------------------------------------
add_library(ur-archive STATIC
    src/archive/comic_archive.cpp
)
target_link_libraries(ur-archive
    PUBLIC  ur-core
    PRIVATE PkgConfig::LIBARCHIVE
)

# --- model: page list, spread + reading-mode detection, no GUI ---------------
add_library(ur-model STATIC
    src/model/page.cpp
    src/model/book.cpp
)
target_link_libraries(ur-model PUBLIC ur-archive)

# --- decode: threaded image decode + memory-budgeted LRU cache ---------------
add_library(ur-decode STATIC
    src/decode/page_cache.cpp
    src/decode/decode_service.cpp
)
target_link_libraries(ur-decode PUBLIC ur-model Qt6::Gui)

# --- session: book identity, progress file, global config (no GUI widgets) ---
add_library(ur-session STATIC
    src/session/book_identity.cpp
    src/session/app_config.cpp
    src/session/progress_file.cpp
)
target_link_libraries(ur-session PUBLIC ur-core Qt6::Gui)

# --- view: QGraphicsView reading widgets (paged + scroll) --------------------
add_library(ur-view STATIC
    src/view/reader_view.cpp
    src/view/paged_view.cpp
    src/view/scroll_view.cpp
)
target_link_libraries(ur-view PUBLIC ur-decode Qt6::Widgets)

# --- app: CLI, mode resolution, chrome, main window --------------------------
add_library(ur-app STATIC
    src/app/cli_options.cpp
    src/app/mode_resolver.cpp
    src/app/chrome_controller.cpp
    src/app/main_window.cpp
)
target_link_libraries(ur-app PUBLIC ur-view ur-session)

# --- the reader executable ---------------------------------------------------
add_executable(ur-reader src/app/main.cpp)
target_link_libraries(ur-reader PRIVATE ur-app)

# --- smoke tests -------------------------------------------------------------
add_executable(archive_smoketest tests/archive_smoketest.cpp)
target_link_libraries(archive_smoketest PRIVATE ur-archive)

add_executable(model_smoketest tests/model_smoketest.cpp)
target_link_libraries(model_smoketest PRIVATE ur-model)

add_executable(cache_smoketest tests/cache_smoketest.cpp)
target_link_libraries(cache_smoketest PRIVATE ur-decode)

add_executable(decode_smoketest tests/decode_smoketest.cpp)
target_link_libraries(decode_smoketest PRIVATE ur-decode)

add_executable(session_smoketest tests/session_smoketest.cpp)
target_link_libraries(session_smoketest PRIVATE ur-session)

add_executable(view_smoketest tests/view_smoketest.cpp)
target_link_libraries(view_smoketest PRIVATE ur-view)

add_executable(app_smoketest tests/app_smoketest.cpp)
target_link_libraries(app_smoketest PRIVATE ur-app)
