# Extract one nonthrowing method verbatim. No full historical runtime is ported.
set(symbian_ordinal_source
    "${PROJECT_SOURCE_DIR}/research/upstream/buildtools/toolsandutils/e32tools/elf2e32/source/pl_elfexecutable.cpp")
file(READ "${symbian_ordinal_source}" symbian_ordinal_original)
string(FIND "${symbian_ordinal_original}"
       "PLUINT32 ElfExecutable::GetSymbolOrdinal( Elf32_Sym* aSym) {" symbian_ordinal_begin)
if(symbian_ordinal_begin LESS 0)
  message(FATAL_ERROR "Pinned historical ordinal method not found")
endif()
string(SUBSTRING "${symbian_ordinal_original}" ${symbian_ordinal_begin} -1 symbian_ordinal_tail)
string(FIND "${symbian_ordinal_tail}" "\n}\n" symbian_ordinal_end)
if(symbian_ordinal_end LESS 0)
  message(FATAL_ERROR "Historical ordinal method boundary not found")
endif()
math(EXPR symbian_ordinal_length "${symbian_ordinal_end} + 3")
string(SUBSTRING "${symbian_ordinal_tail}" 0 ${symbian_ordinal_length} symbian_ordinal_method)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/historical_ordinal.cc"
  "// SPDX-License-Identifier: EPL-1.0\n// Copyright (c) 2004-2009 Nokia Corporation and/or its subsidiary(-ies).\n// Extracted without algorithm changes from ${symbian_ordinal_source}.\n#include \"ordinal_types.h\"\n${symbian_ordinal_method}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${symbian_ordinal_source}")
add_executable(symbian_sdk_ordinal_oracle ordinal_test.cc
               "${CMAKE_CURRENT_BINARY_DIR}/historical_ordinal.cc")
target_include_directories(symbian_sdk_ordinal_oracle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(symbian_sdk_ordinal_oracle PRIVATE symbian::sdk GTest::gtest_main)
symbian_native_library(symbian_sdk_ordinal_oracle)
set(SYMBIAN_SDK_PROXY_TEST_IMAGE
    "${PROJECT_SOURCE_DIR}/.symbian/euser-proxy/euser.dso" CACHE FILEPATH
    "Maintained User::Exit ordinal proxy fixture")
add_test(NAME symbian_sdk_ordinal_oracle COMMAND symbian_sdk_ordinal_oracle)
set_tests_properties(symbian_sdk_ordinal_oracle PROPERTIES
  ENVIRONMENT "SYMBIAN_SDK_PROXY_TEST_IMAGE=${SYMBIAN_SDK_PROXY_TEST_IMAGE}" TIMEOUT 15)
