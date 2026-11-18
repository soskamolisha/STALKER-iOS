#!/usr/bin/env python3
"""Use the OpenXRay renderer's GLES compatibility code with SDL on iOS."""
import argparse
from pathlib import Path
import re


def replace(path, old, new):
    source = path.read_text()
    if source.count(old) != 1:
        raise ValueError(f"Unexpected source layout in {path.name}: {old}")
    path.write_text(source.replace(old, new, 1))


def patch_ios_dds_loader(layers):
    folder = layers / "xrRenderGL"
    header = Path(__file__).with_name("openxray_ios_gles_dds.h")
    (folder / header.name).write_bytes(header.read_bytes())
    texture = folder / "glTexture.cpp"
    replace(texture, "#include <gli/gli.hpp>", """#include <gli/gli.hpp>
#ifdef XR_RENDER_IOS_GLES
#include "openxray_ios_gles_dds.h"
#endif""")
    replace(texture, "    R_ASSERT2(!texture.empty(), fn);", """    if (texture.empty())
    {
        Msg("! Could not read DDS texture '%s'", fn);
        FS.r_close(S);
        return 0;
    }

#ifdef XR_RENDER_IOS_GLES
    texture = ios_gles::selectDDSLevels(texture, fRName);
    const bool expandDDS = ios_gles::requiresDDSConversion(texture.format());
    if (expandDDS)
    {
        texture = ios_gles::expandDDS(texture);
        if (texture.empty())
        {
            Msg("! Could not expand DDS texture '%s' for OpenGL ES", fn);
            FS.r_close(S);
            return 0;
        }
    }
#endif""")
    replace(texture, "    u32 mip_cnt = u32(-1); // XXX: write to it when reading with GLI!\n\n", "")
    replace(texture, "    int img_loaded_lod = is_target_cube(texture.target()) ? 0 : get_texture_load_lod(fn);\n"
        "    ret_msize = calc_texture_size(img_loaded_lod, mip_cnt, img_size);",
        """#ifdef XR_RENDER_IOS_GLES
    ret_msize = static_cast<u32>(texture.size());
#else
    int img_loaded_lod = is_target_cube(texture.target()) ? 0 : get_texture_load_lod(fn);
    ret_msize = calc_texture_size(img_loaded_lod, u32(-1), img_size);
#endif""")


def patch_ios_texture_upload(layers):
    texture = layers / "xrRenderGL/glTexture.cpp"
    replace(texture, '#include "stdafx.h"', '#include "stdafx.h"\n\n#include <algorithm>')
    replace(texture, '    gli::gl::format const format = GL.translate(texture.format(), texture.swizzles());',
        '''    auto format = GL.translate(texture.format(), texture.swizzles());
#ifdef XR_RENDER_IOS_GLES
    // Legacy X8R8G8B8 DDS still supplies four bytes per pixel and requires
    // RGBA storage on GLES. The GLI swizzle forces the unused byte to alpha 1.
    if (texture.format() == gli::FORMAT_BGR8_UNORM_PACK32)
        format.Internal = gli::gl::INTERNAL_RGBA8_UNORM;
    else if (texture.format() == gli::FORMAT_BGR8_SRGB_PACK32)
        format.Internal = gli::gl::INTERNAL_SRGB8_ALPHA8;
#endif''')
    replace(texture, '#if defined(XR_PLATFORM_WEB) || defined(XR_RENDER_IOS_GLES)\n    const bool swapRedBlue',
        '''#if defined(XR_RENDER_IOS_GLES)
    if (format.External != gli::gl::EXTERNAL_RED) // Fonts sample the single red channel directly.
    {
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_R, format.Swizzles[gli::SWIZZLE_RED]);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, format.Swizzles[gli::SWIZZLE_GREEN]);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, format.Swizzles[gli::SWIZZLE_BLUE]);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, format.Swizzles[gli::SWIZZLE_ALPHA]);
    }
    const auto texels = [&](size_t layer, size_t face, size_t level) { return texture.data(layer, face, level); };
#elif defined(XR_PLATFORM_WEB)
    const bool swapRedBlue''')
    replace(texture, "    const texture_upload_unit uploadUnit(target);", """    const texture_upload_unit uploadUnit(target);
    const glm::tvec3<GLsizei> tex_extent(texture.extent());
    size_t levels = texture.levels();
#ifdef XR_RENDER_IOS_GLES
    // DDS rows are tightly packed and must not inherit the GL default of four bytes.
    GLint unpackAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    while (glGetError() != GL_NO_ERROR) {}

    size_t maxLevels = 1;
    for (auto extent = std::max({tex_extent.x, tex_extent.y, tex_extent.z}); extent > 1; extent /= 2)
        ++maxLevels;
    levels = std::min(levels, maxLevels);
#endif""")
    path_source = texture.read_text().replace("texture.levels() - 1", "levels - 1")
    path_source = path_source.replace("static_cast<GLint>(texture.levels())", "static_cast<GLint>(levels)")
    path_source = path_source.replace("level < texture.levels()", "level < levels")
    texture.write_text(path_source)
    replace(texture, "    glm::tvec3<GLsizei> const tex_extent(texture.extent());\n\n    GLenum err;", "    GLenum err;")
    for dimensions in (2, 3):
        replace(texture, f'''            Msg("! OpenGL: 0x%x: Invalid {dimensions}D texture: '%s'", err, fn);''',
            f'''            Msg("! OpenGL: 0x%x: Invalid {dimensions}D texture: '%s' (format 0x%x, %dx%dx%d, %u levels)",
                err, fn, static_cast<unsigned>(format.Internal), tex_extent.x, tex_extent.y, tex_extent.z,
                static_cast<unsigned>(levels));''')
    replace(texture, "    FS.r_close(S);\n\n    xr_strlwr(fn);", """#ifdef XR_RENDER_IOS_GLES
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
#endif
    FS.r_close(S);

    xr_strlwr(fn);""")

    video = layers / "xrRenderGL/glSH_Texture.cpp"
    upload = "            CHK_GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _w, _h, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr));"
    replace(video, upload, "#ifdef XR_RENDER_IOS_GLES\n"
        "            CHK_GL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, _w, _h));\n"
        "#else\n" + upload + "\n#endif")

    theora = layers.parent / "xrEngine/xrTheora_Surface.cpp"
    replace(theora, '#include "xrTheora_Stream.h"', '#include "xrTheora_Stream.h"\n\n#include <algorithm>')
    replace(theora, """                u32& clr = data[++pos];
                clr = subst_alpha(clr, iFloor(float((y - 16)) / K));
            }
        }
    }
}""", """                u32& clr = data[pos++];
                clr = subst_alpha(clr, std::clamp(iFloor(float(y - 16) / K), 0, 255));
            }
            pos += _width;
        }
    }
}""")


def patch_ios_shader_rewriter(layers):
    shader = layers / "xrRenderPC_GL/rgl_essl_web.h"
    replace(shader, "        if (is_builtin_redeclaration(tokens))", r"""#ifdef XR_RENDER_IOS_GLES
        // Legacy shaders from imported resources may still write desktop clip
        // distances. OpenGL ES 3.0 has no such built-in on the iPhone.
        if (vertexStage && tokens.size() > 2 && tokens[0] == "gl_ClipDistance" && tokens[1] == "[")
        {
            output += '\n';
            continue;
        }

        // The imported gather shader can come from a loose file or a DB.
        // Fix its integer coordinate offset even when loaded from the DB.
        constexpr std::string_view legacyOffset = "rcp_scale * offset + tc";
        if (const auto position = strip_comment(line).find(legacyOffset); position != std::string_view::npos)
        {
            output.append(line.data(), position);
            output += "rcp_scale * vec2(offset) + tc";
            output.append(line.data() + position + legacyOffset.size(), line.size() - position - legacyOffset.size());
            output += '\n';
            continue;
        }
#endif
        if (is_builtin_redeclaration(tokens))""")


def patch(layers):
    # Keep WebGL canvas/context calls browser-specific. Its GLES shader,
    # buffer, and draw compatibility code also applies to iOS.
    native_context = {"glHW.cpp", "glHW.h", "r2_test_hw.cpp"}
    platform_guards = {
        "ifdef": "if defined(XR_PLATFORM_WEB) || defined(XR_RENDER_IOS_GLES)",
        "ifndef": "if !defined(XR_PLATFORM_WEB) && !defined(XR_RENDER_IOS_GLES)",
    }
    for name in ("xrRender", "xrRenderGL", "xrRenderPC_GL"):
        for path in (layers / name).iterdir():
            if path.suffix not in {".cpp", ".h"} or path.name in native_context:
                continue
            source = path.read_text()
            revised = re.sub(
                r"(?m)^#[ \t]*(ifdef|ifndef)[ \t]+XR_PLATFORM_WEB\b",
                lambda match: "#" + platform_guards[match[1]],
                source,
            )
            if revised != source:
                path.write_text(revised)

    gl = layers / "xrRenderGL"
    hw = gl / "glHW.cpp"
    replace(hw, '#include "glHW.h"', '''#include "glHW.h"
#ifdef XR_RENDER_IOS_GLES
#include <SDL_syswm.h>
#endif''')
    replace(hw, '        ZoneScopedN("gladLoadGL");\n'
            '        version = gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));',
            '''#ifdef XR_RENDER_IOS_GLES
        version = gladLoadGLES2(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));
#else
        ZoneScopedN("gladLoadGL");
        version = gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));
#endif''')
    replace(hw, '#ifdef XR_PLATFORM_WEB\n    TextureUploadUnit =',
            '#if defined(XR_PLATFORM_WEB) || defined(XR_RENDER_IOS_GLES)\n    TextureUploadUnit =')
    replace(hw, '    CHK_GL(glDeleteFramebuffers(1, &pFB));\n    pFB = 0;\n\n#ifdef XR_PLATFORM_WEB',
            '    if (pFB)\n        CHK_GL(glDeleteFramebuffers(1, &pFB));\n    pFB = 0;\n\n#ifdef XR_PLATFORM_WEB')
    replace(hw, '    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);',
            '''#ifdef XR_RENDER_IOS_GLES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif''')
    replace(hw, '    if (!strstr(Core.Params, "-no_gl_context"))\n'
            '    {\n        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);\n'
            '        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);\n    }',
            '''#ifdef XR_RENDER_IOS_GLES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    if (!strstr(Core.Params, "-no_gl_context"))
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    }
#endif''')
    replace(hw, '    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);',
            '''#ifdef XR_RENDER_IOS_GLES
    SDL_SysWMinfo windowInfo{};
    SDL_VERSION(&windowInfo.version);
    if (!SDL_GetWindowWMInfo(m_window, &windowInfo) || windowInfo.subsystem != SDL_SYSWM_UIKIT)
        return;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, windowInfo.info.uikit.framebuffer);
#else
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
#endif''')
    replace(hw, '''    glBlitFramebuffer(
        0, 0, Device.dwWidth, Device.dwHeight,
        0, 0, Device.dwWidth, Device.dwHeight,
        GL_COLOR_BUFFER_BIT, GL_NEAREST);''', '''#ifdef XR_RENDER_IOS_GLES
    // UIKit owns the native drawable. Render at vid_mode and scale the final
    // framebuffer to that drawable without changing the UIKit window size.
    int drawableWidth = 0, drawableHeight = 0;
    SDL_GL_GetDrawableSize(m_window, &drawableWidth, &drawableHeight);
    if (drawableWidth <= 0 || drawableHeight <= 0)
        return;
    glBlitFramebuffer(0, 0, Device.dwWidth, Device.dwHeight, 0, 0,
        drawableWidth, drawableHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
#else
    glBlitFramebuffer(
        0, 0, Device.dwWidth, Device.dwHeight,
        0, 0, Device.dwWidth, Device.dwHeight,
        GL_COLOR_BUFFER_BIT, GL_NEAREST);
#endif''')
    replace(hw, '#ifdef XR_PLATFORM_WEB\nnamespace xray::render::RENDER_NAMESPACE',
            '#if defined(XR_PLATFORM_WEB) || defined(XR_RENDER_IOS_GLES)\nnamespace xray::render::RENDER_NAMESPACE')
    header = gl / "glHW.h"
    for guard in ('    u32 TextureUploadUnit{};', '    const GLenum target;'):
        replace(header, f'#ifdef XR_PLATFORM_WEB\n{guard}',
                f'#if defined(XR_PLATFORM_WEB) || defined(XR_RENDER_IOS_GLES)\n{guard}')

    draw = gl / "glR_Backend_Runtime.h"
    replace(draw, 'extern "C" void glDrawElementsInstancedBaseVertexBaseInstanceWEBGL(\n'
            '    GLenum, GLsizei, GLenum, const void*, GLsizei, GLint, GLuint);',
            '''#ifdef XR_PLATFORM_WEB
extern "C" void glDrawElementsInstancedBaseVertexBaseInstanceWEBGL(
    GLenum, GLsizei, GLenum, const void*, GLsizei, GLint, GLuint);
#endif''')
    replace(draw, '    if (HW.BaseVertexDrawSupported)',
            '#ifdef XR_PLATFORM_WEB\n    if (HW.BaseVertexDrawSupported)')
    replace(draw, '    SetBaseVertex(baseV);\n    CHK_GL(glDrawElements(topology, indexCount, GL_UNSIGNED_SHORT, indices));',
            '#endif\n    SetBaseVertex(baseV);\n'
            '    CHK_GL(glDrawElements(topology, indexCount, GL_UNSIGNED_SHORT, indices));')

    patch_ios_dds_loader(layers)
    patch_ios_texture_upload(layers)
    patch_ios_shader_rewriter(layers)

    cmake = layers / "xrRenderPC_GL/CMakeLists.txt"
    replace(cmake, '    XRRENDER_GL_EXPORTS\n    USE_OGL',
            '    XRRENDER_GL_EXPORTS\n    $<$<PLATFORM_ID:iOS>:XR_RENDER_IOS_GLES>\n    USE_OGL')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("upstream", type=Path)
    args = parser.parse_args()
    patch(args.upstream / "src/Layers")


if __name__ == "__main__":
    main()
