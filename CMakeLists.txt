cmake_minimum_required(VERSION 3.20)
project(Simple-p6-Setup)

# ---Déclaration des fichiers sources---
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*)

# ---Ajout des fichiers sources au projet---
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE src)

# ---Choix de la version de C++---
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# ---Choix du niveau de warning---
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -pedantic-errors -Wimplicit-fallthrough)
endif()

# ---Éventuellement, activer les warnings comme erreurs---
set(WARNINGS_AS_ERRORS OFF CACHE BOOL "ON iff you want to treat warnings as errors")

if(WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /WX)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -Werror)
    endif()
endif()

# ---Configuration des tests---
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest
    GIT_TAG ae7a13539fb71f270b87eb2e874fbac80bc8dda2
)
FetchContent_MakeAvailable(doctest)
target_link_libraries(${PROJECT_NAME} PRIVATE doctest::doctest)

# ---Ajout de la bibliothèque p6---
set(P6_RAW_OPENGL_MODE ON CACHE BOOL "")
FetchContent_Declare(
    p6
    GIT_REPOSITORY https://github.com/julesfouchy/p6
    GIT_TAG 7a9d1ca49db2694ef05030921b65f0adfe9d69e4
)
FetchContent_MakeAvailable(p6)
target_link_libraries(${PROJECT_NAME} PRIVATE p6::p6)

p6_copy_folder(${PROJECT_NAME} img)
p6_copy_folder(${PROJECT_NAME} assets)
p6_copy_folder(${PROJECT_NAME} shaders)