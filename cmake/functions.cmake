# Printing messages with a $TADAH prefix
function(tadah_message)
  # Define known message modes recognized by CMake
  set(_valid_modes 
    "FATAL_ERROR" "SEND_ERROR" "WARNING" "AUTHOR_WARNING"
    "DEPRECATION" "NOTICE" "STATUS" "VERBOSE"
  )

  # Default to STATUS mode if first argument is not a recognized mode
  list(FIND _valid_modes "${ARGV0}" _modeIndex)
  if(_modeIndex EQUAL -1)
    set(_mode "STATUS")
    set(_messageText "${ARGV}")
  else()
    set(_mode "${ARGV0}")
    list(REMOVE_AT ARGV 0)
    set(_messageText "${ARGV}")
  endif()

  # Concatenate remaining arguments into a single line 
  # (to mimic message() behavior with multiple arguments).
  string(REPLACE ";" " " _messageText "${_messageText}")

  # Print the message with "Tadah!: " prefix
  message(${_mode} "${TADAH} ${_messageText}")
endfunction()
