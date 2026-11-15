"""Read-only PE import DLL scanner for Windows X-Ray binaries in ZIP."""
import argparse
import json
from pathlib import Path
import struct
import zipfile

MAX_FILE = 64 * 1024 * 1024
MAX_IMPORTS = 256


def imports_from_pe(data):
    if len(data) < 0x40 or data[:2] != b'MZ':
        return {'error': 'not_mz'}
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if pe + 24 > len(data) or data[pe:pe+4] != b'PE\0\0':
        return {'error': 'invalid_pe_header'}
    machine, sections = struct.unpack_from('<HH', data, pe+4)
    opt_size = struct.unpack_from('<H', data, pe+20)[0]
    opt = pe + 24
    if sections > 96 or opt + opt_size > len(data) or opt_size < 2:
        return {'error': 'invalid_optional_header'}
    magic = struct.unpack_from('<H', data, opt)[0]
    directory_offset = 96 if magic == 0x10b else 112 if magic == 0x20b else None
    if directory_offset is None:
        return {'error': 'unsupported_optional_header'}
    if opt_size < directory_offset + 16:
        return {'error': 'missing_import_directory'}
    import_rva, import_size = struct.unpack_from('<II', data, opt + directory_offset + 8)
    section_table = opt + opt_size
    if section_table + sections*40 > len(data):
        return {'error': 'invalid_section_table'}
    ranges = []
    for i in range(sections):
        off = section_table + i*40
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from('<IIII', data, off+8)
        ranges.append((rva, max(virtual_size, raw_size), raw_offset, raw_size))

    def file_offset(rva, needed=1):
        for start, virtual_span, raw_start, raw_size in ranges:
            delta = rva - start
            if 0 <= delta and delta + needed <= raw_size and delta + needed <= virtual_span:
                pos = raw_start + delta
                if pos + needed <= len(data):
                    return pos
        return None

    if not import_rva:
        return {'machine': f'0x{machine:04x}', 'imports': []}
    if import_size < 20:
        return {'error': 'invalid_import_directory_size'}
    result = []
    for i in range(min(MAX_IMPORTS, import_size // 20)):
        off = file_offset(import_rva + i*20, 20)
        if off is None:
            return {'error': 'import_descriptor_out_of_bounds', 'imports': result}
        orig, stamp, chain, name_rva, thunk = struct.unpack_from('<IIIII', data, off)
        if not (orig or stamp or chain or name_rva or thunk):
            return {'machine': f'0x{machine:04x}', 'imports': result}
        name_off = file_offset(name_rva)
        if name_off is None:
            return {'error': 'import_name_out_of_bounds', 'imports': result}
        end = data.find(b'\0', name_off, min(len(data), name_off + 260))
        if end == -1:
            return {'error': 'unterminated_import_name', 'imports': result}
        try:
            name = data[name_off:end].decode('ascii')
        except UnicodeDecodeError:
            return {'error': 'non_ascii_import_name', 'imports': result}
        result.append(name)
    return {'error': 'import_descriptor_limit', 'imports': result}


def inventory_zip(path):
    results = []
    with zipfile.ZipFile(path) as z:
        for member in z.infolist():
            if member.is_dir() or member.filename.startswith('__MACOSX/') or not member.filename.lower().endswith(('.exe', '.dll')):
                continue
            entry = {'file': member.filename}
            if member.file_size > MAX_FILE:
                entry['error'] = 'file_too_large'
            elif member.flag_bits & 1:
                entry['error'] = 'encrypted'
            else:
                try:
                    with z.open(member) as f:
                        raw = f.read(MAX_FILE + 1)
                    entry.update(imports_from_pe(raw) if len(raw) <= MAX_FILE else {'error': 'file_too_large'})
                except (OSError, RuntimeError, ValueError, zipfile.BadZipFile) as exc:
                    entry['error'] = type(exc).__name__
            results.append(entry)
    return results


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('zip_file', type=Path)
    args = parser.parse_args()
    print(json.dumps(inventory_zip(args.zip_file), indent=2))
