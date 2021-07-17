if (INSTALL_PROPRIETARY)
    # Note that viewer_manifest.py makes decision based on BUGSPLAT_DB and not USE_BUGSPLAT
    if (BUGSPLAT_DB)
        set(USE_BUGSPLAT ON  CACHE BOOL "Use the BugSplat crash reporting system")
    else (BUGSPLAT_DB)
        set(USE_BUGSPLAT OFF CACHE BOOL "Use the BugSplat crash reporting system")
    endif (BUGSPLAT_DB)
else (INSTALL_PROPRIETARY)
    set(USE_BUGSPLAT OFF CACHE BOOL "Use the BugSplat crash reporting system")
endif (INSTALL_PROPRIETARY)

if (USE_BUGSPLAT)
    if (NOT USESYSTEMLIBS)
        include(Prebuilt)
        use_prebuilt_binary(bugsplat)
        if (WINDOWS)
            set(BUGSPLAT_LIBRARIES 
                ${ARCH_PREBUILT_DIRS_RELEASE}/bugsplat.lib
                )
        elseif (DARWIN)
            find_library(BUGSPLAT_LIBRARIES BugsplatMac REQUIRED
                NO_DEFAULT_PATH PATHS "${ARCH_PREBUILT_DIRS_RELEASE}")
            message("Bugsplat for OSX not fully implemented, please adapt llappdelegate-objc.mm to honor options of sending user name and settings.xml.")
        else (WINDOWS)
            message(FATAL_ERROR "BugSplat is not supported; add -DUSE_BUGSPLAT=OFF")
        endif (WINDOWS)
    else (NOT USESYSTEMLIBS)
        set(BUGSPLAT_FIND_QUIETLY ON)
        set(BUGSPLAT_FIND_REQUIRED ON)
        include(FindBUGSPLAT)
    endif (NOT USESYSTEMLIBS)

    set(BUGSPLAT_DB "" CACHE STRING "BugSplat crash database name")

    set(BUGSPLAT_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/bugsplat)
    set(BUGSPLAT_DEFINE "LL_BUGSPLAT")
endif (USE_BUGSPLAT)

