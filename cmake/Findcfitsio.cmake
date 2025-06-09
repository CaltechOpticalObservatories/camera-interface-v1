#cfitsio *sometimes* includes a CMake config file. This is the worst of all possible worlds.
#On systems where it doesn't, it will fall back to this find module

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  message(VERBOSE "found pkg-config, trying to locate cfitsio using it")
  pkg_check_modules(cfitsio cfitsio REQUIRED IMPORTED_TARGET)

  add_library(cfitsio ALIAS PkgConfig::cfitsio)

  get_target_property(PROP cfitsio INTERFACE_INCLUDE_DIRECTORIES)
  message(VERBOSE "iid: ${PROP}")
  message(VERBOSE "id: ${cfitsio_INCLUDE_DIRS}")


  
else()
    message(VERBOSE "no pkg-config, trying to locate cfitsio manually")
    find_library(CFITS_LIB cfitsio NAMES libcfitsio PATHS /usr/local/lib)
    add_library(cfitsio UNKNOWN IMPORTED)
    set_target_properties(cfitsio PROPERTIES IMPORTED_LOCATION ${CFITS_LIB})

    find_path(FITSIO_INCLUDE fitsio.h)

    message(VERBOSE "fitsio.h include location: ${FITSIO_INCLUDE}")

    if(NOT FITSIO_INCLUDE)
      message(FATAL_ERROR "could not find fitsio.h file, build cannot continue!")
    endif()
     
    get_filename_component(CFITSIO_INCLUDE_DIR ${FITSIO_INCLUDE} DIRECTORY)

    set_target_properties(cfitsio PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CFITSIO_INCLUDE_DIR})
    
    set(cfitsio_FOUND TRUE)
  
endif()
