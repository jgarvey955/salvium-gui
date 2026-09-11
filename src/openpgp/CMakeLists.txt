file(GLOB_RECURSE SOURCES *.cpp)
file(GLOB_RECURSE HEADERS *.h)

find_library(GCRYPT_LIBRARY gcrypt REQUIRED)
find_path(GCRYPT_INCLUDE_DIR gcrypt.h REQUIRED)

find_library(GPG_ERROR_LIBRARY gpg-error REQUIRED)
set(GPG_ERROR_LIBRARIES ${GPG_ERROR_LIBRARY})

if(APPLE AND STATIC AND NOT DEPENDS)
  # GPG error's pkg-config file omits gettext even when its static archive
  # uses libintl, as in Homebrew's macOS build.
  find_package(Intl REQUIRED)
  list(APPEND GPG_ERROR_LIBRARIES ${Intl_LIBRARIES})
endif()

add_library(openpgp
  ${SOURCES}
  ${HEADERS})

target_include_directories(openpgp
  PUBLIC
    ${CMAKE_SOURCE_DIR}/salvium/contrib/epee/include
    ${GCRYPT_INCLUDE_DIR})

target_link_libraries(openpgp
  PUBLIC
    ${GCRYPT_LIBRARY}
    ${GPG_ERROR_LIBRARIES})
