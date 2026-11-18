#!/usr/bin/env python3
"""Adapt the pinned OpenXRay archive decoder for the embedded iOS launcher."""

from pathlib import Path
import sys


def replace(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f"Unexpected OpenXRay source: {old[:100]!r}")
    return source.replace(old, new, 1)


def patch_decoder(path):
    source = path.read_text()
    source = replace(source, "#include <fcntl.h>", "#include <fcntl.h>\n#include <mutex>")
    source = replace(source, "unsigned textsize = 0", "static std::mutex lzhuf_mutex;\n\nunsigned textsize = 0")
    source = replace(source, "    unsigned getlen;", "    unsigned getlen;\n    size_t bits_remaining;\n    bool truncated;")
    source = replace(source, "        getbuf = getlen = putbuf = putlen = 0;",
                     "        getbuf = getlen = putbuf = putlen = 0;\n"
                     "        bits_remaining = 0;\n        truncated = false;")
    source = replace(source, "    IC void Init_Output", """    IC bool InputTruncated() const { return truncated; }
    IC void BeginBitInput() { bits_remaining = size_t(in_end - in_iterator) * 8; }
    IC void ConsumeBits(size_t count)
    {
        if (count > bits_remaining)
            truncated = true;
        else
            bits_remaining -= count;
    }
    IC void Init_Output""")
    source = replace(source, "        getlen--;", "        getlen--;\n        ConsumeBits(1);")
    source = replace(source, "        getlen -= 8;", "        getlen -= 8;\n        ConsumeBits(8);")
    begin = source.index("    textsize = (fs._getb());", source.index("bool Decode("))
    end = source.index("    fs.Init_Output(textsize);", begin)
    source = source[:begin] + """    if (fs.InputSize() <= 4)
        return false;
    textsize = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
        textsize |= unsigned(fs._getb()) << shift;
    if (textsize == 0 || textsize > 0x7fffffffU ||
        (total_size >= 0 && textsize > unsigned(total_size)))
        return false;

    fs.BeginBitInput();
""" + source[end:]
    source = replace(source, "        c = DecodeChar();\n        if (c < 256)", """        c = DecodeChar();
        if (fs.InputTruncated())
        {
            fs.OutRelease();
            return false;
        }
        if (c < 256)""")
    source = replace(source, "            j = c - 255 + THRESHOLD;\n", """            j = c - 255 + THRESHOLD;
            if (fs.InputTruncated() || unsigned(j) > textsize - count)
            {
                fs.OutRelease();
                return false;
            }
""")
    for name in ("_writeLZ", "_compressLZ", "_decompressLZ", "_readLZ"):
        start = source.index("\n{", source.index(" " + name + "(")) + 2
        source = source[:start] + "\n    std::lock_guard<std::mutex> lock(lzhuf_mutex);" + source[start:]
    source = replace(source, "    // Actual compression\n    Decode(-1);\n",
                     "    const bool decoded = Decode(-1);\n")
    source = replace(source, "    xr_free(data);\n    d = fs.OutPointer();",
                     "    xr_free(data);\n    if (!decoded)\n    {\n        d = nullptr;\n        return 0;\n    }\n    d = fs.OutPointer();")
    source = replace(source, "#undef N\n", """// Caller owns its output buffer; never expose xrCore allocator ownership.
extern "C" __attribute__((visibility("default"))) bool st_openxray_decode_archive_directory(
    void* input, size_t size, void* output, size_t expected)
{
    if (!input || !output || size <= 4 || expected == 0 || expected > 64u * 1024u * 1024u)
        return false;
    const auto* bytes = static_cast<const u8*>(input);
    const size_t declared = size_t(bytes[0]) | size_t(bytes[1]) << 8 |
        size_t(bytes[2]) << 16 | size_t(bytes[3]) << 24;
    if (declared != expected)
        return false;
    u8* decoded = nullptr;
    size_t count = 0;
    if (!_decompressLZ(&decoded, &count, input, size, expected))
        return false;
    const bool valid = count == expected;
    if (valid)
        memcpy(output, decoded, count);
    xr_free(decoded);
    return valid;
}

#undef N
""")
    path.write_text(source)


def patch_locator(path):
    source = path.read_text()
    source = "#include <algorithm>\n" + source
    old = """                if (shouldDecrypt)
                    g_trivial_encryptor.decode(src_data, dwSize, src_data);

                bool result = _decompressLZ(&dest, &dest_sz, src_data, dwSize, archiveSize);

                if (!result && shouldDecrypt)
                {
                    // Let's try to decode with RU key
                    g_trivial_encryptor.decode(src_data, dwSize, src_data, trivial_encryptor::key_flag::russian);
                    result = _decompressLZ(&dest, &dest_sz, src_data, dwSize, archiveSize);
                }
                R_ASSERT3(result, "Can't decompress archive", archiveName);"""
    new = """                constexpr size_t maxDirectorySize = 64u * 1024u * 1024u;
                const size_t outputLimit = ID == 1 ? std::min(archiveSize, maxDirectorySize) : archiveSize;
                auto decode = [&] {
                    if (!_decompressLZ(&dest, &dest_sz, src_data, dwSize, outputLimit))
                        return false;
                    if (ID != 1)
                        return true;
                    size_t cursor = 0;
                    while (dest_sz - cursor >= 2)
                    {
                        const size_t record = size_t(dest[cursor]) | size_t(dest[cursor + 1]) << 8;
                        if (record <= 16 || record - 16 >= sizeof(string_path) ||
                            record > dest_sz - cursor - 2)
                            break;
                        cursor += record + 2;
                    }
                    if (cursor == dest_sz)
                        return true;
                    xr_free(dest);
                    dest_sz = 0;
                    return false;
                };

                bool result = decode();
                if (!result && shouldDecrypt)
                {
                    g_trivial_encryptor.decode(src_data, dwSize, src_data);
                    result = decode();
                    if (!result)
                    {
                        g_trivial_encryptor.encode(src_data, dwSize, src_data); // restore original bytes
                        g_trivial_encryptor.decode(src_data, dwSize, src_data, trivial_encryptor::key_flag::russian);
                        result = decode();
                    }
                }
                if (!result)
                {
                    xr_free(src_data);
                    Msg("! Can't decompress archive: %s", archiveName);
                    return nullptr;
                }"""
    source = replace(source, old, new)
    source = replace(source, "    R_ASSERT(hdr);\n\n    while (!hdr->eof())",
                     '    if (!hdr)\n    {\n        Msg("! Cannot read archive directory: %s", A.path.c_str());\n        return;\n    }\n\n    while (!hdr->eof())')
    posix = source.index("IReader* open_chunk(int fd")
    start = source.index("            u8* src_data = xr_alloc<u8>(dwSize);", posix)
    end = source.index("            if (dwType & CFS_CompressMark)", start)
    source = source[:start] + """            if (dwSize > archiveSize || (ID == 1 && dwSize > 64u * 1024u * 1024u))
                return nullptr;
            u8* src_data = xr_alloc<u8>(dwSize);
            read_byte = _read(fd, src_data, dwSize);
            if (read_byte < 0 || size_t(read_byte) != dwSize)
            {
                xr_free(src_data);
                return nullptr;
            }
""" + source[end:]
    source = replace(source, "        IReader* pFSltx = setup_fs_ltx(fs_name);", """        IReader* pFSltx = setup_fs_ltx(fs_name);
        // The imported port's configuration may lack an archive-root alias.
        // Mount root-level archives before resolving any gamedata aliases.
        if (fs_name && fs_name[0] == '/')
        {
            bNoRecurse = true;
            Recurse(get_path("$fs_root$")->m_Path);
        }""")
    path.write_text(source)


if __name__ == "__main__":
    core = Path(sys.argv[1]) / "src/xrCore"
    patch_decoder(core / "LzHuf.cpp")
    patch_locator(core / "LocatorAPI.cpp")
