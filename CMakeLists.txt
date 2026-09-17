cmake_minimum_required(VERSION 3.20)
project(Lufus VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wformat-security)

# Scaffold binary: Linux-native, no windows.h
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 gtk4)
pkg_check_modules(UDEV libudev)
pkg_check_modules(BLKID blkid)

add_executable(lufus
  src/linux/main.c
  src/linux/device.c
  src/linux/iso_probe.c
)

target_include_directories(lufus PRIVATE src/linux src)

if(GTK4_FOUND AND UDEV_FOUND AND BLKID_FOUND)
  message(STATUS "GTK4+udev+blkid found, building full GUI")
  target_sources(lufus PRIVATE src/gui/gui_gtk.c)
  target_compile_definitions(lufus PRIVATE HAVE_GTK=1)
  target_include_directories(lufus PRIVATE ${GTK4_INCLUDE_DIRS} ${UDEV_INCLUDE_DIRS} ${BLKID_INCLUDE_DIRS})
  target_link_directories(lufus PRIVATE ${GTK4_LIBRARY_DIRS} ${UDEV_LIBRARY_DIRS} ${BLKID_LIBRARY_DIRS})
  target_link_libraries(lufus PRIVATE ${GTK4_LIBRARIES} ${UDEV_LIBRARIES} ${BLKID_LIBRARIES})
else()
  message(STATUS "GTK4/udev/blkid missing, building CLI-only scaffold")
endif()

install(TARGETS lufus DESTINATION bin)
