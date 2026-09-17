cmake_minimum_required(VERSION 3.20)
project(Rufux VERSION 1.1.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wformat-security)
add_compile_definitions(RUFUX_VERSION="${PROJECT_VERSION}")
add_compile_definitions(RUFUX_LOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 "gtk4>=4.10")
pkg_check_modules(UDEV libudev)
pkg_check_modules(BLKID blkid)

option(RUFUX_NLS "Native language support (gettext)" ON)
if(RUFUX_NLS)
  find_package(Intl QUIET)
endif()

add_executable(rufux
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
  src/linux/create.c
  src/linux/mount.c
  src/linux/priv.c
  src/linux/secureboot.c
  src/linux/update.c
  src/gui/gui_gtk.c
)

target_include_directories(rufux PRIVATE src/linux src)

# gui_gtk.c always compiles: without HAVE_GTK it builds the CLI stub
# (previously the CLI-only branch failed to link: undefined rufux_gui_run).
if(GTK4_FOUND AND UDEV_FOUND AND BLKID_FOUND)
  message(STATUS "GTK4+udev+blkid found, building full GUI")
  target_compile_definitions(rufux PRIVATE HAVE_GTK=1)
  target_include_directories(rufux PRIVATE ${GTK4_INCLUDE_DIRS} ${UDEV_INCLUDE_DIRS} ${BLKID_INCLUDE_DIRS})
  target_link_directories(rufux PRIVATE ${GTK4_LIBRARY_DIRS} ${UDEV_LIBRARY_DIRS} ${BLKID_LIBRARY_DIRS})
  target_link_libraries(rufux PRIVATE ${GTK4_LIBRARIES} ${UDEV_LIBRARIES} ${BLKID_LIBRARIES})
else()
  message(STATUS "GTK4/udev/blkid missing, building CLI-only")
endif()

if(RUFUX_NLS AND Intl_FOUND)
  message(STATUS "NLS enabled (gettext)")
  target_compile_definitions(rufux PRIVATE ENABLE_NLS=1)
  target_include_directories(rufux PRIVATE ${Intl_INCLUDE_DIRS})
  target_link_libraries(rufux PRIVATE ${Intl_LIBRARIES})
  # compile translations
  file(GLOB PO_FILES po/*.po)
  foreach(PO ${PO_FILES})
    get_filename_component(LANG ${PO} NAME_WE)
    set(MO ${CMAKE_BINARY_DIR}/locale/${LANG}/LC_MESSAGES/rufux.mo)
    add_custom_command(OUTPUT ${MO}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/locale/${LANG}/LC_MESSAGES
      COMMAND msgfmt -o ${MO} ${PO}
      DEPENDS ${PO})
    list(APPEND MO_FILES ${MO})
    install(FILES ${MO} DESTINATION share/locale/${LANG}/LC_MESSAGES)
  endforeach()
  add_custom_target(rufux-locale ALL DEPENDS ${MO_FILES})
  find_program(MSGFMT msgfmt)
endif()

install(TARGETS rufux DESTINATION bin)
install(FILES doc/rufux.1 DESTINATION share/man/man1)
install(FILES res/linux/io.github.hultwl.rufux.desktop DESTINATION share/applications)
install(FILES res/linux/io.github.hultwl.rufux.policy DESTINATION share/polkit-1/actions)
# App icon (upstream Rufus artwork, GPLv3, renamed to our app id)
foreach(SZ 128 64 32)
  install(FILES res/icons/rufus-${SZ}.png
    DESTINATION share/icons/hicolor/${SZ}x${SZ}/apps
    RENAME io.github.hultwl.rufux.png)
endforeach()

enable_testing()
add_test(NAME phase1 COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_phase1.sh $<TARGET_FILE:rufux>)
add_test(NAME phase2 COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_phase2.sh $<TARGET_FILE:rufux>)
add_test(NAME phase3 COMMAND bash ${CMAKE_SOURCE_DIR}/tests/test_phase3.sh $<TARGET_FILE:rufux>)
