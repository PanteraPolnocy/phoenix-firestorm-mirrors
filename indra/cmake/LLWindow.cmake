# -*- cmake -*-

include(Variables)
include(GLEXT)
include(Prebuilt)

if (USESYSTEMLIBS)
  include(FindSDL)

  # This should be done by FindSDL.  Sigh.
  mark_as_advanced(
      SDLMAIN_LIBRARY
      SDL_INCLUDE_DIR
      SDL_LIBRARY
      )
else (USESYSTEMLIBS)
  if (LINUX)
    use_prebuilt_binary(SDL)
    set (SDL_FOUND TRUE)
    set (SDL_LIBRARY SDL directfb fusion direct X11)
  endif (LINUX)
endif (USESYSTEMLIBS)

set(LLWINDOW_INCLUDE_DIRS
    ${GLEXT_INCLUDE_DIR}
    ${LIBS_OPEN_DIR}/llwindow
    )

if (BUILD_HEADLESS)
  set(LLWINDOW_HEADLESS_LIBRARIES
      llwindowheadless
      )
endif (BUILD_HEADLESS)

  set(LLWINDOW_LIBRARIES
      llwindow
      )
