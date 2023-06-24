add_library(libretro-common STATIC)
target_include_directories(libretro-common PUBLIC "${libretro-common_SOURCE_DIR}/include")

target_sources(libretro-common PRIVATE
    ${libretro-common_SOURCE_DIR}/audio/conversion/float_to_s16.c
    ${libretro-common_SOURCE_DIR}/audio/conversion/s16_to_float.c
    ${libretro-common_SOURCE_DIR}/audio/resampler/audio_resampler.c
    ${libretro-common_SOURCE_DIR}/audio/resampler/drivers/sinc_resampler.c
    ${libretro-common_SOURCE_DIR}/compat/compat_fnmatch.c
    ${libretro-common_SOURCE_DIR}/compat/compat_posix_string.c
    ${libretro-common_SOURCE_DIR}/compat/compat_strldup.c
    ${libretro-common_SOURCE_DIR}/compat/fopen_utf8.c
    ${libretro-common_SOURCE_DIR}/dynamic/dylib.c
    ${libretro-common_SOURCE_DIR}/encodings/encoding_base64.c
    ${libretro-common_SOURCE_DIR}/encodings/encoding_crc32.c
    ${libretro-common_SOURCE_DIR}/encodings/encoding_utf.c
    ${libretro-common_SOURCE_DIR}/features/features_cpu.c
    ${libretro-common_SOURCE_DIR}/file/archive_file.c
    ${libretro-common_SOURCE_DIR}/file/config_file.c
    ${libretro-common_SOURCE_DIR}/file/config_file_userdata.c
    ${libretro-common_SOURCE_DIR}/file/file_path.c
    ${libretro-common_SOURCE_DIR}/file/file_path_io.c
    ${libretro-common_SOURCE_DIR}/file/nbio/nbio_intf.c
    ${libretro-common_SOURCE_DIR}/file/nbio/nbio_stdio.c
    ${libretro-common_SOURCE_DIR}/file/retro_dirent.c
    ${libretro-common_SOURCE_DIR}/formats/bmp/rbmp_encode.c
    ${libretro-common_SOURCE_DIR}/formats/image_texture.c
    ${libretro-common_SOURCE_DIR}/formats/image_transfer.c
    ${libretro-common_SOURCE_DIR}/formats/json/rjson.c
    ${libretro-common_SOURCE_DIR}/formats/logiqx_dat/logiqx_dat.c
    ${libretro-common_SOURCE_DIR}/formats/m3u/m3u_file.c
    ${libretro-common_SOURCE_DIR}/gfx/scaler/pixconv.c
    ${libretro-common_SOURCE_DIR}/gfx/scaler/scaler.c
    ${libretro-common_SOURCE_DIR}/gfx/scaler/scaler_filter.c
    ${libretro-common_SOURCE_DIR}/gfx/scaler/scaler_int.c
    ${libretro-common_SOURCE_DIR}/hash/lrc_hash.c
    ${libretro-common_SOURCE_DIR}/lists/dir_list.c
    ${libretro-common_SOURCE_DIR}/lists/file_list.c
    ${libretro-common_SOURCE_DIR}/lists/linked_list.c
    ${libretro-common_SOURCE_DIR}/lists/nested_list.c
    ${libretro-common_SOURCE_DIR}/lists/string_list.c
    ${libretro-common_SOURCE_DIR}/memmap/memalign.c
    ${libretro-common_SOURCE_DIR}/memmap/memmap.c
    ${libretro-common_SOURCE_DIR}/playlists/label_sanitization.c
    ${libretro-common_SOURCE_DIR}/queues/fifo_queue.c
    ${libretro-common_SOURCE_DIR}/queues/generic_queue.c
    ${libretro-common_SOURCE_DIR}/queues/message_queue.c
    ${libretro-common_SOURCE_DIR}/queues/task_queue.c
    ${libretro-common_SOURCE_DIR}/streams/file_stream.c
    ${libretro-common_SOURCE_DIR}/streams/file_stream_transforms.c
    ${libretro-common_SOURCE_DIR}/streams/interface_stream.c
    ${libretro-common_SOURCE_DIR}/streams/memory_stream.c
    ${libretro-common_SOURCE_DIR}/streams/network_stream.c
    ${libretro-common_SOURCE_DIR}/streams/rzip_stream.c
    ${libretro-common_SOURCE_DIR}/streams/stdin_stream.c
    ${libretro-common_SOURCE_DIR}/streams/trans_stream.c
    ${libretro-common_SOURCE_DIR}/streams/trans_stream_pipe.c
    ${libretro-common_SOURCE_DIR}/string/stdstring.c
    ${libretro-common_SOURCE_DIR}/time/rtime.c
    ${libretro-common_SOURCE_DIR}/utils/md5.c
    ${libretro-common_SOURCE_DIR}/vfs/vfs_implementation.c
    )

if (HAVE_THREADS)
    target_sources(libretro-common PRIVATE
        ${libretro-common_SOURCE_DIR}/rthreads/rthreads.c
        )

    target_compile_definitions(libretro-common PUBLIC HAVE_THREADS)
endif ()

if (NOT HAVE_STRL)
    target_sources(libretro-common PRIVATE
        ${libretro-common_SOURCE_DIR}/compat/compat_strl.c
        )
else ()
    target_compile_definitions(libretro-common PUBLIC HAVE_STRL)
endif ()

if (HAVE_OPENGL)
    target_sources(libretro-common PRIVATE
        ${libretro-common_SOURCE_DIR}/glsm/glsm.c
        ${libretro-common_SOURCE_DIR}/glsym/rglgen.c
        ${libretro-common_SOURCE_DIR}/glsym/glsym_gl.c
        )

    target_compile_definitions(libretro-common PUBLIC HAVE_OPENGL OGLRENDERER_ENABLED CORE)
    target_link_libraries(libretro-common PUBLIC OpenGL::GL)
endif ()

if (HAVE_OPENGL_MODERN)
    target_compile_definitions(libretro-common PUBLIC HAVE_OPENGL_MODERN)
endif ()

if (HAVE_OPENGLES)
    target_compile_definitions(libretro-common PUBLIC HAVE_OPENGLES)
endif ()

if (HAVE_OPENGLES3)
    target_compile_definitions(libretro-common PUBLIC HAVE_OPENGLES3)
    target_compile_definitions(libretro-common PUBLIC HAVE_OPENGLES_3)
endif ()

if (HAVE_OPENGLES2)
    target_compile_definitions(libretro-common PUBLIC HAVE_OPENGLES2)
    target_compile_definitions(libretro-common PUBLIC HAVE_OPENGLES_2)
endif ()

if (HAVE_EGL)
    target_compile_definitions(libretro-common PUBLIC HAVE_EGL)
    target_link_libraries(libretro-common PUBLIC OpenGL::EGL)
endif ()

if (HAVE_STRL)
    target_compile_definitions(libretro-common PUBLIC HAVE_STRL)
else ()
    target_sources(libretro-common PRIVATE
        ${libretro-common_SOURCE_DIR}/compat/compat_strldup.c
    )
endif ()

if (HAVE_MMAP)
    target_compile_definitions(libretro-common PUBLIC HAVE_MMAP)
endif ()

if (HAVE_MMAN)
    target_compile_definitions(libretro-common PUBLIC HAVE_MMAN)
endif ()

if (HAVE_ZLIB)
    target_compile_definitions(libretro-common PUBLIC HAVE_ZLIB)
    target_sources(libretro-common PRIVATE
        ${libretro-common_SOURCE_DIR}/file/archive_file_zlib.c
        ${libretro-common_SOURCE_DIR}/streams/trans_stream_zlib.c
    )
    target_link_libraries(libretro-common PUBLIC ZLIB::ZLIB)
endif ()

set_target_properties(libretro-common PROPERTIES PREFIX "" OUTPUT_NAME "libretro-common")
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(libretro-common PRIVATE -fPIC)
    target_link_options(libretro-common PRIVATE -fPIC)
endif ()

if (APPLE)
    target_compile_definitions(libretro-common PUBLIC GL_SILENCE_DEPRECATION)
endif()