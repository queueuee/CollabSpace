# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\ServerConsole_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\ServerConsole_autogen.dir\\ParseCache.txt"
  "ServerConsole_autogen"
  )
endif()
