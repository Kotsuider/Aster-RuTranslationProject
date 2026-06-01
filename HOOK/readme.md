```MSYS2
export PATH="/mingw32/bin:$PATH"

i686-w64-mingw32-gcc -O2 -shared -o translation.asi translation.c -lkernel32
```