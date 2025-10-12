#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "sz" for configuration "Release"
set_property(TARGET sz APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sz PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libSZ.dylib"
  IMPORTED_SONAME_RELEASE "@rpath/libSZ.dylib"
  )

list(APPEND _cmake_import_check_targets sz )
list(APPEND _cmake_import_check_files_for_sz "${_IMPORT_PREFIX}/lib/libSZ.dylib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
