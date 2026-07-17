#!/usr/bin/env python3
"""无需真实硬件即可执行的仓库发布契约。"""
from __future__ import annotations
import argparse,csv,re,subprocess,sys,xml.etree.ElementTree as ET
from pathlib import Path
REQUIRED=['.github/platformio-requirements.in','.github/platformio-requirements.txt','.github/workflows/firmware.yml','.gitignore','.markdownlint-cli2.jsonc','HARDWARE.md','LICENSE','README.md','SECURITY.md','THIRD_PARTY_NOTICES.md','docs/GITHUB_METADATA.md','docs/HARDWARE_LAB_CARD.md','docs/PROJECT_STATUS.md','docs/PROTOCOL.md','docs/SOURCE_PROVENANCE.md','docs/VERIFICATION.md','hardware/BOM.csv','hardware/wiring-diagram.svg','include/config.h','include/esp8266.h','platformio.ini','scripts/check_repo.py','scripts/secret_scan.py','scripts/verify.sh','tests/test_source_contracts.py']
FORBIDDEN_NAMES={'.env','sdkconfig','id_rsa','id_ed25519'}
FORBIDDEN_DIRS={'.pio','__pycache__','.venv','venv','node_modules','.pytest_cache','.vscode','.idea'}
FORBIDDEN_SUFFIXES={'.o','.a','.elf','.bin','.hex','.map','.pyc','.zip','.7z','.tar','.gz','.pem','.key'}
MAX=5*1024*1024

def files(root):
 try:raw=subprocess.run(['git','-C',str(root),'ls-files','-z'],check=True,capture_output=True).stdout
 except (subprocess.CalledProcessError,FileNotFoundError):raw=b''
 if raw:return [root/x.decode('utf-8','surrogateescape') for x in raw.split(b'\0') if x]
 return sorted(p for p in root.rglob('*') if p.is_file() and not any(x in {'.git','.pio','__pycache__'} for x in p.relative_to(root).parts))
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--root',default='.');root=Path(ap.parse_args().root).resolve();err=[]
 for rel in REQUIRED:
  if not (root/rel).is_file():err.append(f'missing required file: {rel}')
 checked=files(root)
 for p in checked:
  rel=p.relative_to(root)
  if p.name in FORBIDDEN_NAMES:err.append(f'forbidden local/config file: {rel}')
  if any(x in FORBIDDEN_DIRS for x in rel.parts):err.append(f'forbidden generated directory: {rel}')
  if p.suffix.lower() in FORBIDDEN_SUFFIXES:err.append(f'forbidden binary/archive/key artifact: {rel}')
  if p.stat().st_size>MAX:err.append(f'file exceeds 5 MiB: {rel}')
 contracts={
  'README.md':['当前 STM32、TTP223、APDS-9960、ESP-01S 和 LED 整机尚未重新真机复测','HTTP 没有认证、TLS','HSI 8 MHz'],
  'platformio.ini':['platform = ststm32@19.5.0','board = genericSTM32F103C8'],
  'include/config.h':['ESP_WIFI_SSID             "TouchGesture"','ESP_WIFI_PASS             "12345678"','TOUCH_PIN_UP              GPIO_PIN_0','APDS9960_I2C_SCL_PIN      GPIO_PIN_6'],
  'src/esp8266.cpp':['HTTP/1.1 %u %s','ESP_SendHTTPResponseStatus(conn_id, 404, "Not Found"'],
  'docs/SOURCE_PROVENANCE.md':['aeadcca953d972baa461423ab2f22426d6c738a62947dccbefff62085b5580c2','a637103fda2aaf1171651d96a4cebc5af42cedd160680ade3857c10045dd0fca'],
 }
 for rel,vals in contracts.items():
  p=root/rel
  if p.is_file():
   text=p.read_text(encoding='utf-8')
   for v in vals:
    if v not in text:err.append(f'fact contract missing in {rel}: {v}')
 try:ET.parse(root/'hardware/wiring-diagram.svg')
 except (ET.ParseError,OSError) as e:err.append(f'invalid wiring SVG: {e}')
 try:
  rows=list(csv.DictReader((root/'hardware/BOM.csv').open(newline='',encoding='utf-8')))
  if len(rows)<8:err.append('BOM must contain at least 8 component rows')
 except (OSError,csv.Error) as e:err.append(f'invalid BOM.csv: {e}')
 for rel in ['README.md','docs/PROJECT_STATUS.md','docs/HARDWARE_LAB_CARD.md']:
  text=(root/rel).read_text(encoding='utf-8').lower() if (root/rel).is_file() else ''
  for claim in ['system online','current hardware verified','hardware re-verified: pass','production ready']:
   if claim in text:err.append(f'unsupported claim in {rel}: {claim}')
 if err:
  print('Repository check: FAIL',file=sys.stderr)
  for e in sorted(set(err)):print(f'- {e}',file=sys.stderr)
  return 1
 print(f'Repository check: PASS ({len(checked)} files checked)');return 0
if __name__=='__main__':raise SystemExit(main())
