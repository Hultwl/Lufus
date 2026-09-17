cmake_minimum_required(VERSION 3.20)
project(Lufus VERSION 0.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wformat-security)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 gtk4)
pkg_check_modules(UDEV libudev)
pkg_check_modules(BLKID blkid)

add_executable(lufus
  src/linux/main.c
  src/linux/device.c
  src/linux/iso_probe.c
  src/linux/checksum.c
  src/linux/writer.c
  src/linux/exec.c
  src/linux/partition.c
  src/linux/mkfs_task.c
  src/linux/extract.c
  src/linux/boot.c
  src/linux/persist.c
  src/linux/badblocks.c
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
  message(STATUS "GTK4/udev/blkid missing, building CLI-only")
endif()

install(TARGETS lufus DESTINATION bin)

enable_testing()
add_test(NAME phase1 COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_phase1.sh $<TARGET_FILE:lufus>)
add_test(NAME phase2 COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_phase2.sh $<TARGET_FILE:lufus>)
