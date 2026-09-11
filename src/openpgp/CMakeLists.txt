file(GLOB_RECURSE SOURCES *.cpp)
file(GLOB_RECURSE HEADERS *.h)

find_library(GCRYPT_LIBRARY gcrypt REQUIRED)
find_path(GCRYPT_INCLUDE_DIR gcrypt.h REQUIRED)

find_library(GPG_ERROR_LIBRARY gpg-error REQUIRED)
set(GPG_ERROR_LIBRARIES ${GPG_ERROR_LIBRARY})

if(APPLE AND STATIC AND NOT DEPENDS)
  # Homebrew's static GPG error library needs its private gettext dependency.
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GPG_ERROR_PC REQUIRED gpg-error)
  foreach(_gpg_error_dependency IN LISTS GPG_ERROR_PC_STATIC_LIBRARIES)
    if(NOT _gpg_error_dependency STREQUAL "gpg-error")
      find_library(GPG_ERROR_${_gpg_error_dependency}_LIBRARY
        NAMES ${_gpg_error_dependency}
        HINTS ${GPG_ERROR_PC_STATIC_LIBRARY_DIRS}
        REQUIRED)
      list(APPEND GPG_ERROR_LIBRARIES ${GPG_ERROR_${_gpg_error_dependency}_LIBRARY})
    endif()
  endforeach()
  list(APPEND GPG_ERROR_LIBRARIES ${GPG_ERROR_PC_STATIC_LDFLAGS_OTHER})
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
