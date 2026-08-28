# SPDX-FileCopyrightText: 2026 gmesh contributors
# SPDX-License-Identifier: GPL-3.0-or-later

set(required_license_files
    "${CMAKE_CURRENT_LIST_DIR}/../LICENSE"
    "${CMAKE_CURRENT_LIST_DIR}/../LICENSES/GPL-2.0-or-later.txt"
    "${CMAKE_CURRENT_LIST_DIR}/../LICENSES/GPL-3.0-or-later.txt"
    "${CMAKE_CURRENT_LIST_DIR}/../LICENSES/LGPL-3.0-or-later.txt"
    "${CMAKE_CURRENT_LIST_DIR}/../NOTICE.md"
    "${CMAKE_CURRENT_LIST_DIR}/../THIRD_PARTY_LICENSES.md"
)

foreach(path IN LISTS required_license_files)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Required license file is missing: ${path}")
    endif()
endforeach()

file(GLOB_RECURSE project_sources
    "${CMAKE_CURRENT_LIST_DIR}/../include/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../tests/*.cpp"
)

foreach(path IN LISTS project_sources)
    file(READ "${path}" content LIMIT 1024)
    if(NOT content MATCHES "SPDX-License-Identifier: GPL-3.0-or-later")
        message(FATAL_ERROR "Missing GPL-3.0-or-later SPDX identifier: ${path}")
    endif()
endforeach()

message(STATUS "License compliance checks passed")
