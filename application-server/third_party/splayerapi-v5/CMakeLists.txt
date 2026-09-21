cmake_minimum_required(VERSION 3.8)
project(libsplayer)

set(CMAKE_CXX_STANDARD 14)

include_directories(include)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(OS_LIB pthread dl m rt)
  set(AUDIO_DRIVER src/audio_output_alsa.c)
  find_library(
    SOUND_LIB asound
    HINTS ${CMAKE_FIND_ROOT_PATH} ${CMAKE_FIND_ROOT_PATH}/lib
  )
  find_path(
    SOUND_INC
    NAME asoundlib.h
    HINTS ${CMAKE_FIND_ROOT_PATH} ${CMAKE_FIND_ROOT_PATH}/include/alsa ${CMAKE_FIND_ROOT_PATH}/usr/include/alsa/
  )
  set(SPLAYER_LINK_FLAGS "-Wl,-rpath=${CMAKE_SOURCE_DIR}/lib")
elseif(APPLE)
  set(AUDIO_DRIVER src/audio_output_darwin.c)
  set(OS_LIB ${OS_LIB} "-framework CoreFoundation")
  find_library(SOUND_LIB AudioToolbox)
  find_path(SOUND_INC AudioQueue.h)
  set(SPLAYER_LINK_FLAGS "-Wl,-rpath,${CMAKE_SOURCE_DIR}/lib")
elseif(WIN32)
  set(AUDIO_DRIVER src/audio_output_dummy.c)
else()
  set(AUDIO_DRIVER src/audio_output_dummy.c)
endif()

if (SOUND_LIB AND SOUND_INC)
  include_directories(${SOUND_INC})
  find_library(SPLAYER_LIB_DYNAMIC splayer-1 HINTS "${CMAKE_SOURCE_DIR}/lib")
  find_library(SPLAYER_LIB splayer HINTS "${CMAKE_SOURCE_DIR}/lib")

  ## Create exec for the simple static example
  if (SPLAYER_LIB)
    add_executable(example_app_simple_static
      src/examples-v5/example_simple.c
      ${AUDIO_DRIVER}
    )

    target_link_libraries(example_app_simple_static
      ${SOUND_LIB}
      ${SPLAYER_LIB}
      ${OS_LIB}
    )

    set_target_properties(example_app_simple_static PROPERTIES LINK_FLAGS ${SPLAYER_LINK_FLAGS} LINKER_LANGUAGE CXX)
  endif()
  # Create exec for the simple dynamic example
  add_executable(example_app_simple_dynamic
    src/examples-v5/example_simple.c
    ${AUDIO_DRIVER}
  )
  target_link_libraries(example_app_simple_dynamic
    ${SOUND_LIB}
    ${SPLAYER_LIB_DYNAMIC}
    ${OS_LIB}
  )
  set_target_properties(example_app_simple_dynamic PROPERTIES LINK_FLAGS ${SPLAYER_LINK_FLAGS})

  # Create exec for the dlopen example
  add_executable(example_app_dlopen
    src/examples-v5/example_dlopen.c
    ${AUDIO_DRIVER}
  )
  target_link_libraries(example_app_dlopen ${SOUND_LIB} ${OS_LIB})
  set_target_properties(example_app_dlopen PROPERTIES LINK_FLAGS ${SPLAYER_LINK_FLAGS})

  ## Create exec for troubles example
  add_executable(example_app_troubles
    src/examples-v5/example_troubles.c
    ${AUDIO_DRIVER}
  )
  target_link_libraries(example_app_troubles
    ${SOUND_LIB}
    ${SPLAYER_LIB_DYNAMIC}
    ${OS_LIB}
  )
  set_target_properties(example_app_troubles PROPERTIES LINK_FLAGS ${SPLAYER_LINK_FLAGS})

  find_library(CURL_LIB curl)
  if (CURL_LIB)
    add_executable(example_app_curl
      src/examples-v5/example_curl.c
      src/examples-v5/curl_update.c
      ${AUDIO_DRIVER}
    )
    target_link_libraries(example_app_curl
      ${SOUND_LIB}
      ${CURL_LIB}
      ${OS_LIB}
    )
    set_target_properties(example_app_curl PROPERTIES LINK_FLAGS ${SPLAYER_LINK_FLAGS})
  else()
    message(WARNING "Could not find curl library skipping curl example\n")
  endif()

  ADD_EXECUTABLE(libsplayer-mock_simple
    src/examples-v5/example_simple.c
    ${AUDIO_DRIVER}
  )

  TARGET_LINK_LIBRARIES(libsplayer-mock_simple
    splayer-mock-123
    ${OS_LIB}
    ${SOUND_LIB}
  )

else()
  message("You need to install headers for soundlibs\n")
endif()

# Compile 2 mock libraries, that can be used to toggle between different versions of the splayer library
ADD_LIBRARY(splayer-mock-123 SHARED
  src/splayer_mock.c
)
set_target_properties(splayer-mock-123 PROPERTIES COMPILE_FLAGS "-DDEF_SPLAYER_VERSION=\"123\"")

ADD_LIBRARY(splayer-mock-321 SHARED
  src/splayer_mock.c
)
set_target_properties(splayer-mock-321 PROPERTIES COMPILE_FLAGS "-DDEF_SPLAYER_VERSION=\"321\"")

ADD_LIBRARY(splayer-empty SHARED
  src/splayer_empty.c
)

if(CMAKE_TOOLCHAIN_FILE)
  message(STATUS "You are using a toolchain file called ${CMAKE_TOOLCHAIN_FILE}. Thank you!")
else()
  message(WARNING "If you are cross-compiling and your host and target differ, you need to provide a toolchain file.")
  message(STATUS "Run cmake with -DCMAKE_TOOLCHAIN_FILE=YOUR_TOOLCHAIN_FILE. Eg:")
  message(WARNING "\tcmake -DCMAKE_TOOLCHAIN_FILE=syb-mips32r2el.txt\n\n")
endif()
