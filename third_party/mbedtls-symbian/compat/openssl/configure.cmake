# Materialize public headers with an explicit ABI configuration. Source snapshot
# stays unchanged and keeps its own license; adaptations are explained in docs.
set(openssl_include "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${openssl_include}/openssl")
file(GLOB public_headers "${openssl_source}/inc/include/openssl/*.h")
foreach(header IN LISTS public_headers)
  get_filename_component(name "${header}" NAME)
  configure_file("${header}" "${openssl_include}/openssl/${name}" COPYONLY)
endforeach()
file(READ "${openssl_source}/inc/include/openssl/opensslconf.h" config)
# Never reference absent assembly or auto-detect ARMv7/NEON on ARMv5/6 guests.
string(REPLACE "#define OPENSSL_CPUID_OBJ" "#undef OPENSSL_CPUID_OBJ" config "${config}")
if(NOT SYMBIAN_MBEDTLS_HOST OR CMAKE_SIZEOF_VOID_P EQUAL 4)
  string(REPLACE "#define SIXTY_FOUR_BIT_LONG" "#undef SIXTY_FOUR_BIT_LONG" config "${config}")
  string(REPLACE "#undef THIRTY_TWO_BIT" "#define THIRTY_TWO_BIT" config "${config}")
endif()
# Restore the original upstream ABI selector changed to an unconditional block
# by the historical port. Exactly one limb representation must be active.
file(READ "${openssl_source}/inc/include/openssl/bn.h" bn_header)
string(REPLACE "# if 1" "# ifdef SIXTY_FOUR_BIT" bn_header "${bn_header}")
file(WRITE "${openssl_include}/openssl/bn.h" "${bn_header}")
file(WRITE "${openssl_include}/openssl/opensslconf.h" "${config}")

# Stable compiler/profile identity instead of a generated build timestamp.
file(WRITE "${openssl_include}/openssl/buildinf.h"
  "#define CFLAGS \"compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}\"\n#define PLATFORM \"platform: ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}\"\n")
