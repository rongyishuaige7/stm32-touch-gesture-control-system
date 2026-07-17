#!/usr/bin/env python3
"""扫描公开候选中的凭据、私钥、个人路径和非示例网络配置。"""
from __future__ import annotations
import argparse,re,subprocess,sys
from pathlib import Path
SKIP_DIRS={'.git','.pio','.venv','venv','__pycache__','.pytest_cache'}
TEXT_SUFFIXES={'','.c','.cc','.cpp','.h','.hpp','.ini','.md','.py','.txt','.yml','.yaml','.json','.csv','.html','.css','.js','.svg'}
PATTERNS=[
 ('private key',re.compile(r'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----')),
 ('GitHub token',re.compile(r'\b(?:gh[opusr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,})\b')),
 ('AWS access key',re.compile(r'\bAKIA[0-9A-Z]{16}\b')),
 ('generic assigned secret',re.compile(r'''(?ix)\b(api[_-]?key|access[_-]?token|auth[_-]?token|secret|password|passwd|pwd)\b\s*[:=]\s*["']?(?!YOUR_|EXAMPLE|REPLACE|CHANGEME|REDACTED|\[REDACTED\])([A-Za-z0-9+/=_!@#$%^&*.-]{8,})''')),
 ('local absolute path',re.compile(r'/(?:home|Users|mnt)/[^\s`"\']+')),
]
ALLOWED_EXACT_LINES={
 ('README.md','Password: ' + '12345678'),
 ('include/config.h','#define ESP_WIFI_PASS             "12345678"'),
 ('docs/SOURCE_PROVENANCE.md','/home/' + 'rongyi/桌面/stm32-touch-gesture-pio'),
 ('docs/SOURCE_PROVENANCE.md','/mnt/' + 'shared/2026项目/stm32-touch-gesture-pio.zip'),
 ('docs/SOURCE_PROVENANCE.md','/home/' + 'rongyi/桌面/stm32-touch-gesture-control-system'),
}
def files(root:Path):
 try: raw=subprocess.run(['git','-C',str(root),'ls-files','-z'],check=True,capture_output=True).stdout
 except (subprocess.CalledProcessError,FileNotFoundError): raw=b''
 if raw:return [root/x.decode('utf-8','surrogateescape') for x in raw.split(b'\0') if x]
 return sorted(p for p in root.rglob('*') if p.is_file() and not any(x in SKIP_DIRS for x in p.relative_to(root).parts))
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--root',default='.');root=Path(ap.parse_args().root).resolve();out=[]
 for p in files(root):
  if not p.exists() or p.stat().st_size>2_000_000 or p.suffix.lower() not in TEXT_SUFFIXES:continue
  try:text=p.read_text(encoding='utf-8')
  except (UnicodeDecodeError,OSError):continue
  rel=p.relative_to(root)
  for n,line in enumerate(text.splitlines(),1):
   if (rel.as_posix(),line.strip()) in ALLOWED_EXACT_LINES:continue
   for label,pat in PATTERNS:
    if pat.search(line):out.append(f'{rel}:{n}: {label}')
 if out:
  print('Secret scan: FAIL',file=sys.stderr);print('\n'.join(out),file=sys.stderr);return 1
 print('Secret scan: PASS');return 0
if __name__=='__main__':raise SystemExit(main())
