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
    replace(texture, "    ret_msize = calc_texture_size(img_loaded_lod, mip_cnt, img_size);",
        """#ifdef XR_RENDER_IOS_GLES
    if (expandDDS)
        ret_msize = static_cast<u32>(texture.size());
    else
#endif
        ret_msize = calc_texture_size(img_loaded_lod, mip_cnt, img_size);""")


def patch_ios_shader_rewriter(layers):
    shader = layers / "xrRenderPC_GL/rgl_essl_web.h"
    replace(shader, "        if (is_builtin_redeclaration(tokens))", """#ifdef XR_RENDER_IOS_GLES
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
