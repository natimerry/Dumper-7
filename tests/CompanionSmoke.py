"""Load the companion into an isolated process without a game or UObject array."""
import ctypes
from ctypes import wintypes
import sys
import time

kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
kernel.GetConsoleWindow.restype = wintypes.HWND
kernel.CloseHandle.argtypes = [wintypes.HANDLE]

class ThreadEntry(ctypes.Structure):
    _fields_ = [("size", wintypes.DWORD), ("usage", wintypes.DWORD),
                ("id", wintypes.DWORD), ("pid", wintypes.DWORD),
                ("base", wintypes.LONG), ("delta", wintypes.LONG), ("flags", wintypes.DWORD)]

for name in ("Thread32First", "Thread32Next"):
    getattr(kernel, name).argtypes = [wintypes.HANDLE, ctypes.POINTER(ThreadEntry)]

def thread_count():
    handle = kernel.CreateToolhelp32Snapshot(4, 0)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    entry = ThreadEntry()
    entry.size = ctypes.sizeof(entry)
    count = 0
    try:
        available = kernel.Thread32First(handle, ctypes.byref(entry))
        while available:
            count += entry.pid == kernel.GetCurrentProcessId()
            available = kernel.Thread32Next(handle, ctypes.byref(entry))
    finally:
        kernel.CloseHandle(handle)
    return count

threads, console = thread_count(), kernel.GetConsoleWindow()
dll = ctypes.CDLL(sys.argv[1])
time.sleep(.2)
assert thread_count() == threads, "loading the companion started a worker"
assert kernel.GetConsoleWindow() == console, "loading the companion changed the console"
assert dll.Dumper7_ApiVersion() == 1
objects = dll.Dumper7_SetObjectArray
objects.argtypes = [ctypes.c_uint64] + [ctypes.c_uint32] * 8
objects.restype = ctypes.c_bool
assert not objects(0, 65536, 32, 0, 0, 16, 20, 24, 28)
names = dll.Dumper7_SetNames
names.argtypes = [ctypes.c_uint64, ctypes.c_uint64]
names.restype = ctypes.c_bool
assert not names(0xffffffff, 0)
assert dll.Dumper7_Regenerate() == -4
assert dll.Dumper7_Regenerate() == -3
assert thread_count() == threads
print("Companion ABI: no loader worker/console, rejected overrides, safe no-engine error, repeat protection passed")
