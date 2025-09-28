#!/bin/env python3

# Copyright 2021 Rémi Bernon for CodeWeavers
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA

from __future__ import print_function

import gdb
import re
import subprocess
import sys

class LoadSymbolFiles(gdb.Command):
  'Command to load symbol files directly from /proc/<pid>/maps.'
  
  def __init__(self):
    sup = super(LoadSymbolFiles, self)
    sup.__init__('load-symbol-files', gdb.COMMAND_FILES, gdb.COMPLETE_NONE,
                 False)

    self.libs = {}
    gdb.execute('alias -a lsf = load-symbol-files', True)

  def invoke(self, arg, from_tty):
    pid = gdb.selected_inferior().pid
    if not pid in self.libs: self.libs[pid] = {}

    def command(cmd, confirm=from_tty, to_string=not from_tty):
      gdb.execute(cmd, from_tty=confirm, to_string=to_string)

    def execute(cmd):
      return subprocess.check_output(cmd, stderr=subprocess.STDOUT) \
                       .decode('utf-8')

    # load mappings addresses
    libs = {}
    with open('/proc/{}/maps'.format(pid), 'r') as maps:
      for line in maps:
        addr, _, _, _, node, path = re.split(r'\s+', line, 5)
        path = path.strip()
        if node == '0': continue
        if path in libs: continue
        libs[path] = int(addr.split('-')[0], 16)

    # unload symbol file if address changed
    for k in set(libs) & set(self.libs[pid]):
      if libs[k] != self.libs[pid][k]:
        try:
          command('remove-symbol-file "{}"'.format(k), confirm=False)
        except:
          print("warn: Failed to unload symbol file {}".format(k))
        finally:
          del self.libs[pid][k]

    # load symbol file for new mappings
    for k in set(libs) - set(self.libs[pid]):
        if arg is not None and re.search(arg, k) is None: continue
        addr = self.libs[pid][k] = libs[k]
        has_debug = False
        offs = None

        try:
          out = execute(['file', k])
        except:
          continue

        # try loading mapping as ELF
        try:
          out = execute(['readelf', '-l', k])
          for line in out.split('\n'):
            if not 'LOAD' in line: continue
            base = int(line.split()[2], 16)
            break
        except:
          # assume mapping is PE
          base = -1

        try:
          name = None
          cmd = 'add-symbol-file "{}"'.format(k)
          out = execute(['objdump', '-h', k])
          for line in out.split('\n'):
            if '2**' in line:
              _, name, _, vma, _, off, _ = line.split(maxsplit=6)
              if base < 0: offs = int(off, 16)
              else: offs = int(vma, 16) - base
            if 'ALLOC' in line:
              cmd += ' -s {} 0x{:x}'.format(name, addr + offs)
            elif name in ['.gnu_debuglink', '.debug_info']:
              has_debug = True
            elif 'DEBUGGING' in line:
              has_debug = True
        except:
          continue

        if not has_debug:
          print('no debugging info found in {}'.format(k))
          continue

        print('loading symbols for {}'.format(k))
        command(cmd, confirm=False, to_string=True)


LoadSymbolFiles()
