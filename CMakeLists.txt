cmake_minimum_required(VERSION 3.8)
project(csnake)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES src/main.c src/log.c src/log.h src/socket.c src/socket.h src/common.c src/common.h src/server.c src/server.h src/client.c src/client.h src/messages.c src/messages.h)

add_executable(csnake ${SOURCE_FILES})
add_definitions(${GLIB_CFLAGS_OTHER})
target_link_libraries(csnake ${GTK2_LIBRARIES})