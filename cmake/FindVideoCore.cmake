# Find Broadcom VideoCore firmware installation
#
#  VIDEOCORE_FOUND
#  VIDEOCORE_INCLUDE_DIRS
#  VIDEOCORE_LIBRARIES
#

# Only need to cater for raspbian as they are not in CMAKE_SYSTEM_PATH
set(VIDEOCORE_INC_SEARCH_PATH ${CMAKE_SYSROOT}/opt/vc/include)
set(VIDEOCORE_LIB_SEARCH_PATH ${CMAKE_SYSROOT}/opt/vc/lib)

find_path (VIDEOCORE_INCLUDE_DIRS bcm_host.h PATHS ${VIDEOCORE_INC_SEARCH_PATH} PATH_SUFFIXES vc DOC "Broadcom VideoCore include directory")
find_library(VIDEOCORE_LIB_MMAL_CORE mmal_core PATHS ${VIDEOCORE_LIB_SEARCH_PATH} PATH_SUFFIXES vc DOC "Broadcom VideoCore mmal_core library")
find_library(VIDEOCORE_LIB_MMAL_UTIL mmal_util PATHS ${VIDEOCORE_LIB_SEARCH_PATH} PATH_SUFFIXES vc DOC "Broadcom VideoCore mmal_util library")
find_library(VIDEOCORE_LIB_MMAL_VC_CLIENT mmal_vc_client PATHS ${VIDEOCORE_LIB_SEARCH_PATH} PATH_SUFFIXES vc DOC "Broadcom VideoCore mmal_vc_client library")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VideoCore
    REQUIRED_VARS
        VIDEOCORE_LIB_MMAL_CORE
        VIDEOCORE_LIB_MMAL_UTIL
        VIDEOCORE_LIB_MMAL_VC_CLIENT
        VIDEOCORE_INCLUDE_DIRS
    FAIL_MESSAGE "Could NOT find Broadcom VideoCore firmware"
)

if(VideoCore_FOUND)
    set(VIDEOCORE_LIBRARIES ${VIDEOCORE_LIB_MMAL_CORE} ${VIDEOCORE_LIB_MMAL_UTIL} ${VIDEOCORE_LIB_MMAL_VC_CLIENT})
endif ()

mark_as_advanced (VIDEOCORE_INCLUDE_DIRS VIDEOCORE_LIBRARIES)
