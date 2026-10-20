/* 32-bit freestanding test: builds a fake "game" image containing the real
 * byte patterns, installs the hooks over it and executes them. */
#define GU_X_TEST 1
#include "../gu_extras.c"

void *gu_xt_jb[5];
volatile int gu_xt_guard;

/* ---- minimal runtime ------------------------------------------------ */
static int sys_write(int fd, const void *b, uint32_t n)
{ int r; __asm__ volatile("int $0x80" : "=a"(r) : "a"(4), "b"(fd), "c"(b), "d"(n) : "memory"); return r; }
static void sys_exit(int c)
{ __asm__ volatile("int $0x80" :: "a"(1), "b"(c)); for (;;) {} }
void *memcpy(void *d, const void *s, size_t n) { uint8_t *a = d; const uint8_t *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n)
{ uint8_t *a = d; const uint8_t *b = s; if (a < b) while (n--) *a++ = *b++; else { a += n; b += n; while (n--) *--a = *--b; } return d; }
void *memset(void *d, int c, size_t n) { uint8_t *a = d; while (n--) *a++ = (uint8_t)c; return d; }
static uint32_t slen(const char *s) { uint32_t n = 0; while (s[n]) ++n; return n; }
static void out(const char *s) { sys_write(1, s, slen(s)); }
static void outu(uint32_t v)
{ char b[12]; int i = 11; b[i] = 0; if (!v) b[--i] = '0'; while (v) { b[--i] = (char)('0' + v % 10); v /= 10; } out(b + i); }

static void *mmap_rwx(uint32_t n)
{
    uint32_t args[6]; int r;
    args[0] = 0; args[1] = n; args[2] = 7; args[3] = 0x22; args[4] = (uint32_t)-1; args[5] = 0;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(90), "b"(args) : "memory");
    return (r < 0 && r > -4096) ? 0 : (void *)(uintptr_t)r;
}

/* ---- platform shim -------------------------------------------------- */
static uint32_t g_fake_us;
void *gu_xt_alloc(uint32_t n, int exec) { (void)exec; return mmap_rwx((n + 4095u) & ~4095u); }
void gu_xt_free(void *p) { (void)p; }
int gu_xt_protect(void *p, uint32_t n, uint32_t *old) { (void)p; (void)n; *old = 0; return 1; }
void gu_xt_restore(void *p, uint32_t n, uint32_t old) { (void)p; (void)n; (void)old; }
void gu_xt_flush(void *p, uint32_t n) { (void)p; (void)n; }
uint32_t gu_xt_now_us(void) { return g_fake_us; }

struct KSigAction { void (*handler)(int); uint32_t flags; void (*restorer)(void); uint32_t mask[2]; };
static void segv_handler(int sig) { (void)sig; if (gu_xt_guard) __builtin_longjmp(gu_xt_jb, 1); sys_exit(99); }
static void install_segv(void)
{
    struct KSigAction a; uint32_t i; int r;
    for (i = 0; i < sizeof a / 4; ++i) ((uint32_t *)&a)[i] = 0;
    a.handler = segv_handler; a.flags = 0x40000000u;       /* SA_NODEFER: usable again after a longjmp */
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(174), "b"(11), "c"(&a), "d"(0), "S"(8) : "memory");
    if (r != 0) { out("sigaction failed\n"); sys_exit(3); }
}

static int g_fail, g_checks;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_fail; if (g_fail <= 12) { out("FAIL line "); outu(__LINE__); out(": " #c "\n"); } } } while (0)
#define SCHECK(c) CHECK(c)   /* the counters are always on in this module */

/* ---- fake image ----------------------------------------------------- */
static uint8_t *img;
static uint32_t cur;
#define IMG_SIZE 0x10000u
#define D_REFL 0x8000u
#define D_SCENE 0x8004u
#define D_CAM 0x8100u          /* 4 rows of 4 floats; position row at +0x30 */
#define D_LAST 0x8200u
#define D_RBZF 0x8204u
#define D_RBECX 0x8208u
#define D_OLDDATA 0x8300u
#define D_DRRES 0x8210u
#define D_RCEND 0x8214u
#define D_RCBUF 0x8400u
static uint32_t A(uint32_t off) { return (uint32_t)(uintptr_t)(img + off); }
static void Eb(uint8_t b) { img[cur++] = b; }
static void E(const char *hex)
{
    while (*hex) {
        if (*hex == ' ') { ++hex; continue; }
        Eb((uint8_t)(hexv(hex[0]) * 16 + hexv(hex[1]))); hex += 2;
    }
}
static void E32(uint32_t v) { Eb((uint8_t)v); Eb((uint8_t)(v >> 8)); Eb((uint8_t)(v >> 16)); Eb((uint8_t)(v >> 24)); }
static void Ecall(uint32_t target) { Eb(0xE8); E32(target - (A(cur) + 4u)); }
static void align16(void) { while (cur & 15u) Eb(0xCC); }

static uint32_t new_calls, delete_calls, last_deleted;
static void *GU_X_CDECL fake_new(uint32_t size) { ++new_calls; (void)size; return (void *)0x1234; }
static void GU_X_CDECL fake_delete(void *p) { ++delete_calls; last_deleted = (uint32_t)(uintptr_t)p; }

static uint32_t off_update, off_reset, off_dtor, off_cam, off_r0, off_la, off_lb;
static uint32_t off_m1, off_m2, off_m3, off_m4, off_m5, off_callee;
static uint32_t off_dinit, off_ddrv, off_rcstart, off_rcs[4], off_after;
static int g_scan_copies = 2;

static int g_limit_patched;
static void build_image(int duplicate_m3)
{
    uint32_t i;
    for (i = 0; i < IMG_SIZE; ++i) img[i] = 0xCC;
    cur = 0;
    /* callee: records its argument */
    off_callee = cur; E("8B 44 24 04 A3"); E32(A(D_LAST)); E("C3"); align16();
    /* update function: the "before queue" window */
    off_update = cur;
    E("56 53 55 57 8B 74 24 14 BB 11 11 11 11 BD 22 22 22 22 B9 33 33 33 33 BA 44 44 44 44");
    E("33 C0 33 FF 66 3B 86 28 52 00 00");
    E("0F 92 C0 0F B6 C0 89 86 00 60 00 00 0F B7 86 28 52 00 00 89 86 04 60 00 00");
    E("89 BE 08 60 00 00 89 9E 0C 60 00 00 89 AE 10 60 00 00 89 8E 14 60 00 00 89 96 18 60 00 00");
    E("5F 5D 5B 5E C3"); align16();     /* the fake game function keeps edi, as a cdecl callee must */
    off_reset = cur;
    E("55 8B 6C 24 08 FF B5 24 52 00 00"); Ecall(A(off_callee)); E("83 C4 04 5D C3"); align16();
    off_dtor = cur;
    E("56 8B 74 24 08 FF B6 24 52 00 00"); Ecall(A(off_callee)); E("83 C4 04 5E C3"); align16();
    off_cam = cur;
    E("F3 0F 10 05"); E32(A(D_CAM + 0x30)); E("F3 0F 10 15"); E32(A(D_CAM + 0x30));
    E("F3 0F 10 0D"); E32(A(D_CAM + 0x30)); E("56 8B 75 0C C3"); align16();
    off_r0 = cur;
    E("85 0D"); E32(A(D_REFL)); E("75 05 0F B6 46 63 3D EF 00 00 00 C3"); align16();
    off_la = cur;
    E("56 53 57 8B 74 24 10 8B 4C 24 14 8B 5C 24 18 8B 7C 24 1C");
    E("F7 43 40 00 01 00 00 F3 0F 10 47 04");
    E("0F 94 C0 0F B6 D0 89 C8 89 53 44 5F 5B 5E C3"); align16();
    off_lb = cur;
    E("56 8B 74 24 08 8B 4C 24 0C 8B 44 24 10 83 EC 30 BA 01 00 00 00");
    E("89 4C 24 28 A9 00 00 00 08 74 06 85 15"); E32(A(D_SCENE));
    E("0F 94 C2 0F B6 D2 8B 44 24 28 89 15"); E32(A(D_RBZF)); E("89 0D"); E32(A(D_RBECX));
    E("83 C4 30 5E C3"); align16();
    off_m1 = cur; E("81 3D"); E32(A(D_OLDDATA)); E(g_limit_patched ? "00 20 00 00 7C 05 57 B9 00 00 00 00 C3" : "00 02 00 00 7C 05 57 B9 00 00 00 00 C3"); align16();
    off_m2 = cur; E(g_limit_patched ? "81 F9 00 20 00 00 0F 8D 00 00 00 00 8B 4F 08 E8 00 00 00 00 C3" : "81 F9 00 02 00 00 0F 8D 00 00 00 00 8B 4F 08 E8 00 00 00 00 C3"); align16();
    for (i = 0; i < (uint32_t)(duplicate_m3 ? 2 : 1); ++i) {
        off_m3 = cur; E("68 58 1B 00 00 B9 00 00 00 00 C7 05 00 00 00 00 00 00 00 00 C3"); align16();
    }
    off_m4 = cur;
    E("53 8B 5C 24 08 8B 4C 24 0C 51"); Ecall((uint32_t)(uintptr_t)&fake_new);
    E("89 83 E0 01 00 00 83 C4 04 5B C3"); align16();
    off_m5 = cur;
    E("56 8B 74 24 08 FF B6 E0 01 00 00"); Ecall((uint32_t)(uintptr_t)&fake_delete);
    E("83 C4 04 5E C3"); align16();
    /* drawable references: init(count) {... push ecx; call new; ...; ret 4} and the driver that pushes the count */
    off_dinit = cur;
    E("8B 4C 24 04"); while (cur - off_dinit < 0x1Cu) Eb(0x90);   /* the push is at +0x1C */
    E("51"); Ecall((uint32_t)(uintptr_t)&fake_new);
    E("83 C4 04 A3"); E32(A(D_DRRES)); E("C2 04 00"); align16();
    off_ddrv = cur; E("68"); E32(13000); Ecall(A(off_dinit)); E("C3"); align16();
    /* the end of the generator update: mov [esi+5230h],ax ; mov ecx,esi ; pop esi ; jmp tail */
    off_after = cur;
    E("56 8B 74 24 08 31 C0"); E("66 89 86 30 52 00 00 8B CE 5E E9"); E32(0); E("C3"); align16();
    /* render cache: the start-of-buffer function and the sector scans that compare against 0x10000 */
    off_rcstart = cur; E("B8"); E32(A(D_RCBUF)); E("C3"); align16();
    for (i = 0; i < (uint32_t)g_scan_copies && i < 4; ++i) {
        off_rcs[i] = cur; Ecall(A(off_rcstart)); E("8B 0D"); E32(A(D_RCEND));
        E("2B C8 83 E1 F8 81 F9 00 00 01 00 0F 93 C0 0F B6 C0 C3"); align16();
    }
}

typedef uint32_t (*fn4)(uint32_t, uint32_t, uint32_t, uint32_t);
#define CALL4(off, a, b, c, d) (((fn4)(uintptr_t)A(off))((uint32_t)(a), (uint32_t)(b), (uint32_t)(c), (uint32_t)(d)))

static uint32_t g_busy_lo, g_busy_hi;
static int busy_always_site;
static int busy_cb(uint32_t start, uint32_t len, void *ctx)
{ (void)ctx; if (g_busy_hi && start < g_busy_hi && start + len > g_busy_lo) return 1; return busy_always_site && start <= A(off_la) + 19u + 6u && A(off_la) + 19u < start + len; }
static uint32_t g_prop_entity;
static int is_prop_cb(uint32_t e) { return e == g_prop_entity; }

static void reset_report(void) { memset(&g_gu_extras_report, 0, sizeof g_gu_extras_report); }


/* ---- XMM preservation helper ---------------------------------------- */
/* loads xmm0..7 with known patterns, calls the (patched) function at addr
 * with one cdecl argument, stores xmm0..7 afterwards. */
static uint8_t g_xmm_in[144], g_xmm_out[144];
static void call_with_xmm(uint32_t addr, uint32_t arg)
{
    uint8_t *in = (uint8_t *)(((uintptr_t)g_xmm_in + 15u) & ~(uintptr_t)15u);
    uint8_t *ou = (uint8_t *)(((uintptr_t)g_xmm_out + 15u) & ~(uintptr_t)15u);
    uint32_t i;
    for (i = 0; i < 128; ++i) { in[i] = (uint8_t)(0xA0u + i * 7u); ou[i] = 0; }
    __asm__ volatile(
        "movups 0(%[in]), %%xmm0\n movups 16(%[in]), %%xmm1\n movups 32(%[in]), %%xmm2\n"
        "movups 48(%[in]), %%xmm3\n movups 64(%[in]), %%xmm4\n movups 80(%[in]), %%xmm5\n"
        "movups 96(%[in]), %%xmm6\n movups 112(%[in]), %%xmm7\n"
        "pushl %[arg]\n call *%[fn]\n addl $4, %%esp\n"
        "movups %%xmm0, 0(%[ou])\n movups %%xmm1, 16(%[ou])\n movups %%xmm2, 32(%[ou])\n"
        "movups %%xmm3, 48(%[ou])\n movups %%xmm4, 64(%[ou])\n movups %%xmm5, 80(%[ou])\n"
        "movups %%xmm6, 96(%[ou])\n movups %%xmm7, 112(%[ou])\n"
        :
        : [in] "r"(in), [ou] "r"(ou), [fn] "r"(addr), [arg] "r"(arg)
        : "eax", "ecx", "edx", "memory", "cc",
          "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");
    for (i = 0; i < 128; ++i) CHECK(in[i] == ou[i]);
}

/* ---- generator / entity helpers ------------------------------------- */
static uint8_t *new_gen(uint32_t capacity)
{
    uint8_t *g = (uint8_t *)mmap_rwx(0x8000), *q = (uint8_t *)mmap_rwx(0x40000), *rem = (uint8_t *)mmap_rwx(0x1000);
    wr32(g + 0x5220, capacity); wr32(g + 0x5224, addr32(q)); wr32(g + 0x522C, addr32(rem));
    wr16(g + 0x5228, 0); wr16(g + 0x5230, 0);
    return g;
}
static void put(uint8_t *gen, uint32_t i, float x, float y, float z, uint32_t provider, uint32_t tag)
{
    uint8_t *e = rdptr(gen + 0x5224) + (size_t)i * 0x20u;
    float p[3]; uint32_t k;
    p[0] = x; p[1] = y; p[2] = z;
    for (k = 0; k < 0x20; ++k) e[k] = 0;
    x_copy(e, p, 12); wr32(e + 0x18, tag); wr32(e + 0x1C, provider);
}
static uint32_t tag_at(uint8_t *gen, uint32_t i) { return rd32(rdptr(gen + 0x5224) + (size_t)i * 0x20u + 0x18); }
static float *camf(void) { return (float *)(uintptr_t)A(D_CAM); }
static void set_camera(float px, float py, float pz, float fx, float fy, float fz)
{
    float *c = camf();
    uint32_t i;
    for (i = 0; i < 16; ++i) c[i] = 0.0f;
    c[0] = -fy; c[1] = fx;                 /* right row (unused) */
    c[4] = fx; c[5] = fy; c[6] = fz;       /* forward row, 0x10 into the matrix */
    c[12] = px; c[13] = py; c[14] = pz;    /* position row */
}

static uint8_t *g_mgr;
static void set_gather(float r) { union { float f; uint32_t u; } v; v.f = r; wr32(g_mgr + 0x1C, v.u); }

struct Dir { float x, y; };
static struct Dir az(float deg, float radius)
{   /* azimuth from +x toward +y using a small table-free sin/cos */
    struct Dir d; float r = deg * 0.017453292f;
    float c = x_cosf_small(r > 2.2f ? 3.14159265f - r : r);
    float s = x_cosf_small(1.57079633f - r > 0 ? (1.57079633f - r) : (r - 1.57079633f));
    if (r > 2.2f) c = -c;
    d.x = c * radius; d.y = s * radius;
    return d;
}

/* ====================================================================== */
int main(void)
{
    install_segv();
    struct GuExtrasConfig cfg;
    uint8_t *gen; uint32_t i, installed;
    { uint32_t zi; for (zi = 0; zi < sizeof cfg / 4; ++zi) ((uint32_t *)&cfg)[zi] = 0; }
    uint8_t before_img[IMG_SIZE];

    img = (uint8_t *)mmap_rwx(IMG_SIZE);
    g_mgr = (uint8_t *)mmap_rwx(0x1000);
    if (!img || !g_mgr) { out("mmap failed\n"); sys_exit(2); }

    /* ---------- 1. everything requested, all sites present ---------- */
    g_limit_patched = 0;
    build_image(0);
    reset_report();
    wr32(img + D_REFL, 0x6); wr32(img + D_SCENE, 0x4);
    cfg.matrix_limit = 4096; cfg.matrix_pool = 0; cfg.reflections_props = 0;
    cfg.cone_back_meters = 70; cfg.cone_full_angle_deg = 75; cfg.cone_release_per_update = 2; cfg.cone_lead_ms = 300;
    cfg.is_prop_entity = is_prop_cb;
    set_camera(0, 0, 0, 1, 0, 0); set_gather(200.0f);
    installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
    CHECK(installed == 7);
    CHECK(g_gu_extras_report.matrix_status == GU_X_ON && g_gu_extras_report.reflections_status == GU_X_ON &&
          g_gu_extras_report.cone_status == GU_X_ON);
    for (i = 0; i < S_D1; ++i) CHECK(g_gu_extras_report.site_matches[i] == 1);
    CHECK(g_gu_extras_report.matrix_limit_applied == 4096 && g_gu_extras_report.matrix_pool_applied == 28000);

    /* matrix: immediates and call targets */
    CHECK(rd32(img + off_m1 + 6) == 4096);
    CHECK(rd32(img + off_m2 + 2) == 4096);
    CHECK(rd32(img + off_m3 + 1) == 28000);
    CHECK(addr32(img + off_m4 + 9 + 1) + 5u + rd32(img + off_m4 + 9 + 2) == addr32((const void *)(uintptr_t)&gu_x_matrix_alloc));
    CHECK(addr32(img + off_m5 + 5 + 6) + 5u + rd32(img + off_m5 + 5 + 7) == addr32((const void *)(uintptr_t)&gu_x_matrix_free));
    {   /* run the patched alloc and free sites */
        uint8_t *holder = (uint8_t *)mmap_rwx(0x1000);
        uint32_t p, n0 = new_calls, d0 = delete_calls;
        CALL4(off_m4, holder, 28000u * 80u, 0, 0);
        p = rd32(holder + 0x1E0);
        CHECK(p != 0 && p != 0x1234 && new_calls == n0);        /* ours, not the game's new */
        CHECK(g_gu_extras_report.matrix_pool_allocs == 1);
        CALL4(off_m5, holder, 0, 0, 0);
        CHECK(delete_calls == d0 && g_gu_extras_report.matrix_pool_frees_ours == 1);   /* ours freed, not forwarded */
        wr32(holder + 0x1E0, 0xABCD0);                            /* a pointer we do not own */
        CALL4(off_m5, holder, 0, 0, 0);
        CHECK(delete_calls == d0 + 1 && last_deleted == 0xABCD0); /* forwarded to the game's delete */
        CHECK(g_gu_extras_report.matrix_pool_frees_forwarded == 1);
    }

    /* reflections: props lose the reflection phase, others keep it */
    {
        uint8_t *struc = (uint8_t *)mmap_rwx(0x1000), *ent = (uint8_t *)mmap_rwx(0x1000), *other = (uint8_t *)mmap_rwx(0x1000);
        float *fl = (float *)mmap_rwx(0x1000);
        uint32_t r;
        g_prop_entity = addr32(ent);
        wr32(ent + 0x24, 0x40010000u); wr32(other + 0x24, 0x40010000u);
        wr32(struc + 0x40, 0x100);       /* test [ebx+40h],100h -> nonzero -> ZF=0 */
        r = CALL4(off_la, ent, 0x7, struc, fl);
        CHECK(r == 0x5);                                  /* 0b111 -> 0b101: reflection bit removed */
        CHECK(rd32(struc + 0x44) == 0);                   /* ZF came from the original test */
        wr32(struc + 0x40, 0);
        r = CALL4(off_la, ent, 0x7, struc, fl);
        CHECK(r == 0x5 && rd32(struc + 0x44) == 1);       /* ZF=1 this time */
        r = CALL4(off_la, other, 0x7, struc, fl);
        CHECK(r == 0x7);                                  /* not an owned prop: untouched */
        wr32(ent + 0x24, 0x00010000u);
        r = CALL4(off_la, ent, 0x7, struc, fl);
        CHECK(r == 0x7);                                  /* flags do not match: untouched */
        wr32(ent + 0x24, 0x40010000u);
        r = CALL4(off_la, ent, 0x1, struc, fl);
        CHECK(r == 0x1);                                  /* no reflection phase asked */
        r = CALL4(off_lb, ent, 0x7, 0, 0);                /* eax=0 -> test ZF=1, je taken */
        CHECK(r == 0x5 && rd32(img + D_RBECX) == 0x5 && rd32(img + D_RBZF) == 1);
        r = CALL4(off_lb, ent, 0x7, 0x08000000u, 0);      /* ZF=0, je not taken, test [scene],edx runs */
        CHECK(r == 0x5 && rd32(img + D_RBECX) == 0x5);
        wr32(img + D_SCENE, 0x1);
        r = CALL4(off_lb, other, 0x7, 0x08000000u, 0);
        CHECK(r == 0x7 && rd32(img + D_RBZF) == 0);
        wr32(img + D_SCENE, 0x4);
        CHECK(g_gu_extras_report.reflections_skipped == 4);
        CHECK(g_gu_extras_report.reflections_candidates == 7 && g_gu_extras_report.reflections_flagged == 6);
    }

    /* queue hook: registers, flags and the original instructions survive */
    gen = new_gen(8192);
    put(gen, 0, 20, 0, 0, 1, 100); put(gen, 1, 30, 0, 0, 1, 101);
    wr16(gen + 0x5228, 2);
    CALL4(off_update, gen, 0, 0, 0);
    CHECK(rd32(gen + 0x6000) == 1);                 /* CF from cmp ax,[count] (0 < 2) */
    CHECK(rd32(gen + 0x6008) == 0);                 /* xor edi,edi executed */
    CHECK(rd32(gen + 0x600C) == 0x11111111 && rd32(gen + 0x6010) == 0x22222222);
    CHECK(rd32(gen + 0x6014) == 0x33333333 && rd32(gen + 0x6018) == 0x44444444);
    CHECK(rd32(gen + 0x6004) == 2);                 /* both inside the cone: unchanged */
    CHECK(g_gu_extras_report.cone_updates == 1);

    /* reset / destructor hooks: clear the waiting ones, original push+call intact */
    g_park_count = 5;
    wr32(gen + 0x5224, rd32(gen + 0x5224));
    CALL4(off_reset, gen, 0, 0, 0);
    CHECK(g_park_count == 0 && rd32(img + D_LAST) == rd32(gen + 0x5224));
    g_park_count = 3; wr32(img + D_LAST, 0);
    CALL4(off_dtor, gen, 0, 0, 0);
    CHECK(g_park_count == 0 && rd32(img + D_LAST) == rd32(gen + 0x5224));
    CHECK(g_gu_extras_report.cone_resets == 2 && g_gu_extras_report.cone_dropped_on_reset == 8);

    /* ---------- 2. cone behaviour (direct calls) ---------- */
    {
        uint8_t *g = new_gen(8192);
        uint32_t made, tags_seen[64], nseen = 0, u, k;
        struct Dir d;
        g_back = 70.0f; g_release = 2; g_lead_ms = 0;
        g_gu_extras_report.cone_parked_total = 0;
        gu_x_on_reset(g);
        set_camera(0, 0, 0, 1, 0, 0); set_gather(200.0f); g_fake_us = 1000000;
        /* a ring at 150 m: ahead and to the sides stay, behind waits; 40 m all stay */
        {
            float degs[8] = { 0, 45, 70, 90, 120, 150, 180, 270 };
            for (i = 0; i < 8; ++i) { d = az(degs[i], 150.0f); put(g, i, d.x, d.y, 0, 1, 200 + i); }
            for (i = 0; i < 4; ++i) { d = az((float)(i * 90 + 100), 40.0f); put(g, 8 + i, d.x, d.y, 0, 1, 300 + i); }
        }
        wr16(g + 0x5228, 12);
        gu_x_before_queue(g);
        made = rd16(g + 0x5228);
        CHECK(made >= 8);                          /* ahead 0,45,70,90 + the four 40 m ones */
        CHECK(g_park_count == 12 - made && g_park_count >= 2);
        {   /* the 40 m entries are always made */
            uint32_t found = 0;
            for (i = 0; i < made; ++i) if (tag_at(g, i) >= 300 && tag_at(g, i) < 304) ++found;
            CHECK(found == 4);
            for (i = 0; i < made; ++i) { uint32_t t = tag_at(g, i); if (t < 64 + 300) tags_seen[nseen < 64 ? nseen++ : 63] = t; }
        }
        CHECK(made == 8 + 0 || made >= 8);
        {   /* 180 deg and 270(=-90 -> side) : 180 must be parked */
            uint32_t parked180 = 0;
            for (i = 0; i < g_park_count; ++i) if (rd32(g_park + (size_t)i * 0x20u + 0x18) == 206) parked180 = 1;
            CHECK(parked180);
        }
        /* turn around: the waiting ones come back, two per update, nearest first, none twice */
        set_camera(0, 0, 0, -1, 0, 0);
        g_fake_us += 16000;
        wr16(g + 0x5228, 0);
        {
            uint32_t total_back = 0, before = g_park_count;
            for (u = 0; u < 20 && g_park_count; ++u) {
                g_fake_us += 16000;
                wr16(g + 0x5228, 0);
                gu_x_before_queue(g);
                k = rd16(g + 0x5228);
                if (g_gu_extras_report.cone_burst_updates == 0) CHECK(k <= 2);
                total_back += k;
            }
            CHECK(g_park_count == 0 && total_back == before);
        }
        /* a gradual turn is not a cut: the per-update budget applies */
        gu_x_on_reset(g); g_release = 2; g_lead_ms = 0;
        set_camera(0, 0, 0, 1, 0, 0); g_fake_us += 16000; wr16(g + 0x5228, 0); gu_x_before_queue(g);
        for (i = 0; i < 12; ++i) { d = az(170.0f, 150.0f); put(g, i, d.x, d.y + (float)i * 0.5f, 0, 1, 900 + i); }
        wr16(g + 0x5228, 12); g_fake_us += 16000; gu_x_before_queue(g);
        CHECK(g_park_count == 12 && rd16(g + 0x5228) == 0);
        {
            uint32_t b0 = g_gu_extras_report.cone_budget_limited, burst0 = g_gu_extras_report.cone_burst_updates;
            uint32_t total = 0, step, over = 0;
            for (step = 1; step <= 26; ++step) {
                struct Dir f = az((float)step * 7.0f, 1.0f);
                set_camera(0, 0, 0, f.x, f.y, 0); g_fake_us += 16000; wr16(g + 0x5228, 0);
                gu_x_before_queue(g);
                k = rd16(g + 0x5228); total += k; if (k > 2) ++over;
            }
            CHECK(total == 12 && g_park_count == 0 && over == 0);
            CHECK(g_gu_extras_report.cone_burst_updates == burst0);
            CHECK(g_gu_extras_report.cone_budget_limited > b0 && g_gu_extras_report.cone_max_released >= 2);
        }
        /* removed providers are dropped from the waiting list */
        gu_x_on_reset(g);
        set_camera(0, 0, 0, 1, 0, 0); g_fake_us += 16000;
        for (i = 0; i < 6; ++i) { d = az(180.0f, 150.0f); put(g, i, d.x + (float)i, d.y, 0, (i & 1) ? 7 : 9, 400 + i); }
        wr16(g + 0x5228, 6); gu_x_before_queue(g);
        CHECK(rd16(g + 0x5228) == 0 && g_park_count == 6);
        wr32(rdptr(g + 0x522C), 7); wr16(g + 0x5230, 1);
        wr16(g + 0x5228, 0); g_fake_us += 16000;
        gu_x_before_queue(g);
        CHECK(g_park_count == 3);
        wr16(g + 0x5230, 0);
        /* the waiting area full: new far ones are made instead of lost */
        gu_x_on_reset(g);
        { uint32_t q; for (q = 0; q < X_PARK_CAP; ++q) { float far_[3]; far_[0] = -150.0f; far_[1] = 0.0f; far_[2] = 0.0f;
            x_copy(g_park + (size_t)q * 0x20u, far_, 12); wr32(g_park + (size_t)q * 0x20u + 0x1C, 1); wr32(g_park + (size_t)q * 0x20u + 0x18, 9); }
          g_park_count = X_PARK_CAP; }
        d = az(180.0f, 150.0f); put(g, 0, d.x, d.y, 0, 1, 500); wr16(g + 0x5228, 1);
        g_fake_us += 16000; gu_x_before_queue(g);
        CHECK(rd16(g + 0x5228) == 1 && g_gu_extras_report.cone_overflow_made >= 1);
        gu_x_on_reset(g);
        /* no camera: nothing waits any more, everything is made */
        set_camera(0, 0, 0, 0, 0, 0);
        d = az(180.0f, 150.0f); put(g, 0, d.x, d.y, 0, 1, 600); wr16(g + 0x5228, 1);
        g_fake_us += 16000; gu_x_before_queue(g);
        CHECK(rd16(g + 0x5228) == 1 && g_park_count == 0);
        set_camera(0, 0, 0, 1, 0, 0);
        /* turn prediction widens the cone toward the turn */
        gu_x_on_reset(g); g_lead_ms = 300;
        {
            struct Dir fnow = az(0.0f, 1.0f), fprev = az(-3.0f, 1.0f);
            /* previous frame looked 3 deg to the right of the x axis */
            set_camera(0, 0, 0, fprev.x, fprev.y, 0); g_fake_us += 16000; wr16(g + 0x5228, 0); gu_x_before_queue(g);
            set_camera(0, 0, 0, fnow.x, fnow.y, 0); g_fake_us += 16000;
            d = az(100.0f, 180.0f); put(g, 0, d.x, d.y, 0, 1, 700); wr16(g + 0x5228, 1);
            gu_x_before_queue(g);
            CHECK(rd16(g + 0x5228) == 1);          /* predicted into the cone */
            gu_x_on_reset(g); g_lead_ms = 0;
            set_camera(0, 0, 0, fprev.x, fprev.y, 0); g_fake_us += 16000; wr16(g + 0x5228, 0); gu_x_before_queue(g);
            set_camera(0, 0, 0, fnow.x, fnow.y, 0); g_fake_us += 16000;
            put(g, 0, d.x, d.y, 0, 1, 701); wr16(g + 0x5228, 1);
            gu_x_before_queue(g);
            CHECK(rd16(g + 0x5228) == 0 && g_park_count == 1);   /* same entry, no prediction: waits */
            g_lead_ms = 300;
        }
        /* a cut (camera turns 120 deg at once): everything that qualifies comes back in one update */
        gu_x_on_reset(g); set_camera(0, 0, 0, 1, 0, 0); g_fake_us += 16000; wr16(g + 0x5228, 0); gu_x_before_queue(g);
        for (i = 0; i < 10; ++i) { d = az(180.0f, 150.0f); put(g, i, d.x + (float)i, d.y, 0, 1, 800 + i); }
        wr16(g + 0x5228, 10); g_fake_us += 16000; gu_x_before_queue(g);
        CHECK(g_park_count == 10);
        set_camera(0, 0, 0, -1, 0, 0); g_fake_us += 16000; wr16(g + 0x5228, 0);
        gu_x_before_queue(g);
        CHECK(rd16(g + 0x5228) == 10 && g_park_count == 0);
    }

    /* ---------- 3. randomized conservation test ---------- */
    {
        uint8_t *g = new_gen(8192);
        static uint8_t seen[80000];
        uint32_t rng = 12345, tag = 1000, u, j, in_queue_total = 0, dropped_expected = 0, dropped_before;
        float yaw = 0.0f;
        gu_x_on_reset(g); g_release = 7; g_lead_ms = 200;
        g_gu_extras_report.cone_dropped_removed = 0;
        for (u = 0; u < 3000; ++u) {
            uint32_t n, rem_provider = 0, rn;
            rng = rng * 1664525u + 1013904223u; yaw += ((float)((rng >> 16) & 255u) - 127.5f) * 0.004f;
            { float c = x_cosf_small(yaw > 0 ? (yaw > 3.14159f ? 6.28318f - yaw : yaw) : (yaw < -3.14159f ? 6.28318f + yaw : -yaw));
              (void)c; }
            if (yaw > 6.28318f) yaw -= 6.28318f; if (yaw < 0) yaw += 6.28318f;
            {   /* forward from yaw using the same series, quadrant-safe */
                float a = yaw, cx, sx;
                float aa = a > 3.14159265f ? 6.2831853f - a : a;
                cx = x_cosf_small(aa > 2.2f ? 3.14159265f - aa : aa); if (aa > 2.2f) cx = -cx;
                { float b = a; float bb = b > 1.57079633f && b < 4.71238898f ? 0 : 0; (void)bb; }
                sx = (a < 3.14159265f) ? 1.0f : -1.0f;
                { float t = 1.0f - cx * cx; sx *= x_sqrtf(t > 0 ? t : 0); }
                set_camera((float)(u % 50), 0, 0, cx, sx, 0);
            }
            g_fake_us += 16000;
            n = (rng >> 8) % 40;
            rng = rng * 1664525u + 1013904223u;
            for (j = 0; j < n; ++j) {
                float x, y; uint32_t p;
                rng = rng * 1664525u + 1013904223u; x = ((float)((rng >> 8) & 1023u) - 512.0f) * 0.4f;
                rng = rng * 1664525u + 1013904223u; y = ((float)((rng >> 8) & 1023u) - 512.0f) * 0.4f;
                rng = rng * 1664525u + 1013904223u; p = 1 + ((rng >> 8) % 12);
                put(g, j, x, y, 0, p, tag++);
            }
            wr16(g + 0x5228, n);
            wr16(g + 0x5230, 0);
            rn = 0;
            if (((rng >> 20) % 9) == 0) { rem_provider = 1 + ((rng >> 4) % 12); rn = 1; wr32(rdptr(g + 0x522C), rem_provider); wr16(g + 0x5230, 1); }
            if (rn) {   /* expected drops: parked entries of that provider */
                for (j = 0; j < g_park_count; ++j)
                    if (rd32(g_park + (size_t)j * 0x20u + 0x1C) == rem_provider) ++dropped_expected;
            }
            dropped_before = g_gu_extras_report.cone_dropped_removed; (void)dropped_before;
            gu_x_before_queue(g);
            n = rd16(g + 0x5228);
            for (j = 0; j < n; ++j) {
                uint32_t t = tag_at(g, j) - 1000u;
                CHECK(t < 80000u);
                if (t < 80000u) { CHECK(seen[t] == 0); seen[t] = 1; ++in_queue_total; }
            }
        }
        /* flush: invalid camera releases everything that is still waiting */
        set_camera(0, 0, 0, 0, 0, 0);
        for (u = 0; u < 400 && g_park_count; ++u) {
            wr16(g + 0x5228, 0); wr16(g + 0x5230, 0); gu_x_before_queue(g);
            n_flush: { uint32_t n = rd16(g + 0x5228), j2; for (j2 = 0; j2 < n; ++j2) { uint32_t t = tag_at(g, j2) - 1000u; CHECK(t < 80000u && seen[t] == 0); if (t < 80000u) { seen[t] = 1; ++in_queue_total; } } }
        }
        CHECK(g_park_count == 0);
        CHECK(g_gu_extras_report.cone_dropped_removed == dropped_expected);
        CHECK(in_queue_total + g_gu_extras_report.cone_dropped_removed == tag - 1000u);
        out("random test: made "); outu(in_queue_total); out(", dropped "); outu(g_gu_extras_report.cone_dropped_removed);
        out(", entries "); outu(tag - 1000u); out("\n");
    }

    /* XMM: the cone hooks run float code (distance, cone, sqrt); xmm0-7 must
     * come back unchanged for all three cone stubs */
    {
        uint8_t *g2 = new_gen(8192);
        put(g2, 0, -150, 10, 0, 1, 200); put(g2, 1, 30, 5, 0, 1, 201); put(g2, 2, -300, 0, 0, 1, 202);
        wr16(g2 + 0x5228, 3);
        call_with_xmm(A(off_update), addr32(g2));
        CHECK(g_gu_extras_report.cone_parked_total >= 1);   /* float path really ran */
        call_with_xmm(A(off_reset), addr32(g2));
        call_with_xmm(A(off_dtor), addr32(g2));
    }

    /* ---------- 4. all-or-nothing behaviour ---------- */
    /* a pattern that matches twice: that feature writes nothing */
    g_limit_patched = 0;
    build_image(1);
    reset_report();
    x_copy(before_img, img, IMG_SIZE);
    cfg.matrix_limit = 4096; cfg.reflections_props = 1; cfg.cone_back_meters = 0;
    installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
    CHECK(installed == 0 && g_gu_extras_report.matrix_status == GU_X_ERR_SITE);
    CHECK(g_gu_extras_report.site_matches[S_M3] == 2);
    CHECK(x_equal(before_img, img, IMG_SIZE));
    /* a stopped thread inside a window: nothing written */
    build_image(0);
    reset_report();
    x_copy(before_img, img, IMG_SIZE);
    cfg.matrix_limit = 0; cfg.reflections_props = 0; cfg.cone_back_meters = 0;
    busy_always_site = 1;
    installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
    CHECK(installed == 0 && g_gu_extras_report.reflections_status == GU_X_ERR_BUSY);
    CHECK(x_equal(before_img, img, IMG_SIZE));
    busy_always_site = 0;
    /* nothing requested: nothing touched */
    cfg.reflections_props = 1; reset_report();
    installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
    CHECK(installed == 0 && x_equal(before_img, img, IMG_SIZE));
    /* the main fix already raised the two limit immediates: they are left alone,
     * the pool is still raised */
    g_limit_patched = 1; build_image(0); reset_report();
    cfg.matrix_limit = 4096; cfg.matrix_pool = 0; cfg.reflections_props = 1; cfg.cone_back_meters = 0;
    installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
    CHECK(installed == 1 && g_gu_extras_report.matrix_status == GU_X_ON);
    CHECK(g_gu_extras_report.matrix_limit_applied == 0 && g_gu_extras_report.matrix_pool_applied == 28000);
    CHECK(g_gu_extras_report.matrix_limit_sites_original == 0 && g_gu_extras_report.site_matches[S_M1] == 0);
    CHECK(rd32(img + off_m1 + 6) == 0x2000 && rd32(img + off_m2 + 2) == 0x2000);   /* untouched */
    CHECK(rd32(img + off_m3 + 1) == 28000);
    /* limit asked, limit sites gone, pool explicitly vanilla: nothing to do */
    g_limit_patched = 1; build_image(0); reset_report();
    cfg.matrix_pool = 7000;
    x_copy(before_img, img, IMG_SIZE);
    installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
    CHECK(installed == 0 && g_gu_extras_report.matrix_status == GU_X_ERR_SITE && x_equal(before_img, img, IMG_SIZE));
    cfg.matrix_pool = 0; g_limit_patched = 0;
    /* a camera pointer outside the image is refused */
    build_image(0); reset_report();
    wr32(img + off_cam + 12, 0x1000);
    cfg.cone_back_meters = 70; cfg.reflections_props = 1; cfg.matrix_limit = 0;
    x_copy(before_img, img, IMG_SIZE);
    installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
    CHECK(installed == 0 && g_gu_extras_report.cone_status == GU_X_ERR_CAMERA && x_equal(before_img, img, IMG_SIZE));

    /* ---------- 6. drawable references and render cache ---------- */
    {
        uint32_t newbuf, limit, p, n0;
        g_limit_patched = 0; g_scan_copies = 2; g_busy_lo = g_busy_hi = 0;
        build_image(0); reset_report();
        cfg.matrix_limit = 0; cfg.matrix_pool = 0; cfg.reflections_props = 1; cfg.cone_back_meters = 0;
        cfg.drawable_refs = 100000; cfg.render_cache = 32768; cfg.drawable_own_memory = 1;
        /* the untouched fake game: the old count, the game's allocator, the old limit */
        n0 = new_calls;
        CALL4(off_ddrv, 0, 0, 0, 0);
        CHECK(rd32(img + D_DRRES) == 0x1234 && new_calls == n0 + 1);
        wr32(img + D_RCEND, A(D_RCBUF) + 0x10000u); CHECK(CALL4(off_rcs[0], 0, 0, 0, 0) == 1);
        wr32(img + D_RCEND, A(D_RCBUF) + 0xFFF8u);  CHECK(CALL4(off_rcs[0], 0, 0, 0, 0) == 0);
        x_copy(before_img, img, IMG_SIZE);
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 24);
        CHECK(g_gu_extras_report.drawable_status == GU_X_ON && g_gu_extras_report.render_cache_status == GU_X_ON);
        CHECK(g_gu_extras_report.drawable_refs_applied == 100000 && g_gu_extras_report.render_cache_applied == 32768);
        CHECK(g_gu_extras_report.site_matches[S_D1] == 1 && g_gu_extras_report.site_matches[S_RC] == 2);
        /* drawable refs: the new count is pushed, the array comes from our memory, not the game's */
        CHECK(rd32(img + off_ddrv + 1) == 100000);
        n0 = new_calls;
        CALL4(off_ddrv, 0, 0, 0, 0);
        p = rd32(img + D_DRRES);
        CHECK(p != 0 && p != 0x1234 && new_calls == n0 && g_gu_extras_report.drawable_allocs == 1);
        { uint8_t *mem = (uint8_t *)(uintptr_t)p; mem[0] = 1; mem[99999] = 2; CHECK(mem[0] == 1 && mem[99999] == 2); }
        /* render cache: new buffer, both limits moved to three quarters of the new size */
        newbuf = rd32(img + off_rcstart + 1);
        limit = (32768u / 4u) * 3u * 8u;
        CHECK(newbuf != A(D_RCBUF) && newbuf != 0);
        CHECK(rd32(img + off_rcs[0] + 18) == limit && rd32(img + off_rcs[1] + 18) == limit);
        for (i = 0; i < 2; ++i) {
            wr32(img + D_RCEND, newbuf + limit);      CHECK(CALL4(off_rcs[i], 0, 0, 0, 0) == 1);
            wr32(img + D_RCEND, newbuf + limit - 8u); CHECK(CALL4(off_rcs[i], 0, 0, 0, 0) == 0);
        }
        { uint8_t *mem = (uint8_t *)(uintptr_t)newbuf; mem[32768u * 8u + 56u] = 7; CHECK(mem[32768u * 8u + 56u] == 7); }   /* room for the unchecked add */

        /* the game already has at least that many: nothing to do, nothing written */
        build_image(0); reset_report(); cfg.render_cache = 0; cfg.drawable_refs = 13000;
        x_copy(before_img, img, IMG_SIZE);
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 0 && g_gu_extras_report.drawable_status == GU_X_NOT_NEEDED && x_equal(before_img, img, IMG_SIZE));
        /* the function that makes the array looks different than expected */
        build_image(0); reset_report(); cfg.drawable_refs = 100000;
        img[off_dinit + 0x1C] = 0x52;
        x_copy(before_img, img, IMG_SIZE);
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 0 && g_gu_extras_report.drawable_status == GU_X_ERR_SITE && x_equal(before_img, img, IMG_SIZE));
        /* a stopped thread inside the push of the count */
        build_image(0); reset_report();
        g_busy_lo = A(off_ddrv); g_busy_hi = A(off_ddrv) + 11u;
        x_copy(before_img, img, IMG_SIZE);
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 0 && g_gu_extras_report.drawable_status == GU_X_ERR_BUSY && x_equal(before_img, img, IMG_SIZE));
        g_busy_lo = g_busy_hi = 0;

        /* render cache: three scans, one scan, a start function that is not mov+ret, a different immediate */
        cfg.drawable_refs = 0; cfg.render_cache = 32768;
        g_scan_copies = 3; build_image(0); reset_report(); x_copy(before_img, img, IMG_SIZE);
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 0 && g_gu_extras_report.render_cache_status == GU_X_ERR_SITE && g_gu_extras_report.site_matches[S_RC] == 3 && x_equal(before_img, img, IMG_SIZE));
        g_scan_copies = 1; build_image(0); reset_report(); x_copy(before_img, img, IMG_SIZE);
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 0 && g_gu_extras_report.render_cache_status == GU_X_ERR_SITE && x_equal(before_img, img, IMG_SIZE));
        g_scan_copies = 2; build_image(0); reset_report(); img[off_rcstart] = 0xB9; x_copy(before_img, img, IMG_SIZE);
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 0 && g_gu_extras_report.render_cache_status == GU_X_ERR_SITE && x_equal(before_img, img, IMG_SIZE));
        g_scan_copies = 2; build_image(0); reset_report(); x_copy(before_img, img, IMG_SIZE);
        cfg.render_cache = 9000;                       /* below what the game has: not needed */
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 0 && g_gu_extras_report.render_cache_status == GU_X_NOT_NEEDED && x_equal(before_img, img, IMG_SIZE));

        /* everything at once */
        build_image(0); reset_report();
        wr32(img + D_REFL, 0x6); wr32(img + D_SCENE, 0x4);
        cfg.matrix_limit = 4096; cfg.matrix_pool = 0; cfg.reflections_props = 0; cfg.cone_back_meters = 70;
        cfg.cone_full_angle_deg = 75; cfg.cone_release_per_update = 384; cfg.cone_lead_ms = 300;
        cfg.drawable_refs = 100000; cfg.render_cache = 32768; cfg.drawable_own_memory = 1; cfg.is_prop_entity = is_prop_cb;
        set_camera(0, 0, 0, 1, 0, 0); set_gather(200.0f);
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 31);
        CHECK(g_gu_extras_report.matrix_status == GU_X_ON && g_gu_extras_report.reflections_status == GU_X_ON &&
              g_gu_extras_report.cone_status == GU_X_ON && g_gu_extras_report.drawable_status == GU_X_ON &&
              g_gu_extras_report.render_cache_status == GU_X_ON);
        for (i = 0; i < S_C2; ++i) CHECK(g_gu_extras_report.site_matches[i] == (i == S_RC ? 2u : 1u));
    }

    /* ---------- 7. spawn limit, drawable refs in the game's heap, and both queue features together ---------- */
    {
        uint8_t *g = new_gen(8192); uint32_t k, r, round, total = 0, made = 0, dropped = 0;
        static uint8_t seen[200000];
        uint32_t last_max = 0, ordered = 1, first_round_count = 0;
        for (k = 0; k < sizeof seen; ++k) seen[k] = 0;
        /* --- drawable refs without the redirect: only the count changes, the game's allocator is used --- */
        g_limit_patched = 0; g_scan_copies = 2; g_busy_lo = g_busy_hi = 0;
        build_image(0); reset_report();
        { uint32_t zi; for (zi = 0; zi < sizeof cfg / 4; ++zi) ((uint32_t *)&cfg)[zi] = 0; }
        cfg.reflections_props = 1; cfg.drawable_refs = 100000; cfg.drawable_own_memory = 0;
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 8 && rd32(img + off_ddrv + 1) == 100000);
        { uint32_t n0 = new_calls; CALL4(off_ddrv, 0, 0, 0, 0);
          CHECK(rd32(img + D_DRRES) == 0x1234 && new_calls == n0 + 1 && g_gu_extras_report.drawable_allocs == 0); }

        /* --- spawn limit alone, through the stubs: 6 near props, 17 further ones in a line ahead --- */
        build_image(0); reset_report();
        { uint32_t zi; for (zi = 0; zi < sizeof cfg / 4; ++zi) ((uint32_t *)&cfg)[zi] = 0; }
        cfg.reflections_props = 1; cfg.spawns_per_update = 4; cfg.spawn_near_always = 10;
        set_camera(0, 0, 0, 1, 0, 0); g_fake_us += 16000;
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == 32 && g_gu_extras_report.spawn_status == GU_X_ON && g_gu_extras_report.cone_status == GU_X_OFF);
        CHECK(g_gu_extras_report.site_matches[S_C2] == 1);
        for (k = 0; k < 6; ++k) put(g, k, -5.0f - (float)k * 0.2f, 0, 0, 1 + (k & 1), 3000 + k);          /* near, behind */
        for (k = 0; k < 17; ++k) put(g, 6 + k, 60.0f - (float)k * 2.0f, 0, 0, 1 + (k & 1), 3100 + k);       /* shuffled far order: 60,58,..,28 */
        wr16(g + 0x5228, 23); total = 23;
        for (round = 0; round < 30; ++round) {
            uint32_t n, carried, minr = 0xFFFFFFFFu, maxr = 0;
            g_fake_us += 16000;
            CALL4(off_update, g, 0, 0, 0);
            n = rd32(g + 0x6004);                                          /* the count the game code after the hook saw */
            CHECK(n == rd16(g + 0x5228));
            if (round == 0) first_round_count = n;
            for (k = 0; k < n; ++k) {
                uint32_t tg = tag_at(g, k) - 3000u;
                CHECK(tg < 200000u && !seen[tg]); if (tg < 200000u) { seen[tg] = 1; ++made; }
                if (tg >= 100u) { uint32_t d = 60u - 2u * (tg - 100u); if (d < minr) minr = d; if (d > maxr) maxr = d; }
            }
            if (round > 0 && minr != 0xFFFFFFFFu) { if (minr < last_max) ordered = 0; }
            if (maxr) last_max = maxr;
            wr16(g + 0x5228, 0);                                           /* the game made its props and emptied the queue */
            CALL4(off_after, g, 0, 0, 0);
            carried = rd16(g + 0x5228);
            if (carried == 0) break;
        }
        CHECK(made == total);                                              /* every position made, exactly once */
        CHECK(first_round_count == 6);                                     /* all 6 near ones at once, although the budget is 4 */
        CHECK(ordered);                                                    /* the far ones came nearest first, round after round */
        SCHECK(g_gu_extras_report.spawn_limited_updates >= 3 && g_gu_extras_report.spawn_hold_max == 17);

        /* --- positions of a provider the game removed while they waited are dropped --- */
        reset_report();
        gu_x_on_reset(g);
        for (k = 0; k < 20; ++k) put(g, k, 40.0f + (float)k, 0, 0, 1 + (k % 4), 4000 + k);
        wr16(g + 0x5228, 20); g_fake_us += 16000;
        CALL4(off_update, g, 0, 0, 0);
        CHECK(rd16(g + 0x5228) == 4);
        wr16(g + 0x5228, 0); CALL4(off_after, g, 0, 0, 0);
        CHECK(rd16(g + 0x5228) == 16);                                     /* carried at the front */
        { uint32_t expect_dropped = 0; uint8_t *q = rdptr(g + 0x5224);
          for (k = 0; k < 16; ++k) if (rd32(q + (size_t)k * 0x20u + 0x1C) == 2) ++expect_dropped;
          wr32(rdptr(g + 0x522C), 2); wr16(g + 0x5230, 1);
          g_fake_us += 16000; CALL4(off_update, g, 0, 0, 0);
          SCHECK(g_gu_extras_report.spawn_dropped_removed == expect_dropped);
          CHECK(expect_dropped > 0 && rd16(g + 0x5228) == 4);
          for (k = 0; k < rd16(g + 0x5228); ++k) CHECK(rd32(rdptr(g + 0x5224) + (size_t)k * 0x20u + 0x1C) != 2);
          wr16(g + 0x5230, 0); }

        /* --- camera not trusted: nothing is kept back --- */
        gu_x_on_reset(g);
        set_camera(0, 0, 0, 0, 0, 0);
        for (k = 0; k < 30; ++k) put(g, k, 50.0f + (float)k, 0, 0, 1, 5000 + k);
        wr16(g + 0x5228, 30); g_fake_us += 16000;
        CALL4(off_update, g, 0, 0, 0);
        CHECK(rd16(g + 0x5228) == 30);
        wr16(g + 0x5228, 0); CALL4(off_after, g, 0, 0, 0);
        CHECK(rd16(g + 0x5228) == 0);                                      /* nothing to put back */
        set_camera(0, 0, 0, 1, 0, 0);

        /* --- a REAL memory fault in the after hook / in the before hook is contained --- */
        {
            float *saved_key = g_sp_key; uint8_t *saved_hold = g_hold;
            gu_x_on_reset(g);
            for (k = 0; k < 20; ++k) put(g, k, 40.0f + (float)k, 0, 0, 1, 6000 + k);
            wr16(g + 0x5228, 20); g_fake_us += 16000; CALL4(off_update, g, 0, 0, 0);
            CHECK(rd16(g + 0x5228) == 4 && g_hold_count == 16);
            g_hold = (uint8_t *)(uintptr_t)0x20000;                        /* unmapped: the copy back faults */
            wr16(g + 0x5228, 0);
            CALL4(off_after, g, 0, 0, 0);
            CHECK(g_spawn_disabled == 1 && g_gu_extras_report.spawn_faults == 1 && g_gu_extras_report.spawn_disabled == 1 && g_hold_count == 0);
            g_hold = saved_hold;
            for (k = 0; k < 12; ++k) put(g, k, 40.0f + (float)k, 0, 0, 1, 6100 + k);
            wr16(g + 0x5228, 12); CALL4(off_update, g, 0, 0, 0);
            CHECK(rd16(g + 0x5228) == 12);                                 /* switched off: a plain pass-through */
            CHECK(rd32(g + 0x600C) == 0x11111111 && rd32(g + 0x6010) == 0x22222222);   /* registers intact through the stub */
            g_spawn_disabled = 0; g_gu_extras_report.spawn_disabled = 0; g_gu_extras_report.spawn_faults = 0;

            g_sp_key = (float *)(uintptr_t)0x20000;                        /* unmapped: the next sort key write faults */
            for (k = 0; k < 12; ++k) put(g, k, 40.0f + (float)k, 0, 0, 1, 6200 + k);
            wr16(g + 0x5228, 12); g_fake_us += 16000;
            CALL4(off_update, g, 0, 0, 0);
            CHECK(g_spawn_disabled == 1 && g_gu_extras_report.spawn_faults == 1);
            CHECK(rd16(g + 0x5228) == 12 && g_hold_count == 0);            /* the game still gets the whole queue */
            CHECK(rd32(g + 0x6014) == 0x33333333 && rd32(g + 0x6018) == 0x44444444);
            g_sp_key = saved_key; g_spawn_disabled = 0; g_gu_extras_report.spawn_disabled = 0; g_gu_extras_report.spawn_faults = 0;
        }

        /* --- cone and spawn limit together: every position is accounted for exactly once --- */
        build_image(0); reset_report();
        { uint32_t zi; for (zi = 0; zi < sizeof cfg / 4; ++zi) ((uint32_t *)&cfg)[zi] = 0; }
        cfg.reflections_props = 1; cfg.cone_back_meters = 70; cfg.cone_full_angle_deg = 75;
        cfg.cone_release_per_update = 200; cfg.cone_lead_ms = 300; cfg.spawns_per_update = 64; cfg.spawn_near_always = 20;
        set_camera(0, 0, 0, 1, 0, 0); set_gather(300.0f);
        installed = (uint32_t)gu_extras_install_range(&cfg, img, 0x8000, img, IMG_SIZE, g_mgr, busy_cb, 0);
        CHECK(installed == (4u | 32u) && g_gu_extras_report.cone_status == GU_X_ON && g_gu_extras_report.spawn_status == GU_X_ON);
        {
            uint32_t rng = 4242, tag = 100000, u, j, got = 0, gone = 0;
            float cx = 1.0f, cy = 0.0f, px = 0.0f;
            static uint8_t seen2[400000];
            for (j = 0; j < sizeof seen2; ++j) seen2[j] = 0;
            gu_x_on_reset(g); wr16(g + 0x5228, 0); wr16(g + 0x5230, 0);   /* nothing left over from the earlier scenarios */
            for (u = 0; u < 1500; ++u) {
                uint32_t carried = rd16(g + 0x5228), n, rem = 0, add, idx;
                float a;
                rng = rng * 1664525u + 1013904223u; a = ((float)((rng >> 16) & 255u) - 127.5f) * 0.003f;
                { float nc = cx - cy * a, ns = cy + cx * a, l = x_sqrtf(nc * nc + ns * ns); cx = nc / l; cy = ns / l; }
                px += 0.4f; set_camera(px, 0, 0, cx, cy, 0); g_fake_us += 16000;
                rng = rng * 1664525u + 1013904223u; add = (rng >> 8) % 300;
                if (add > 8192u - carried) add = 8192u - carried;     /* the game itself does not queue beyond the capacity */
                for (j = 0; j < add; ++j) {
                    float x, y; uint32_t p;
                    rng = rng * 1664525u + 1013904223u; x = px + ((float)((rng >> 8) & 1023u) - 512.0f) * 0.5f;
                    rng = rng * 1664525u + 1013904223u; y = ((float)((rng >> 8) & 1023u) - 512.0f) * 0.5f;
                    rng = rng * 1664525u + 1013904223u; p = 1 + ((rng >> 8) % 12);
                    put(g, carried + j, x, y, 0, p, tag++);
                }
                wr16(g + 0x5228, carried + add); wr16(g + 0x5230, 0);
                if (((rng >> 20) % 9) == 0) {
                    uint32_t prov = 1 + ((rng >> 4) % 12); uint8_t *q = rdptr(g + 0x5224); rem = 1;
                    wr32(rdptr(g + 0x522C), prov); wr16(g + 0x5230, 1);
                    for (j = 0; j < g_park_count; ++j) if (rd32(g_park + (size_t)j * 0x20u + 0x1C) == prov) ++gone;
                    for (j = 0; j < carried; ++j) if (rd32(q + (size_t)j * 0x20u + 0x1C) == prov) ++gone;
                }
                (void)rem;
                gu_x_before_queue(g);
                n = rd16(g + 0x5228);
                for (j = 0; j < n; ++j) { idx = tag_at(g, j) - 100000u; CHECK(idx < 400000u && !seen2[idx]); if (idx < 400000u) { seen2[idx] = 1; ++got; } }
                wr16(g + 0x5228, 0); gu_x_after_queue(g);
            }
            /* flush: an untrusted camera releases everything that waits, nothing is kept back any more */
            set_camera(0, 0, 0, 0, 0, 0);
            for (u = 0; u < 400 && (g_park_count || rd16(g + 0x5228)); ++u) {
                uint32_t n;
                wr16(g + 0x5230, 0); g_fake_us += 16000;
                gu_x_before_queue(g);
                n = rd16(g + 0x5228);
                for (j = 0; j < n; ++j) { idx_flush: { uint32_t id2 = tag_at(g, j) - 100000u; CHECK(id2 < 400000u && !seen2[id2]); if (id2 < 400000u) { seen2[id2] = 1; ++got; } } }
                wr16(g + 0x5228, 0); gu_x_after_queue(g);
            }
            CHECK(g_park_count == 0 && g_hold_count == 0 && rd16(g + 0x5228) == 0);
            CHECK(got + gone == tag - 100000u);
            out("cone + spawn limit: positions "); outu(tag - 100000u); out(", made "); outu(got); out(", dropped (provider removed) "); outu(gone); out("\n");
        }
        (void)r; (void)dropped;
    }

    out("checks: "); outu((uint32_t)g_checks); out(", failed: "); outu((uint32_t)g_fail); out("\n");
    sys_exit(g_fail ? 1 : 0);
    return 0;
}
void _start(void) { sys_exit(main()); }
