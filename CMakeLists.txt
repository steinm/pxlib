option(ENABLE_GSF   "Build with gsf support" OFF)

if(CMAKE_COMPILER_IS_GNUCC)
  add_definitions(
    -DHAVE_CONFIG_H
    -Wall -Wpointer-arith -W
    ${PXLIB_EXTRA_GCC_FLAGS}
  )
else(CMAKE_COMPILER_IS_GNUCC)
  add_definitions(-DHAVE_CONFIG_H)
endif(CMAKE_COMPILER_IS_GNUCC)

#check system for includes
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
check_include_file("string.h"          HAVE_STRING_H)
check_include_file("strings.h"         HAVE_STRINGS_H)
check_include_file("sys/time.h"        HAVE_SYS_TIME_H)
check_include_file("sys/types.h"       HAVE_SYS_TYPES_H)
check_include_file("gsf/gsf-input-stdio.h" HAVE_GSF_GSFINPUTSTDIO_H)

#Functions
include(CheckFunctionExists)

#Big or little endian ?
include(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)

if(HAVE_ICONV_H)
	set(PX_HAVE_ICONV 1)
	set(PX_HAVE_RECODE 0)
else(HAVE_ICONV_H)
	if(HAVE_RECODE_H)
		set(PX_HAVE_RECODE 1)
	endif(HAVE_RECODE_H)
endif(HAVE_ICONV_H)

if(HAVE_GSF_GSFINPUTSTDIO_H)
	set(PX_HAVE_GSF 1)
else(HAVE_GSF_GSFINPUTSTDIO_H)
	set(PX_HAVE_GSF 0)
endif(HAVE_GSF_GSFINPUTSTDIO_H)

SUBDIRS( src )
SET(SOURCES
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
	include/paradox-gsf.h
	include/paradox.h
	include/paradox-mp.h
	include/pxversion.h
  )

INCLUDE_DIRECTORIES( include . )

ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
ADD_DEFINITIONS(-DPXLIB_EXPORTS)

#packaging
set(CPACK_PACKAGE_NAME pxlib)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Reading and writing paradox databases")
set(CPACK_PACKAGE_VENDOR "pxlib")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "4")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "/usr")
set(CPACK_GENERATOR "TGZ")
set(CPACK_SOURCE_GENERATOR "TGZ")

configure_file(${CMAKE_SOURCE_DIR}/cmakeconfig.h.in ${CMAKE_BINARY_DIR}/config.h)
configure_file(${CMAKE_SOURCE_DIR}/include/paradox.h.in ${CMAKE_BINARY_DIR}/include/paradox.h)
configure_file(${CMAKE_SOURCE_DIR}/include/paradox-gsf.h.in ${CMAKE_BINARY_DIR}/include/paradox-gsf.h)
ADD_LIBRARY(pxlib SHARED ${SOURCES})
