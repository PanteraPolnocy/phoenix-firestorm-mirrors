# -*- cmake -*-

project(llrender)

include(00-Common)
include(OpenGL)
include(FreeType)
include(LLCommon)
include(LLImage)
include(LLMath)
include(LLRender)
include(LLWindow)
include(LLXML)
include(LLFileSystem)

include_directories(
    ${FREETYPE_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLIMAGE_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLRENDER_INCLUDE_DIRS}
    ${LLFILESYSTEM_INCLUDE_DIRS}
    ${LLWINDOW_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
    )
include_directories(SYSTEM
    ${LLCOMMON_SYSTEM_INCLUDE_DIRS}
    ${LLXML_SYSTEM_INCLUDE_DIRS}
    )

set(llrender_SOURCE_FILES
    llatmosphere.cpp
    llcubemap.cpp
    llfontbitmapcache.cpp
    llfontfreetype.cpp
    llfontgl.cpp
    llfontregistry.cpp
    llgl.cpp
    llgldbg.cpp
    llglslshader.cpp
    llgltexture.cpp
    llimagegl.cpp
    llpostprocess.cpp
    llrender.cpp
    llrender2dutils.cpp
    llrendernavprim.cpp
    llrendersphere.cpp
    llrendertarget.cpp
    llshadermgr.cpp
    lltexture.cpp
    lluiimage.cpp
    llvertexbuffer.cpp
    llglcommonfunc.cpp
    )
    
set(llrender_HEADER_FILES
    CMakeLists.txt

    llatmosphere.h
    llcubemap.h
    llfontgl.h
    llfontfreetype.h
    llfontbitmapcache.h
    llfontregistry.h
    llgl.h
    llgldbg.h
    llglheaders.h
    llglslshader.h
    llglstates.h
    llgltexture.h
    llgltypes.h
    llimagegl.h
    llpostprocess.h
    llrender.h
    llrender2dutils.h
    llrendernavprim.h
    llrendersphere.h
    llshadermgr.h
    lltexture.h
    lluiimage.h
    lluiimage.inl
    llvertexbuffer.h
    llglcommonfunc.h
    )

set_source_files_properties(${llrender_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llrender_SOURCE_FILES ${llrender_HEADER_FILES})

if (BUILD_HEADLESS)
  add_library (llrenderheadless
    ${llrender_SOURCE_FILES}
    )

  set_property(TARGET llrenderheadless
    PROPERTY COMPILE_DEFINITIONS LL_MESA=1 LL_MESA_HEADLESS=1
      )

  target_link_libraries(llrenderheadless
    ${LLCOMMON_LIBRARIES}
    ${LLIMAGE_LIBRARIES}
    ${LLMATH_LIBRARIES}
    ${LLRENDER_HEADLESS_LIBRARIES}
    ${LLXML_LIBRARIES}
    ${LLFILESYSTEM_LIBRARIES}
    ${LLWINDOW_HEADLESS_LIBRARIES}
    ${OPENGL_HEADLESS_LIBRARIES})

endif (BUILD_HEADLESS)

add_library (llrender ${llrender_SOURCE_FILES})

if (SDL_FOUND)
    set_property(TARGET llrender
            PROPERTY COMPILE_DEFINITIONS LL_SDL=1
            )
endif (SDL_FOUND)
if (SDL2_FOUND)
    set_property(TARGET llrender
            PROPERTY COMPILE_DEFINITIONS LL_SDL2=1 LL_SDL=1
            )
endif ()

# Libraries on which this library depends, needed for Linux builds
# Sort by high-level to low-level
target_link_libraries(llrender 
    ${LLCOMMON_LIBRARIES}
    ${LLIMAGE_LIBRARIES}
    ${LLMATH_LIBRARIES}
    ${LLFILESYSTEM_LIBRARIES}
    ${LLXML_LIBRARIES}
    ${LLWINDOW_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${OPENGL_LIBRARIES})

