# cmake-format: off
##------------------------------------------------------------------------------
## logger ( UNIT <unitname>
##          LEVEL <level>
##          MESSAEGE <message>)
##
## Log message
##------------------------------------------------------------------------------
# cmake-format: on
function(logger)
    cmake_parse_arguments(
        _RULE
        "" # options
        "UNIT;LEVEL" # single value argumentes
        "" # multi value argumentes
        ${ARGN})
    set(_LOCAL_LEVEL "STATUS")
    if(_RULE_LEVEL)
        set(_LOCAL_LEVEL ${_RULE_LEVEL})
    endif()
    message("${_LOCAL_LEVEL}" "[${_RULE_UNIT}]: ${_RULE_UNPARSED_ARGUMENTS}")
endfunction()
