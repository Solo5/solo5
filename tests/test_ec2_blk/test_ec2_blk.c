/*
 * test_ec2_blk: block I/O beyond test_blk's one-block-at-a-time checks, for
 * the ec2 bindings' NVMe driver on EBS.
 *
 * The "storage" device's first DATA_BYTES hold known data (a disk image
 * whose SHA-256 is computed on the workstation); its last WRITE_BYTES are
 * scratch space. Prints:
 *   - SHA-256 of the data region read in 1 MiB requests (PRP lists), and
 *     in one-block requests, with the time each took;
 *   - whether an unaligned buffer (bounce path) reads the same bytes;
 *   - whether multi-block writes of several sizes read back intact;
 * then "test_ec2_blk done" and a heartbeat every 10 s, so EC2 posts the
 * console.
 */

#include "solo5.h"
#include "../../bindings/lib.c"

#define DATA_BYTES  (64ULL * 1024 * 1024)
#define BIG         (1024 * 1024)
#define WRITE_BYTES (4ULL * 1024 * 1024)

static void puts(const char *s)
{
    solo5_console_write(s, strlen(s));
}

static void putu(uint64_t n)
{
    char buf[21];
    int i = sizeof buf - 1;

    buf[i] = '\0';
    do {
        buf[--i] = '0' + (n % 10);
        n /= 10;
    } while (n);
    puts(&buf[i]);
}

/* SHA-256 (FIPS 180-4), small and slow is fine here. */

struct sha256 {
    uint32_t h[8];
    uint8_t block[64];
    size_t used;
    uint64_t bytes;
};

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(struct sha256 *s, const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, h;

    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 |
               (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3];
    e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) +
                      ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) +
                      ((a & b) ^ (a & c) ^ (b & c));
        h = g, g = f, f = e, e = d + t1;
        d = c, c = b, b = a, a = t1 + t2;
    }
    s->h[0] += a, s->h[1] += b, s->h[2] += c, s->h[3] += d;
    s->h[4] += e, s->h[5] += f, s->h[6] += g, s->h[7] += h;
}

static void sha256_init(struct sha256 *s)
{
    static const uint32_t h0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                   0xa54ff53a, 0x510e527f, 0x9b05688c,
                                   0x1f83d9ab, 0x5be0cd19};
    memcpy(s->h, h0, sizeof h0);
    s->used = 0;
    s->bytes = 0;
}

static void sha256_update(struct sha256 *s, const uint8_t *p, size_t n)
{
    s->bytes += n;
    while (n > 0) {
        size_t take = 64 - s->used;
        if (take > n)
            take = n;
        memcpy(s->block + s->used, p, take);
        s->used += take;
        p += take;
        n -= take;
        if (s->used == 64) {
            sha256_block(s, s->block);
            s->used = 0;
        }
    }
}

static void sha256_print(struct sha256 *s)
{
    uint64_t bits = s->bytes * 8;
    uint8_t pad = 0x80, zero = 0, len[8];

    sha256_update(s, &pad, 1);
    while (s->used != 56)
        sha256_update(s, &zero, 1);
    for (int i = 0; i < 8; i++)
        len[i] = bits >> (56 - 8 * i);
    sha256_update(s, len, 8);

    const char *hex = "0123456789abcdef";
    char out[65];
    for (int i = 0; i < 32; i++) {
        uint8_t byte = s->h[i / 4] >> (24 - 8 * (i % 4));
        out[2 * i] = hex[byte >> 4];
        out[2 * i + 1] = hex[byte & 0xf];
    }
    out[64] = '\0';
    puts(out);
}

static uint8_t big[BIG + 64] __attribute__((aligned(4096)));
static uint8_t check[BIG] __attribute__((aligned(4096)));

static void report(const char *what, bool ok)
{
    puts(what);
    puts(ok ? ": ok\n" : ": FAILED\n");
}

static uint64_t ms_since(solo5_time_t t0)
{
    return (solo5_clock_monotonic() - t0) / 1000000ULL;
}

int solo5_app_main(const struct solo5_start_info *si __attribute__((unused)))
{
    puts("\n**** Solo5 test_ec2_blk ****\n\n");

    solo5_handle_t h;
    struct solo5_block_info bi;
    if (solo5_block_acquire("storage", &h, &bi) != SOLO5_R_OK) {
        puts("Could not acquire 'storage' block device\n");
        return 99;
    }
    puts("storage: ");
    putu(bi.capacity >> 20);
    puts(" MiB, block size ");
    putu(bi.block_size);
    puts("\n");
    if (bi.capacity < DATA_BYTES + WRITE_BYTES) {
        puts("storage is too small\n");
        return 98;
    }

    /* 1. The data region in 1 MiB reads. */
    struct sha256 s;
    sha256_init(&s);
    solo5_time_t t0 = solo5_clock_monotonic();
    bool ok = true;
    for (uint64_t off = 0; off < DATA_BYTES && ok; off += BIG) {
        ok = solo5_block_read(h, off, big, BIG) == SOLO5_R_OK;
        sha256_update(&s, big, BIG);
    }
    uint64_t ms = ms_since(t0);
    puts("sha256 of the first 64 MiB, 1 MiB reads: ");
    sha256_print(&s);
    puts(" (");
    putu(ms);
    puts(" ms)\n");
    report("1 MiB reads", ok);

    /* 2. The same region again, one block per request (as mirage does). */
    sha256_init(&s);
    t0 = solo5_clock_monotonic();
    for (uint64_t off = 0; off < DATA_BYTES && ok; off += bi.block_size) {
        ok = solo5_block_read(h, off, big, bi.block_size) == SOLO5_R_OK;
        sha256_update(&s, big, bi.block_size);
    }
    ms = ms_since(t0);
    puts("sha256 of the first 64 MiB, one-block reads: ");
    sha256_print(&s);
    puts(" (");
    putu(ms);
    puts(" ms)\n");
    report("one-block reads", ok);

    /* 3. Unaligned buffer (bounce path) reads the same bytes. */
    ok = solo5_block_read(h, 0, big + 1, 128 * 1024) == SOLO5_R_OK &&
         solo5_block_read(h, 0, check, 128 * 1024) == SOLO5_R_OK &&
         memcmp(big + 1, check, 128 * 1024) == 0;
    report("unaligned 128 KiB read matches aligned read", ok);

    /* 4. Writes of several sizes and alignments read back intact. */
    static const size_t sizes[] = {512, 4096, 8192, 65536, 300 * 1024, BIG};
    uint64_t base = bi.capacity - WRITE_BYTES;
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        size_t n = sizes[i];
        for (unsigned misalign = 0; misalign < 2; misalign++) {
            uint8_t *src = big + misalign * 8;
            for (size_t j = 0; j < n; j++)
                src[j] = (uint8_t)(j * 7 + i * 13 + misalign);
            /* Each write is checked at once, so overlaps don't matter. */
            uint64_t off = base + misalign * BIG + (uint64_t)i * 4096;
            ok = solo5_block_write(h, off, src, n) == SOLO5_R_OK &&
                 solo5_block_read(h, off, check, n) == SOLO5_R_OK &&
                 memcmp(src, check, n) == 0;
            puts("write+read ");
            putu(n);
            puts(misalign ? " bytes, buffer offset 8" : " bytes, aligned");
            report("", ok);
        }
    }

    /* 5. Invalid requests are refused. */
    ok = solo5_block_read(h, bi.capacity, big, bi.block_size) != SOLO5_R_OK &&
         solo5_block_read(h, 1, big, bi.block_size) != SOLO5_R_OK &&
         solo5_block_read(h, 0, big, bi.block_size + 1) != SOLO5_R_OK &&
         solo5_block_read(h, bi.capacity - bi.block_size, big,
                          2 * bi.block_size) != SOLO5_R_OK;
    report("out-of-range and unaligned requests refused", ok);

    puts("test_ec2_blk done\n");

    solo5_time_t start = solo5_clock_monotonic();
    for (uint64_t i = 1;; i++) {
        solo5_yield(start + i * 10000000000ULL, NULL);
        puts("heartbeat ");
        putu(i * 10);
        puts(" s\n");
    }
    return SOLO5_EXIT_SUCCESS;
}
