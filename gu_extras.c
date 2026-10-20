/* gu_extras.c - see gu_extras.h. Freestanding: no C runtime calls. */
#include "gu_extras.h"

#if defined(_MSC_VER)
#define GU_X_CDECL __cdecl
#else
#define GU_X_CDECL
#endif

/* ------------------------------------------------------------------ */
/* platform layer                                                      */
/* ------------------------------------------------------------------ */
#ifdef GU_X_TEST
extern void *gu_xt_alloc(uint32_t n, int exec);
extern void gu_xt_free(void *p);
extern int gu_xt_protect(void *p, uint32_t n, uint32_t *old);
extern void gu_xt_restore(void *p, uint32_t n, uint32_t old);
extern void gu_xt_flush(void *p, uint32_t n);
extern uint32_t gu_xt_now_us(void);
#define os_alloc_data(n) gu_xt_alloc((n), 0)
#define os_alloc_exec(n) gu_xt_alloc((n), 1)
#define os_free_data(p) gu_xt_free(p)
#define os_protect_rwx(p, n, o) gu_xt_protect((p), (n), (o))
#define os_protect_restore(p, n, o) gu_xt_restore((p), (n), (o))
#define os_flush(p, n) gu_xt_flush((p), (n))
#define os_now_us() gu_xt_now_us()
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static void *os_alloc_data(uint32_t n)
{
    return VirtualAlloc(0, n, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}
static void *os_alloc_exec(uint32_t n)
{
    return VirtualAlloc(0, n, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}
static void os_free_data(void *p) { VirtualFree(p, 0, MEM_RELEASE); }
static int os_protect_rwx(void *p, uint32_t n, uint32_t *old)
{
    DWORD o = 0;
    int ok = VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &o) != 0;
    *old = (uint32_t)o;
    return ok;
}
static void os_protect_restore(void *p, uint32_t n, uint32_t old)
{
    DWORD unused = 0;
    (void)VirtualProtect(p, n, (DWORD)old, &unused);
}
static void os_flush(void *p, uint32_t n)
{
    (void)FlushInstructionCache(GetCurrentProcess(), p, n);
}
static uint32_t os_now_us(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        if (freq.QuadPart == 0) return 0;
    }
    QueryPerformanceCounter(&now);
    return (uint32_t)((now.QuadPart * 1000000LL) / freq.QuadPart);
}
#endif

struct GuExtrasReport g_gu_extras_report;

/* ------------------------------------------------------------------ */
/* SEH guard for the hooks                                             */
/* ------------------------------------------------------------------ */
/* A memory fault inside a hook (reading entity+0x24, the generator queue,
 * the camera globals) must not take the game down.  The hook body runs
 * under __try; on an access violation or in-page error the owning feature
 * is switched off for the rest of the session (the hook then returns at
 * once and the game runs its original code) and a counter goes up.  Other
 * exception codes are not handled here.  Outside MSVC (the Linux tests)
 * the guard compiles to nothing. */
#if defined(_MSC_VER) && !defined(GU_X_TEST)
static int gu_x_fault_filter(unsigned long code)
{
    return (code == EXCEPTION_ACCESS_VIOLATION ||
            code == EXCEPTION_IN_PAGE_ERROR) ?
        EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}
#define GU_X_TRY __try
#define GU_X_EXCEPT __except (gu_x_fault_filter(GetExceptionCode()))
#elif defined(GU_X_TEST)
/* Linux tests: a SIGSEGV handler jumps back here (see extras_tests). */
extern void *gu_xt_jb[5];
extern volatile int gu_xt_guard;
#define GU_X_TRY if (gu_xt_guard = 1, !__builtin_setjmp(gu_xt_jb))
#define GU_X_EXCEPT else
#else
#define GU_X_TRY if (1)
#define GU_X_EXCEPT else
#endif
static volatile uint32_t g_refl_disabled, g_cone_disabled, g_spawn_disabled;

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */
static void x_copy(void *d, const void *s, uint32_t n)
{
    uint8_t *dd = (uint8_t *)d; const uint8_t *ss = (const uint8_t *)s;
    uint32_t i;
    for (i = 0; i < n; ++i) dd[i] = ss[i];
}
static int x_equal(const void *a, const void *b, uint32_t n)
{
    const uint8_t *aa = (const uint8_t *)a, *bb = (const uint8_t *)b;
    uint32_t i;
    for (i = 0; i < n; ++i) if (aa[i] != bb[i]) return 0;
    return 1;
}
static uint32_t rd32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static void wr32(uint8_t *p, uint32_t v)
{ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static uint8_t *rdptr(const uint8_t *p) { return (uint8_t *)(uintptr_t)rd32(p); }
static uint32_t addr32(const void *p) { return (uint32_t)(uintptr_t)p; }

static float x_sqrtf(float x)
{
    union { float f; uint32_t u; } v;
    float y;
    if (!(x > 0.0f)) return 0.0f;
    v.f = x;
    v.u = (v.u >> 1) + 0x1FBD1DF5u;
    y = v.f;
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return y;
}

static float x_cosf_small(float x) /* 0 <= x <= ~2.2 */
{
    float x2 = x * x;
    return 1.0f - x2 * (0.5f - x2 * (1.0f / 24.0f - x2 * (1.0f / 720.0f -
        x2 * (1.0f / 40320.0f - x2 * (1.0f / 3628800.0f - x2 * (1.0f / 479001600.0f))))));
}

/* ------------------------------------------------------------------ */
/* patterns                                                            */
/* ------------------------------------------------------------------ */
static int hexv(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

#define X_PAT_MAX 48u
static uint32_t pat_parse(const char *s, uint8_t *bytes, uint8_t *mask)
{
    uint32_t n = 0;
    while (*s) {
        if (*s == ' ') { ++s; continue; }
        if (n >= X_PAT_MAX) return 0;
        if (*s == '?') {
            bytes[n] = 0; mask[n] = 0; ++n; ++s;
            if (*s == '?') ++s;
        } else {
            int hi = hexv(s[0]), lo = hexv(s[1]);
            if (hi < 0 || lo < 0) return 0;
            bytes[n] = (uint8_t)(hi * 16 + lo); mask[n] = 1; ++n; s += 2;
        }
    }
    return n;
}

struct Range { const uint8_t *p; uint32_t n; };

/* counts matches over all ranges (stops counting at 2); first match returned */
/* counts matches over all ranges up to `cap`; keeps the first two */
static void scan_pattern(const struct Range *ranges, uint32_t nranges, const char *pattern,
                         uint32_t cap, const uint8_t **first, const uint8_t **second, uint32_t *matches)
{
    uint8_t b[X_PAT_MAX], m[X_PAT_MAX];
    uint32_t plen = pat_parse(pattern, b, m), r, i, j;
    *matches = 0; *first = 0; *second = 0;
    if (plen == 0) return;
    for (r = 0; r < nranges; ++r) {
        const uint8_t *t = ranges[r].p;
        uint32_t n = ranges[r].n;
        if (n < plen) continue;
        for (i = 0; i + plen <= n; ++i) {
            if (m[0] && t[i] != b[0]) continue;
            for (j = 1; j < plen; ++j)
                if (m[j] && t[i + j] != b[j]) break;
            if (j == plen) {
                if (*matches == 0) *first = t + i;
                else if (*matches == 1) *second = t + i;
                if (++*matches >= cap) return;
            }
        }
    }
}

enum {
    S_M1, S_M2, S_M3, S_M4, S_M5,
    S_R0, S_RA, S_RB,
    S_C1, S_C3, S_C4, S_CAM,
    S_D1, S_RC, S_C2,
    S_COUNT
};
static const char *const k_site_name[S_COUNT] = {
    "matrix.limit.a", "matrix.limit.b", "matrix.pool.size", "matrix.pool.alloc",
    "matrix.pool.free", "refl.phases", "refl.list.a", "refl.list.b",
    "cone.before", "cone.reset", "cone.destructor", "cone.camera",
    "drawable.refs", "rendercache.scans", "spawn.after"
};
static const char *const k_site_pat[S_COUNT] = {
    "81 3D ? ? ? ? 00 02 00 00 7C ? 57 B9",
    "81 F9 00 02 00 00 0F 8D ? ? ? ? 8B 4F 08 E8",
    "68 58 1B 00 00 B9 ? ? ? ? C7 05",
    "51 E8 ? ? ? ? 89 83 E0 01 00 00",
    "FF B6 E0 01 00 00 E8 ? ? ? ? 83 C4 04",
    "85 0D ? ? ? ? 75 ? 0F B6 46 63 3D EF 00 00 00",
    "F7 43 40 00 01 00 00 F3 0F 10 47 04",
    "89 4C 24 28 A9 00 00 00 08",
    "33 C0 33 FF 66 3B 86 28 52 00 00",
    "FF B5 24 52 00 00 E8",
    "FF B6 24 52 00 00 E8",
    "F3 0F 10 05 ? ? ? ? F3 0F 10 15 ? ? ? ? F3 0F 10 0D ? ? ? ? 56 8B 75 0C",
    "68 ? ? ? ? E8 ? FF FF FF C3",
    "E8 ? ? ? ? 8B 0D ? ? ? ? 2B C8 83 E1 F8 81 F9 00 00 01 00",
    "66 89 86 30 52 00 00 8B CE 5E E9"
};

static const uint8_t *g_site[S_COUNT];
static const uint8_t *g_site2[S_COUNT];   /* second match, only for the render-cache scans */

static int resolve(uint32_t site_mask, const struct Range *ranges, uint32_t nranges,
                   const uint8_t *image_base)
{
    uint32_t i, all_ok = 1;
    for (i = 0; i < S_COUNT; ++i) {
        uint32_t matches = 0, want = (i == S_RC) ? 2u : 1u;   /* the render-cache scans exist twice */
        const uint8_t *p, *p2;
        g_gu_extras_report.site_name[i] = k_site_name[i];
        if (!(site_mask & (1u << i))) continue;
        scan_pattern(ranges, nranges, k_site_pat[i], 3u, &p, &p2, &matches);
        g_gu_extras_report.site_matches[i] = matches;
        g_gu_extras_report.site_rva[i] = p ? (uint32_t)(p - image_base) : 0;
        g_site[i] = (matches == want) ? p : 0;
        g_site2[i] = (matches == want) ? p2 : 0;
        if (matches != want) all_ok = 0;
    }
    g_gu_extras_report.sites_known = S_COUNT;
    return all_ok;
}
#define BIT(s) (1u << (s))

/* ------------------------------------------------------------------ */
/* patch transaction                                                   */
/* ------------------------------------------------------------------ */
struct Patch { uint8_t *addr; uint32_t len; uint8_t orig[16]; uint8_t neu[16]; };
struct Tx { struct Patch p[8]; uint32_t n; };

static int tx_add(struct Tx *t, uint8_t *addr, const uint8_t *neu, uint32_t len)
{
    struct Patch *p;
    if (t->n >= 8 || len == 0 || len > 16) return 0;
    p = &t->p[t->n++];
    p->addr = addr; p->len = len;
    x_copy(p->orig, addr, len);
    x_copy(p->neu, neu, len);
    return 1;
}

static void tx_restore(struct Tx *t, uint32_t upto)
{
    uint32_t i;
    for (i = 0; i < upto; ++i) {
        uint32_t old = 0;
        if (os_protect_rwx(t->p[i].addr, t->p[i].len, &old)) {
            x_copy(t->p[i].addr, t->p[i].orig, t->p[i].len);
            os_flush(t->p[i].addr, t->p[i].len);
            os_protect_restore(t->p[i].addr, t->p[i].len, old);
        }
    }
}

static uint32_t tx_commit(struct Tx *t, int (*busy)(uint32_t, uint32_t, void *), void *ctx)
{
    uint32_t i, old;
    if (busy)
        for (i = 0; i < t->n; ++i)
            if (busy(addr32(t->p[i].addr), t->p[i].len, ctx)) return GU_X_ERR_BUSY;
    for (i = 0; i < t->n; ++i) {
        old = 0;
        if (!os_protect_rwx(t->p[i].addr, t->p[i].len, &old)) {
            tx_restore(t, i);
            return GU_X_ERR_MEMORY;
        }
        x_copy(t->p[i].addr, t->p[i].neu, t->p[i].len);
        os_flush(t->p[i].addr, t->p[i].len);
        os_protect_restore(t->p[i].addr, t->p[i].len, old);
    }
    for (i = 0; i < t->n; ++i) {
        if (!x_equal(t->p[i].addr, t->p[i].neu, t->p[i].len)) {
            tx_restore(t, t->n);
            return GU_X_ERR_WRITE;
        }
    }
    return GU_X_ON;
}

/* ------------------------------------------------------------------ */
/* stubs                                                               */
/* ------------------------------------------------------------------ */
#define X_ARENA_SIZE 4096u
#define X_STUB_MAX 192u
static uint8_t *g_arena;
static uint32_t g_arena_used;

static uint8_t *arena_take(void)
{
    uint8_t *p;
    if (!g_arena) {
        g_arena = (uint8_t *)os_alloc_exec(X_ARENA_SIZE);
        if (!g_arena) return 0;
    }
    if (g_arena_used + X_STUB_MAX > X_ARENA_SIZE) return 0;
    p = g_arena + g_arena_used;
    g_arena_used += X_STUB_MAX;
    return p;
}

/* pushfd; pushad; [save xmm0-7]; push <regs>; call fn; add esp,n;
 * [mov [esp+18h],eax]; [restore xmm0-7]; popad; popfd; <tail>;
 * <original bytes>; jmp back.  Returns stub length.
 *
 * save_xmm: the C hooks are compiled with SSE2, so they may overwrite
 * xmm0..xmm7 that the game still holds live.  When set, the stub keeps EDI
 * as a frame pointer (saved by pushad, restored by popad), aligns ESP to 16,
 * stores xmm0..xmm7 with movups and puts them back after the call.  EDI must
 * not be one of push_regs.  The saved ECX is then at [edi+18h]. */
static uint32_t build_stub(uint8_t *out, uint32_t fn, const uint8_t *push_regs,
                           uint32_t npush, int writeback_ecx, int save_xmm,
                           const uint8_t *tail, uint32_t tail_len,
                           const uint8_t *orig, uint32_t orig_len, uint32_t ret_addr)
{
    uint32_t n = 0, i;
    out[n++] = 0x9C;                 /* pushfd */
    out[n++] = 0x60;                 /* pushad */
    if (save_xmm) {
        out[n++] = 0x89; out[n++] = 0xE7;                         /* mov edi, esp */
        out[n++] = 0x81; out[n++] = 0xEC; wr32(out + n, 128u); n += 4;  /* sub esp, 128 */
        out[n++] = 0x83; out[n++] = 0xE4; out[n++] = 0xF0;        /* and esp, -16 */
        for (i = 0; i < 8; ++i) {                                 /* movups [esp+i*16], xmm<i> */
            out[n++] = 0x0F; out[n++] = 0x11;
            out[n++] = (uint8_t)(0x44u | (i << 3)); out[n++] = 0x24; out[n++] = (uint8_t)(i * 16u);
        }
    }
    for (i = 0; i < npush; ++i) out[n++] = (uint8_t)(0x50 + push_regs[i]);
    out[n++] = 0xB8; wr32(out + n, fn); n += 4;   /* mov eax, fn */
    out[n++] = 0xFF; out[n++] = 0xD0;             /* call eax */
    out[n++] = 0x83; out[n++] = 0xC4; out[n++] = (uint8_t)(npush * 4u); /* add esp, n */
    if (writeback_ecx) {             /* mov [..+18h], eax : the saved ECX */
        if (save_xmm) { out[n++] = 0x89; out[n++] = 0x47; out[n++] = 0x18; }   /* [edi+18h] */
        else { out[n++] = 0x89; out[n++] = 0x44; out[n++] = 0x24; out[n++] = 0x18; } /* [esp+18h] */
    }
    if (save_xmm) {
        for (i = 0; i < 8; ++i) {                                 /* movups xmm<i>, [esp+i*16] */
            out[n++] = 0x0F; out[n++] = 0x10;
            out[n++] = (uint8_t)(0x44u | (i << 3)); out[n++] = 0x24; out[n++] = (uint8_t)(i * 16u);
        }
        out[n++] = 0x89; out[n++] = 0xFC;                         /* mov esp, edi */
    }
    out[n++] = 0x61;                 /* popad */
    out[n++] = 0x9D;                 /* popfd */
    for (i = 0; i < tail_len; ++i) out[n++] = tail[i];
    for (i = 0; i < orig_len; ++i) out[n++] = orig[i];
    out[n++] = 0xE9;
    wr32(out + n, ret_addr - (addr32(out + n) + 4u)); n += 4;
    return n;
}

/* site patch: E9 rel32 to stub, NOP-padded to the window length */
static void make_jump(uint8_t *bytes, uint32_t window, uint32_t site, uint32_t stub)
{
    uint32_t i;
    bytes[0] = 0xE9;
    wr32(bytes + 1, stub - (site + 5u));
    for (i = 5; i < window; ++i) bytes[i] = 0x90;
}

/* ------------------------------------------------------------------ */
/* feature 1: matrix limit and pool                                    */
/* ------------------------------------------------------------------ */
#define X_MATRIX_VANILLA_LIMIT 512u
#define X_MATRIX_VANILLA_POOL 7000u
static uint32_t g_orig_new, g_orig_delete;
static void *g_pool_ptr[4];

static void *GU_X_CDECL gu_x_matrix_alloc(uint32_t size)
{
    void *p = os_alloc_data(size);
    uint32_t i;
    if (p) {
        for (i = 0; i < 4; ++i)
            if (!g_pool_ptr[i]) { g_pool_ptr[i] = p; ++g_gu_extras_report.matrix_pool_allocs; return p; }
        os_free_data(p);   /* no room to remember it: do not hand it out */
    }
    ++g_gu_extras_report.matrix_pool_alloc_fallbacks;
    return ((void *(GU_X_CDECL *)(uint32_t))(uintptr_t)g_orig_new)(size);
}

static void GU_X_CDECL gu_x_matrix_free(void *p)
{
    uint32_t i;
    for (i = 0; i < 4; ++i)
        if (p && g_pool_ptr[i] == p) {
            g_pool_ptr[i] = 0; os_free_data(p); ++g_gu_extras_report.matrix_pool_frees_ours; return;
        }
    ++g_gu_extras_report.matrix_pool_frees_forwarded;
    ((void (GU_X_CDECL *)(void *))(uintptr_t)g_orig_delete)(p);
}

static uint32_t install_matrix(const struct GuExtrasConfig *cfg,
                               int (*busy)(uint32_t, uint32_t, void *), void *ctx)
{
    struct Tx tx;
    uint32_t pool = X_MATRIX_VANILLA_POOL, limit, rel, dest;
    uint8_t buf[4];
    int want_pool;
    uint8_t *m1 = (uint8_t *)g_site[S_M1], *m2 = (uint8_t *)g_site[S_M2];
    uint8_t *m3 = (uint8_t *)g_site[S_M3], *m4 = (uint8_t *)g_site[S_M4];
    uint8_t *m5 = (uint8_t *)g_site[S_M5];
    tx.n = 0;
    if (cfg->matrix_pool != 0) pool = cfg->matrix_pool;
    else if (cfg->matrix_limit > X_MATRIX_VANILLA_LIMIT) pool = 28000u;
    if (pool < X_MATRIX_VANILLA_POOL) pool = X_MATRIX_VANILLA_POOL;
    if (pool > 65536u) pool = 65536u;
    want_pool = (pool != X_MATRIX_VANILLA_POOL);

    limit = cfg->matrix_limit;
    if (limit != 0) {
        uint32_t cap = pool / 6u;            /* keep matrices for everything else */
        if (cap < X_MATRIX_VANILLA_LIMIT) cap = X_MATRIX_VANILLA_LIMIT;
        if (limit > 4096u) limit = 4096u;
        if (limit > cap) limit = cap;
        if (limit <= X_MATRIX_VANILLA_LIMIT) limit = 0;
    }
    if (limit == 0 && !want_pool) return GU_X_OFF;

    if (limit != 0) {
        /* The two compare immediates are the same instructions the main fix
         * already raises (rendered capacity). Only touch them when they are
         * still in their original form. */
        int original = m1 && m2 && rd32(m1 + 6) == X_MATRIX_VANILLA_LIMIT &&
                       rd32(m2 + 2) == X_MATRIX_VANILLA_LIMIT;
        g_gu_extras_report.matrix_limit_sites_original = original ? 1u : 0u;
        if (original) {
            wr32(buf, limit);
            if (!tx_add(&tx, m1 + 6, buf, 4) || !tx_add(&tx, m2 + 2, buf, 4)) return GU_X_ERR_SITE;
        } else {
            limit = 0;
        }
    }
    if (limit == 0 && !want_pool) return GU_X_ERR_SITE;    /* nothing left to do */
    if (want_pool) {
        if (!m3 || !m4 || !m5) return GU_X_ERR_SITE;
        if (rd32(m3 + 1) != X_MATRIX_VANILLA_POOL) return GU_X_ERR_SITE;
        if (m4[1] != 0xE8 || m5[6] != 0xE8) return GU_X_ERR_SITE;
        wr32(buf, pool);
        if (!tx_add(&tx, m3 + 1, buf, 4)) return GU_X_ERR_SITE;
        dest = addr32(m4 + 1) + 5u + rd32(m4 + 2);
        g_orig_new = dest;
        rel = addr32((const void *)(uintptr_t)&gu_x_matrix_alloc) - (addr32(m4 + 1) + 5u);
        wr32(buf, rel);
        if (!tx_add(&tx, m4 + 2, buf, 4)) return GU_X_ERR_SITE;
        dest = addr32(m5 + 6) + 5u + rd32(m5 + 7);
        g_orig_delete = dest;
        rel = addr32((const void *)(uintptr_t)&gu_x_matrix_free) - (addr32(m5 + 6) + 5u);
        wr32(buf, rel);
        if (!tx_add(&tx, m5 + 7, buf, 4)) return GU_X_ERR_SITE;
    }
    {
        uint32_t st = tx_commit(&tx, busy, ctx);
        if (st == GU_X_ON) {
            g_gu_extras_report.matrix_limit_applied = limit;
            g_gu_extras_report.matrix_pool_applied = want_pool ? pool : X_MATRIX_VANILLA_POOL;
        }
        return st;
    }
}

/* ------------------------------------------------------------------ */
/* feature 2: props out of the reflection passes                       */
/* ------------------------------------------------------------------ */
static const volatile uint32_t *g_refl_phases;
static const volatile uint32_t *g_scene_phase;
static int (*g_is_prop)(uint32_t);

static uint32_t gu_x_drop_reflections_impl(uint32_t entity, uint32_t phases)
{
    uint32_t refl, flags;
    if (!g_refl_phases || !g_scene_phase) return phases;
    refl = *g_refl_phases & ~*g_scene_phase;
    if ((phases & refl) == 0) return phases;
    ++g_gu_extras_report.reflections_candidates;
    if (entity < 0x10000u) return phases;
    flags = *(const uint32_t *)(uintptr_t)(entity + 0x24u);
    if ((flags & 0x40010000u) != 0x40010000u) return phases;   /* generator props have both */
    ++g_gu_extras_report.reflections_flagged;
    if (g_is_prop && !g_is_prop(entity)) return phases;
    ++g_gu_extras_report.reflections_skipped;
    return phases & ~refl;
}

static uint32_t GU_X_CDECL gu_x_drop_reflections(uint32_t entity, uint32_t phases)
{
    uint32_t result = phases;
    if (g_refl_disabled) return phases;
    GU_X_TRY {
        result = gu_x_drop_reflections_impl(entity, phases);
    }
    GU_X_EXCEPT {
        g_refl_disabled = 1;
        ++g_gu_extras_report.reflections_faults;
        g_gu_extras_report.reflections_disabled = 1;
        result = phases;
    }
    return result;
}

static int in_image(const uint8_t *base, uint32_t size, uint32_t a, uint32_t len)
{
    uint32_t b = addr32(base);
    return a >= b && a + len >= a && a + len <= b + size;
}

static uint32_t install_reflections(const struct GuExtrasConfig *cfg,
                                    const uint8_t *image_base, uint32_t image_size,
                                    int (*busy)(uint32_t, uint32_t, void *), void *ctx)
{
    struct Tx tx;
    uint8_t *ra = (uint8_t *)g_site[S_RA], *rb = (uint8_t *)g_site[S_RB];
    const uint8_t *r0 = g_site[S_R0];
    uint8_t *sa, *sb, jump[16];
    uint32_t phases_ptr, scene_ptr, len;
    static const uint8_t regs_a[2] = { 1 /* ecx */, 6 /* esi */ };
    static const uint8_t tail_b[4] = { 0x89, 0x4C, 0x24, 0x28 };   /* mov [esp+28h], ecx */
    if (cfg->reflections_props != 0) return GU_X_OFF;
    if (!ra || !rb || !r0) return GU_X_ERR_SITE;
    /* the scene-phase global: test [abs], edx right after the short je */
    if (rb[9] != 0x74 || rb[11] != 0x85 || rb[12] != 0x15) return GU_X_ERR_SITE;
    phases_ptr = rd32(r0 + 2);
    scene_ptr = rd32(rb + 13);
    if (!in_image(image_base, image_size, phases_ptr, 4) ||
        !in_image(image_base, image_size, scene_ptr, 4)) return GU_X_ERR_SITE;
    sa = arena_take(); sb = arena_take();
    if (!sa || !sb) return GU_X_ERR_MEMORY;
    len = build_stub(sa, addr32((const void *)(uintptr_t)&gu_x_drop_reflections),
                     regs_a, 2, 1, 0, 0, 0, ra, 7, addr32(ra) + 7u);
    (void)len;
    len = build_stub(sb, addr32((const void *)(uintptr_t)&gu_x_drop_reflections),
                     regs_a, 2, 1, 0, tail_b, 4, rb + 4, 5, addr32(rb + 4) + 5u);
    (void)len;
    tx.n = 0;
    make_jump(jump, 7, addr32(ra), addr32(sa));
    if (!tx_add(&tx, ra, jump, 7)) return GU_X_ERR_SITE;
    make_jump(jump, 5, addr32(rb + 4), addr32(sb));
    if (!tx_add(&tx, rb + 4, jump, 5)) return GU_X_ERR_SITE;
    g_refl_disabled = 0;
    g_refl_phases = (const volatile uint32_t *)(uintptr_t)phases_ptr;
    g_scene_phase = (const volatile uint32_t *)(uintptr_t)scene_ptr;
    g_is_prop = cfg->is_prop_entity;
    {
        uint32_t st = tx_commit(&tx, busy, ctx);
        if (st != GU_X_ON) { g_refl_phases = 0; g_scene_phase = 0; }
        return st;
    }
}

/* ------------------------------------------------------------------ */
/* feature 3: cone back                                                */
/* ------------------------------------------------------------------ */
#define X_PARK_CAP 32768u
#define X_SCAN_MAX 8192u
#define X_CAND_MAX 4096u

static uint8_t *g_park;
static uint32_t g_park_count, g_cursor;
static const float *g_cam;                 /* position; forward is g_cam - 8 */
static uint8_t *g_manager;
static float g_back, g_cos_full;
static uint32_t g_release, g_lead_ms;
static int g_prev_valid;
/* spawn limit (and which of the two queue features are on) */
#define X_SPAWN_MAX 16384u
static int g_cone_on, g_spawn_on;
static uint8_t *g_hold, *g_scratch;
static float *g_sp_key, *g_sp_d2, *g_sp_tk;
static uint32_t *g_sp_idx, *g_sp_ti;
static uint32_t g_hold_count, g_carried, g_spawn_budget;
static float g_spawn_near2;
static float g_prev_f[3], g_prev_pos[3];
static uint32_t g_prev_us;
static uint32_t g_cand_idx[X_CAND_MAX];
static float g_cand_key[X_CAND_MAX];
static uint32_t g_tmp_idx[X_CAND_MAX];
static float g_tmp_key[X_CAND_MAX];

struct Frame {
    float pos[3], f[3], p[3];
    int use_pred, burst;
    float back, back2, ahead, ahead2, cos_full;
};

static void sort_pairs_with(float *key, uint32_t *idx, uint32_t n, float *tk, uint32_t *ti)
{
    uint32_t width, lo, mid, hi, a, b, k, i;
    for (width = 1; width < n; width *= 2) {
        for (lo = 0; lo < n; lo += 2 * width) {
            mid = lo + width; if (mid > n) mid = n;
            hi = lo + 2 * width; if (hi > n) hi = n;
            a = lo; b = mid; k = lo;
            while (a < mid && b < hi) {
                if (key[b] < key[a]) { tk[k] = key[b]; ti[k++] = idx[b++]; }
                else { tk[k] = key[a]; ti[k++] = idx[a++]; }
            }
            while (a < mid) { tk[k] = key[a]; ti[k++] = idx[a++]; }
            while (b < hi)  { tk[k] = key[b]; ti[k++] = idx[b++]; }
        }
        for (i = 0; i < n; ++i) { key[i] = tk[i]; idx[i] = ti[i]; }
    }
}

static void sort_pairs(float *key, uint32_t *idx, uint32_t n)
{
    sort_pairs_with(key, idx, n, g_tmp_key, g_tmp_idx);
}

static int in_cone(const struct Frame *c, const float *q)
{
    float dx = q[0] - c->pos[0], dy = q[1] - c->pos[1], dz = q[2] - c->pos[2];
    float d2 = dx * dx + dy * dy + dz * dz, d, cs, cp, t, s, r;
    if (d2 != d2) return 1;                  /* NaN: not ours to judge */
    if (d2 <= c->back2) return 1;
    if (d2 > c->ahead2) return 0;
    d = x_sqrtf(d2);
    cs = (dx * c->f[0] + dy * c->f[1] + dz * c->f[2]) / d;
    if (c->use_pred) {
        cp = (dx * c->p[0] + dy * c->p[1] + dz * c->p[2]) / d;
        if (cp > cs) cs = cp;
    }
    if (cs >= c->cos_full) return 1;
    t = (cs + 1.0f) / (c->cos_full + 1.0f);
    if (t < 0.0f) t = 0.0f;
    s = t * t * (3.0f - 2.0f * t);
    r = c->back + (c->ahead - c->back) * s;
    return d <= r;
}

static float dist2(const struct Frame *c, const float *q)
{
    float dx = q[0] - c->pos[0], dy = q[1] - c->pos[1], dz = q[2] - c->pos[2];
    return dx * dx + dy * dy + dz * dz;
}

static int make_frame(struct Frame *c)
{
    const float *pos = g_cam, *fwd = g_cam - 8;
    float ahead, l2, df[3], turn2, moved2, k, p2;
    uint32_t us, dt, i;
    float pf[3];
    if (!g_cam) return 0;
    if (g_cone_on && !g_manager) return 0;
    for (i = 0; i < 3; ++i) { c->pos[i] = pos[i]; c->f[i] = fwd[i]; }
    l2 = c->f[0] * c->f[0] + c->f[1] * c->f[1] + c->f[2] * c->f[2];
    if (!(l2 > 0.25f && l2 < 4.0f)) return 0;          /* also rejects NaN */
    if (c->pos[0] != c->pos[0] || c->pos[1] != c->pos[1] || c->pos[2] != c->pos[2]) return 0;
    {
        float inv = 1.0f / x_sqrtf(l2);
        for (i = 0; i < 3; ++i) c->f[i] *= inv;
    }
    c->back = 0.0f; c->back2 = 0.0f; c->ahead = 0.0f; c->ahead2 = 0.0f; c->cos_full = 1.0f;
    if (g_cone_on) {
        union { uint32_t u; float f; } v;
        v.u = rd32(g_manager + 0x1C);
        ahead = v.f;
        c->back = g_back; c->back2 = g_back * g_back;
        if (!(ahead > c->back + 1.0f) || ahead > 100000.0f) return 0;
        c->ahead = ahead; c->ahead2 = ahead * ahead;
        c->cos_full = g_cos_full;
    }
    c->use_pred = 0; c->burst = 0;
    us = os_now_us();
    if (g_prev_valid) {
        dt = us - g_prev_us;
        for (i = 0; i < 3; ++i) df[i] = c->f[i] - g_prev_f[i];
        turn2 = df[0] * df[0] + df[1] * df[1] + df[2] * df[2];
        moved2 = 0.0f;
        for (i = 0; i < 3; ++i) { float m = c->pos[i] - g_prev_pos[i]; moved2 += m * m; }
        if (turn2 > 0.7f || moved2 > 2500.0f) c->burst = 1;   /* a cut or a very fast turn */
        if (!c->burst && g_lead_ms != 0 && dt > 500u && dt < 250000u && turn2 > 1e-8f) {
            k = (float)g_lead_ms * 1000.0f / (float)dt;
            if (k > 15.0f) k = 15.0f;
            for (i = 0; i < 3; ++i) pf[i] = c->f[i] + df[i] * k;
            p2 = pf[0] * pf[0] + pf[1] * pf[1] + pf[2] * pf[2];
            if (p2 > 1e-6f) {
                float inv = 1.0f / x_sqrtf(p2);
                for (i = 0; i < 3; ++i) c->p[i] = pf[i] * inv;
                c->use_pred = 1;
            }
        }
    }
    g_prev_valid = 1; g_prev_us = us;
    for (i = 0; i < 3; ++i) { g_prev_f[i] = c->f[i]; g_prev_pos[i] = c->pos[i]; }
    return 1;
}

static void park_remove_at(uint32_t idx)
{
    uint32_t last = g_park_count - 1u;
    if (idx != last) x_copy(g_park + (size_t)idx * 0x20u, g_park + (size_t)last * 0x20u, 0x20u);
    g_park_count = last;
}

static void park_drop_removed(const uint8_t *removals, uint32_t rcount)
{
    uint32_t i = 0, j;
    while (i < g_park_count) {
        uint32_t provider = rd32(g_park + (size_t)i * 0x20u + 0x1C);
        int gone = 0;
        for (j = 0; j < rcount && !gone; ++j)
            gone = rd32(removals + (size_t)j * 8u) == provider;
        if (gone) { park_remove_at(i); ++g_gu_extras_report.cone_dropped_removed; }
        else ++i;
    }
}

static void gu_x_on_reset_impl(uint8_t *gen)
{
    (void)gen;
    if (g_cone_on) {
        ++g_gu_extras_report.cone_resets;
        g_gu_extras_report.cone_dropped_on_reset += g_park_count;
        g_park_count = 0; g_cursor = 0;
        g_gu_extras_report.cone_parked_now = 0;
    }
    g_hold_count = 0; g_carried = 0; g_prev_valid = 0;
}

/* ConeBack: what lies outside the cone waits, what the cone reaches comes back */
static uint32_t cone_step(uint8_t *queue, uint32_t count, uint32_t capacity,
                          const struct Frame *fr, int ok)
{
    uint32_t i, w;
    if (!ok) {
        /* no trustworthy camera: make everything that waits (as far as it fits) */
        ++g_gu_extras_report.cone_invalid_camera;
        while (g_park_count && count < capacity) {
            x_copy(queue + (size_t)count * 0x20u, g_park + (size_t)(g_park_count - 1u) * 0x20u, 0x20u);
            ++count; --g_park_count; ++g_gu_extras_report.cone_released_total;
        }
        g_gu_extras_report.cone_parked_now = g_park_count;
        return count;
    }

    /* new entries: in the cone they stay, outside they wait */
    for (i = 0, w = 0; i < count; ++i) {
        uint8_t *e = queue + (size_t)i * 0x20u;
        if (in_cone(fr, (const float *)e)) {
            if (w != i) x_copy(queue + (size_t)w * 0x20u, e, 0x20u);
            ++w; ++g_gu_extras_report.cone_made_directly;
        } else if (g_park_count < X_PARK_CAP) {
            x_copy(g_park + (size_t)g_park_count * 0x20u, e, 0x20u);
            ++g_park_count; ++g_gu_extras_report.cone_parked_total;
        } else {
            if (w != i) x_copy(queue + (size_t)w * 0x20u, e, 0x20u);   /* no room to wait: make it */
            ++w; ++g_gu_extras_report.cone_overflow_made;
        }
    }
    count = w;

    /* waiting ones that the cone now reaches come back, nearest first */
    if (g_park_count) {
        uint32_t n = g_park_count, scan, start, k, ncand = 0, take, room;
        scan = n < X_SCAN_MAX ? n : X_SCAN_MAX;
        start = g_cursor % n;
        for (k = 0; k < scan && ncand < X_CAND_MAX; ++k) {
            uint32_t idx = start + k;
            const float *q;
            if (idx >= n) idx -= n;
            q = (const float *)(g_park + (size_t)idx * 0x20u);
            if (in_cone(fr, q)) { g_cand_idx[ncand] = idx; g_cand_key[ncand] = dist2(fr, q); ++ncand; }
        }
        g_cursor = (start + k) % n;
        room = capacity - count;
        take = ncand;
        if (!fr->burst && g_release != 0 && take > g_release) {
            take = g_release; ++g_gu_extras_report.cone_budget_limited;
        }
        if (take > room) { take = room; ++g_gu_extras_report.cone_room_limited; }
        if (take > g_gu_extras_report.cone_max_released) g_gu_extras_report.cone_max_released = take;
        if (fr->burst) ++g_gu_extras_report.cone_burst_updates;
        if (take) {
            sort_pairs(g_cand_key, g_cand_idx, ncand);               /* nearest first */
            for (k = 0; k < take; ++k) {
                x_copy(queue + (size_t)count * 0x20u, g_park + (size_t)g_cand_idx[k] * 0x20u, 0x20u);
                ++count;
                g_cand_key[k] = -(float)g_cand_idx[k];               /* for removal: highest index first */
            }
            sort_pairs(g_cand_key, g_cand_idx, take);
            for (k = 0; k < take; ++k) park_remove_at(g_cand_idx[k]);
            g_gu_extras_report.cone_released_total += take;
            if (g_park_count) g_cursor %= g_park_count; else g_cursor = 0;
        }
    }
    g_gu_extras_report.cone_parked_now = g_park_count;
    if (g_park_count > g_gu_extras_report.cone_parked_max)
        g_gu_extras_report.cone_parked_max = g_park_count;
    return count;
}

/* the first g_carried entries are the ones the after hook put back: drop those
 * whose provider the game removed in the meantime */
static uint32_t carried_drop_removed(uint8_t *queue, uint32_t count, const uint8_t *removals, uint32_t rcount)
{
    uint32_t carried = g_carried < count ? g_carried : count, i, w, j;
    for (i = 0, w = 0; i < count; ++i) {
        uint8_t *e = queue + (size_t)i * 0x20u;
        if (i < carried) {
            uint32_t p = rd32(e + 0x1C);
            int gone = 0;
            for (j = 0; j < rcount && !gone; ++j) gone = rd32(removals + (size_t)j * 8u) == p;
            if (gone) { ++g_gu_extras_report.spawn_dropped_removed; continue; }
        }
        if (w != i) x_copy(queue + (size_t)w * 0x20u, e, 0x20u);
        ++w;
    }
    return w;
}

/* spawn limit: only the nearest / in-view props (plus everything within
 * spawn_near_always metres) are made now, the rest is kept for the next update */
static uint32_t spawn_step(uint8_t *queue, uint32_t count, const struct Frame *fr, int ok)
{
    uint32_t i, taken = 0, rest = 0;
    if (!ok || g_spawn_budget == 0 || count <= g_spawn_budget || count > X_SPAWN_MAX) return count;
    for (i = 0; i < count; ++i) {
        const float *q = (const float *)(queue + (size_t)i * 0x20u);
        float dx = q[0] - fr->pos[0], dy = q[1] - fr->pos[1], dz = q[2] - fr->pos[2];
        float d2 = dx * dx + dy * dy + dz * dz, w = 2.0f;
        if (d2 != d2) d2 = 0.0f;                       /* NaN: make it now */
        if (d2 > 1e-6f) {
            float c = (dx * fr->f[0] + dy * fr->f[1] + dz * fr->f[2]) / x_sqrtf(d2);
            if (c > 1.0f) c = 1.0f; else if (c < -1.0f) c = -1.0f;
            w = 2.0f - c;                              /* ahead 1x, to the side 2x, behind 3x the distance */
        }
        g_sp_d2[i] = d2; g_sp_key[i] = d2 * w * w; g_sp_idx[i] = i;
    }
    sort_pairs_with(g_sp_key, g_sp_idx, count, g_sp_tk, g_sp_ti);
    for (i = 0; i < count; ++i) {
        uint32_t src = g_sp_idx[i];
        const uint8_t *e = queue + (size_t)src * 0x20u;
        if (i < g_spawn_budget || g_sp_d2[src] < g_spawn_near2) {
            x_copy(g_scratch + (size_t)taken * 0x20u, e, 0x20u); ++taken;
        } else {
            x_copy(g_hold + (size_t)rest * 0x20u, e, 0x20u); ++rest;
        }
    }
    x_copy(queue, g_scratch, taken * 0x20u);
    g_hold_count = rest;
    ++g_gu_extras_report.spawn_limited_updates;
    g_gu_extras_report.spawn_held_total += rest;
    if (rest > g_gu_extras_report.spawn_hold_max) g_gu_extras_report.spawn_hold_max = rest;
    return taken;
}

static void gu_x_before_queue_impl(uint8_t *gen)
{
    uint8_t *queue; uint32_t count, capacity, rcount;
    const uint8_t *removals;
    struct Frame fr;
    int ok;
    int cone = g_cone_on && !g_cone_disabled, spawn = g_spawn_on && !g_spawn_disabled;
    if (!gen) return;
    queue = rdptr(gen + 0x5224);
    if (!queue) return;
    count = rd16(gen + 0x5228);
    capacity = rd32(gen + 0x5220);
    if (capacity == 0 || capacity > 0x10000u || count > capacity) return;
    ++g_gu_extras_report.cone_updates;
    if (count > g_gu_extras_report.cone_queue_max) g_gu_extras_report.cone_queue_max = count;

    removals = rdptr(gen + 0x522C);
    rcount = rd16(gen + 0x5230);
    if (cone && rcount && removals && g_park_count) park_drop_removed(removals, rcount);
    if (spawn) {
        g_hold_count = 0;                              /* the after hook did not run last time: nothing to keep */
        if (g_carried && rcount && removals) count = carried_drop_removed(queue, count, removals, rcount);
        g_carried = 0;
    }

    ok = make_frame(&fr);
    if (cone) count = cone_step(queue, count, capacity, &fr, ok);
    if (spawn) count = spawn_step(queue, count, &fr, ok);
    wr16(gen + 0x5228, count);
}

/* after the game has made its props: put the ones we kept back at the front of the queue */
static void gu_x_after_queue_impl(uint8_t *gen)
{
    uint8_t *queue; uint32_t capacity, n;
    if (!gen) return;
    if (g_hold_count == 0) { g_carried = 0; return; }
    queue = rdptr(gen + 0x5224);
    capacity = rd32(gen + 0x5220);
    if (!queue || capacity == 0 || capacity > 0x10000u) { g_hold_count = 0; g_carried = 0; return; }
    n = g_hold_count < capacity ? g_hold_count : capacity;
    x_copy(queue, g_hold, n * 0x20u);
    wr16(gen + 0x5228, n);
    g_carried = n;
    g_hold_count = 0;
}

static void gu_x_cone_fault(void)
{
    /* the waiting list is dropped: with the feature off nothing would make
     * those props, and the generator re-queues what it still wants */
    g_cone_disabled = 1;
    ++g_gu_extras_report.cone_faults;
    g_gu_extras_report.cone_disabled = 1;
    g_gu_extras_report.cone_dropped_on_reset += g_park_count;
    g_park_count = 0; g_cursor = 0; g_prev_valid = 0;
    g_gu_extras_report.cone_parked_now = 0;
}

static void gu_x_spawn_fault(void)
{
    /* what was kept for the next update is dropped: the generator re-queues what it still wants */
    g_spawn_disabled = 1;
    ++g_gu_extras_report.spawn_faults;
    g_gu_extras_report.spawn_disabled = 1;
    g_hold_count = 0; g_carried = 0;
}

static int queue_hooks_idle(void)
{
    return (!g_cone_on || g_cone_disabled) && (!g_spawn_on || g_spawn_disabled);
}

static void GU_X_CDECL gu_x_on_reset(uint8_t *gen)
{
    if (queue_hooks_idle()) return;
    GU_X_TRY { gu_x_on_reset_impl(gen); }
    GU_X_EXCEPT { if (g_cone_on) gu_x_cone_fault(); if (g_spawn_on) gu_x_spawn_fault(); }
}

static void GU_X_CDECL gu_x_before_queue(uint8_t *gen)
{
    if (queue_hooks_idle()) return;
    GU_X_TRY { gu_x_before_queue_impl(gen); }
    GU_X_EXCEPT { if (g_cone_on) gu_x_cone_fault(); if (g_spawn_on) gu_x_spawn_fault(); }
}

static void GU_X_CDECL gu_x_after_queue(uint8_t *gen)
{
    if (!g_spawn_on || g_spawn_disabled) return;
    GU_X_TRY { gu_x_after_queue_impl(gen); }
    GU_X_EXCEPT { gu_x_spawn_fault(); }
}

/* ------------------------------------------------------------------ */
/* feature 4: drawable references                                      */
/* ------------------------------------------------------------------ */
/* Every object with its model loaded holds one (20 bytes each, the game has
 * 13000). When they run out the game takes models from other objects, and with
 * many props trees, fences and cars flicker. The array is made once and never
 * freed by the game, so only its size constant and its allocation are changed;
 * the new array comes from our own memory. */
static uint32_t g_orig_new_drawable;

static void *GU_X_CDECL gu_x_drawable_alloc(uint32_t size)
{
    void *p = os_alloc_data(size);
    if (p) { ++g_gu_extras_report.drawable_allocs; return p; }
    ++g_gu_extras_report.drawable_alloc_fallbacks;
    return ((void *(GU_X_CDECL *)(uint32_t))(uintptr_t)g_orig_new_drawable)(size);
}

static uint32_t install_drawable(const struct GuExtrasConfig *cfg,
                                 const uint8_t *image_base, uint32_t image_size,
                                 int (*busy)(uint32_t, uint32_t, void *), void *ctx)
{
    struct Tx tx;
    const uint8_t *d = g_site[S_D1];
    uint32_t want = cfg->drawable_refs, old, init, call, st;
    uint8_t buf[4];
    if (want == 0) return GU_X_OFF;
    if (!d) return GU_X_ERR_SITE;
    if (d[0] != 0x68 || d[5] != 0xE8 || d[10] != 0xC3) return GU_X_ERR_SITE;
    old = rd32(d + 1);
    init = addr32(d + 5) + 5u + rd32(d + 6);               /* the function that makes the array */
    if (!in_image(image_base, image_size, init, 0x1Du + 5u)) return GU_X_ERR_SITE;
    if (*(const uint8_t *)(uintptr_t)(init + 0x1Cu) != 0x51 ||        /* push ecx (the size) */
        *(const uint8_t *)(uintptr_t)(init + 0x1Du) != 0xE8) return GU_X_ERR_SITE;   /* call operator new */
    if (old >= want) return GU_X_NOT_NEEDED;
    call = init + 0x1Du;
    g_orig_new_drawable = call + 5u + rd32((const uint8_t *)(uintptr_t)(call + 1u));
    tx.n = 0;
    wr32(buf, want);
    if (!tx_add(&tx, (uint8_t *)(uintptr_t)(addr32(d) + 1u), buf, 4)) return GU_X_ERR_SITE;
    if (cfg->drawable_own_memory) {   /* 0 = the array stays in the game's own heap (and is freed by it) */
        wr32(buf, addr32((const void *)(uintptr_t)&gu_x_drawable_alloc) - (call + 5u));
        if (!tx_add(&tx, (uint8_t *)(uintptr_t)(call + 1u), buf, 4)) return GU_X_ERR_SITE;
    }
    st = tx_commit(&tx, busy, ctx);
    if (st == GU_X_ON) g_gu_extras_report.drawable_refs_applied = want;
    return st;
}

/* ------------------------------------------------------------------ */
/* feature 5: render cache                                             */
/* ------------------------------------------------------------------ */
/* The entities a frame draws go into one buffer of 8-byte entries (room for
 * 9692). The sector scans stop taking entities once it holds 8192 (cmp ecx,
 * 0x10000 in two places) and whatever they meet afterwards is not drawn that
 * frame: looking at a whole district, cars, trees and the player vanish for
 * frames. We give the game a bigger buffer (its start comes from one small
 * "mov eax, imm32; ret" function) and move both limits to three quarters of it. */
static void *g_render_cache_buffer;

static uint32_t install_rendercache(const struct GuExtrasConfig *cfg,
                                    const uint8_t *image_base, uint32_t image_size,
                                    int (*busy)(uint32_t, uint32_t, void *), void *ctx)
{
    struct Tx tx;
    const uint8_t *s0 = g_site[S_RC], *s1 = g_site2[S_RC];
    uint32_t want = cfg->render_cache, d0, d1, limit, st;
    uint8_t *start, *buffer, buf[4];
    if (want == 0) return GU_X_OFF;
    if (!s0 || !s1) return GU_X_ERR_SITE;
    d0 = addr32(s0) + 5u + rd32(s0 + 1);
    d1 = addr32(s1) + 5u + rd32(s1 + 1);
    if (d0 != d1 || !in_image(image_base, image_size, d0, 6u)) return GU_X_ERR_SITE;
    start = (uint8_t *)(uintptr_t)d0;
    if (start[0] != 0xB8 || start[5] != 0xC3) return GU_X_ERR_SITE;   /* mov eax, imm32 ; ret */
    if (rd32(s0 + 18) != 0x10000u || rd32(s1 + 18) != 0x10000u) return GU_X_ERR_SITE;
    if (want < 9692u) return GU_X_NOT_NEEDED;
    buffer = (uint8_t *)os_alloc_data(want * 8u + 64u);
    if (!buffer) return GU_X_ERR_MEMORY;
    limit = (want / 4u) * 3u * 8u;
    tx.n = 0;
    wr32(buf, addr32(buffer));
    if (!tx_add(&tx, start + 1, buf, 4)) { os_free_data(buffer); return GU_X_ERR_SITE; }
    wr32(buf, limit);
    if (!tx_add(&tx, (uint8_t *)(uintptr_t)(addr32(s0) + 18u), buf, 4) ||
        !tx_add(&tx, (uint8_t *)(uintptr_t)(addr32(s1) + 18u), buf, 4)) { os_free_data(buffer); return GU_X_ERR_SITE; }
    st = tx_commit(&tx, busy, ctx);
    if (st == GU_X_ON) { g_render_cache_buffer = buffer; g_gu_extras_report.render_cache_applied = want; }
    else os_free_data(buffer);
    return st;
}

static uint32_t install_queue_hooks(const struct GuExtrasConfig *cfg,
                                    const uint8_t *image_base, uint32_t image_size,
                                    uint8_t *manager, int want_cone, int want_spawn,
                                    int (*busy)(uint32_t, uint32_t, void *), void *ctx)
{
    struct Tx tx;
    uint8_t *c1 = (uint8_t *)g_site[S_C1], *c2 = (uint8_t *)g_site[S_C2];
    uint8_t *c3 = (uint8_t *)g_site[S_C3], *c4 = (uint8_t *)g_site[S_C4];
    const uint8_t *cam = g_site[S_CAM];
    uint8_t *s1, *s2 = 0, *s3, *s4, jump[16];
    uint32_t cam_ptr, len, deg, st;
    static const uint8_t reg_esi[1] = { 6 };
    static const uint8_t reg_ebp[1] = { 5 };
    if (!want_cone && !want_spawn) return GU_X_OFF;
    if (!c1 || !c3 || !c4 || !cam || (want_spawn && !c2)) return GU_X_ERR_SITE;
    if (want_cone && !manager) return GU_X_ERR_CONFIG;
    cam_ptr = rd32(cam + 12);
    if ((cam_ptr & 3u) != 0 || !in_image(image_base, image_size, cam_ptr - 32u, 44u))
        return GU_X_ERR_CAMERA;
    if (want_cone && !g_park) {
        g_park = (uint8_t *)os_alloc_data(X_PARK_CAP * 0x20u);
        if (!g_park) return GU_X_ERR_MEMORY;
    }
    if (want_spawn && !g_hold) {
        uint32_t rec = X_SPAWN_MAX * 0x20u, vec = X_SPAWN_MAX * 4u;
        uint8_t *blk = (uint8_t *)os_alloc_data(2u * rec + 5u * vec);
        if (!blk) return GU_X_ERR_MEMORY;
        g_hold = blk; g_scratch = blk + rec;
        g_sp_key = (float *)(blk + 2u * rec);
        g_sp_d2 = (float *)(blk + 2u * rec + vec);
        g_sp_tk = (float *)(blk + 2u * rec + 2u * vec);
        g_sp_idx = (uint32_t *)(blk + 2u * rec + 3u * vec);
        g_sp_ti = (uint32_t *)(blk + 2u * rec + 4u * vec);
    }
    s1 = arena_take(); s3 = arena_take(); s4 = arena_take();
    if (want_spawn) s2 = arena_take();
    if (!s1 || !s3 || !s4 || (want_spawn && !s2)) return GU_X_ERR_MEMORY;
    len = build_stub(s1, addr32((const void *)(uintptr_t)&gu_x_before_queue),
                     reg_esi, 1, 0, 1, 0, 0, c1, 11, addr32(c1) + 11u); (void)len;
    len = build_stub(s3, addr32((const void *)(uintptr_t)&gu_x_on_reset),
                     reg_ebp, 1, 0, 1, 0, 0, c3, 6, addr32(c3) + 6u); (void)len;
    len = build_stub(s4, addr32((const void *)(uintptr_t)&gu_x_on_reset),
                     reg_esi, 1, 0, 1, 0, 0, c4, 6, addr32(c4) + 6u); (void)len;
    if (want_spawn) {
        len = build_stub(s2, addr32((const void *)(uintptr_t)&gu_x_after_queue),
                         reg_esi, 1, 0, 1, 0, 0, c2, 7, addr32(c2) + 7u); (void)len;
    }
    tx.n = 0;
    make_jump(jump, 11, addr32(c1), addr32(s1));
    if (!tx_add(&tx, c1, jump, 11)) return GU_X_ERR_SITE;
    make_jump(jump, 6, addr32(c3), addr32(s3));
    if (!tx_add(&tx, c3, jump, 6)) return GU_X_ERR_SITE;
    make_jump(jump, 6, addr32(c4), addr32(s4));
    if (!tx_add(&tx, c4, jump, 6)) return GU_X_ERR_SITE;
    if (want_spawn) {
        make_jump(jump, 7, addr32(c2), addr32(s2));
        if (!tx_add(&tx, c2, jump, 7)) return GU_X_ERR_SITE;
    }

    deg = cfg->cone_full_angle_deg;
    if (deg < 30u) deg = 30u;
    if (deg > 120u) deg = 120u;
    g_cos_full = x_cosf_small((float)deg * 0.017453292f);
    g_back = (float)cfg->cone_back_meters;
    g_release = cfg->cone_release_per_update;
    g_lead_ms = cfg->cone_lead_ms > 1000u ? 1000u : cfg->cone_lead_ms;
    g_manager = manager;
    g_cam = (const float *)(uintptr_t)cam_ptr;
    g_park_count = 0; g_cursor = 0; g_prev_valid = 0;
    g_hold_count = 0; g_carried = 0;
    g_spawn_budget = cfg->spawns_per_update;
    g_spawn_near2 = (float)cfg->spawn_near_always * (float)cfg->spawn_near_always;
    g_cone_disabled = 0; g_spawn_disabled = 0;
    g_gu_extras_report.cone_disabled = 0; g_gu_extras_report.spawn_disabled = 0;
    g_cone_on = want_cone; g_spawn_on = want_spawn;
    st = tx_commit(&tx, busy, ctx);
    if (st != GU_X_ON) { g_cam = 0; g_cone_on = 0; g_spawn_on = 0; }
    return st;
}

/* ------------------------------------------------------------------ */
/* entry points                                                        */
/* ------------------------------------------------------------------ */
static int install_core(const struct GuExtrasConfig *cfg,
                        const struct Range *ranges, uint32_t nranges,
                        const uint8_t *image_base, uint32_t image_size, uint8_t *manager,
                        int (*busy)(uint32_t, uint32_t, void *), void *ctx)
{
    uint32_t need = 0, i;
    int matrix_req, refl_req, cone_req, drawable_req, cache_req, spawn_req, installed = 0;
    struct GuExtrasReport *r = &g_gu_extras_report;
    for (i = 0; i < S_COUNT; ++i) { g_site[i] = 0; g_site2[i] = 0; r->site_name[i] = k_site_name[i]; }
    r->sites_known = S_COUNT;
    matrix_req = cfg->matrix_limit > X_MATRIX_VANILLA_LIMIT || cfg->matrix_pool != 0;
    refl_req = cfg->reflections_props == 0;
    cone_req = cfg->cone_back_meters != 0;
    drawable_req = cfg->drawable_refs != 0;
    cache_req = cfg->render_cache != 0;
    spawn_req = cfg->spawns_per_update != 0;
    if (matrix_req) need |= BIT(S_M1) | BIT(S_M2) | BIT(S_M3) | BIT(S_M4) | BIT(S_M5);
    if (refl_req) need |= BIT(S_R0) | BIT(S_RA) | BIT(S_RB);
    if (cone_req || spawn_req) need |= BIT(S_C1) | BIT(S_C3) | BIT(S_C4) | BIT(S_CAM);
    if (spawn_req) need |= BIT(S_C2);
    if (drawable_req) need |= BIT(S_D1);
    if (cache_req) need |= BIT(S_RC);
    if (!need) return 0;
    /* the pool pieces are only needed when the pool changes; resolve them all
     * and let each feature decide, but report what was found */
    (void)resolve(need, ranges, nranges, image_base);

    if (matrix_req) {
        r->matrix_status = install_matrix(cfg, busy, ctx);
        if (r->matrix_status == GU_X_ON) installed |= 1;
    }
    if (refl_req) {
        r->reflections_status = install_reflections(cfg, image_base, image_size, busy, ctx);
        if (r->reflections_status == GU_X_ON) installed |= 2;
    }
    if (cone_req || spawn_req) {
        uint32_t st = install_queue_hooks(cfg, image_base, image_size, manager, cone_req, spawn_req, busy, ctx);
        if (cone_req) { r->cone_status = st; if (st == GU_X_ON) installed |= 4; }
        if (spawn_req) { r->spawn_status = st; if (st == GU_X_ON) installed |= 32; }
    }
    if (drawable_req) {
        r->drawable_status = install_drawable(cfg, image_base, image_size, busy, ctx);
        if (r->drawable_status == GU_X_ON) installed |= 8;
    }
    if (cache_req) {
        r->render_cache_status = install_rendercache(cfg, image_base, image_size, busy, ctx);
        if (r->render_cache_status == GU_X_ON) installed |= 16;
    }
    return installed;
}

int gu_extras_install_range(const struct GuExtrasConfig *config,
                            const uint8_t *text, uint32_t text_size,
                            const uint8_t *image_base, uint32_t image_size,
                            uint8_t *manager,
                            int (*addr_busy)(uint32_t, uint32_t, void *), void *ctx)
{
    struct Range rg;
    if (!config) return 0;
    rg.p = text; rg.n = text_size;
    return install_core(config, &rg, 1, image_base, image_size, manager, addr_busy, ctx);
}

#ifndef GU_X_TEST
int gu_extras_install(const struct GuExtrasConfig *config,
                      const uint8_t *image_base, uint32_t image_size,
                      uint8_t *manager,
                      int (*addr_busy)(uint32_t, uint32_t, void *), void *ctx)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)image_base;
    const IMAGE_NT_HEADERS *nt;
    const IMAGE_SECTION_HEADER *sec;
    struct Range ranges[4];
    uint32_t n = 0, i;
    if (!config || !image_base || image_size < sizeof(IMAGE_DOS_HEADER)) return 0;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    if ((uint32_t)dos->e_lfanew > image_size - sizeof(IMAGE_NT_HEADERS)) return 0;
    nt = (const IMAGE_NT_HEADERS *)(image_base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections && n < 4; ++i) {
        uint32_t va = sec[i].VirtualAddress, vs = sec[i].Misc.VirtualSize;
        if (!(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        if (vs == 0 || va > image_size || vs > image_size - va) continue;
        ranges[n].p = image_base + va; ranges[n].n = vs; ++n;
    }
    if (n == 0) return 0;
    return install_core(config, ranges, n, image_base, image_size, manager, addr_busy, ctx);
}
#endif
