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

from gdb.unwinder import Unwinder
import gdb
import re

ARCH_OFFSETS_X86_64 = {
    "rax":           (0x0000, 'void *'),
    "rbx":           (0x0008, 'void *'),
    "rcx":           (0x0010, 'void *'),
    "rdx":           (0x0018, 'void *'),
    "rsi":           (0x0020, 'void *'),
    "rdi":           (0x0028, 'void *'),
    "r8":            (0x0030, 'void *'),
    "r9":            (0x0038, 'void *'),
    "r10":           (0x0040, 'void *'),
    "r11":           (0x0048, 'void *'),
    "r12":           (0x0050, 'void *'),
    "r13":           (0x0058, 'void *'),
    "r14":           (0x0060, 'void *'),
    "r15":           (0x0068, 'void *'),
    "rip":           (0x0070, 'void *'),
    "cs":            (0x0078, 'void *'),
    "eflags":        (0x0080, 'void *'),
    "rsp":           (0x0088, 'void *'),
    "ss":            (0x0090, 'void *'),
    "rbp":           (0x0098, 'void *'),
    "prev_frame":    (0x00a0, 'void *'),
    "syscall_cfa":   (0x00a8, 'void *'),
    # "syscall_flags": (0x00b0, 'unsigned int'),
    # "restore_flags": (0x00b4, 'unsigned int'),
    "teb_offset":    (0x0328, 'void *'),
}

ARCH_OFFSETS_I386 = {
    # "syscall_flags": (0x000, 'unsigned short'),
    # "restore_flags": (0x002, 'unsigned short'),
    "eflags":        (0x004, 'unsigned int'),
    "eip":           (0x008, 'void *'),
    "esp":           (0x00c, 'void *'),
    "cs":            (0x010, 'unsigned short'),
    "ss":            (0x012, 'unsigned short'),
    "ds":            (0x014, 'unsigned short'),
    "es":            (0x016, 'unsigned short'),
    "fs":            (0x018, 'unsigned short'),
    "gs":            (0x01a, 'unsigned short'),
    "eax":           (0x01c, 'void *'),
    "ebx":           (0x020, 'void *'),
    "ecx":           (0x024, 'void *'),
    "edx":           (0x028, 'void *'),
    "edi":           (0x02c, 'void *'),
    "esi":           (0x030, 'void *'),
    "ebp":           (0x034, 'void *'),
    "syscall_cfa":   (0x038, 'void *'),
    "prev_frame":    (0x03c, 'void *'),
    "teb_offset":    (0x1f8, 'void *'),
}


def arch_offsets(arch):
    if 'x86-64' in arch:
        return ARCH_OFFSETS_X86_64
    if 'i386' in arch:
        return ARCH_OFFSETS_I386


def find_syscall_frame(sp, arch, teb):
    (off, type) = arch_offsets(arch)['teb_offset']
    frame = int(gdb.parse_and_eval(f'*(void **){teb + off}'))

    while frame:
        (off, type) = arch_offsets(arch)['prev_frame']
        next_frame = int(gdb.parse_and_eval(f'*(void **){(frame + off)}'))
        if not next_frame: return frame

        (off, type) = arch_offsets(arch)['ebp']
        ebp = int(gdb.parse_and_eval(f'*(void **){(next_frame + off)}'))
        if ebp >= sp: return frame


def callback_registers(pc, sp, pending_frame):
    arch = pending_frame.architecture().name()

    if 'x86-64' in arch:
        frame = int(pending_frame.read_register("rbp"))
        frame = (frame - 0x448) & ~0x3f

        (off, type) = arch_offsets(arch)['syscall_cfa']
        val = gdb.parse_and_eval(f'*({type} *){int(frame + off)}')
        yield 'rip', gdb.parse_and_eval(f'*(void **){int(val - 8)}')
        yield 'rbp', val - 16

    elif 'i386' in arch:
        teb = int(pending_frame.read_register('edx'))
        frame = find_syscall_frame(sp, arch, teb)

        (off, type) = arch_offsets(arch)['syscall_cfa']
        val = gdb.parse_and_eval(f'*({type} *){int(frame + off)}')
        yield 'eip', gdb.parse_and_eval(f'*(void **){int(val - 4)}')
        yield 'ebp', val - 8


def registers(pc, sp, pending_frame):
    arch = pending_frame.architecture().name()
    frame = sp

    if 'x86-64' in arch:
        if 'syscall' in str(pc):
            rbp = pending_frame.read_register("rbp")
            frame = rbp - 0x98
    elif 'i386' in arch:
        if 'syscall' in str(pc):
            ebp = pending_frame.read_register("ebp")
            frame = ebp - 0x34
        else:
            frame += 16

    for reg, (off, type) in arch_offsets(arch).items():
        val = gdb.parse_and_eval(f'*({type} *){int(frame + off)}')

        if reg in ('eflags', 'cs', 'ss', 'ds', 'es', 'fs', 'gs'):
            int32 = gdb.lookup_type('int')
            val = val.cast(int32)
        if reg in ('syscall_cfa', 'prev_frame', 'teb_offset'):
            continue

        yield reg, val


class WineSyscallFrameId(object):
    def __init__(self, sp, pc):
        self.sp = sp
        self.pc = pc


class WineSyscallUnwinder(Unwinder):
    def __init__(self):
        super().__init__("WineSyscallUnwinder", gdb.SIGTRAMP_FRAME)
        self.pattern = re.compile('__wine_(syscall|unix_call)'
                                  '|KiUserCallbackDispatcher')

    def __call__(self, pending_frame):
        pc = pending_frame.read_register("pc")
        if self.pattern.search(str(pc)) is None:
            return None

        sp = pending_frame.read_register("sp")
        frame = WineSyscallFrameId(sp, pc)
        if 'KiUserCallbackDispatcher' in str(pc):
            unwind_info = pending_frame.create_unwind_info(frame)
            for reg, val in callback_registers(pc, sp, pending_frame):
                unwind_info.add_saved_register(reg, val)
            return unwind_info

        unwind_info = pending_frame.create_unwind_info(frame)
        for reg, val in registers(pc, sp, pending_frame):
            unwind_info.add_saved_register(reg, val)
        return unwind_info


gdb.unwinder.register_unwinder(None, WineSyscallUnwinder(), replace=True)
