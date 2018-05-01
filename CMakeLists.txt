cmake_minimum_required(VERSION 3.0)
set (SRC cda.c)

# --[ libarchive ]--
find_package (LibArchive REQUIRED)

add_executable (cda ${SRC})
target_link_libraries (cda ${LibArchive_LIBRARIES})
