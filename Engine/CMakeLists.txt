cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0025 NEW)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.15")
    cmake_policy(SET CMP0093 NEW)
endif()

project(Engine VERSION 1.0)

set(SOURCES     
    src/Engine/Core/Application.h                               src/Engine/Core/Application.cpp         
    src/Engine/Core/Window.h                                    src/Engine/Core/Window.cpp              
    src/Engine/Core/Base.h
    src/Engine/Core/Timestep.h
    src/Engine/Core/Inputs.h
    src/Engine/Core/Assert.h
    src/Engine/Core/KeyCodes.h          
    src/Engine/Core/EntryPoint.h          
    src/Engine/Core/MouseCodes.h
    
    src/Engine/ImGui/imgui_impl_glfw.h                          src/Engine/ImGui/imgui_impl_glfw.cpp
    src/Engine/ImGui/imgui_impl_opengl3.h                       src/Engine/ImGui/imgui_impl_opengl3.cpp
    src/Engine/ImGui/imgui_impl_opengl3_loader.h            
    src/Engine/ImGui/ImGuiButtonColored.hpp                     src/Engine/ImGui/ImGuiButtonColored.cpp

    src/Engine/Textures/Texture2D.h                             src/Engine/Textures/Texture2D.cpp
    src/Engine/Textures/TextureLoader.h                         src/Engine/Textures/TextureLoader.cpp
    
    src/Engine/Events/Event.h
    src/Engine/Events/KeyEvent.h
    src/Engine/Events/MouseEvent.h 
    src/Engine/Events/WindowEvent.h
    src/Engine/Events/EventDispatcher.h 
    
    src/Engine/Layers/Layer.h                                   src/Engine/Layers/Layer.cpp             
    src/Engine/Layers/LayerStack.h                              src/Engine/Layers/LayerStack.cpp        
    src/Engine/Layers/ImGuiLayer.h                              src/Engine/Layers/ImGuiLayer.cpp        

    # src/Engine/Utils/Timer.h          
    # src/Engine/Utils/DataLoading.h            
    src/Engine/Utils/Logger.hpp                                 src/Engine/Utils/Logger.cpp         
    src/Engine/Utils/Log.hpp                                    src/Engine/Utils/Log.cpp         
    src/Engine/Utils/FileDialogs.h                              src/Engine/Utils/FileDialogs.cpp
    src/Engine/Utils/json.hpp
    src/Engine/Utils/utf8.h

    src/Platform/OpenGL/Core/GLFWWindow.h                       src/Platform/OpenGL/Core/GLFWWindow.cpp
    src/Platform/OpenGL/Core/GLFWInputs.cpp
    src/Platform/OpenGL/Textures/OGLTexture2D.h                 src/Platform/OpenGL/Textures/OGLTexture2D.cpp
    src/Platform/OpenGL/Textures/OGLCalcTextureParameters.h     src/Platform/OpenGL/Textures/OGLCalcTextureParameters.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})

add_compile_definitions(GLEW_STATIC _LIB IMGUI_DEFINE_MATH_OPERATORS RES_FOLDER="${CMAKE_SOURCE_DIR}/")

find_package(OpenGL REQUIRED)

add_library(${PROJECT_NAME} STATIC)
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES} glm args glfw glew stb imgui nfd)


include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 162896e5b9a40dc382c5c438cd12c90a5ff86ddd
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    glew
    GIT_REPOSITORY https://github.com/mihaillatyshov/glew.git
    GIT_TAG bd68b6e4b996d165451049637c1d66018f207b53
)
FetchContent_MakeAvailable(glew)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.2
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    args
    GIT_REPOSITORY https://github.com/Taywee/args.git
    GIT_TAG 6.4.7
)
FetchContent_MakeAvailable(args)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.4-docking
)
FetchContent_GetProperties(imgui)
if (NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    set(IMGUI_INCLUDE_DIR ${imgui_SOURCE_DIR}/)
    set(IMGUI_SOURCES 
        ${imgui_SOURCE_DIR}/imconfig.h
        ${imgui_SOURCE_DIR}/imgui.cpp                   ${imgui_SOURCE_DIR}/imgui.h
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_internal.h
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imstb_rectpack.h
        ${imgui_SOURCE_DIR}/imstb_textedit.h
        ${imgui_SOURCE_DIR}/imstb_truetype.h
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp

        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp   ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h
    )
    add_library(imgui STATIC)
    target_sources(imgui PRIVATE
        ${IMGUI_SOURCES}
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.h ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC ${IMGUI_INCLUDE_DIR})
    target_link_libraries(imgui glfw)
endif ()

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/mihaillatyshov/stb.git
    GIT_TAG c21c3bd687736635033267bd836168103cce59dd
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
    nativefiledialog
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG v1.3.0
)
FetchContent_MakeAvailable(nativefiledialog)

# add_subdirectory(tests)

# if(MSVC)
#     set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Engine")
#     set_target_properties(glfw PROPERTIES FOLDER "Engine/deps")
#     set_target_properties(uninstall PROPERTIES FOLDER "Engine/deps")
#     set_target_properties(update_mappings PROPERTIES FOLDER "Engine/deps")
#     set_target_properties(glew PROPERTIES FOLDER "Engine/deps")
#     set_target_properties(imgui PROPERTIES FOLDER "Engine/deps")
#     set_target_properties(nfd PROPERTIES FOLDER "Engine/deps")
# endif()
