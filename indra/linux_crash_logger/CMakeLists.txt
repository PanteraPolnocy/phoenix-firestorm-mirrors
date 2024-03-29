# -*- cmake -*-

project(linux_crash_logger)

include(00-Common)
include(Linking)
include(UI)
include(CURL)
include(OpenSSL)
include(ZLIB)

include_directories(
    ${LLCOREHTTP_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLCRASHLOGGER_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLFILESYSTEM_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    )
include_directories(SYSTEM
    ${LLCOMMON_SYSTEM_INCLUDE_DIRS}
    ${LLXML_SYSTEM_INCLUDE_DIRS}
    )
include_directories(SYSTEM
    ${LLCOMMON_SYSTEM_INCLUDE_DIRS}
    ${LLXML_SYSTEM_INCLUDE_DIRS}
    )

set(linux_crash_logger_SOURCE_FILES
    linux_crash_logger.cpp
    )

set(linux_crash_logger_HEADER_FILES
    CMakeLists.txt

    llcrashloggerlinux.h
    )

set_source_files_properties(${linux_crash_logger_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND linux_crash_logger_SOURCE_FILES
     ${linux_crash_logger_HEADER_FILES}
     )

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")

add_executable(linux-crash-logger ${linux_crash_logger_SOURCE_FILES})

# llcommon uses `clock_gettime' which is provided by librt on linux.
set(LIBRT_LIBRARY rt)


target_link_libraries(linux-crash-logger
    ${UI_LIBRARIES}
	${CURL_LIBRARIES}
	${OPENSSL_LIBRARIES}
	${CRYPTO_LIBRARIES}
	${ZLIB_LIBRARIES}
    ${LIBRT_LIBRARY}
	X11
    )

add_custom_target(linux-crash-logger-target ALL
    DEPENDS linux-crash-logger)
