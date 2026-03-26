cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0025 NEW)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.15")
    cmake_policy(SET CMP0093 NEW)
endif()

project(App VERSION 1.0)

set(SOURCES     
    src/main.cpp
    
    src/EditorLayer.cpp                                         src/EditorLayer.h    
    
    src/Python/PythonCommand.cpp                                src/Python/PythonCommand.h

    src/Project/Project.cpp                                     src/Project/Project.h
    src/Project/Catalog.cpp                                     src/Project/Catalog.h
    src/Project/CatalogCutPattern.cpp                           src/Project/CatalogCutPattern.h
    src/Project/ProjectVariantExcelTables.cpp                   src/Project/ProjectVariantExcelTables.hpp
    src/Project/Processing/GenImgsByCutPattern.cpp              src/Project/Processing/GenImgsByCutPattern.h
    src/Project/Processing/GenRawExcel.cpp                      src/Project/Processing/GenRawExcel.h
    src/Project/Processing/XlsxPageViewData.cpp                 src/Project/Processing/XlsxPageViewData.h

    src/ImGui/Tables/Table.cpp                                  src/ImGui/Tables/Table.h
    
    src/ImGui/Overlays/Overlay.cpp                              src/ImGui/Overlays/Overlay.h
    src/ImGui/Overlays/ScriptPopup.cpp                          src/ImGui/Overlays/ScriptPopup.h

    src/ImGui/Project/CreateNewProject.cpp                      src/ImGui/Project/CreateNewProject.h
    src/ImGui/Project/OpenProject.cpp                           src/ImGui/Project/OpenProject.h
    src/ImGui/Project/SetupProject.cpp                          src/ImGui/Project/SetupProject.h
    src/ImGui/Project/PageView/PageViewManager.cpp              src/ImGui/Project/PageView/PageViewManager.h
    src/ImGui/Project/PageView/IPageView.cpp                    src/ImGui/Project/PageView/IPageView.h
    src/ImGui/Project/PageView/IImgPageView.cpp                 src/ImGui/Project/PageView/IImgPageView.h
    src/ImGui/Project/PageView/RawImgPageView.cpp               src/ImGui/Project/PageView/RawImgPageView.h
    src/ImGui/Project/PageView/CutByPatternImgPageView.cpp      src/ImGui/Project/PageView/CutByPatternImgPageView.h
    src/ImGui/Project/PageView/XlsxPageView.cpp                 src/ImGui/Project/PageView/XlsxPageView.hpp

    src/ImGui/Constructions/SelectConstructionFromTree.cpp      src/ImGui/Constructions/SelectConstructionFromTree.hpp

    src/ImGui/Img/ScalableImg.cpp                               src/ImGui/Img/ScalableImg.h

    src/ImGui/Configs/SharedConnectionConfigSetup.cpp           src/ImGui/Configs/SharedConnectionConfigSetup.hpp
    
    src/Serializer/Serializer.cpp                               src/Serializer/Serializer.h

    src/Managers/TextureManager.cpp                             src/Managers/TextureManager.h

    src/Excel/ExcelFileEditable.cpp                             src/Excel/ExcelFileEditable.h
    
    src/Utils/FileFormat.h
    src/Utils/FileSystemUtils.h
    src/Utils/MakeScreenshot.hpp                                src/Utils/MakeScreenshot.cpp      
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})

include(FetchContent)

FetchContent_Declare(
    xlnt
    GIT_REPOSITORY https://github.com/xlnt-community/xlnt.git
    GIT_TAG d657345076cdd8edaf449b17a25f0db3899b3cab
)
FetchContent_MakeAvailable(xlnt)

# set(BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
# FetchContent_Declare(
#     screen_capture_lite
#     GIT_REPOSITORY https://github.com/smasherprog/screen_capture_lite.git
#     GIT_TAG 17.1.1368
# )
# FetchContent_MakeAvailable(screen_capture_lite)

# find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
# find_package(PythonLibs REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${PYTHON_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine xlnt)
# target_link_libraries(${PROJECT_NAME} PRIVATE Engine ${PYTHON_LIBRARIES} ${PYTHON_DEBUG_LIBRARY})

# add_subdirectory(tests)

if(MSVC)
    set_target_properties(xlnt PROPERTIES FOLDER "deps")
endif()
