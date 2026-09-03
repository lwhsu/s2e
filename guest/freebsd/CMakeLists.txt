# S2E Selective Symbolic Execution Platform
#
# Copyright (c) 2026 Li-Wen Hsu
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# FreeBSD guest support: the LD_PRELOAD library, the launch script and the
# s2e.ko kernel module.

add_subdirectory(s2e.so)

install(DIRECTORY scripts/ DESTINATION . USE_SOURCE_PERMISSIONS)

# The kernel module is built with the system make and bsd.kmod.mk against the
# kernel sources in S2E_FREEBSD_SYSDIR (default /usr/src/sys). It is skipped
# when the sources are not available.
if(NOT S2E_FREEBSD_SYSDIR)
  set(S2E_FREEBSD_SYSDIR "/usr/src/sys")
endif()

if(EXISTS "${S2E_FREEBSD_SYSDIR}/conf/kmod.mk")
  set(S2E_KMOD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/s2e.ko")
  set(S2E_KMOD_OBJ_DIR "${CMAKE_CURRENT_BINARY_DIR}/s2e.ko")
  file(MAKE_DIRECTORY ${S2E_KMOD_OBJ_DIR})
  # GNU make exports its job server flags in MAKEFLAGS, which the system make does not understand
  add_custom_target(s2e_kmod ALL
                    COMMAND ${CMAKE_COMMAND} -E env --unset=MAKEFLAGS --unset=MFLAGS
                            make -C ${S2E_KMOD_SOURCE_DIR} SYSDIR=${S2E_FREEBSD_SYSDIR}
                            MAKEOBJDIR=${S2E_KMOD_OBJ_DIR} S2E_INCLUDE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../common/include
                    WORKING_DIRECTORY ${S2E_KMOD_SOURCE_DIR})
  install(FILES ${S2E_KMOD_OBJ_DIR}/s2e.ko DESTINATION .)
else()
  message(WARNING "FreeBSD kernel sources not found in ${S2E_FREEBSD_SYSDIR}, s2e.ko will not be built")
endif()
