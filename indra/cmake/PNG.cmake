# -*- cmake -*-
include(Prebuilt)

set(PNG_FIND_QUIETLY ON)
set(PNG_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindPNG)
else (USESYSTEMLIBS)
  use_prebuilt_binary(libpng)
  if (WINDOWS)
    set(PNG_LIBRARIES libpng15)
    set(PNG_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/libpng15)
  elseif(DARWIN)
    set(PNG_LIBRARIES png15)
    set(PNG_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/libpng15)
  else()
    set(PNG_LIBRARIES png15)
    set(PNG_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/libpng15)
  endif()
endif (USESYSTEMLIBS)
