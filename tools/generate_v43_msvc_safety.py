#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path


def one(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f'{label}: anchor missing')
    return text.replace(old, new, 1)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--input', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    text = a.input.read_text(encoding='utf-8')

    text = one(
        text,
        'auto ext=it->path().extension().wstring();for(auto &c:ext)c=static_cast<wchar_t>(std::towlower(c));\n        if(ext==L".dds")return true;',
        'auto ext=it->path().extension().wstring();\n        if(ext==L".dds"||ext==L".DDS")return true;',
        'remove towlower dependency')

    text = one(
        text,
        'void *state=nullptr;__try{state=g_host_spec_state();}__except(EXCEPTION_EXECUTE_HANDLER){state=nullptr;}',
        'void *state=g_host_spec_state();',
        'remove SEH from filesystem function state call')

    text = one(
        text,
        '__try{\n        auto *dst=reinterpret_cast<wchar_t *>(static_cast<std::uint8_t *>(state)+0xB38u);\n        std::memset(dst,0,k_host_spec_path_wchars*sizeof(wchar_t));\n        std::memcpy(dst,s.data(),s.size()*sizeof(wchar_t));\n    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}',
        'auto *dst=reinterpret_cast<wchar_t *>(static_cast<std::uint8_t *>(state)+0xB38u);\n    std::memset(dst,0,k_host_spec_path_wchars*sizeof(wchar_t));\n    std::memcpy(dst,s.data(),s.size()*sizeof(wchar_t));',
        'remove SEH from filesystem function path write')

    # The provider generator intentionally avoids depending on a source-level escaped wchar literal here.
    # Replace the generated single-backslash token with an unambiguous numeric wchar value.
    text = one(
        text,
        "std::wstring s=chosen.wstring();if(s.empty())return false;if(s.back()!=L'\\'&&s.back()!=L'/')s.push_back(L'\\');",
        "std::wstring s=chosen.wstring();if(s.empty())return false;if(s.back()!=static_cast<wchar_t>(0x5C)&&s.back()!=L'/')s.push_back(static_cast<wchar_t>(0x5C));",
        'fix path separator wchar')

    if '__try{state=g_host_spec_state' in text:
        raise SystemExit('unsafe state SEH remains')
    if "s.back()!=L'\\'" in text:
        raise SystemExit('ambiguous backslash wchar remains')
    a.output.write_text(text, encoding='utf-8')
    print('PASS V4.3 MSVC safety transform')


if __name__ == '__main__':
    main()
