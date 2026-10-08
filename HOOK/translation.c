/*
 * translation.c — патч перевода для движка G2 (Aster)
 *
 * Сборка:
 *   i686-w64-mingw32-gcc -O2 -shared -o translation.dll translation.c -lkernel32
 *
 * translations\<script>.txt (UTF-8, без BOM):
 *   # комментарий
 *   0x1092=Cicadas are so loud...
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TRANS_DIR   "translations"
#define MAX_ENTRIES 8192
#define MAX_TEXT    1024
#define LOG_LEVEL   1

static void dbg(int lvl, const char *fmt, ...) {
    if (lvl > LOG_LEVEL) return;
    char buf[512], out[520];
    va_list va; va_start(va, fmt);
    _vsnprintf(buf, sizeof(buf)-1, fmt, va); va_end(va);
    buf[sizeof(buf)-1] = '\0';
    _snprintf(out, sizeof(out), "[TRANS] %s", buf);
    OutputDebugStringA(out);
}

/* ── Карта PAK ──────────────────────────────────────────────────── */

static const struct {
    DWORD offset; DWORD size; DWORD unpacked; const char *name;
} g_pak_map[] = {
    { 0x002eb1d8, 0x0001b50e, 246102, "a0001" },
    { 0x003066e6, 0x0001218e, 204694, "a0002" },
    { 0x00318874, 0x0000f81a, 163850, "a0003" },
    { 0x0032808e, 0x00006fa2,  98002, "a0004" },
    { 0x0032f030, 0x00008a4f, 118860, "a0005" },
    { 0x00337a7f, 0x00005ef2,  86946, "a0006" },
    { 0x0033d971, 0x0000767f, 103548, "a0007" },
    { 0x00344ff0, 0x00007e21, 110110, "a0008" },
    { 0x0034ce11, 0x00002da6,  42754, "a0009" },
    { 0x0034fbb7, 0x00006ade,  96890, "a0010" },
    { 0x00356695, 0x00012a5b, 206754, "a0011" },
    { 0x003690f0, 0x00005e95,  88064, "a0012" },
    { 0x00000010, 0x00006bb1,      0, "a0013" },
    { 0x00006bc1, 0x00003016,      0, "a0014" },
    { 0x00009bd7, 0x00005abb,      0, "a0015" },
    { 0x0000f692, 0x00002c3c,      0, "a0016" },
    { 0x000122ce, 0x00002125,      0, "a0017" },
    { 0x000143f3, 0x0000ac44,      0, "a0018" },
    { 0x0001f037, 0x0001760f,      0, "a0019" },
    { 0x00036646, 0x000082cd,      0, "a0020" },
    { 0x0003e913, 0x000053f2,      0, "a0021" },
    { 0x00043d05, 0x00003d62,      0, "a0022" },
    { 0x00047a67, 0x00001763,      0, "a0023" },
    { 0x0004fd39, 0x0000e9bd,      0, "b0001" },
    { 0x0005e6f6, 0x000026ec,      0, "b0002" },
    { 0x00060de2, 0x0000ad30,      0, "b0003" },
    { 0x002a7255, 0x0000015e,      0, "end"   },
    { 0x002e7fa1, 0x00000775,      0, "main"  },
};
#define PAK_MAP_N (sizeof(g_pak_map)/sizeof(g_pak_map[0]))

static int pak_find(DWORD off) {
    for (int i=0;i<(int)PAK_MAP_N;i++)
        if (off==g_pak_map[i].offset) return i;
    return -1;
}

/* ── G2 шифрование ───────────────────────────────────────────────── */

static void g2_encrypt(unsigned char *buf, int n) {
    for (int i=0;i<n;i++) buf[i]=(unsigned char)((buf[i]+i+n)&0xFF);
}

/* ── GCE1 декомпрессор ───────────────────────────────────────────── */

static BYTE *gce1_decompress(const BYTE *data, DWORD data_size, DWORD unpacked_size) {
    BYTE *out=(BYTE*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,unpacked_size);
    if(!out) return NULL;
    DWORD src=0,dst=0;
    DWORD frame[0x10000];
    memset(frame,0,sizeof(frame));
    while(src+8<=data_size){
        DWORD seg_id=*(DWORD*)(data+src); src+=4;
        DWORD seg_len=*(DWORD*)(data+src); src+=4;
        if(seg_id==0x30454347){ /* GCE0 */
            DWORD n=seg_len<(unpacked_size-dst)?seg_len:(unpacked_size-dst);
            memcpy(out+dst,data+src,n); dst+=n; src+=seg_len;
        } else if(seg_id==0x31454347){ /* GCE1 */
            src+=4;
            DWORD data_len=*(DWORD*)(data+src); src+=4;
            src+=4;
            DWORD cmd_len=*(DWORD*)(data+src); src+=4;
            const BYTE *ldata=data+src;
            const BYTE *ctrl=data+src+data_len;
            DWORD li=0; int cp=0,cl=(int)cmd_len,bp=8;
            DWORD fp=0,dst_end=dst+seg_len;
#define GB() ((--bp<0?(++cp,bp=7,--cl,0):0),(cl>0?(1&(ctrl[cp]>>bp)):0))
#define GL() ({ int _f=GB();int _v=0; \
    if(!_f){int _d=0;while(GB()==0)_d++;_v=1<<_d; \
    for(int _i=_d-1;_i>=0;_i--)_v|=GB()<<_i;}_v;})
            while(dst<dst_end){
                int n=GL();
                while(n-->0&&dst<dst_end){frame[fp]=dst;BYTE b=ldata[li++];fp=((fp<<8)|b)&0xFFFF;out[dst++]=b;}
                if(dst>=dst_end)break;
                int nr=GL()+1; DWORD s=frame[fp];
                while(nr-->0&&dst<dst_end){frame[fp]=dst;fp=((fp<<8)|out[s])&0xFFFF;out[dst++]=out[s++];}
            }
#undef GB
#undef GL
            dst=dst_end; src+=data_len+cmd_len;
        } else break;
    }
    return out;
}

/* ── Словарь ─────────────────────────────────────────────────────── */

typedef struct { DWORD offset; char text[MAX_TEXT]; } TransEntry;
typedef struct {
    char name[64]; TransEntry entries[MAX_ENTRIES]; int count;
} TransDict;

static TransDict        g_dict={{0}};
static char             g_dir[MAX_PATH]={0};
static CRITICAL_SECTION g_cs;

static void dict_load(const char *name){
    if(strcmp(g_dict.name,name)==0) return;
    char path[MAX_PATH];
    _snprintf(path,MAX_PATH,"%s\\%s.txt",g_dir,name);
    FILE *f=fopen(path,"r");
    if(!f){dbg(2,"no dict: %s",path);g_dict.name[0]='\0';g_dict.count=0;return;}
    g_dict.count=0;
    strncpy(g_dict.name,name,sizeof(g_dict.name)-1);
    char line[MAX_TEXT+32];
    while(fgets(line,sizeof(line),f)&&g_dict.count<MAX_ENTRIES){
        int len=(int)strlen(line);
        while(len>0&&(line[len-1]=='\r'||line[len-1]=='\n'))line[--len]='\0';
        if(!line[0]||line[0]=='#') continue;
        char *eq=strchr(line,'='); if(!eq) continue; *eq='\0';
        TransEntry *e=&g_dict.entries[g_dict.count++];
        e->offset=(DWORD)strtoul(line,NULL,16);
        strncpy(e->text,eq+1,MAX_TEXT-1); e->text[MAX_TEXT-1]='\0';
    }
    fclose(f);
    dbg(1,"dict '%s': %d entries",name,g_dict.count);
}

static const char *dict_lookup(DWORD off){
    for(int i=0;i<g_dict.count;i++)
        if(g_dict.entries[i].offset==off) return g_dict.entries[i].text;
    return NULL;
}

/* ── Патч строк в распакованном буфере ───────────────────────────── */

static void patch_raw(unsigned char *data, DWORD size){
    if(!g_dict.count) return;
    int patched=0; DWORD i=0;
    while(i+6<=size){
        unsigned short mk=(unsigned short)(data[i]|(data[i+1]<<8));
        if(mk==0x0100||mk==0x0104){
            DWORD ln=(DWORD)(data[i+2]|(data[i+3]<<8)|(data[i+4]<<16)|(data[i+5]<<24));
            if(ln>0&&ln<384&&i+6+ln<=size){
                const char *trans=dict_lookup(i+6);
                if(trans&&trans[0]){
                    WCHAR wbuf[MAX_TEXT]; char cpbuf[MAX_TEXT];
                    int wl=MultiByteToWideChar(CP_UTF8,0,trans,-1,wbuf,MAX_TEXT);
                    if(wl>0){
                        int cl=WideCharToMultiByte(932,0,wbuf,-1,cpbuf,MAX_TEXT,NULL,NULL);
                        int bytes=cl>0?cl-1:0;
                        if(bytes>0){
                            unsigned char enc[384]={0};
                            int copy=bytes<(int)ln?bytes:(int)ln;
                            memcpy(enc,cpbuf,copy);
                            /* Дополняем пробелами до оригинальной длины.
                               Движок считает символы по length (ln),
                               пробелы невидимы — двойного клика нет. */
                            if(copy < (int)ln)
                                memset(enc+copy, 0x20, (int)ln - copy);
                            g2_encrypt(enc,(int)ln);
                            memcpy(data+i+6,enc,ln);
                            patched++;
                            dbg(1,"patch 0x%x '%s'",i+6,trans);
                        }
                    }
                }
                i+=6+ln; continue;
            }
        }
        i++;
    }
    if(patched) dbg(1,"patched %d strings",patched);
}

/* ── Активный override (как в pakmod.c) ──────────────────────────── */

typedef struct {
    HANDLE handle;
    BYTE  *data;
    DWORD  size;
    DWORD  pos;
    BOOL   active;
} ActiveOvr;

static ActiveOvr g_ovr={INVALID_HANDLE_VALUE,NULL,0,0,FALSE};
static HANDLE g_data_pak=INVALID_HANDLE_VALUE;

static void ovr_clear(void){
    if(g_ovr.data){HeapFree(GetProcessHeap(),0,g_ovr.data);g_ovr.data=NULL;}
    g_ovr.handle=INVALID_HANDLE_VALUE;
    g_ovr.size=g_ovr.pos=0;
    g_ovr.active=FALSE;
}

/* ── Трекинг PAK хэндлов ─────────────────────────────────────────── */

#define MAX_PAK 16
static HANDLE g_pak[MAX_PAK]; static int g_npak=0;
static BOOL is_pak(HANDLE h){for(int i=0;i<g_npak;i++)if(g_pak[i]==h)return TRUE;return FALSE;}
static void pak_add(HANDLE h){if(g_npak<MAX_PAK)g_pak[g_npak++]=h;}
static void pak_remove(HANDLE h){for(int i=0;i<g_npak;i++)if(g_pak[i]==h){g_pak[i]=g_pak[--g_npak];return;}}

/* ── Оригинальные функции ────────────────────────────────────────── */

typedef DWORD (WINAPI*PFN_GetGlyphOutlineA)(HDC,UINT,UINT,LPGLYPHMETRICS,DWORD,LPVOID,const MAT2*);
typedef HFONT (WINAPI*PFN_CreateFontA)(int,int,int,int,int,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCSTR);
typedef HANDLE(WINAPI*PFN_CFA)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
typedef BOOL  (WINAPI*PFN_RF) (HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
typedef BOOL  (WINAPI*PFN_CH) (HANDLE);
typedef DWORD (WINAPI*PFN_GFS)(HANDLE,LPDWORD);
typedef DWORD (WINAPI*PFN_SFP)(HANDLE,LONG,PLONG,DWORD);
static PFN_GetGlyphOutlineA Real_GetGlyphOutlineA=NULL;
static PFN_CreateFontA Real_CreateFontA=NULL;
static PFN_CFA Real_CFA=NULL; static PFN_RF  Real_RF=NULL;
static PFN_CH  Real_CH=NULL;  static PFN_GFS Real_GFS=NULL;
static PFN_SFP Real_SFP=NULL;

/* ── Хуки ────────────────────────────────────────────────────────── */

/* GetGlyphOutlineA хук: конвертируем символы CP932 в Unicode кодпоинты.
 * Движок передаёт uChar как CP932 байт(ы). С DEFAULT_CHARSET Windows
 * трактует uChar как Unicode — нужно конвертировать. */
/* Таблица замены символов при рендеринге:
 * Движок передаёт ASCII символ -> мы возвращаем глиф из Unicode
 * ><+-=# -> ÓÕ¹²×É (специальные глифы шрифта YasuSakuuta) */
static const char    g_src_chars[] = "><+%#^";
static const WCHAR   g_dst_chars[] = {0x00D3, 0x00D5, 0x00B9, 0x00B2, 0x00C9, 0x00C9, 0};
/* О   Õ       ¹       ²       ×       É */

DWORD WINAPI Hook_GetGlyphOutlineA(HDC hdc, UINT uChar, UINT fuFormat,
    LPGLYPHMETRICS lpgm, DWORD cjBuf, LPVOID pvBuf, const MAT2 *lpmat2)
{
    UINT uCharW = uChar;

    /* Сначала проверяем таблицу замены (однобайтовые ASCII символы) */
    if (uChar < 0x100) {
        for (int k = 0; g_src_chars[k]; k++) {
            if ((char)uChar == g_src_chars[k]) {
                uCharW = (UINT)g_dst_chars[k];
                goto call_orig;
            }
        }
    }

    /* Конвертируем из CP932 в Unicode для остальных символов */
    {
        char mb[3] = {0};
        if (uChar > 0xFF) {
            mb[0] = (char)((uChar >> 8) & 0xFF);
            mb[1] = (char)(uChar & 0xFF);
        } else {
            mb[0] = (char)(uChar & 0xFF);
        }
        WCHAR wc = 0;
        if (MultiByteToWideChar(932, 0, mb, -1, &wc, 1) > 0 && wc != 0)
            uCharW = (UINT)wc;
    }

call_orig:
    return Real_GetGlyphOutlineA(hdc, uCharW, fuFormat, lpgm, cjBuf, pvBuf, lpmat2);
}

#define FONT_ORIG  "\x82\x6c\x82\x72\x20\x83\x53\x83\x56\x83\x62\x83\x4e"  /* ＭＳ ゴシック CP932 */
#define FONT_NEW   "OUCTSfontD1"

HFONT WINAPI Hook_CreateFontA(int h,int w,int e,int o,int fw,DWORD i,DWORD u,DWORD so,
    DWORD cs,DWORD op,DWORD cp,DWORD q,DWORD pf,LPCSTR face)
{
    const char *use_face = face;
    if (face && strcmp(face, FONT_ORIG)==0) {
        use_face = FONT_NEW;
        cs = DEFAULT_CHARSET;  /* позволяет рендерить кириллицу */
        dbg(1,"CreateFontA: '%s' -> '%s' charset=DEFAULT", face, FONT_NEW);
    }
    return Real_CreateFontA(h,w,e,o,fw,i,u,so,cs,op,cp,q,pf,use_face);
}

HANDLE WINAPI Hook_CFA(LPCSTR name,DWORD acc,DWORD share,
    LPSECURITY_ATTRIBUTES sec,DWORD disp,DWORD flags,HANDLE tpl){
    /* Логируем обращения к картинкам */
    if(name&&(strstr(name,".bmp")||strstr(name,".argb")||strstr(name,".bgra")))
        dbg(1,"CFA img: '%s'",name);
    HANDLE h=Real_CFA(name,acc,share,sec,disp,flags,tpl);
    if(name&&strstr(name,"script.pak")&&h!=INVALID_HANDLE_VALUE){
        const char *p=strstr(name,"script.pak")+strlen("script.pak");
        if(*p=='\0'){pak_add(h);dbg(1,"pak: %p",h);}
    }
    if(name&&strstr(name,"data.pak")&&h!=INVALID_HANDLE_VALUE){
        const char *p=strstr(name,"data.pak")+strlen("data.pak");
        if(*p=='\0'){
            g_data_pak=h;
            dbg(1,"data.pak opened: %p",h);
        }
    }
    return h;
}

BOOL WINAPI Hook_RF(HANDLE h,LPVOID buf,DWORD n,LPDWORD rd,LPOVERLAPPED ov){
    /* Если активен override для этого хэндла — отдаём из нашего буфера */
    if(g_ovr.active && g_ovr.handle==h){
        DWORD avail=g_ovr.size-g_ovr.pos;
        DWORD got=(n<avail)?n:avail;
        if(got) memcpy(buf,g_ovr.data+g_ovr.pos,got);
        g_ovr.pos+=got; if(rd)*rd=got;
        dbg(2,"ReadFile OVR: req=%u got=%u pos=%u/%u",n,got,g_ovr.pos,g_ovr.size);
        /* Сброс если прочитали всё — движок может читать дважды */
        if(g_ovr.pos>=g_ovr.size){
            dbg(1,"OVR end, reset pos");
            g_ovr.pos=0;
        }
        return TRUE;
    }
    return Real_RF(h,buf,n,rd,ov);
}

BOOL WINAPI Hook_CH(HANDLE h){
    if(is_pak(h)){
        pak_remove(h);
        dbg(1,"pak closed: %p",h);
        EnterCriticalSection(&g_cs);
        if(g_ovr.handle==h) ovr_clear();
        LeaveCriticalSection(&g_cs);
    }
    return Real_CH(h);
}

DWORD WINAPI Hook_GFS(HANDLE h,LPDWORD hi){return Real_GFS(h,hi);}

DWORD WINAPI Hook_SFP(HANDLE h,LONG dist,PLONG hi,DWORD method){
    /* Логируем seek на data.pak для поиска картинок */
    if(h==g_data_pak && method==FILE_BEGIN){
        static int s_img_log=0;
        if(s_img_log<50){
            dbg(1,"data.pak SFP -> 0x%x",( DWORD)dist);
            s_img_log++;
        }
    }
    if(is_pak(h) && method==FILE_BEGIN){
        int idx=pak_find((DWORD)dist);
        if(idx>=0){
            const char *name=g_pak_map[idx].name;
            DWORD comp_size=g_pak_map[idx].size;
            DWORD unp_size=g_pak_map[idx].unpacked;

            EnterCriticalSection(&g_cs);
            dict_load(name);
            int has_dict=(g_dict.count>0);
            LeaveCriticalSection(&g_cs);

            dbg(1,"seek -> %s (dict=%d)",name,has_dict);

            if(has_dict && unp_size>0){
                /* Читаем сжатые данные */
                Real_SFP(h,(LONG)dist,NULL,FILE_BEGIN);
                BYTE *comp=(BYTE*)HeapAlloc(GetProcessHeap(),0,comp_size);
                if(comp){
                    DWORD nr=0;
                    Real_RF(h,comp,comp_size,&nr,NULL);
                    if(nr==comp_size){
                        BYTE *raw=gce1_decompress(comp,comp_size,unp_size);
                        HeapFree(GetProcessHeap(),0,comp);
                        if(raw){
                            EnterCriticalSection(&g_cs);
                            patch_raw(raw,unp_size);
                            LeaveCriticalSection(&g_cs);

                            /* GCE0 обёртка */
                            DWORD gce0_sz=8+unp_size;
                            BYTE *gce0=(BYTE*)HeapAlloc(GetProcessHeap(),0,gce0_sz);
                            if(gce0){
                                gce0[0]='G';gce0[1]='C';gce0[2]='E';gce0[3]='0';
                                *(DWORD*)(gce0+4)=unp_size;
                                memcpy(gce0+8,raw,unp_size);
                                HeapFree(GetProcessHeap(),0,raw);

                                EnterCriticalSection(&g_cs);
                                ovr_clear();
                                g_ovr.handle=h;
                                g_ovr.data=gce0;
                                g_ovr.size=gce0_sz;
                                g_ovr.pos=0;
                                g_ovr.active=TRUE;
                                LeaveCriticalSection(&g_cs);

                                dbg(1,"OVR armed for %s (%u bytes GCE0)",name,gce0_sz);
                                /* Позиционируем реальный файл куда угодно —
                                   ReadFile всё равно перехватим */
                                if(hi)*hi=0;
                                return (DWORD)dist;
                            }
                            HeapFree(GetProcessHeap(),0,raw);
                        }
                    } else {
                        HeapFree(GetProcessHeap(),0,comp);
                        dbg(1,"read error: %u/%u",nr,comp_size);
                    }
                }
            } else {
                /* Нет словаря — сбрасываем override */
                EnterCriticalSection(&g_cs);
                ovr_clear();
                LeaveCriticalSection(&g_cs);
            }
        } else {
            /* Seek на что-то не из нашей карты — сбрасываем */
            EnterCriticalSection(&g_cs);
            if(g_ovr.handle==h) ovr_clear();
            LeaveCriticalSection(&g_cs);
        }
    }
    return Real_SFP(h,dist,hi,method);
}

/* ── IAT патчер ─────────────────────────────────────────────────── */

typedef struct{const char*name;PVOID hook;PVOID*real;}HookE;
static HookE g_hooks[]={
    {"GetGlyphOutlineA",Hook_GetGlyphOutlineA,(PVOID*)&Real_GetGlyphOutlineA},
    {"CreateFontA",    Hook_CreateFontA,(PVOID*)&Real_CreateFontA},
    {"CreateFileA",    Hook_CFA,(PVOID*)&Real_CFA},
    {"ReadFile",       Hook_RF, (PVOID*)&Real_RF },
    {"CloseHandle",    Hook_CH, (PVOID*)&Real_CH },
    {"GetFileSize",    Hook_GFS,(PVOID*)&Real_GFS},
    {"SetFilePointer", Hook_SFP,(PVOID*)&Real_SFP},
};
#define NH (sizeof(g_hooks)/sizeof(g_hooks[0]))

static void patch_iat(void){
    HMODULE exe=GetModuleHandleA(NULL),k32=GetModuleHandleA("KERNEL32.dll");
    Real_GetGlyphOutlineA=(PFN_GetGlyphOutlineA)GetProcAddress(GetModuleHandleA("GDI32.dll"),"GetGlyphOutlineA");
    Real_CreateFontA=(PFN_CreateFontA)GetProcAddress(GetModuleHandleA("GDI32.dll"),"CreateFontA");
    Real_CFA=(PFN_CFA)GetProcAddress(k32,"CreateFileA");
    Real_RF =(PFN_RF) GetProcAddress(k32,"ReadFile");
    Real_CH =(PFN_CH) GetProcAddress(k32,"CloseHandle");
    Real_GFS=(PFN_GFS)GetProcAddress(k32,"GetFileSize");
    Real_SFP=(PFN_SFP)GetProcAddress(k32,"SetFilePointer");
    PIMAGE_DOS_HEADER dos=(PIMAGE_DOS_HEADER)exe;
    PIMAGE_NT_HEADERS nt=(PIMAGE_NT_HEADERS)((BYTE*)exe+dos->e_lfanew);
    PIMAGE_IMPORT_DESCRIPTOR imp=(PIMAGE_IMPORT_DESCRIPTOR)(
        (BYTE*)exe+nt->OptionalHeader.DataDirectory[1].VirtualAddress);
    int patched=0;
    for(;imp->Name;imp++){
        PIMAGE_THUNK_DATA th=(PIMAGE_THUNK_DATA)((BYTE*)exe+imp->FirstThunk);
        PIMAGE_THUNK_DATA or=imp->OriginalFirstThunk
            ?(PIMAGE_THUNK_DATA)((BYTE*)exe+imp->OriginalFirstThunk):th;
        for(;or->u1.Function;or++,th++){
            if(IMAGE_SNAP_BY_ORDINAL(or->u1.Ordinal))continue;
            PIMAGE_IMPORT_BY_NAME ibn=(PIMAGE_IMPORT_BY_NAME)((BYTE*)exe+or->u1.AddressOfData);
            for(DWORD i=0;i<NH;i++){
                if(strcmp((char*)ibn->Name,g_hooks[i].name))continue;
                DWORD old;
                VirtualProtect(&th->u1.Function,sizeof(PVOID),PAGE_READWRITE,&old);
                th->u1.Function=(ULONG_PTR)g_hooks[i].hook;
                VirtualProtect(&th->u1.Function,sizeof(PVOID),old,&old);
                patched++;
            }
        }
    }
    dbg(1,"IAT: %d/%d hooks",patched,(int)NH);
}

BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID r){
    if(reason==DLL_PROCESS_ATTACH){
        DisableThreadLibraryCalls(h);
        InitializeCriticalSection(&g_cs);
        char exe[MAX_PATH]; GetModuleFileNameA(NULL,exe,MAX_PATH);
        char *last=strrchr(exe,'\\'); if(last)*(last+1)='\0';
        _snprintf(g_dir,MAX_PATH,"%s%s",exe,TRANS_DIR);
        CreateDirectoryA(g_dir,NULL);
        patch_iat();
        dbg(1,"ready. dir=%s",g_dir);
    } else if(reason==DLL_PROCESS_DETACH){
        DeleteCriticalSection(&g_cs);
    }
    return TRUE;
}