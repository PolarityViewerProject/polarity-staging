include(Linking)
include(Prebuilt)

set(APR_FIND_QUIETLY ON)
set(APR_FIND_REQUIRED ON)

set(APRUTIL_FIND_QUIETLY ON)
set(APRUTIL_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindAPR)
else (USESYSTEMLIBS)
  use_prebuilt_binary(apr_suite)
  if (WINDOWS)
    if (LLCOMMON_LINK_SHARED)
      set(APR_selector "lib")
    else (LLCOMMON_LINK_SHARED)
      set(APR_selector "")
    endif (LLCOMMON_LINK_SHARED)
    set(APR_LIBRARIES 
      debug ${ARCH_PREBUILT_DIRS_DEBUG}/${APR_selector}apr-1.lib
      optimized ${ARCH_PREBUILT_DIRS_RELEASE}/${APR_selector}apr-1.lib
      )
    set(APRICONV_LIBRARIES 
      debug ${ARCH_PREBUILT_DIRS_DEBUG}/${APR_selector}apriconv-1.lib
      optimized ${ARCH_PREBUILT_DIRS_RELEASE}/${APR_selector}apriconv-1.lib
      )
    set(APRUTIL_LIBRARIES 
      debug ${ARCH_PREBUILT_DIRS_DEBUG}/${APR_selector}aprutil-1.lib ${APRICONV_LIBRARIES}
      optimized ${ARCH_PREBUILT_DIRS_RELEASE}/${APR_selector}aprutil-1.lib ${APRICONV_LIBRARIES}
      )
    if(NOT LLCOMMON_LINK_SHARED)
      list(APPEND APR_LIBRARIES Rpcrt4)
    endif(NOT LLCOMMON_LINK_SHARED)
  elseif (DARWIN)
    if (LLCOMMON_LINK_SHARED)
      set(APR_selector     "0.dylib")
      set(APRUTIL_selector "0.dylib")
    else (LLCOMMON_LINK_SHARED)
      set(APR_selector     "a")
      set(APRUTIL_selector "a")
    endif (LLCOMMON_LINK_SHARED)
    set(APR_LIBRARIES libapr-1.${APR_selector})
    set(APRUTIL_LIBRARIES libaprutil-1.${APRUTIL_selector})
    set(APRICONV_LIBRARIES iconv)
  else (WINDOWS)
    set(APR_LIBRARIES apr-1)
    set(APRUTIL_LIBRARIES aprutil-1)
    set(APRICONV_LIBRARIES iconv)
  endif (WINDOWS)
  set(APR_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/apr-1)

  if (LINUX)
    list(APPEND APRUTIL_LIBRARIES rt)
  endif (LINUX)
endif (USESYSTEMLIBS)
