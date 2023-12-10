function(enable_profiling project_name)

  option(ENABLE_NO_OMIT_FRAME_POINTER "Enable the -fno-omit-frame pointer flag"
         OFF)

  if(ENABLE_NO_OMIT_FRAME_POINTER)
    target_compile_options(${project_name} INTERFACE -fno-omit-frame-pointer)
  endif()

  option(ENABLE_TIME_TRACE "Enable the -ftime-trace flag (clang only)" OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang" AND ENABLE_TIME_TRACE)
    target_compile_options(${project_name} INTERFACE -ftime-trace)
  endif()

  option(ENABLE_PROFILE_INSTRUMENTATION
         "Enable clangs profiling instrumentation" OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang" AND ENABLE_TIME_TRACE)
    target_compile_options(${project_name} INTERFACE -fprofile-instr-generate)
  endif()

  set(ENABLE_PGO
      "OFF"
      CACHE
        STRING
        "Enable clangs profiling instrumentation by providing the path to the profdata"
  )
  string(TOUPPER ${ENABLE_PGO} ENABLE_PGO_UPPER)
  if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang"
     AND NOT
         ENABLE_PGO_UPPER
         STREQUAL
         "OFF"
     AND EXISTS ${ENABLE_PGO})
    target_compile_options(${project_name}
                           INTERFACE -fprofile-instr-use=${ENABLE_PGO})
  endif()

endfunction()
