vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tillt/ChapterForge
    REF 7fad7de42eb3e950b1f35351ebeb0e713a925e32
    SHA512 2eb0086fcc341fd72a14c668369bb23f717cbb2dcd81df1867b31a1cced329511e485a80d29ce035516534c6fb82bff6321b550446af826f7d7e10d2f8916be8
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
)

vcpkg_cmake_build(TARGETS chapterforge chapterforge_cli)

# Install headers
file(INSTALL ${SOURCE_PATH}/include/ DESTINATION ${CURRENT_PACKAGES_DIR}/include)
if(EXISTS "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/generated/chapterforge_version.hpp")
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/generated/chapterforge_version.hpp"
        DESTINATION ${CURRENT_PACKAGES_DIR}/include)
endif()

# Install static lib and CLI (optional)
if(EXISTS "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/libchapterforge.a")
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/libchapterforge.a"
        DESTINATION ${CURRENT_PACKAGES_DIR}/lib)
endif()
if(EXISTS "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/chapterforge_cli")
    file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/chapterforge_cli"
        DESTINATION ${CURRENT_PACKAGES_DIR}/bin)
endif()

file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
configure_file(${SOURCE_PATH}/README.md ${CURRENT_PACKAGES_DIR}/share/${PORT}/usage.txt COPYONLY)
