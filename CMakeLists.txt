cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0.22621.0 CACHE STRING "" FORCE)

project(PhoenixEngine C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_VS_PLATFORM_TOOLSET v143)

################################################################################
# Set target arch type if empty. Visual studio solution generator provides it.
################################################################################
if(NOT CMAKE_VS_PLATFORM_NAME)
    set(CMAKE_VS_PLATFORM_NAME "x64")
endif()
message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32"
     OR "${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"))
    message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
endif()

################################################################################
# Global configuration types
################################################################################
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "MinSizeRel"
    "Release"
    "RelWithDebInfo"
    CACHE STRING "" FORCE
)

################################################################################
# Global compiler options
################################################################################
if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_C_FLAGS "")
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_C_FLAGS_MINSIZEREL "")
    set(CMAKE_C_FLAGS_RELEASE "")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "")
    set(CMAKE_CXX_FLAGS "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "")
    set(CMAKE_CXX_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
endif()

################################################################################
# Global linker options
################################################################################
if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_EXE_LINKER_FLAGS "")
    set(CMAKE_MODULE_LINKER_FLAGS "")
    set(CMAKE_SHARED_LINKER_FLAGS "")
    set(CMAKE_STATIC_LINKER_FLAGS "")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_MINSIZEREL "${CMAKE_STATIC_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_STATIC_LINKER_FLAGS}")
endif()

################################################################################
# Nuget packages function stub.
################################################################################
function(use_package TARGET PACKAGE VERSION)
    message(WARNING "No implementation of use_package. Create yours. "
                    "Package \"${PACKAGE}\" with version \"${VERSION}\" "
                    "for target \"${TARGET}\" is ignored!")
endfunction()

################################################################################
# Common utils
################################################################################
include(CMake/Utils.cmake)

################################################################################
# Additional Global Settings(add specific info there)
################################################################################
include(CMake/GlobalSettingsInclude.cmake OPTIONAL)

################################################################################
# Use solution folders feature
################################################################################
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################################################################
# Sub-projects
################################################################################
add_subdirectory(luau)
add_subdirectory(Vendor)



























set(PROJECT_NAME PhoenixEngine)

################################################################################
# Source groups
################################################################################
set(Header_Files
    "src/decl/AnimationService.hpp"
    "src/decl/Debug.hpp"
    "src/decl/Editor.hpp"
    "src/decl/Engine.hpp"
    "src/decl/EntityComponent.hpp"
    "src/decl/FileRW.hpp"
    "src/decl/GlobalJsonConfig.hpp"
    "src/decl/IntersectionLib.hpp"
    "src/decl/PhysicsEngine.hpp"
    "src/decl/Reflection.hpp"
    "src/decl/ThreadManager.hpp"
    "src/decl/UserInput.hpp"
)
source_group("Header Files" FILES ${Header_Files})

set(Header_Files__asset
    "src/decl/asset/Material.hpp"
    "src/decl/asset/MeshProvider.hpp"
    "src/decl/asset/ModelLoader.hpp"
    "src/decl/asset/PrimitiveMeshes.hpp"
    "src/decl/asset/SceneFormat.hpp"
    "src/decl/asset/TextureManager.hpp"
)
source_group("Header Files\\asset" FILES ${Header_Files__asset})

set(Header_Files__datatype
    "src/decl/datatype/Color.hpp"
    "src/decl/datatype/Event.hpp"
    "src/decl/datatype/GameObject.hpp"
    "src/decl/datatype/Mesh.hpp"
    "src/decl/datatype/ValueSequence.hpp"
    "src/decl/datatype/Vector2.hpp"
    "src/decl/datatype/Vector3.hpp"
)
source_group("Header Files\\datatype" FILES ${Header_Files__datatype})

set(Header_Files__gameobject
    "src/decl/gameobject/Base3D.hpp"
    "src/decl/gameobject/Camera.hpp"
    "src/decl/gameobject/DataModel.hpp"
    "src/decl/gameobject/Example.hpp"
    "src/decl/gameobject/GameObjects.hpp"
    "src/decl/gameobject/Light.hpp"
    "src/decl/gameobject/MeshObject.hpp"
    "src/decl/gameobject/Model.hpp"
    "src/decl/gameobject/ParticleEmitter.hpp"
    "src/decl/gameobject/Primitive.hpp"
    "src/decl/gameobject/Script.hpp"
    "src/decl/gameobject/ScriptEngine.hpp"
    "src/decl/gameobject/Workspace.hpp"
)
source_group("Header Files\\gameobject" FILES ${Header_Files__gameobject})

set(Header_Files__render
    "src/decl/render/Buffer.hpp"
    "src/decl/render/GraphicsAbstractionLayer.hpp"
    "src/decl/render/Renderer.hpp"
    "src/decl/render/ShaderProgram.hpp"
)
source_group("Header Files\\render" FILES ${Header_Files__render})

set(Resource_Files
    "README.md"
)
source_group("Resource Files" FILES ${Resource_Files})

set(Resource_Files__shaders
    "resources/shaders/base2d.frag"
    "resources/shaders/base2d.vert"
    "resources/shaders/default.geom"
    "resources/shaders/dlshadowmap.frag"
    "resources/shaders/dlshadowmap.vert"
    "resources/shaders/error.frag"
    "resources/shaders/error.shp"
    "resources/shaders/particle.frag"
    "resources/shaders/particle.shp"
    "resources/shaders/particle.vert"
    "resources/shaders/pointlightshadow.frag"
    "resources/shaders/pointlightshadow.geom"
    "resources/shaders/pointlightshadow.vert"
    "resources/shaders/postprocessing.frag"
    "resources/shaders/postprocessing.shp"
    "resources/shaders/postprocessing.vert"
    "resources/shaders/skybox.frag"
    "resources/shaders/skybox.shp"
    "resources/shaders/skybox.vert"
    "resources/shaders/watertest.frag"
    "resources/shaders/watertest.geom"
    "resources/shaders/watertest.shp"
    "resources/shaders/watertest.vert"
    "resources/shaders/worldUber.frag"
    "resources/shaders/worldUber.shp"
    "resources/shaders/worldUber.vert"
)
source_group("Resource Files\\shaders" FILES ${Resource_Files__shaders})

set(Source_Files
    "src/impl/AnimationService.cpp"
    "src/impl/Debug.cpp"
    "src/impl/Editor.cpp"
    "src/impl/Engine.cpp"
    "src/impl/EntityComponent.cpp"
    "src/impl/FileRW.cpp"
    "src/impl/GlobalJsonConfig.cpp"
    "src/impl/IntersectionLib.cpp"
    "src/impl/Main.cpp"
    "src/impl/PhysicsEngine.cpp"
    "src/impl/Reflection.cpp"
    "src/impl/ThreadManager.cpp"
    "src/impl/UserInput.cpp"
)
source_group("Source Files" FILES ${Source_Files})

set(Source_Files__asset
    "src/impl/asset/Material.cpp"
    "src/impl/asset/MeshProvider.cpp"
    "src/impl/asset/ModelLoader.cpp"
    "src/impl/asset/PrimitiveMeshes.cpp"
    "src/impl/asset/SceneFormat.cpp"
    "src/impl/asset/TextureManager.cpp"
)
source_group("Source Files\\asset" FILES ${Source_Files__asset})

set(Source_Files__datatype
    "src/impl/datatype/Color.cpp"
    "src/impl/datatype/GameObject.cpp"
    "src/impl/datatype/Mesh.cpp"
    "src/impl/datatype/Vector2.cpp"
    "src/impl/datatype/Vector3.cpp"
    "src/impl/render/Buffer.cpp"
)
source_group("Source Files\\datatype" FILES ${Source_Files__datatype})

set(Source_Files__gameobject
    "src/impl/gameobject/Base3D.cpp"
    "src/impl/gameobject/Camera.cpp"
    "src/impl/gameobject/DataModel.cpp"
    "src/impl/gameobject/Example.cpp"
    "src/impl/gameobject/Light.cpp"
    "src/impl/gameobject/MeshObject.cpp"
    "src/impl/gameobject/Model.cpp"
    "src/impl/gameobject/ParticleEmitter.cpp"
    "src/impl/gameobject/Primitive.cpp"
    "src/impl/gameobject/Script.cpp"
    "src/impl/gameobject/Workspace.cpp"
    "src/impl/ScriptEngine.cpp"
)
source_group("Source Files\\gameobject" FILES ${Source_Files__gameobject})

set(Source_Files__render
    "src/impl/render/GraphicsAbstractionLayer.cpp"
    "src/impl/render/Renderer.cpp"
    "src/impl/render/ShaderProgram.cpp"
)
source_group("Source Files\\render" FILES ${Source_Files__render})

set(ALL_FILES
    ${Header_Files}
    ${Header_Files__asset}
    ${Header_Files__datatype}
    ${Header_Files__gameobject}
    ${Header_Files__render}
    ${Resource_Files}
    ${Resource_Files__shaders}
    ${Source_Files}
    ${Source_Files__asset}
    ${Source_Files__datatype}
    ${Source_Files__gameobject}
    ${Source_Files__render}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
################################################################################
# Target name
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        TARGET_NAME_DEBUG          "PhoenixEngine"
        TARGET_NAME_MINSIZEREL     "PhoenixEngine"
        TARGET_NAME_RELEASE        "PhoenixEngine"
        TARGET_NAME_RELWITHDEBINFO "PhoenixEngine"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        TARGET_NAME_DEBUG          "PhoenixEngine"
        TARGET_NAME_MINSIZEREL     "PhoenixEngine"
        TARGET_NAME_RELEASE        "PhoenixEngine"
        TARGET_NAME_RELWITHDEBINFO "PhoenixEngine"
    )
endif()
################################################################################
# Output directory
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_DIRECTORY_DEBUG          "${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug/"
        OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release/"
        OUTPUT_DIRECTORY_RELEASE        "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release/"
        OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release/"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_DIRECTORY_DEBUG          "${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug/"
        OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release/"
        OUTPUT_DIRECTORY_RELEASE        "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release/"
        OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release/"
    )
endif()
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug"
        ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release"
        ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug"
        ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release"
        ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release"
    )
endif()
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        ARCHIVE_OUTPUT_NAME_DEBUG          "PhoenixEngine"
        ARCHIVE_OUTPUT_NAME_MINSIZEREL     "PhoenixEngine"
        ARCHIVE_OUTPUT_NAME_RELEASE        "PhoenixEngine"
        ARCHIVE_OUTPUT_NAME_RELWITHDEBINFO "PhoenixEngine"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        ARCHIVE_OUTPUT_NAME_DEBUG          "PhoenixEngine"
        ARCHIVE_OUTPUT_NAME_MINSIZEREL     "PhoenixEngine"
        ARCHIVE_OUTPUT_NAME_RELEASE        "PhoenixEngine"
        ARCHIVE_OUTPUT_NAME_RELWITHDEBINFO "PhoenixEngine"
    )
endif()
################################################################################
# MSVC runtime library
################################################################################
get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
        $<$<CONFIG:Debug>:
            MultiThreadedDebugDLL
        >
        $<$<CONFIG:MinSizeRel>:
            MultiThreadedDLL
        >
        $<$<CONFIG:Release>:
            MultiThreadedDLL
        >
        $<$<CONFIG:RelWithDebInfo>:
            MultiThreadedDLL
        >
        $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:MinSizeRel>,$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
        $<$<CONFIG:Debug>:
            MultiThreadedDebugDLL
        >
        $<$<CONFIG:MinSizeRel>:
            MultiThreadedDLL
        >
        $<$<CONFIG:Release>:
            MultiThreadedDLL
        >
        $<$<CONFIG:RelWithDebInfo>:
            MultiThreadedDLL
        >
        $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:MinSizeRel>,$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
    )
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

################################################################################
# Include directories
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/decl;"
        "${CMAKE_CURRENT_SOURCE_DIR}/Vendor;"
        "${CMAKE_CURRENT_SOURCE_DIR}/luau/Common/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/luau/VM/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/decl;"
        "${CMAKE_CURRENT_SOURCE_DIR}/Vendor;"
        "${CMAKE_CURRENT_SOURCE_DIR}/luau/Common/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/luau/VM/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    )
endif()

################################################################################
# Compile definitions
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "CMAKE_INTDIR=\"Debug\""
        ">"
        "$<$<CONFIG:MinSizeRel>:"
            "CMAKE_INTDIR=\"Release\""
        ">"
        "$<$<CONFIG:Release>:"
            "CMAKE_INTDIR=\"Release\""
        ">"
        "$<$<CONFIG:RelWithDebInfo>:"
            "CMAKE_INTDIR=\"Release\""
        ">"
        "_MBCS"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "CMAKE_INTDIR=\"Debug\""
        ">"
        "$<$<CONFIG:MinSizeRel>:"
            "CMAKE_INTDIR=\"Release\""
        ">"
        "$<$<CONFIG:Release>:"
            "CMAKE_INTDIR=\"Release\""
        ">"
        "$<$<CONFIG:RelWithDebInfo>:"
            "CMAKE_INTDIR=\"Release\""
        ">"
        "_MBCS"
    )
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:MinSizeRel>:
                /Zc:__cplusplus
            >
            $<$<CONFIG:Release>:
                /Zc:__cplusplus
            >
            $<$<CONFIG:RelWithDebInfo>:
                /Zc:__cplusplus
            >
            /permissive-;
            /MP;
            /W4;
            /Zi;
            ${DEFAULT_CXX_EXCEPTION_HANDLING};
            /Y-
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:MinSizeRel>:
                /Zc:__cplusplus
            >
            $<$<CONFIG:Release>:
                /Zc:__cplusplus
            >
            $<$<CONFIG:RelWithDebInfo>:
                /Zc:__cplusplus
            >
            /permissive-;
            /MP;
            /W4;
            /Zi;
            ${DEFAULT_CXX_EXCEPTION_HANDLING};
            /Y-
        )
    endif()
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /NODEFAULTLIB:msvcrtd.lib
            >
            /DEBUG:FULL;
            /SUBSYSTEM:CONSOLE
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /NODEFAULTLIB:msvcrtd.lib
            >
            /DEBUG:FULL;
            /SUBSYSTEM:CONSOLE
        )
    endif()
endif()

################################################################################
# Dependencies
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "opengl32;"
        "SDL2;"
        "Vendor;"
        "Luau.Ast;"
        "Luau.Compiler;"
        "Luau.VM"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "opengl32;"
        "SDL2;"
        "Vendor;"
        "Luau.Ast;"
        "Luau.Compiler;"
        "Luau.VM"
    )
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    target_link_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dll;"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_link_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/dll;"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    )
endif()

set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT PhoenixEngine)