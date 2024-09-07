cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)

project(PhoenixEngine C CXX)

# PWF2K 07/09/2024
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

if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"))
    message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
endif()

################################################################################
# Global configuration types
################################################################################
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    CACHE STRING "" FORCE
)

################################################################################
# Global compiler options
################################################################################
if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_C_FLAGS "")
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_C_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_RELEASE "")
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
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS}")
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



























set(PROJECT_NAME PhoenixEngine)

# PWFK2K 07/09/2024
set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT PhoenixEngine)

################################################################################
# Source groups
################################################################################
set(Header_Files
    "include/AnimationService.hpp"
    "include/BaseMeshes.hpp"
    "include/Debug.hpp"
    "include/Engine.hpp"
    "include/EntityComponent.hpp"
    "include/FileRW.hpp"
    "include/GlobalJsonConfig.hpp"
    "include/ModelLoader.hpp"
    "include/PhysicsEngine.hpp"
    "include/Reflection.hpp"
    "include/SceneFormat.hpp"
    "include/ThreadManager.hpp"
    "include/UserInput.hpp"
)
source_group("Header Files" FILES ${Header_Files})

set(Header_Files__datatype
    "include/datatype/Buffer.hpp"
    "include/datatype/Color.hpp"
    "include/datatype/Event.cpp"
    "include/datatype/Event.hpp"
    "include/datatype/GameObject.hpp"
    "include/datatype/Mesh.hpp"
    "include/datatype/ValueSequence.hpp"
    "include/datatype/Vector2.hpp"
    "include/datatype/Vector3.hpp"
)
source_group("Header Files\\datatype" FILES ${Header_Files__datatype})

set(Header_Files__editor
    "include/editor/Editor.hpp"
    "include/editor/intersectionlib.hpp"
)
source_group("Header Files\\editor" FILES ${Header_Files__editor})

set(Header_Files__gameobject
    "include/gameobject/Base3D.hpp"
    "include/gameobject/Camera.hpp"
    "include/gameobject/DataModel.hpp"
    "include/gameobject/GameObjects.hpp"
    "include/gameobject/Light.hpp"
    "include/gameobject/MeshObject.hpp"
    "include/gameobject/Model.hpp"
    "include/gameobject/ParticleEmitter.hpp"
    "include/gameobject/Primitive.hpp"
    "include/gameobject/Script.hpp"
    "include/gameobject/Workspace.hpp"
)
source_group("Header Files\\gameobject" FILES ${Header_Files__gameobject})

set(Header_Files__render
    "include/render/GraphicsAbstractionLayer.hpp"
    "include/render/Material.hpp"
    "include/render/Renderer.hpp"
    "include/render/ShaderProgram.hpp"
    "include/render/TextureManager.hpp"
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
    "src/AnimationService.cpp"
    "src/BaseMeshes.cpp"
    "src/Debug.cpp"
    "src/Engine.cpp"
    "src/EntityComponent.cpp"
    "src/FileRW.cpp"
    "src/GlobalJsonConfig.cpp"
    "src/Main.cpp"
    "src/ModelLoader.cpp"
    "src/PhysicsEngine.cpp"
    "src/Reflection.cpp"
    "src/SceneFormat.cpp"
    "src/ThreadManager.cpp"
    "src/UserInput.cpp"
)
source_group("Source Files" FILES ${Source_Files})

set(Source_Files__datatype
    "src/datatype/Buffer.cpp"
    "src/datatype/Color.cpp"
    "src/datatype/GameObject.cpp"
    "src/datatype/Mesh.cpp"
    "src/datatype/Vector2.cpp"
    "src/datatype/Vector3.cpp"
)
source_group("Source Files\\datatype" FILES ${Source_Files__datatype})

set(Source_Files__editor
    "src/editor/editor.cpp"
    "src/editor/intersectionlib.cpp"
)
source_group("Source Files\\editor" FILES ${Source_Files__editor})

set(Source_Files__gameobject
    "src/gameobject/Base3D.cpp"
    "src/gameobject/Camera.cpp"
    "src/gameobject/DataModel.cpp"
    "src/gameobject/Light.cpp"
    "src/gameobject/MeshObject.cpp"
    "src/gameobject/Model.cpp"
    "src/gameobject/ParticleEmitter.cpp"
    "src/gameobject/Primitive.cpp"
    "src/gameobject/Script.cpp"
    "src/gameobject/Workspace.cpp"
)
source_group("Source Files\\gameobject" FILES ${Source_Files__gameobject})

set(Source_Files__render
    "src/render/GraphicsAbstractionLayer.cpp"
    "src/render/Material.cpp"
    "src/render/Renderer.cpp"
    "src/render/ShaderProgram.cpp"
    "src/render/TextureManager.cpp"
)
source_group("Source Files\\render" FILES ${Source_Files__render})

set(Vendor_Files
    "vendor/nljson.hpp"
    "vendor/stb.cpp"
    "vendor/stb_image.h"
)
source_group("Vendor Files" FILES ${Vendor_Files})

set(Vendor_Files__SDL2
    "vendor/SDL2/begin_code.h"
    "vendor/SDL2/close_code.h"
    "vendor/SDL2/SDL.h"
    "vendor/SDL2/SDL_assert.h"
    "vendor/SDL2/SDL_atomic.h"
    "vendor/SDL2/SDL_audio.h"
    "vendor/SDL2/SDL_bits.h"
    "vendor/SDL2/SDL_blendmode.h"
    "vendor/SDL2/SDL_clipboard.h"
    "vendor/SDL2/SDL_config.h"
    "vendor/SDL2/SDL_cpuinfo.h"
    "vendor/SDL2/SDL_egl.h"
    "vendor/SDL2/SDL_endian.h"
    "vendor/SDL2/SDL_error.h"
    "vendor/SDL2/SDL_events.h"
    "vendor/SDL2/SDL_filesystem.h"
    "vendor/SDL2/SDL_gamecontroller.h"
    "vendor/SDL2/SDL_gesture.h"
    "vendor/SDL2/SDL_guid.h"
    "vendor/SDL2/SDL_haptic.h"
    "vendor/SDL2/SDL_hidapi.h"
    "vendor/SDL2/SDL_hints.h"
    "vendor/SDL2/SDL_joystick.h"
    "vendor/SDL2/SDL_keyboard.h"
    "vendor/SDL2/SDL_keycode.h"
    "vendor/SDL2/SDL_loadso.h"
    "vendor/SDL2/SDL_locale.h"
    "vendor/SDL2/SDL_log.h"
    "vendor/SDL2/SDL_main.h"
    "vendor/SDL2/SDL_messagebox.h"
    "vendor/SDL2/SDL_metal.h"
    "vendor/SDL2/SDL_misc.h"
    "vendor/SDL2/SDL_mouse.h"
    "vendor/SDL2/SDL_mutex.h"
    "vendor/SDL2/SDL_name.h"
    "vendor/SDL2/SDL_opengl.h"
    "vendor/SDL2/SDL_opengl_glext.h"
    "vendor/SDL2/SDL_opengles.h"
    "vendor/SDL2/SDL_opengles2.h"
    "vendor/SDL2/SDL_opengles2_gl2.h"
    "vendor/SDL2/SDL_opengles2_gl2ext.h"
    "vendor/SDL2/SDL_opengles2_gl2platform.h"
    "vendor/SDL2/SDL_opengles2_khrplatform.h"
    "vendor/SDL2/SDL_pixels.h"
    "vendor/SDL2/SDL_platform.h"
    "vendor/SDL2/SDL_power.h"
    "vendor/SDL2/SDL_quit.h"
    "vendor/SDL2/SDL_rect.h"
    "vendor/SDL2/SDL_render.h"
    "vendor/SDL2/SDL_revision.h"
    "vendor/SDL2/SDL_rwops.h"
    "vendor/SDL2/SDL_scancode.h"
    "vendor/SDL2/SDL_sensor.h"
    "vendor/SDL2/SDL_shape.h"
    "vendor/SDL2/SDL_stdinc.h"
    "vendor/SDL2/SDL_surface.h"
    "vendor/SDL2/SDL_system.h"
    "vendor/SDL2/SDL_syswm.h"
    "vendor/SDL2/SDL_test.h"
    "vendor/SDL2/SDL_test_assert.h"
    "vendor/SDL2/SDL_test_common.h"
    "vendor/SDL2/SDL_test_compare.h"
    "vendor/SDL2/SDL_test_crc32.h"
    "vendor/SDL2/SDL_test_font.h"
    "vendor/SDL2/SDL_test_fuzzer.h"
    "vendor/SDL2/SDL_test_harness.h"
    "vendor/SDL2/SDL_test_images.h"
    "vendor/SDL2/SDL_test_log.h"
    "vendor/SDL2/SDL_test_md5.h"
    "vendor/SDL2/SDL_test_memory.h"
    "vendor/SDL2/SDL_test_random.h"
    "vendor/SDL2/SDL_thread.h"
    "vendor/SDL2/SDL_timer.h"
    "vendor/SDL2/SDL_touch.h"
    "vendor/SDL2/SDL_types.h"
    "vendor/SDL2/SDL_version.h"
    "vendor/SDL2/SDL_video.h"
    "vendor/SDL2/SDL_vulkan.h"
)
source_group("Vendor Files\\SDL2" FILES ${Vendor_Files__SDL2})

set(Vendor_Files__glad
    "vendor/glad/gl.c"
    "vendor/glad/gl.h"
)
source_group("Vendor Files\\glad" FILES ${Vendor_Files__glad})

set(Vendor_Files__glad__KHR
    "vendor/glad/KHR/khrplatform.h"
)
source_group("Vendor Files\\glad\\KHR" FILES ${Vendor_Files__glad__KHR})

set(Vendor_Files__glm
    "vendor/glm/common.hpp"
    "vendor/glm/copying.txt"
    "vendor/glm/exponential.hpp"
    "vendor/glm/ext.hpp"
    "vendor/glm/fwd.hpp"
    "vendor/glm/geometric.hpp"
    "vendor/glm/glm.cppm"
    "vendor/glm/glm.hpp"
    "vendor/glm/integer.hpp"
    "vendor/glm/mat2x2.hpp"
    "vendor/glm/mat2x3.hpp"
    "vendor/glm/mat2x4.hpp"
    "vendor/glm/mat3x2.hpp"
    "vendor/glm/mat3x3.hpp"
    "vendor/glm/mat3x4.hpp"
    "vendor/glm/mat4x2.hpp"
    "vendor/glm/mat4x3.hpp"
    "vendor/glm/mat4x4.hpp"
    "vendor/glm/matrix.hpp"
    "vendor/glm/packing.hpp"
    "vendor/glm/trigonometric.hpp"
    "vendor/glm/vec2.hpp"
    "vendor/glm/vec3.hpp"
    "vendor/glm/vec4.hpp"
    "vendor/glm/vector_relational.hpp"
)
source_group("Vendor Files\\glm" FILES ${Vendor_Files__glm})

set(Vendor_Files__glm__detail
    "vendor/glm/detail/_features.hpp"
    "vendor/glm/detail/_fixes.hpp"
    "vendor/glm/detail/_noise.hpp"
    "vendor/glm/detail/_swizzle.hpp"
    "vendor/glm/detail/_swizzle_func.hpp"
    "vendor/glm/detail/_vectorize.hpp"
    "vendor/glm/detail/compute_common.hpp"
    "vendor/glm/detail/compute_vector_decl.hpp"
    "vendor/glm/detail/compute_vector_relational.hpp"
    "vendor/glm/detail/func_common.inl"
    "vendor/glm/detail/func_common_simd.inl"
    "vendor/glm/detail/func_exponential.inl"
    "vendor/glm/detail/func_exponential_simd.inl"
    "vendor/glm/detail/func_geometric.inl"
    "vendor/glm/detail/func_geometric_simd.inl"
    "vendor/glm/detail/func_integer.inl"
    "vendor/glm/detail/func_integer_simd.inl"
    "vendor/glm/detail/func_matrix.inl"
    "vendor/glm/detail/func_matrix_simd.inl"
    "vendor/glm/detail/func_packing.inl"
    "vendor/glm/detail/func_packing_simd.inl"
    "vendor/glm/detail/func_trigonometric.inl"
    "vendor/glm/detail/func_trigonometric_simd.inl"
    "vendor/glm/detail/func_vector_relational.inl"
    "vendor/glm/detail/func_vector_relational_simd.inl"
    "vendor/glm/detail/glm.cpp"
    "vendor/glm/detail/qualifier.hpp"
    "vendor/glm/detail/setup.hpp"
    "vendor/glm/detail/type_float.hpp"
    "vendor/glm/detail/type_half.hpp"
    "vendor/glm/detail/type_half.inl"
    "vendor/glm/detail/type_mat2x2.hpp"
    "vendor/glm/detail/type_mat2x2.inl"
    "vendor/glm/detail/type_mat2x3.hpp"
    "vendor/glm/detail/type_mat2x3.inl"
    "vendor/glm/detail/type_mat2x4.hpp"
    "vendor/glm/detail/type_mat2x4.inl"
    "vendor/glm/detail/type_mat3x2.hpp"
    "vendor/glm/detail/type_mat3x2.inl"
    "vendor/glm/detail/type_mat3x3.hpp"
    "vendor/glm/detail/type_mat3x3.inl"
    "vendor/glm/detail/type_mat3x4.hpp"
    "vendor/glm/detail/type_mat3x4.inl"
    "vendor/glm/detail/type_mat4x2.hpp"
    "vendor/glm/detail/type_mat4x2.inl"
    "vendor/glm/detail/type_mat4x3.hpp"
    "vendor/glm/detail/type_mat4x3.inl"
    "vendor/glm/detail/type_mat4x4.hpp"
    "vendor/glm/detail/type_mat4x4.inl"
    "vendor/glm/detail/type_mat4x4_simd.inl"
    "vendor/glm/detail/type_quat.hpp"
    "vendor/glm/detail/type_quat.inl"
    "vendor/glm/detail/type_quat_simd.inl"
    "vendor/glm/detail/type_vec1.hpp"
    "vendor/glm/detail/type_vec1.inl"
    "vendor/glm/detail/type_vec2.hpp"
    "vendor/glm/detail/type_vec2.inl"
    "vendor/glm/detail/type_vec3.hpp"
    "vendor/glm/detail/type_vec3.inl"
    "vendor/glm/detail/type_vec4.hpp"
    "vendor/glm/detail/type_vec4.inl"
    "vendor/glm/detail/type_vec4_simd.inl"
)
source_group("Vendor Files\\glm\\detail" FILES ${Vendor_Files__glm__detail})

set(Vendor_Files__glm__ext
    "vendor/glm/ext/_matrix_vectorize.hpp"
    "vendor/glm/ext/matrix_clip_space.hpp"
    "vendor/glm/ext/matrix_clip_space.inl"
    "vendor/glm/ext/matrix_common.hpp"
    "vendor/glm/ext/matrix_common.inl"
    "vendor/glm/ext/matrix_double2x2.hpp"
    "vendor/glm/ext/matrix_double2x2_precision.hpp"
    "vendor/glm/ext/matrix_double2x3.hpp"
    "vendor/glm/ext/matrix_double2x3_precision.hpp"
    "vendor/glm/ext/matrix_double2x4.hpp"
    "vendor/glm/ext/matrix_double2x4_precision.hpp"
    "vendor/glm/ext/matrix_double3x2.hpp"
    "vendor/glm/ext/matrix_double3x2_precision.hpp"
    "vendor/glm/ext/matrix_double3x3.hpp"
    "vendor/glm/ext/matrix_double3x3_precision.hpp"
    "vendor/glm/ext/matrix_double3x4.hpp"
    "vendor/glm/ext/matrix_double3x4_precision.hpp"
    "vendor/glm/ext/matrix_double4x2.hpp"
    "vendor/glm/ext/matrix_double4x2_precision.hpp"
    "vendor/glm/ext/matrix_double4x3.hpp"
    "vendor/glm/ext/matrix_double4x3_precision.hpp"
    "vendor/glm/ext/matrix_double4x4.hpp"
    "vendor/glm/ext/matrix_double4x4_precision.hpp"
    "vendor/glm/ext/matrix_float2x2.hpp"
    "vendor/glm/ext/matrix_float2x2_precision.hpp"
    "vendor/glm/ext/matrix_float2x3.hpp"
    "vendor/glm/ext/matrix_float2x3_precision.hpp"
    "vendor/glm/ext/matrix_float2x4.hpp"
    "vendor/glm/ext/matrix_float2x4_precision.hpp"
    "vendor/glm/ext/matrix_float3x2.hpp"
    "vendor/glm/ext/matrix_float3x2_precision.hpp"
    "vendor/glm/ext/matrix_float3x3.hpp"
    "vendor/glm/ext/matrix_float3x3_precision.hpp"
    "vendor/glm/ext/matrix_float3x4.hpp"
    "vendor/glm/ext/matrix_float3x4_precision.hpp"
    "vendor/glm/ext/matrix_float4x2.hpp"
    "vendor/glm/ext/matrix_float4x2_precision.hpp"
    "vendor/glm/ext/matrix_float4x3.hpp"
    "vendor/glm/ext/matrix_float4x3_precision.hpp"
    "vendor/glm/ext/matrix_float4x4.hpp"
    "vendor/glm/ext/matrix_float4x4_precision.hpp"
    "vendor/glm/ext/matrix_int2x2.hpp"
    "vendor/glm/ext/matrix_int2x2_sized.hpp"
    "vendor/glm/ext/matrix_int2x3.hpp"
    "vendor/glm/ext/matrix_int2x3_sized.hpp"
    "vendor/glm/ext/matrix_int2x4.hpp"
    "vendor/glm/ext/matrix_int2x4_sized.hpp"
    "vendor/glm/ext/matrix_int3x2.hpp"
    "vendor/glm/ext/matrix_int3x2_sized.hpp"
    "vendor/glm/ext/matrix_int3x3.hpp"
    "vendor/glm/ext/matrix_int3x3_sized.hpp"
    "vendor/glm/ext/matrix_int3x4.hpp"
    "vendor/glm/ext/matrix_int3x4_sized.hpp"
    "vendor/glm/ext/matrix_int4x2.hpp"
    "vendor/glm/ext/matrix_int4x2_sized.hpp"
    "vendor/glm/ext/matrix_int4x3.hpp"
    "vendor/glm/ext/matrix_int4x3_sized.hpp"
    "vendor/glm/ext/matrix_int4x4.hpp"
    "vendor/glm/ext/matrix_int4x4_sized.hpp"
    "vendor/glm/ext/matrix_integer.hpp"
    "vendor/glm/ext/matrix_integer.inl"
    "vendor/glm/ext/matrix_projection.hpp"
    "vendor/glm/ext/matrix_projection.inl"
    "vendor/glm/ext/matrix_relational.hpp"
    "vendor/glm/ext/matrix_relational.inl"
    "vendor/glm/ext/matrix_transform.hpp"
    "vendor/glm/ext/matrix_transform.inl"
    "vendor/glm/ext/matrix_uint2x2.hpp"
    "vendor/glm/ext/matrix_uint2x2_sized.hpp"
    "vendor/glm/ext/matrix_uint2x3.hpp"
    "vendor/glm/ext/matrix_uint2x3_sized.hpp"
    "vendor/glm/ext/matrix_uint2x4.hpp"
    "vendor/glm/ext/matrix_uint2x4_sized.hpp"
    "vendor/glm/ext/matrix_uint3x2.hpp"
    "vendor/glm/ext/matrix_uint3x2_sized.hpp"
    "vendor/glm/ext/matrix_uint3x3.hpp"
    "vendor/glm/ext/matrix_uint3x3_sized.hpp"
    "vendor/glm/ext/matrix_uint3x4.hpp"
    "vendor/glm/ext/matrix_uint3x4_sized.hpp"
    "vendor/glm/ext/matrix_uint4x2.hpp"
    "vendor/glm/ext/matrix_uint4x2_sized.hpp"
    "vendor/glm/ext/matrix_uint4x3.hpp"
    "vendor/glm/ext/matrix_uint4x3_sized.hpp"
    "vendor/glm/ext/matrix_uint4x4.hpp"
    "vendor/glm/ext/matrix_uint4x4_sized.hpp"
    "vendor/glm/ext/quaternion_common.hpp"
    "vendor/glm/ext/quaternion_common.inl"
    "vendor/glm/ext/quaternion_common_simd.inl"
    "vendor/glm/ext/quaternion_double.hpp"
    "vendor/glm/ext/quaternion_double_precision.hpp"
    "vendor/glm/ext/quaternion_exponential.hpp"
    "vendor/glm/ext/quaternion_exponential.inl"
    "vendor/glm/ext/quaternion_float.hpp"
    "vendor/glm/ext/quaternion_float_precision.hpp"
    "vendor/glm/ext/quaternion_geometric.hpp"
    "vendor/glm/ext/quaternion_geometric.inl"
    "vendor/glm/ext/quaternion_relational.hpp"
    "vendor/glm/ext/quaternion_relational.inl"
    "vendor/glm/ext/quaternion_transform.hpp"
    "vendor/glm/ext/quaternion_transform.inl"
    "vendor/glm/ext/quaternion_trigonometric.hpp"
    "vendor/glm/ext/quaternion_trigonometric.inl"
    "vendor/glm/ext/scalar_common.hpp"
    "vendor/glm/ext/scalar_common.inl"
    "vendor/glm/ext/scalar_constants.hpp"
    "vendor/glm/ext/scalar_constants.inl"
    "vendor/glm/ext/scalar_int_sized.hpp"
    "vendor/glm/ext/scalar_integer.hpp"
    "vendor/glm/ext/scalar_integer.inl"
    "vendor/glm/ext/scalar_packing.hpp"
    "vendor/glm/ext/scalar_packing.inl"
    "vendor/glm/ext/scalar_reciprocal.hpp"
    "vendor/glm/ext/scalar_reciprocal.inl"
    "vendor/glm/ext/scalar_relational.hpp"
    "vendor/glm/ext/scalar_relational.inl"
    "vendor/glm/ext/scalar_uint_sized.hpp"
    "vendor/glm/ext/scalar_ulp.hpp"
    "vendor/glm/ext/scalar_ulp.inl"
    "vendor/glm/ext/vector_bool1.hpp"
    "vendor/glm/ext/vector_bool1_precision.hpp"
    "vendor/glm/ext/vector_bool2.hpp"
    "vendor/glm/ext/vector_bool2_precision.hpp"
    "vendor/glm/ext/vector_bool3.hpp"
    "vendor/glm/ext/vector_bool3_precision.hpp"
    "vendor/glm/ext/vector_bool4.hpp"
    "vendor/glm/ext/vector_bool4_precision.hpp"
    "vendor/glm/ext/vector_common.hpp"
    "vendor/glm/ext/vector_common.inl"
    "vendor/glm/ext/vector_double1.hpp"
    "vendor/glm/ext/vector_double1_precision.hpp"
    "vendor/glm/ext/vector_double2.hpp"
    "vendor/glm/ext/vector_double2_precision.hpp"
    "vendor/glm/ext/vector_double3.hpp"
    "vendor/glm/ext/vector_double3_precision.hpp"
    "vendor/glm/ext/vector_double4.hpp"
    "vendor/glm/ext/vector_double4_precision.hpp"
    "vendor/glm/ext/vector_float1.hpp"
    "vendor/glm/ext/vector_float1_precision.hpp"
    "vendor/glm/ext/vector_float2.hpp"
    "vendor/glm/ext/vector_float2_precision.hpp"
    "vendor/glm/ext/vector_float3.hpp"
    "vendor/glm/ext/vector_float3_precision.hpp"
    "vendor/glm/ext/vector_float4.hpp"
    "vendor/glm/ext/vector_float4_precision.hpp"
    "vendor/glm/ext/vector_int1.hpp"
    "vendor/glm/ext/vector_int1_sized.hpp"
    "vendor/glm/ext/vector_int2.hpp"
    "vendor/glm/ext/vector_int2_sized.hpp"
    "vendor/glm/ext/vector_int3.hpp"
    "vendor/glm/ext/vector_int3_sized.hpp"
    "vendor/glm/ext/vector_int4.hpp"
    "vendor/glm/ext/vector_int4_sized.hpp"
    "vendor/glm/ext/vector_integer.hpp"
    "vendor/glm/ext/vector_integer.inl"
    "vendor/glm/ext/vector_packing.hpp"
    "vendor/glm/ext/vector_packing.inl"
    "vendor/glm/ext/vector_reciprocal.hpp"
    "vendor/glm/ext/vector_reciprocal.inl"
    "vendor/glm/ext/vector_relational.hpp"
    "vendor/glm/ext/vector_relational.inl"
    "vendor/glm/ext/vector_uint1.hpp"
    "vendor/glm/ext/vector_uint1_sized.hpp"
    "vendor/glm/ext/vector_uint2.hpp"
    "vendor/glm/ext/vector_uint2_sized.hpp"
    "vendor/glm/ext/vector_uint3.hpp"
    "vendor/glm/ext/vector_uint3_sized.hpp"
    "vendor/glm/ext/vector_uint4.hpp"
    "vendor/glm/ext/vector_uint4_sized.hpp"
    "vendor/glm/ext/vector_ulp.hpp"
    "vendor/glm/ext/vector_ulp.inl"
)
source_group("Vendor Files\\glm\\ext" FILES ${Vendor_Files__glm__ext})

set(Vendor_Files__glm__gtc
    "vendor/glm/gtc/bitfield.hpp"
    "vendor/glm/gtc/bitfield.inl"
    "vendor/glm/gtc/color_space.hpp"
    "vendor/glm/gtc/color_space.inl"
    "vendor/glm/gtc/constants.hpp"
    "vendor/glm/gtc/constants.inl"
    "vendor/glm/gtc/epsilon.hpp"
    "vendor/glm/gtc/epsilon.inl"
    "vendor/glm/gtc/integer.hpp"
    "vendor/glm/gtc/integer.inl"
    "vendor/glm/gtc/matrix_access.hpp"
    "vendor/glm/gtc/matrix_access.inl"
    "vendor/glm/gtc/matrix_integer.hpp"
    "vendor/glm/gtc/matrix_inverse.hpp"
    "vendor/glm/gtc/matrix_inverse.inl"
    "vendor/glm/gtc/matrix_transform.hpp"
    "vendor/glm/gtc/matrix_transform.inl"
    "vendor/glm/gtc/noise.hpp"
    "vendor/glm/gtc/noise.inl"
    "vendor/glm/gtc/packing.hpp"
    "vendor/glm/gtc/packing.inl"
    "vendor/glm/gtc/quaternion.hpp"
    "vendor/glm/gtc/quaternion.inl"
    "vendor/glm/gtc/quaternion_simd.inl"
    "vendor/glm/gtc/random.hpp"
    "vendor/glm/gtc/random.inl"
    "vendor/glm/gtc/reciprocal.hpp"
    "vendor/glm/gtc/round.hpp"
    "vendor/glm/gtc/round.inl"
    "vendor/glm/gtc/type_aligned.hpp"
    "vendor/glm/gtc/type_precision.hpp"
    "vendor/glm/gtc/type_precision.inl"
    "vendor/glm/gtc/type_ptr.hpp"
    "vendor/glm/gtc/type_ptr.inl"
    "vendor/glm/gtc/ulp.hpp"
    "vendor/glm/gtc/ulp.inl"
    "vendor/glm/gtc/vec1.hpp"
)
source_group("Vendor Files\\glm\\gtc" FILES ${Vendor_Files__glm__gtc})

set(Vendor_Files__glm__gtx
    "vendor/glm/gtx/associated_min_max.hpp"
    "vendor/glm/gtx/associated_min_max.inl"
    "vendor/glm/gtx/bit.hpp"
    "vendor/glm/gtx/bit.inl"
    "vendor/glm/gtx/closest_point.hpp"
    "vendor/glm/gtx/closest_point.inl"
    "vendor/glm/gtx/color_encoding.hpp"
    "vendor/glm/gtx/color_encoding.inl"
    "vendor/glm/gtx/color_space.hpp"
    "vendor/glm/gtx/color_space.inl"
    "vendor/glm/gtx/color_space_YCoCg.hpp"
    "vendor/glm/gtx/color_space_YCoCg.inl"
    "vendor/glm/gtx/common.hpp"
    "vendor/glm/gtx/common.inl"
    "vendor/glm/gtx/compatibility.hpp"
    "vendor/glm/gtx/compatibility.inl"
    "vendor/glm/gtx/component_wise.hpp"
    "vendor/glm/gtx/component_wise.inl"
    "vendor/glm/gtx/dual_quaternion.hpp"
    "vendor/glm/gtx/dual_quaternion.inl"
    "vendor/glm/gtx/easing.hpp"
    "vendor/glm/gtx/easing.inl"
    "vendor/glm/gtx/euler_angles.hpp"
    "vendor/glm/gtx/euler_angles.inl"
    "vendor/glm/gtx/extend.hpp"
    "vendor/glm/gtx/extend.inl"
    "vendor/glm/gtx/extended_min_max.hpp"
    "vendor/glm/gtx/extended_min_max.inl"
    "vendor/glm/gtx/exterior_product.hpp"
    "vendor/glm/gtx/exterior_product.inl"
    "vendor/glm/gtx/fast_exponential.hpp"
    "vendor/glm/gtx/fast_exponential.inl"
    "vendor/glm/gtx/fast_square_root.hpp"
    "vendor/glm/gtx/fast_square_root.inl"
    "vendor/glm/gtx/fast_trigonometry.hpp"
    "vendor/glm/gtx/fast_trigonometry.inl"
    "vendor/glm/gtx/float_notmalize.inl"
    "vendor/glm/gtx/functions.hpp"
    "vendor/glm/gtx/functions.inl"
    "vendor/glm/gtx/gradient_paint.hpp"
    "vendor/glm/gtx/gradient_paint.inl"
    "vendor/glm/gtx/handed_coordinate_space.hpp"
    "vendor/glm/gtx/handed_coordinate_space.inl"
    "vendor/glm/gtx/hash.hpp"
    "vendor/glm/gtx/hash.inl"
    "vendor/glm/gtx/integer.hpp"
    "vendor/glm/gtx/integer.inl"
    "vendor/glm/gtx/intersect.hpp"
    "vendor/glm/gtx/intersect.inl"
    "vendor/glm/gtx/io.hpp"
    "vendor/glm/gtx/io.inl"
    "vendor/glm/gtx/log_base.hpp"
    "vendor/glm/gtx/log_base.inl"
    "vendor/glm/gtx/matrix_cross_product.hpp"
    "vendor/glm/gtx/matrix_cross_product.inl"
    "vendor/glm/gtx/matrix_decompose.hpp"
    "vendor/glm/gtx/matrix_decompose.inl"
    "vendor/glm/gtx/matrix_factorisation.hpp"
    "vendor/glm/gtx/matrix_factorisation.inl"
    "vendor/glm/gtx/matrix_interpolation.hpp"
    "vendor/glm/gtx/matrix_interpolation.inl"
    "vendor/glm/gtx/matrix_major_storage.hpp"
    "vendor/glm/gtx/matrix_major_storage.inl"
    "vendor/glm/gtx/matrix_operation.hpp"
    "vendor/glm/gtx/matrix_operation.inl"
    "vendor/glm/gtx/matrix_query.hpp"
    "vendor/glm/gtx/matrix_query.inl"
    "vendor/glm/gtx/matrix_transform_2d.hpp"
    "vendor/glm/gtx/matrix_transform_2d.inl"
    "vendor/glm/gtx/mixed_product.hpp"
    "vendor/glm/gtx/mixed_product.inl"
    "vendor/glm/gtx/norm.hpp"
    "vendor/glm/gtx/norm.inl"
    "vendor/glm/gtx/normal.hpp"
    "vendor/glm/gtx/normal.inl"
    "vendor/glm/gtx/normalize_dot.hpp"
    "vendor/glm/gtx/normalize_dot.inl"
    "vendor/glm/gtx/number_precision.hpp"
    "vendor/glm/gtx/optimum_pow.hpp"
    "vendor/glm/gtx/optimum_pow.inl"
    "vendor/glm/gtx/orthonormalize.hpp"
    "vendor/glm/gtx/orthonormalize.inl"
    "vendor/glm/gtx/pca.hpp"
    "vendor/glm/gtx/pca.inl"
    "vendor/glm/gtx/perpendicular.hpp"
    "vendor/glm/gtx/perpendicular.inl"
    "vendor/glm/gtx/polar_coordinates.hpp"
    "vendor/glm/gtx/polar_coordinates.inl"
    "vendor/glm/gtx/projection.hpp"
    "vendor/glm/gtx/projection.inl"
    "vendor/glm/gtx/quaternion.hpp"
    "vendor/glm/gtx/quaternion.inl"
    "vendor/glm/gtx/range.hpp"
    "vendor/glm/gtx/raw_data.hpp"
    "vendor/glm/gtx/raw_data.inl"
    "vendor/glm/gtx/rotate_normalized_axis.hpp"
    "vendor/glm/gtx/rotate_normalized_axis.inl"
    "vendor/glm/gtx/rotate_vector.hpp"
    "vendor/glm/gtx/rotate_vector.inl"
    "vendor/glm/gtx/scalar_multiplication.hpp"
    "vendor/glm/gtx/scalar_relational.hpp"
    "vendor/glm/gtx/scalar_relational.inl"
    "vendor/glm/gtx/spline.hpp"
    "vendor/glm/gtx/spline.inl"
    "vendor/glm/gtx/std_based_type.hpp"
    "vendor/glm/gtx/std_based_type.inl"
    "vendor/glm/gtx/string_cast.hpp"
    "vendor/glm/gtx/string_cast.inl"
    "vendor/glm/gtx/texture.hpp"
    "vendor/glm/gtx/texture.inl"
    "vendor/glm/gtx/transform.hpp"
    "vendor/glm/gtx/transform.inl"
    "vendor/glm/gtx/transform2.hpp"
    "vendor/glm/gtx/transform2.inl"
    "vendor/glm/gtx/type_aligned.hpp"
    "vendor/glm/gtx/type_aligned.inl"
    "vendor/glm/gtx/type_trait.hpp"
    "vendor/glm/gtx/type_trait.inl"
    "vendor/glm/gtx/vec_swizzle.hpp"
    "vendor/glm/gtx/vector_angle.hpp"
    "vendor/glm/gtx/vector_angle.inl"
    "vendor/glm/gtx/vector_query.hpp"
    "vendor/glm/gtx/vector_query.inl"
    "vendor/glm/gtx/wrap.hpp"
    "vendor/glm/gtx/wrap.inl"
)
source_group("Vendor Files\\glm\\gtx" FILES ${Vendor_Files__glm__gtx})

set(Vendor_Files__glm__simd
    "vendor/glm/simd/common.h"
    "vendor/glm/simd/exponential.h"
    "vendor/glm/simd/geometric.h"
    "vendor/glm/simd/integer.h"
    "vendor/glm/simd/matrix.h"
    "vendor/glm/simd/neon.h"
    "vendor/glm/simd/packing.h"
    "vendor/glm/simd/platform.h"
    "vendor/glm/simd/trigonometric.h"
    "vendor/glm/simd/vector_relational.h"
)
source_group("Vendor Files\\glm\\simd" FILES ${Vendor_Files__glm__simd})

set(Vendor_Files__imgui
    "vendor/imgui/imconfig.h"
    "vendor/imgui/imgui.cpp"
    "vendor/imgui/imgui.h"
    "vendor/imgui/imgui_demo.cpp"
    "vendor/imgui/imgui_draw.cpp"
    "vendor/imgui/imgui_internal.h"
    "vendor/imgui/imgui_tables.cpp"
    "vendor/imgui/imgui_widgets.cpp"
    "vendor/imgui/imstb_rectpack.h"
    "vendor/imgui/imstb_textedit.h"
    "vendor/imgui/imstb_truetype.h"
    "vendor/imgui/LICENSE.txt"
)
source_group("Vendor Files\\imgui" FILES ${Vendor_Files__imgui})

set(Vendor_Files__imgui__backends
    "vendor/imgui/backends/imgui_impl_opengl3.cpp"
    "vendor/imgui/backends/imgui_impl_opengl3.h"
    "vendor/imgui/backends/imgui_impl_opengl3_loader.h"
    "vendor/imgui/backends/imgui_impl_sdl2.cpp"
    "vendor/imgui/backends/imgui_impl_sdl2.h"
)
source_group("Vendor Files\\imgui\\backends" FILES ${Vendor_Files__imgui__backends})

set(ALL_FILES
    ${Header_Files}
    ${Header_Files__datatype}
    ${Header_Files__editor}
    ${Header_Files__gameobject}
    ${Header_Files__render}
    ${Resource_Files}
    ${Resource_Files__shaders}
    ${Source_Files}
    ${Source_Files__datatype}
    ${Source_Files__editor}
    ${Source_Files__gameobject}
    ${Source_Files__render}
    ${Vendor_Files}
    ${Vendor_Files__SDL2}
    ${Vendor_Files__glad}
    ${Vendor_Files__glad__KHR}
    ${Vendor_Files__glm}
    ${Vendor_Files__glm__detail}
    ${Vendor_Files__glm__ext}
    ${Vendor_Files__glm__gtc}
    ${Vendor_Files__glm__gtx}
    ${Vendor_Files__glm__simd}
    ${Vendor_Files__imgui}
    ${Vendor_Files__imgui__backends}
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
set_target_properties(${PROJECT_NAME} PROPERTIES
    TARGET_NAME_DEBUG   "PhoenixEngine"
    TARGET_NAME_RELEASE "PhoenixEngine"
)
################################################################################
# Output directory
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_DIRECTORY_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug/"
    OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release/"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/x64/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_NAME_DEBUG   "PhoenixEngine"
    ARCHIVE_OUTPUT_NAME_RELEASE "PhoenixEngine"
)
################################################################################
# MSVC runtime library
################################################################################
get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
    $<$<CONFIG:Debug>:
        MultiThreadedDebugDLL
    >
    $<$<CONFIG:Release>:
        MultiThreadedDLL
    >
    $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
)
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

################################################################################
# Include directories
################################################################################
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.;"
    "${CMAKE_CURRENT_SOURCE_DIR}/include;"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor;"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui;"
    "${CMAKE_CURRENT_SOURCE_DIR}/luau/Common/include;"
    "${CMAKE_CURRENT_SOURCE_DIR}/luau/VM/include"
)

################################################################################
# Compile definitions
################################################################################
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug>:"
        "CMAKE_INTDIR=\"Debug\""
    ">"
    "$<$<CONFIG:Release>:"
        "CMAKE_INTDIR=\"Release\""
    ">"
    "_MBCS"
)

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /MP;
        /W4;
        /Zi;
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
        /Y-
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        /MANIFEST;
        /DEBUG:FULL;
        /SUBSYSTEM:CONSOLE;
        /INCREMENTAL
    )
endif()

################################################################################
# Dependencies
################################################################################
set(ADDITIONAL_LIBRARY_DEPENDENCIES
    "opengl32;"
    "SDL2"
)
target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

target_link_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/dll;"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib"
)

