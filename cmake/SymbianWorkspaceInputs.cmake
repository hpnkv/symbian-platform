# The root project never consumes the user's installed SDK selection.
include_guard(GLOBAL)
set(SYMBIAN_SOURCE_WORKSPACE "${CMAKE_CURRENT_LIST_DIR}/..")
get_filename_component(SYMBIAN_SOURCE_WORKSPACE "${SYMBIAN_SOURCE_WORKSPACE}" ABSOLUTE)
set(SYMBIAN_SDK_PREFIX "${SYMBIAN_SOURCE_WORKSPACE}/.symbian/workspace-inputs")
set(SYMBIAN_WORKSPACE_BUILD ON)
set(SYMBIAN_WORKSPACE_INPUTS ON)
set(SYMBIAN_LIBCXX_SOURCE "${SYMBIAN_SOURCE_WORKSPACE}/research/upstream/llvm-project/libcxx")
set(SYMBIAN_OPENC_SOURCE "${SYMBIAN_SOURCE_WORKSPACE}/research/upstream/ossrv/genericopenlibs/openenvcore")
set(SYMBIAN_GUI_SDK_INCLUDE "${SYMBIAN_SDK_PREFIX}/include/platform")
set(SYMBIAN_RUNTIME_LOCALE_STREAM ON)
if(EXISTS "${SYMBIAN_SOURCE_WORKSPACE}/.venv/bin/python")
  set(workspace_python "${SYMBIAN_SOURCE_WORKSPACE}/.venv/bin/python")
else()
  find_program(workspace_python NAMES python3 REQUIRED)
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "PYTHONPATH=${SYMBIAN_SOURCE_WORKSPACE}"
  "${workspace_python}" -m symbian.project.workspace
  --root "${SYMBIAN_SOURCE_WORKSPACE}"
  WORKING_DIRECTORY "${SYMBIAN_SOURCE_WORKSPACE}"
  RESULT_VARIABLE prepare_result OUTPUT_VARIABLE prepare_output
  ERROR_VARIABLE prepare_error
  ECHO_OUTPUT_VARIABLE ECHO_ERROR_VARIABLE)
if(NOT prepare_result EQUAL 0)
  message(FATAL_ERROR "Cannot prepare source workspace inputs. See ${SYMBIAN_SOURCE_WORKSPACE}/.symbian/workspace-inputs.log and the Build from source guide.\n${prepare_error}")
endif()
file(GLOB workspace_modules CONFIGURE_DEPENDS
  "${SYMBIAN_SOURCE_WORKSPACE}/symbian/project/templates/*.cmake"
  "${SYMBIAN_SOURCE_WORKSPACE}/symbian/toolchain/cmake/*"
  "${SYMBIAN_SOURCE_WORKSPACE}/research/abseil/*.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${workspace_modules} "${SYMBIAN_SOURCE_WORKSPACE}/symbian/project/workspace.py"
  "${SYMBIAN_SOURCE_WORKSPACE}/symbian/project/sdk.py")
list(PREPEND CMAKE_MODULE_PATH
  "${SYMBIAN_SOURCE_WORKSPACE}/symbian/project/templates"
  "${SYMBIAN_SOURCE_WORKSPACE}/symbian/toolchain/cmake")
