cmake_minimum_required(VERSION 3.12)

project(pxlib)

option(ENABLE_GSF "Build with gsf support" OFF)

if(CMAKE_COMPILER_IS_GNUCC)
    add_definitions(
        -DHAVE_CONFIG_H
        -Wall -Wpointer-arith -W
        ${PXLIB_EXTRA_GCC_FLAGS}
    )
else()
    add_definitions(-DHAVE_CONFIG_H)
endif()

# Check system for includes
include(CheckIncludeFile)
check_include_file("fcntl.h"           HAVE_FCNTL_H)
check_include_file("iconv.h"           HAVE_ICONV_H)
check_include_file("locale.h"          HAVE_LOCALE_H)
check_include_file("unistd.h"          HAVE_UNISTD_H)
check_include_file("inttypes.h"        HAVE_INTTYPES_H)
check_include_file("stdarg.h"          HAVE_STDARG_H)
check_include_file("stddef.h"          HAVE_STDDEF_H)
check_include_file("stdint.h"          HAVE_STDINT_H)
check_include_file("stdlib.h"          HAVE_STDLIB_H)
check_include_file("stdbool.h"         HAVE_STDBOOL_H)
check_include_file("string.h"          HAVE_STRING_H)
check_include_file("strings.h"         HAVE_STRINGS_H)
check_include_file("sys/time.h"        HAVE_SYS_TIME_H)
check_include_file("sys/types.h"       HAVE_SYS_TYPES_H)
check_include_file("gsf/gsf-input-stdio.h" HAVE_GSF_GSFINPUTSTDIO_H)

# Functions
include(CheckFunctionExists)
include(CheckLibraryExists)

# Check for iconv
if(HAVE_ICONV_H)
    # First check if iconv is in libc
    check_function_exists(iconv ICONV_IN_LIBC)
    
    if(NOT ICONV_IN_LIBC)
        # Check if iconv is in a separate library
        check_library_exists(iconv iconv "" HAVE_ICONV_LIB)
        if(HAVE_ICONV_LIB)
            set(ICONV_LIBRARIES iconv)
        endif()
    endif()
    
    if(ICONV_IN_LIBC OR HAVE_ICONV_LIB)
        set(HAVE_ICONV 1)
    endif()
endif()

# Endianess
include(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)

if(HAVE_ICONV)
    set(PX_HAVE_ICONV 1)
    set(PX_HAVE_RECODE 0)
else()
    set(PX_HAVE_ICONV 0)
    if(HAVE_RECODE_H)
        set(PX_HAVE_RECODE 1)
    else()
        set(PX_HAVE_RECODE 0)
    endif()
endif()

if(HAVE_GSF_GSFINPUTSTDIO_H)
    set(PX_HAVE_GSF 1)
else()
    set(PX_HAVE_GSF 0)
endif()

set(SOURCES
    src/gregor.c
    src/paradox.c
    src/px_crypt.c
    src/px_encode.c
    src/px_error.c
    src/px_head.c
    src/px_io.c
    src/px_memory.c
    src/px_memprof.c
    src/px_misc.c
    src/fileformat.h
    src/px_crypt.h
    src/px_encode.h
    src/px_error.h
    src/px_head.h
    src/px_intern.h
    src/px_io.h
    src/px_memory.h
    src/px_misc.h
    src/sdncal.h
    include/paradox-mp.h
)

add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
add_definitions(-DPXLIB_EXPORTS)

# Versioning
set(PXLIB_MAJOR_VERSION "0")
set(PXLIB_MINOR_VERSION "6")
set(PXLIB_MICRO_VERSION "8")
set(PXLIB_DOTTED_VERSION "${PXLIB_MAJOR_VERSION}.${PXLIB_MINOR_VERSION}.${PXLIB_MICRO_VERSION}")

# Packaging
set(CPACK_PACKAGE_NAME pxlib)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Reading and writing paradox databases")
set(CPACK_PACKAGE_VENDOR "pxlib")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR ${PXLIB_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${PXLIB_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${PXLIB_MICRO_VERSION})
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "/usr")
set(CPACK_GENERATOR "TGZ")
set(CPACK_SOURCE_GENERATOR "TGZ")

configure_file(${CMAKE_SOURCE_DIR}/cmakeconfig.h.in ${CMAKE_BINARY_DIR}/config.h)
configure_file(${CMAKE_SOURCE_DIR}/include/pxversion.h.in ${CMAKE_BINARY_DIR}/pxversion.h)

configure_file(${CMAKE_SOURCE_DIR}/include/paradox.h.in ${CMAKE_BINARY_DIR}/include/paradox.h)
configure_file(${CMAKE_SOURCE_DIR}/include/paradox-gsf.h.in ${CMAKE_BINARY_DIR}/include/paradox-gsf.h)

# Configure Windows resource file
if(WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/pxlib.rc.in ${CMAKE_BINARY_DIR}/pxlib.rc)
endif()

add_library(pxlib SHARED ${SOURCES})

# Add Windows resource file to the library
if(WIN32)
    target_sources(pxlib PRIVATE ${CMAKE_BINARY_DIR}/pxlib.rc)
endif()

target_include_directories(pxlib
    PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}
    PUBLIC ${CMAKE_BINARY_DIR}/include
)

# Link iconv library if needed
if(ICONV_LIBRARIES)
    target_link_libraries(pxlib PRIVATE ${ICONV_LIBRARIES})
endif()