
// Imports
// =======

#pragma clang fp contract(off)

#ifdef __METAL_VERSION__
#include <metal_stdlib>
using namespace metal;
#elif !defined(__CUDACC_RTC__)
#ifndef __APPLE__
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>
#include <poll.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __OBJC__
// #include, not #import: bend -o reads an #import as an effect's framework
#include <Metal/Metal.h>
#include <Foundation/Foundation.h>
#elif BEND_CUDA
#include <cuda.h>
#include <nvrtc.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif
#endif

// Dialect
// =======

#ifdef __METAL_VERSION__
// coherent(device) (MSL 3.2): M1-class parts else lose stores across
// threadgroups within a dispatch
#if __METAL_VERSION__ >= 320
#define DEV     coherent(device) device
#define DEVL    coherent(device) device
#else
#define DEV     device
#define DEVL    device
#endif
#define GA32    threadgroup atomic_uint
#define THR     thread
#define INLINE  inline
#define OUTLINE static
#define CONSTV  constant
#define DEVICE  1
#define CLZ(x)  clz(x)
#define A32(p)  ((DEV atomic_uint*)(p))
#define RLX     memory_order_relaxed
#define FENCE() atomic_thread_fence(mem_flags::mem_device, memory_order_seq_cst)
#define BAR()   threadgroup_barrier(mem_flags::mem_threadgroup)
#define BARD()  threadgroup_barrier(mem_flags::mem_device \
  | mem_flags::mem_threadgroup)

#define g32_ini(p)    atomic_store_explicit(p, 0, RLX)
#define g32_add(p, v) atomic_fetch_add_explicit(p, v, RLX)
#define g32_get(p)    atomic_load_explicit(p, RLX)
#else
#define DEVL
#define THR
#define INLINE  static inline
#define CONSTV  static const

#define g32_ini(p)    a32_store(p, 0)
#define g32_add(p, v) a32_add(p, v)
#define g32_get(p)    a32_load(p)
#ifdef __CUDACC_RTC__
// plain data stays L1-cacheable: cross-lane handoffs go through a32 + FENCE
#define DEV
#define GA32    __shared__ u32
#define OUTLINE static __attribute__((noinline))
#define DEVICE  1
#define CLZ(x)  (u32)__clz((int)(x))
#define FENCE() __threadfence()
#define BAR()   __syncthreads()
#define BARD()  \
  { __threadfence(); __syncthreads(); }
#else
#define DEV
// only clang 19+ has both, and only it compiles preserve_most soundly
#if __has_attribute(preserve_none) && __has_attribute(preserve_most)
#define PRESERVE(A) __attribute__((A))
#else
#define PRESERVE(A)
#endif
#define OUTLINE static __attribute__((noinline, cold)) PRESERVE(preserve_most)
#define DEVICE  0
#define CLZ(x)  (u32)__builtin_clz(x)
#endif
#endif
#define FAR static __attribute__((noinline))

// A segment: a case of the device's switch; on the host, a preserve_none
// function (WL_SIG) left by a musttail call, its words fresh at WL_OPEN.
#if DEVICE
#define LOCK(l)
#define UNLOCK(l)
#define WL_CASE(F) case F:
#define WL_OPEN    {
#define WL_JMP(F)  { fid = (F); break; }
#define WL_DYN     WL_JMP
#else
#define LOCK(l)    while (__atomic_exchange_n(&(l), 1, __ATOMIC_ACQUIRE)) {}
#define UNLOCK(l)  __atomic_store_n(&(l), 0, __ATOMIC_RELEASE)
#define WL_FN      static PRESERVE(preserve_none) __attribute__((noinline)) Reply
#define WL_CASE(F) WL_FN WL_##F(WL_SIG)
#define WL_OPEN    { WL_BANK u32 rn;
#define WL_JMP(F)  __attribute__((musttail)) return WL_##F(WL_ALL)
#define WL_DYN(F)  __attribute__((musttail)) return wl_tab[F](WL_ALL)
#endif
#define WL_SPIN     for (;;) { if (err_spun(e.mem, &wpoll)) { return 0; }
#define WL_SPUN     } break;
#define WL_AGAIN(F) continue
#define WL_POP()    { sp -= LANE_STEP; WL_DYN((Fid)STK(0)); }

#define LANE_STEP (DEVICE ? (int64_t)CUBE : 1)
#define STK(I)    sp[(int64_t)(I) * LANE_STEP]

#define WL_RETN(N)  { rn = (N); WL_POP(); }
#define WL_CONT     STK(-3)
#define WL_IDX      STK(-2)
#define WL_POPN(N)  sp -= N * LANE_STEP
#define WL_PUSHN(N) sp += N * LANE_STEP
#define WL_FRAME(T) \
  Loc wtl = task_tail(T); \
  u64 wtw = e.mem[wtl + 1]; \
  STK(0) = e.mem[wtl]; \
  STK(1) = (wtw >> 32) & 0xFFFF; \
  STK(2) = FID_EXIT; \
  sp += 3 * LANE_STEP;
#define WL_ARGS(A, N) \
  for (u32 wi = 0; wi + 1 < N; wi += 1) { \
    STK(wi) = e.mem[A + wi]; \
  } \
  sp += (N - 1) * LANE_STEP;
#define WL_ROOM(N) \
  if (DEVICE && sp + (N) * CUBE >= e.mem + HEAP_OFF + CUBE) { \
    err_post(e.mem, ERR_DEEP); \
    return 0; \
  }

// Types
// =====

#ifdef __METAL_VERSION__
typedef ulong u64;
typedef uint  u32;
typedef uchar u8;
typedef float f32;
#elif defined(__CUDACC_RTC__)
typedef unsigned long long u64;
typedef long long          int64_t;
typedef unsigned int       u32;
typedef unsigned char      u8;
typedef float              f32;
#else
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t  u8;
typedef float    f32;
#endif

typedef u64 Loc;
#define LOC_MASK ((1ull << 40) - 1)

typedef u32 Cls;
typedef u32 Fid;

typedef u64 Term;
#define TAG_PAK 1ull
#define TAG_CTR 2ull
#define TAG_CLO 3ull
#define TAG_BUF 4ull
#define TAG_TSK 5ull
#define TAG_ARR 6ull

#define TERM_HOLE (~0ull)

#define RFC_BIT  (1ull << 63)
#define RFC_CNT  ((1u << 24) - 1)

typedef Term Reply;

typedef u32 Err;
#define ERR_RING 1
#define ERR_TAGS 2
#define ERR_HEAP 3
#define ERR_FIDS 4
#define ERR_NATS 5
#define ERR_RFCS 6
#define ERR_DEEP 7
#define ERR_ARRS 8

typedef u32 Ring;

typedef DEV u64* Corpus;

typedef struct {
  Corpus   mem;
  DEV u64* alc;
} Env;

typedef struct {
  u64 off;
  u32 rd;
  u32 wr;
  u32 top;
} Bank;

typedef DEVL Term* Stk;

typedef Term Nat;
#define NAT_IMM ((1ull << 48) - 1)

typedef Term U32;

#if DEVICE
typedef u32 u32a;
#else
typedef u32 __attribute__((may_alias)) u32a;
#endif

#ifdef __METAL_VERSION__
typedef threadgroup atomic_uint* Cur;
#else
typedef u32* Cur;
#endif

// Constants
// =========

#define LINE      16
#define PAGE_BITS 7
#define PAGE_LEN  (1ull << PAGE_BITS)
#define CUBE_T    128
#define CUBE      ((u64)CUBE_T * CUBE_T)
#define CUBE_G    (1u << CUBE_LOG)
#define LANES     ((u64)CUBE_T << CUBE_LOG)
#define RING_LOG  (17 - CUBE_LOG)
#define RING_LEN  (1ull << RING_LOG)
#define STAK_LEN  (1ull << 11)
#define NCLS      8
#define NCLS_ALL  32
#define IO_HELP   64

#define ALC_WORDS NCLS_ALL
#define TG_HOLD   2304
#define CHUNK     256
#define CAP_WORDS 32768
#define QUANTUM   (DEVICE ? PAGE_LEN \
  : KEEP_WORDS < 32 * PAGE_LEN ? KEEP_WORDS : 32 * PAGE_LEN)
#if DEVICE
#define KEEP_WORDS CHUNK
#endif
#define RING_WORDS ((1ull << 10) + 2)

#define H_BUMP       0
#define H_CAP        1
#define H_CURSOR     LINE
#define H_ROOT_DONE  (2 * LINE)
#define H_ERROR_CODE (3 * LINE)
#define H_ROOT_WORD  (4 * LINE)
#define H_BANK       (H_ROOT_WORD + WL_RESW)

#define PAGE_UP(n) (((n) + PAGE_LEN - 1) & ~(PAGE_LEN - 1))
#define ALC_OFF  PAGE_UP(H_BANK + 3 * NCLS_ALL)
#define RING_OFF (ALC_OFF + CUBE * 2 * ALC_WORDS)
#define STAK_OFF (RING_OFF + CUBE * RING_WORDS)
#define STAT_OFF (STAK_OFF + CUBE * STAK_LEN)
#define HEAP_OFF (STAT_OFF + PAGE_UP(STAT_LEN))

// Globals
// =======

#if !DEVICE

typedef pthread_mutex_t lock;

static Corpus CORPUS;
static u64    ALC[CUBE_T + 1][3 * ALC_WORDS] __attribute__((aligned(128)));
static u32    KEEP_WORDS;
// the bag: 2^CUBE_LOG groups of CUBE_T lanes (a -D constant on the device)
static u32    CUBE_LOG = 7;
static u32    bank_lock;

static u32            pool_size;
static _Atomic u32    pool_row;
static bool           pool_grow;
static _Atomic u64    pool_tick;
static _Atomic u32    pool_done;
static lock           pool_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pool_wake = PTHREAD_COND_INITIALIZER;

// The device program compiles from the binary's own text.
#if BEND_METAL || BEND_CUDA
#pragma clang diagnostic ignored "-Wc23-extensions"
static const char BEND_SRC[] = {
#embed __FILE__
, 0 };
#endif

#ifdef __OBJC__
static id<MTLDevice>               gpu_dev;
static id<MTLCommandQueue>         gpu_que;
static id<MTLComputePipelineState> gpu_pso;
static id<MTLBuffer>               gpu_buf;
static id<MTLComputeCommandEncoder> gpu_enc;
#elif BEND_CUDA
static CUdevice   gpu_dev;
static CUmodule   gpu_lib;
static CUfunction gpu_pso;
#endif
static bool io_gpu;
static Stk  io_stk;

static const char* CLI_HELP =
  "usage: %s [options] [arguments]\n"
  "  --threads N       worker threads, 1 to 128 (default: the CPU count)\n"
  "  --gpu on|off|4GB  run ! calls on the GPU, over this much of its memory\n"
  "                    (default: on if present, over 2GB on Metal)\n"
  "  --gpu-build       write the GPU program and exit\n"
  "  --help            show this text\n"
  "  --                the rest are the program's arguments (IO.args)\n";

#endif

// Tables
// ======

#define CID_TUPLE 0
#define CID_SNIL 1
#define CID_SCON 2
#define CID_WCON 3
#define CID_EMIT 4
#define CID_HALT 5
#define CID_FAIL 6
#define CID_DONE 7
#define CID_NONE 8
#define CID_SOME 9
#define CID_FALSE 10
#define CID_TRUE 11
#define CID_UNIT 12
#define CID_NIL 13
#define CID_CON 14
#define CID_CHR 15
#define CID_ONE 16
#define CID_TWO 17
#define CID_ENV 18
#define CID_ST 19
#define CID_INFORCE 20
#define CID_BROKEUP 21
#define CID_BROKEDOWN 22
#define CID_NX 23
#define CID_TR 24
#define CID_BAR 25
#define CID_TQ 26
#define CID_BK 27
#define CID_UPD 28
#define CID_SCAN 29
#define CID_IO_ARGS 30
#define CID_FILE_OPEN 31
#define CID_FILE_SIZE 32
#define CID_FILE_READ 33
#define CID_FILE_CLOSE 34
#define CID_IO_PRINT 35
#define CID_IO_NOW 36
#define CID_FILE_WRITE_BYTES 37
#define CID_FILE_READ_BYTES 38
#define FID_OUT_DONE2 0
#define FID_RUN_ONE 1
#define FID_RUN_ONE_K118 2
#define FID_LIST_APPEND 3
#define FID_LIST_APPEND_K129 4
#define FID_READ_TICKER_BYTES_C133 5
#define FID_READ_TICKER_BYTES_C134 6
#define FID_READ_TICKER_BYTES_C135 7
#define FID_RUN_ALL 8
#define FID_RUN_ALL_K145 9
#define FID_RUN_ALL_K146 10
#define FID_RUN_ALL_J145 11
#define FID_RUN_ALL_K148 12
#define FID_LIST_CONCAT 13
#define FID_LIST_CONCAT_K150 14
#define FID_IO_TRY_C153 15
#define FID_IO_TRY_C154 16
#define FID_IO_PASS_C155 17
#define FID_IO_PASS_C156 18
#define FID_READ_TICKER_SIZED_C160 19
#define FID_READ_TICKER_SIZED_C161 20
#define FID_NAT_SHOW_FIN 21
#define FID_STRING_LENGTH 22
#define FID_STRING_LENGTH_K165 23
#define FID_STRING_SPLIT 24
#define FID_STRING_SPLIT_K168 25
#define FID_MAIN_WRITTEN_C185 26
#define FID_MAIN_WRITTEN_C186 27
#define FID_READ_ALL 28
#define FID_READ_ALL_C188 29
#define FID_READ_ALL_C189 30
#define FID_READ_TICKER_C190 31
#define FID_READ_TICKER_C191 32
#define FID_READ_TICKER_C192 33
#define FID_READ_TICKER_C193 34
#define FID_READ_ALL_C194 35
#define FID_READ_ALL_K195 36
#define FID_READ_ALL_C196 37
#define FID_READ_ALL_C197 38
#define FID_READ_ALL_C198 39
#define FID_NAT_SHOW 40
#define FID_LIST_LENGTH 41
#define FID_LIST_LENGTH_K201 42
#define FID_STRING_LINES 43
#define FID_PATHS_OF 44
#define FID_PATHS_OF_K205 45
#define FID_PATHS_OF_K206 46
#define FID_PATHS_OF_K207 47
#define FID_PATHS_OF_K208 48
#define FID_PATHS_OF_K209 49
#define FID_IO_PURE 50
#define FID_STRING_APPEND 51
#define FID_STRING_APPEND_K275 52
#define FID_IO_BIND 53
#define FID_IO_BIND_C316 54
#define FID_IO_BIND_K317 55
#define FID_MAIN 56
#define FID_MAIN_C319 57
#define FID_MAIN_C320 58
#define FID_MAIN_K321 59
#define FID_MAIN_C322 60
#define FID_MAIN_C323 61
#define FID_MAIN_C324 62
#define FID_MAIN_C325 63
#define FID_MAIN_C326 64
#define FID_MAIN_C327 65
#define FID_MAIN_K328 66
#define FID_MAIN_K329 67
#define FID_MAIN_K330 68
#define FID_MAIN_C331 69
#define FID_MAIN_C332 70
#define FID_MAIN_K333 71
#define FID_MAIN_K334 72
#define FID_MAIN_C335 73
#define FID_MAIN_C336 74
#define FID_MAIN_K337 75
#define FID_MAIN_C338 76
#define FID_MAIN_C339 77
#define FID_MAIN_C340 78
#define FID_MAIN_C341 79
#define FID_MAIN_K342 80
#define FID_MAIN_K343 81
#define FID_MAIN_C344 82
#define FID_MAIN_C345 83
#define FID_MAIN_K346 84
#define FID_MAIN_K347 85
#define FID_MAIN_C348 86
#define FID_MAIN_C349 87
#define FID_MAIN_K350 88
#define FID_MAIN_C351 89
#define FID_MAIN_C352 90
#define FID_MAIN_C353 91
#define FID_MAIN_C354 92
#define FID_IO_ARGS 93
#define FID_FILE_OPEN 94
#define FID_FILE_SIZE 95
#define FID_FILE_READ 96
#define FID_FILE_CLOSE 97
#define FID_IO_PRINT 98
#define FID_IO_NOW 99
#define FID_FILE_WRITE_BYTES 100
#define FID_FILE_READ_BYTES 101
#define FID_IO_EMIT 102
#define FID_CLO_APPLY 103
#define FID_EXIT 104
#define FID_ENTER 105
CONSTV u8 FID_ARITY_T[] = { 4, 2, 2, 2, 2, 6, 5, 5, 6, 4, 2, 2, 1, 1, 2, 2, 1, 2, 3, 3, 2, 4, 1, 1, 2, 3, 2, 1, 1, 1, 3, 2, 1, 2, 1, 2, 3, 4, 3, 4, 1, 1, 1, 1, 3, 4, 5, 5, 5, 5, 2, 2, 2, 3, 3, 2, 0, 1, 1, 2, 3, 2, 3, 2, 6, 2, 3, 3, 4, 5, 4, 4, 4, 5, 4, 3, 4, 3, 4, 4, 3, 3, 4, 4, 3, 3, 4, 3, 2, 3, 2, 3, 1, 1, 3, 2, 3, 2, 2, 1, 3, 3, 1, 2 };
CONSTV u8 FID_FLAG_T[] = { 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 2, 0, 2, 0, 2, 2, 2, 2, 2, 2, 0, 2, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0 };
CONSTV u8 FID_RESW_T[] = { 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
CONSTV u8 CID_ARITY_T[] = { 2, 0, 2, 2, 1, 2, 1, 1, 0, 1, 0, 0, 0, 0, 2, 1, 2, 4, 57, 53, 0, 0, 0, 3, 7, 5, 4, 9, 64, 5, 1, 3, 2, 3, 2, 2, 1, 3, 3 };
#define STAT_LEN 112

#define IO_HOTS 1

#define WL_RESW 1
#define BANGS   0

#define WL_BANK Term r0, r1, r2, r3, r4, r5;

#define WL_LOAD(A, N) \
  do { \
    if ((N) <= 0) break; r0 = e.mem[(A) + 0]; \
    if ((N) <= 1) break; r1 = e.mem[(A) + 1]; \
    if ((N) <= 2) break; r2 = e.mem[(A) + 2]; \
    if ((N) <= 3) break; r3 = e.mem[(A) + 3]; \
    if ((N) <= 4) break; r4 = e.mem[(A) + 4]; \
    if ((N) <= 5) break; r5 = e.mem[(A) + 5]; \
  } while (0);

#define WL_LAST(X) \
  switch (war) { \
    case 0: r0 = (X); \
      break; \
    case 1: r1 = (X); \
      break; \
    case 2: r2 = (X); \
      break; \
    case 3: r3 = (X); \
      break; \
    case 4: r4 = (X); \
      break; \
    case 5: r5 = (X); \
      break; \
  }

#define WL_SAVE(V) (V)[0] = r0;

#define WL_TAKE(V) r0 = (V)[0];

#define WL_SIG Env e, Stk sp, u32 seq, u32 rn, Term r0, Term r1, Term r2, Term r3, Term r4, Term r5

#define WL_ALL e, sp, seq, rn, r0, r1, r2, r3, r4, r5

#define WL_TABLE WL_X(FID_OUT_DONE2) WL_X(FID_RUN_ONE) WL_X(FID_RUN_ONE_K118) WL_X(FID_LIST_APPEND) WL_X(FID_LIST_APPEND_K129) WL_X(FID_READ_TICKER_BYTES_C133) WL_X(FID_READ_TICKER_BYTES_C134) WL_X(FID_READ_TICKER_BYTES_C135) WL_X(FID_RUN_ALL) WL_X(FID_RUN_ALL_K145) WL_X(FID_RUN_ALL_K146) WL_X(FID_RUN_ALL_J145) WL_X(FID_RUN_ALL_K148) WL_X(FID_LIST_CONCAT) WL_X(FID_LIST_CONCAT_K150) WL_X(FID_IO_TRY_C153) WL_X(FID_IO_TRY_C154) WL_X(FID_IO_PASS_C155) WL_X(FID_IO_PASS_C156) WL_X(FID_READ_TICKER_SIZED_C160) WL_X(FID_READ_TICKER_SIZED_C161) WL_X(FID_NAT_SHOW_FIN) WL_X(FID_STRING_LENGTH) WL_X(FID_STRING_LENGTH_K165) WL_X(FID_STRING_SPLIT) WL_X(FID_STRING_SPLIT_K168) WL_X(FID_MAIN_WRITTEN_C185) WL_X(FID_MAIN_WRITTEN_C186) WL_X(FID_READ_ALL) WL_X(FID_READ_ALL_C188) WL_X(FID_READ_ALL_C189) WL_X(FID_READ_TICKER_C190) WL_X(FID_READ_TICKER_C191) WL_X(FID_READ_TICKER_C192) WL_X(FID_READ_TICKER_C193) WL_X(FID_READ_ALL_C194) WL_X(FID_READ_ALL_K195) WL_X(FID_READ_ALL_C196) WL_X(FID_READ_ALL_C197) WL_X(FID_READ_ALL_C198) WL_X(FID_NAT_SHOW) WL_X(FID_LIST_LENGTH) WL_X(FID_LIST_LENGTH_K201) WL_X(FID_STRING_LINES) WL_X(FID_PATHS_OF) WL_X(FID_PATHS_OF_K205) WL_X(FID_PATHS_OF_K206) WL_X(FID_PATHS_OF_K207) WL_X(FID_PATHS_OF_K208) WL_X(FID_PATHS_OF_K209) WL_X(FID_IO_PURE) WL_X(FID_STRING_APPEND) WL_X(FID_STRING_APPEND_K275) WL_X(FID_IO_BIND) WL_X(FID_IO_BIND_C316) WL_X(FID_IO_BIND_K317) WL_X(FID_MAIN) WL_X(FID_MAIN_C319) WL_X(FID_MAIN_C320) WL_X(FID_MAIN_K321) WL_X(FID_MAIN_C322) WL_X(FID_MAIN_C323) WL_X(FID_MAIN_C324) WL_X(FID_MAIN_C325) WL_X(FID_MAIN_C326) WL_X(FID_MAIN_C327) WL_X(FID_MAIN_K328) WL_X(FID_MAIN_K329) WL_X(FID_MAIN_K330) WL_X(FID_MAIN_C331) WL_X(FID_MAIN_C332) WL_X(FID_MAIN_K333) WL_X(FID_MAIN_K334) WL_X(FID_MAIN_C335) WL_X(FID_MAIN_C336) WL_X(FID_MAIN_K337) WL_X(FID_MAIN_C338) WL_X(FID_MAIN_C339) WL_X(FID_MAIN_C340) WL_X(FID_MAIN_C341) WL_X(FID_MAIN_K342) WL_X(FID_MAIN_K343) WL_X(FID_MAIN_C344) WL_X(FID_MAIN_C345) WL_X(FID_MAIN_K346) WL_X(FID_MAIN_K347) WL_X(FID_MAIN_C348) WL_X(FID_MAIN_C349) WL_X(FID_MAIN_K350) WL_X(FID_MAIN_C351) WL_X(FID_MAIN_C352) WL_X(FID_MAIN_C353) WL_X(FID_MAIN_C354) WL_X(FID_IO_ARGS) WL_X(FID_FILE_OPEN) WL_X(FID_FILE_SIZE) WL_X(FID_FILE_READ) WL_X(FID_FILE_CLOSE) WL_X(FID_IO_PRINT) WL_X(FID_IO_NOW) WL_X(FID_FILE_WRITE_BYTES) WL_X(FID_FILE_READ_BYTES) WL_X(FID_IO_EMIT) WL_X(FID_CLO_APPLY) WL_X(FID_EXIT)
#define MAIN_FID FID_MAIN
#define MAIN_PURE 0

#define TAB_AT(T, S, I) T[S < I ? S : I]

// Fid
// ===

#define fid_arity(x) ((u32)FID_ARITY_T[x])

#define fid_bangs(x) ((bool)(FID_FLAG_T[x] & 1))

#define fid_nofk(x) ((bool)(FID_FLAG_T[x] & 2))

#define fid_seqk(x) (fid_resw(x) != 0)

#define fid_resw(x) ((u32)FID_RESW_T[x])

// Cid
// ===

#define cid_arity(x) ((u32)CID_ARITY_T[x])

// A32
// ===

#ifdef __METAL_VERSION__

// via a volatile local: else the M1 backend folds the zext into the atomic
// load, cannot legalize it, and the pipeline build dies
#define a32_load(p)      \
  ({ volatile thread u32 _a32v = atomic_load_explicit(A32(p), RLX); _a32v; })
#define a32_store(p, v)  atomic_store_explicit(A32(p), v, RLX)
#define a32_add(p, v)    atomic_fetch_add_explicit(A32(p), v, RLX)
#define a32_sub(p, v)    atomic_fetch_sub_explicit(A32(p), v, RLX)
#define a32_swp(p, e, v) \
  atomic_compare_exchange_weak_explicit(A32(p), e, v, RLX, RLX)

#elif defined(__CUDACC_RTC__)

#define a32_load(p)     (*(volatile u32*)(p))
#define a32_store(p, v) (*(volatile u32*)(p) = (v))
#define a32_add(p, v)   atomicAdd((u32*)(p), v)
#define a32_sub(p, v)   atomicSub((u32*)(p), v)

INLINE bool a32_swp(DEV u32* p, u32* e, u32 v) {
  u32 x = *e;
  *e = atomicCAS((u32*)p, x, v);
  return *e == x;
}

#endif

#if DEVICE

INLINE u32 a32_sub_rel(DEV u32* p, u32 v) {
  FENCE();
  return a32_sub(p, v);
}

INLINE void a32_store_rel(DEV u32* p, u32 v) {
  FENCE();
  a32_store(p, v);
}

INLINE u32 a32_load_acq(DEV u32* p) {
  u32 v = a32_load(p);
  FENCE();
  return v;
}

#define a32_acq(p) FENCE()

INLINE bool a32_cas(DEV u32* p, THR u32* e, u32 v) {
  FENCE();
  bool ok = a32_swp(p, e, v);
  FENCE();
  return ok;
}

#else

#define a32_load(p)         __atomic_load_n(p, __ATOMIC_RELAXED)
#define a32_store(p, v)     __atomic_store_n(p, v, __ATOMIC_RELAXED)
#define a32_add(p, v)       __atomic_fetch_add(p, v, __ATOMIC_RELAXED)
#define a32_sub(p, v)       __atomic_fetch_sub(p, v, __ATOMIC_RELAXED)
#define a32_sub_rel(p, v)   __atomic_fetch_sub(p, v, __ATOMIC_RELEASE)
#define a32_store_rel(p, v) __atomic_store_n(p, v, __ATOMIC_RELEASE)
#define a32_load_acq(p)     __atomic_load_n(p, __ATOMIC_ACQUIRE)
#define a32_acq(p)          ((void)a32_load_acq(p))

INLINE bool a32_cas(u32* p, u32* e, u32 v) {
  return __atomic_compare_exchange_n(
    p, e, v, 1, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

#endif

#define a32_at(H, word) ((DEV u32*)&(H)[word])

// Err
// ===

#if DEVICE

INLINE void err_post(Corpus H, Err code) {
  u32 seen = 0;
  while (seen == 0 && !a32_cas(a32_at(H, H_ERROR_CODE), &seen, code)) {}
}

#else

static const char* ERR_TEXT[] = { "",
  "runtime fail-stop",
  "runtime fail-stop",
  "out of memory: run again with a bigger span, as in --gpu 8GB",
  "a function the device does not hold",
  "a Nat past the largest immediate 2^48-1",
  "runtime fail-stop",
  "memory fault (machine stack overflow?)",
  "an array past the deepest block class 31" };

static void err_fail(const char* msg) {
  fflush(stdout);
  fprintf(stderr, "bend: %s\n", msg);
  _exit(1);
}

static void err_post(Corpus H, Err code) {
  err_fail(ERR_TEXT[code]);
}

static void err_trap(int sig) {
  err_post(NULL, ERR_DEEP);
}

#endif

#define err_seen(H)    (DEVICE && a32_load(a32_at(H, H_ERROR_CODE)) != 0)
#define err_spun(H, n) ((++*(n) & 4095) == 0 && err_seen(H))

#ifdef __METAL_VERSION__
// Metal's atan2 is NaN at the origin; libm answers +-0 or +-pi there
INLINE f32 atan2_c99(f32 y, f32 x) {
  return y == 0.0f && x == x
    ? copysign(signbit(x) ? M_PI_F : 0.0f, y) : atan2(y, x);
}
#define sqrt  precise::sqrt
#define exp   precise::exp
#define log   precise::log
#define log2  precise::log2
#define log10 precise::log10
#define sin   precise::sin
#define cos   precise::cos
#define tan   precise::tan
#define pow   precise::pow
#define fmod  precise::fmod
#define atan2 atan2_c99
#endif

#define U32_BIN(a, o, b) ((u64)((u32)(a) o (u32)(b)))

// Metal folds a constant dividend within 128 of 2^32 through an f32: divide
// its half, then fix the odd bit.
#define U32_QUO(a, b) \
  ((a) / 2 / (b) * 2 + ((a) - (a) / 2 / (b) * 2 * (b) >= (b)))

INLINE f32 f32_unbox(u64 x) {
  union { u32 u; f32 f; } p = { (u32)x };
  return p.f;
}

INLINE u64 f32_rewrap(f32 x) {
  union { f32 f; u32 u; } p = { x };
  return p.u;
}

INLINE U32 f32_to_u32(U32 a) {
  f32 v = f32_unbox(a);
  return v >= 0.0f && v < 4294967296.0f ? (u32)v : 0;
}

INLINE Nat nat_chk(Env e, Nat n) {
  if (n > NAT_IMM) {
    err_post(e.mem, ERR_NATS);
    return NAT_IMM;
  }
  return n;
}

INLINE Nat nat_mul(Env e, Nat a, Nat b) {
  return nat_chk(e, b != 0 && a > NAT_IMM / b ? NAT_IMM + 1 : a * b);
}

#if DEVICE

#define f32_show(e, x) (err_post(e.mem, ERR_FIDS), 0)
#define f32_read(e, s) (err_post(e.mem, ERR_FIDS), 0)

#else

static Term f32_show(Env e, Term x);
static Term f32_read(Env e, Term s);

#endif

// Cls
// ===

INLINE Cls cls_fit(u32 words) {
  return words > 1 ? 32 - CLZ(words - 1) : 0;
}

// Bank
// ====

// One stack of exact generations per class; 2 heap_words / max(CHUNK,
// 2^c) entries cover the old ones plus a pass of returns. The host
// pops and pushes at rd under bank_lock; a device pass pops down from
// rd and pushes above top, and the host then compacts [top, wr) onto
// rd, so a pass never sees what it handed.

#define bank_at(H, c) ((DEV Bank*)((H) + H_BANK) + (c))

INLINE Loc bank_pop(Corpus H, Cls c) {
  DEV Bank* b = bank_at(H, c);
  Loc got = 0;
  LOCK(bank_lock);
  u32 t = a32_sub(&b->rd, 1);
  if ((int)t > 0) {
    got = H[b->off + t - 1];
  } else {
    a32_add(&b->rd, 1);
  }
  if (!DEVICE) {
    b->wr = b->top = b->rd;
  }
  UNLOCK(bank_lock);
  return got;
}

INLINE void bank_push(Corpus H, Cls c, Loc head) {
  DEV Bank* b = bank_at(H, c);
  LOCK(bank_lock);
  H[b->off + a32_add(&b->wr, 1)] = head;
  if (!DEVICE) {
    b->rd = b->top = b->wr;
  }
  UNLOCK(bank_lock);
}

// Heap
// ====

// Per lane and class (a tile row on the device): HOT, a LIFO chain of
// free slots (word 0 the head it replaced); LEN, its exact length in
// words, off the chain; on the host COLD, one generation. A free is a
// push and an add. A host free at KEEP_WORDS (a slot for a wide class)
// runs heap_hand: COLD to the bank, HOT parked as COLD, generations
// exact. A miss takes COLD, else a bank entry, else a quantum of at
// most a generation, and sets LEN to what it took: no adoption past a
// generation, no list re-aged. A device lane keeps its frees for the
// pass; at the kernel end dev_cut hands its complete generations,
// walking only those. KEEP_WORDS is CAP_WORDS, or CHUNK with the GPU
// (fixed at boot), so a device lane may adopt every host entry.
// Bounds: a host lane and class under 2 max(KEEP_WORDS, 2^c) words, a
// device one under max(CHUNK, 2^c) after each kernel plus its own
// frees within one, bank entries exact. The bump grows only when this
// lane's HOT and COLD and the class's bank are empty. A zero row is an
// empty lane.

#define ALC_AT(e, i)   (e).alc[(i) * LANE_STEP]
#define ALC_LEN(e, c)  ALC_AT(e, ALC_WORDS + (c))
#define ALC_COLD(e, c) ALC_AT(e, 2 * ALC_WORDS + (c))
#define KEEP(c)        (KEEP_WORDS >> (c) ? KEEP_WORDS >> (c) : 1)

OUTLINE void heap_hand(Env e, Cls cls) {
  Loc cold = ALC_COLD(e, cls);
  if (cold) {
    bank_push(e.mem, cls, cold);
  }
  ALC_COLD(e, cls) = ALC_AT(e, cls);
  ALC_AT(e, cls)   = 0;
  ALC_LEN(e, cls)  = 0;
}

OUTLINE Loc heap_alloc_miss(Env e, Cls cls) {
  Corpus H = e.mem;
  Loc  got = 0;
  if (!DEVICE) {
    got = ALC_COLD(e, cls);
    ALC_COLD(e, cls) = 0;
  }
  if (!got) {
    got = bank_pop(H, cls);
  }
  u32 n = got ? KEEP(cls) : cls < NCLS ? QUANTUM >> cls : 1;
  if (!got) {
    u32 pages = (n << cls) >> PAGE_BITS;
    u32 p     = a32_add(a32_at(H, H_BUMP), pages);
    if ((u64)p + pages > a32_load(a32_at(H, H_CAP))) {
      err_post(H, ERR_HEAP);
      p = 0;
    }
    got = HEAP_OFF + ((u64)p << PAGE_BITS);
    for (u32 i = 1; i <= n; i += 1) {
      H[got + ((u64)(i - 1) << cls)] = i < n ? got + ((u64)i << cls) : 0;
    }
  }
  ALC_AT(e, cls)  = H[got];
  ALC_LEN(e, cls) = (u64)(n - 1) << cls;
  return got;
}

INLINE Loc heap_alloc(Env e, Cls cls) {
  Loc h = ALC_AT(e, cls);
  if (h) {
    ALC_AT(e, cls)   = e.mem[h];
    ALC_LEN(e, cls) -= 1ull << cls;
    return h;
  }
  return heap_alloc_miss(e, cls);
}

INLINE void heap_free(Env e, Cls cls, Loc loc) {
  if (err_seen(e.mem)) {
    return;
  }
  e.mem[loc]       = ALC_AT(e, cls);
  ALC_AT(e, cls)   = loc;
  ALC_LEN(e, cls) += 1ull << cls;
  if (!DEVICE && ALC_LEN(e, cls) >= KEEP_WORDS) {
    heap_hand(e, cls);
  }
}

// Spare
// =====

INLINE void spare_free(Env e, Cls cls, Loc loc) {
  if (loc >= HEAP_OFF) {
    heap_free(e, cls, loc);
  }
}

// Term
// ====

#define term_make(tag, aux, loc) \
  (((u64)(tag) << 56) | ((u64)(aux) << 40) | (u64)(loc))

#define term_ctr(cid, loc) term_make(TAG_CTR, cid, loc)
#define term_pak(cid, loc) term_make(TAG_PAK, cid, loc)
#define term_clo(fid, loc) term_make(TAG_CLO, fid, loc)
#define term_buf(cls, loc) term_make(TAG_BUF, cls, loc)
#define term_tsk(fid, loc) term_make(TAG_TSK, fid, loc)

INLINE Term term_blk(bool arr, Cls cls, Loc loc) {
  return term_buf(cls, loc) | ((u64)arr << 57);
}

INLINE u64 term_tag(Term t) {
  return (t >> 56) & 0x7f;
}

INLINE bool term_rfc(Term t) {
  return (t & RFC_BIT) != 0;
}

INLINE u64 term_aux(Term t) {
  return (t >> 40) & 0xFFFF;
}

INLINE Loc term_loc(Term t) {
  return t & LOC_MASK;
}

// A static node (below the heap) is trivial, as is a captureless closure.
INLINE bool term_triv(Term t) {
  return term_tag(t) <= TAG_PAK || t == TERM_HOLE || term_loc(t) < HEAP_OFF;
}

OUTLINE Term rfc_wrap(Env e, Term t, u32 cnt) {
  if (term_tag(t) == TAG_CLO || term_tag(t) == TAG_TSK) {
    err_post(e.mem, ERR_RFCS);
    return t;
  }
  Loc r = heap_alloc(e, 0);
  e.mem[r] = ((u64)term_loc(t) << 24) | cnt;
  return (t & ~LOC_MASK) | RFC_BIT | r;
}

INLINE Term rfc_seal(Env e, Term t) {
  if (term_tag(t) != TAG_CTR || term_rfc(t)) {
    return t;
  }
  return rfc_wrap(e, t, 1);
}

INLINE u64 rfc_view(Env e, Loc r) {
  DEV u32* w = a32_at(e.mem, r);
  u64 cell = ((u64)a32_load(w + 1) << 32) | a32_load(w);
  if ((cell & RFC_CNT) == 1) {
    a32_acq(w);
  }
  return cell;
}

INLINE void rfc_bump(Env e, Loc r, u32 k) {
  u32 c = a32_add(a32_at(e.mem, r), k);
  if ((c & RFC_CNT) >= RFC_CNT - k) {
    err_post(e.mem, ERR_RFCS);
  }
}

INLINE Term term_keep(Env e, Term t) {
  if (term_rfc(t)) {
    rfc_bump(e, term_loc(t), 1);
    return t;
  }
  if (term_triv(t)) {
    return t;
  }
  return rfc_wrap(e, t, 2);
}

INLINE Loc term_peek(Env e, Term t) {
  if (term_rfc(t)) {
    return rfc_view(e, term_loc(t)) >> 24;
  }
  return term_loc(t);
}

INLINE Cls blk_cls(Term t) {
  return (u32)term_aux(t) & 31;
}

#define buf_wcls(c) ((c) == 0 ? 0 : (c) - 1)

INLINE Cls blk_span(Term t) {
  Cls c = blk_cls(t);
  return term_tag(t) == TAG_ARR ? c : buf_wcls(c);
}

INLINE void blk_free(Env e, Term t) {
  heap_free(e, blk_span(t), term_loc(t));
}

FAR void term_drop(Env e, Term t) {
  Corpus H = e.mem;
  u64  cur = 0;
  Term c0  = 0;
  u32  step = 0;
  for (;;) {
    if (!term_triv(t) && term_rfc(t)) {
      Loc      r = term_loc(t);
      DEV u32* p = a32_at(H, r);
      if ((a32_sub_rel(p, 1) & RFC_CNT) != 1) {
        t = 0;
      } else {
        a32_acq(p);
        t = (t & ~(RFC_BIT | LOC_MASK)) | (H[r] >> 24);
        heap_free(e, 0, r);
      }
    }
    if (!term_triv(t)) {
      u64 tag = term_tag(t);
      if (tag == TAG_BUF) {
        blk_free(e, t);
      } else {
        u32 aux = (u32)term_aux(t);
        Loc loc = term_loc(t);
        u32 n   = 0;
        Cls cls;
        if (tag == TAG_ARR) {
          cls = 64 | blk_cls(t);
        } else {
          u32 ar;
          if (tag == TAG_CTR) {
            ar = cid_arity(aux);
          } else if (tag == TAG_CLO) {
            ar = fid_arity(aux) - 1;
          } else {
            ar = fid_arity(aux);
          }
          n   = ar;
          cls = cls_fit(tag == TAG_TSK ? ar + 2 : ar);
        }
        c0 = H[loc];
        H[loc] = cur;
        cur = loc | ((u64)n << 48) | ((u64)cls << 56);
      }
    }
    for (;;) {
      if (err_spun(H, &step)) {
        return;
      }
      if (cur == 0) {
        return;
      }
      Loc  loc = cur & LOC_MASK;
      u32  i   = (u8)(cur >> 40);
      u32  n   = (u8)(cur >> 48);
      Cls  cls = (u32)(cur >> 56);
      bool arr = cls > 63;
      u32  j   = i;
      if (arr) {
        cls &= 63;
        n   = 1u << cls;
        if (i == 2) {
          j = (u32)H[loc + 1];
        }
      }
      if (j < n) {
        Term c = j == 0 ? c0 : H[loc + j];
        if (arr && j > 0) {
          H[loc + 1] = j + 1;
        }
        if (!arr || i < 2) {
          cur += 1ull << 40;
        }
        if (!term_triv(c)) {
          t = c;
          break;
        }
      } else {
        u64 up = H[loc];
        heap_free(e, cls, loc);
        cur = up;
      }
    }
  }
}

INLINE void term_sink(Env e, Term t) {
  if (!term_triv(t)) {
    term_drop(e, t);
  }
}

OUTLINE void span_fade(Env e, Term t, Loc src, u32 n) {
  for (u32 j = 0; j < n; j += 1) {
    Term f = e.mem[src + j];
    if (term_rfc(f)) {
      rfc_bump(e, term_loc(f), 1);
    } else if (!term_triv(f)) {
      err_post(e.mem, ERR_RFCS);
    }
  }
  term_drop(e, t);
}

INLINE Loc ctr_take(Env e, Term t, u32 n, THR Term* out) {
  Corpus H = e.mem;
  if (!term_rfc(t)) {
    for (u32 j = 0; j < n; j += 1) {
      out[j] = H[term_loc(t) + j];
    }
    return term_loc(t);
  }
  Loc r    = term_loc(t);
  u64 cell = rfc_view(e, r);
  Loc src  = cell >> 24;
  for (u32 j = 0; j < n; j += 1) {
    out[j] = H[src + j];
  }
  if ((cell & RFC_CNT) == 1) {
    heap_free(e, 0, r);
    return src;
  }
  span_fade(e, t, src, n);
  return 0;
}

INLINE Term term_word(Env e, Term w) {
  u32 x = 0;
  Term t = w;
  for (u32 i = 0; i < 32 && term_aux(t) == CID_WCON; i += 1) {
    Loc l = term_peek(e, t);
    x |= (u32)(e.mem[l] & 1) << i;
    t = e.mem[l + 1];
  }
  term_sink(e, w);
  return x;
}

// Blk
// ===

// A block owns one allocation in its physical class (an ARR of class
// c 2^c Terms in 2^c words, a BUF 2^c u32 in 2^buf_wcls(c) words) and
// blk_free returns it there. A match on ANode is blk_half twice: each
// half allocated in its class and copied, the source freed shallow by
// the high call (its elements moved; the emitter binds the low half
// first). ANode{l, r} is blk_node: the merged class, l and r copied
// and freed shallow. Array.clone is blk_copy: a BUF raw, an ARR's
// elements retained through blk_keep. A match to the leaves copies
// O(n log n) words where a view copied none; get, set, swap, size and
// new open no half.

#define BLK_ALLOC(n, w) \
  Loc n = heap_alloc(e, w); \
  if (err_seen(e.mem)) { \
    return term_buf(0, n); \
  }

INLINE DEV u32a* blk_ptr(Corpus H, Loc loc, u32 i) {
  return (DEV u32a*)(H + loc) + i;
}

INLINE Term blk_read(Corpus H, bool arr, Loc loc, u32 i) {
  if (arr) {
    return H[loc + i];
  }
  return (u64)*blk_ptr(H, loc, i);
}

INLINE void blk_write(Corpus H, bool arr, Loc loc, u32 i, Term v) {
  if (arr) {
    H[loc + i] = v;
  } else {
    *blk_ptr(H, loc, i) = (u32)v;
  }
}

INLINE u32 blk_at(Term a, U32 i, u32 lgs) {
  return ((u32)i & (u32)((1ull << (blk_cls(a) - lgs)) - 1)) << lgs;
}

INLINE Term blk_keep(Env e, Loc at) {
  Term w = e.mem[at];
  Term v = term_keep(e, w);
  if (v != w) {
    e.mem[at] = v;
  }
  return v;
}

OUTLINE Term blk_copy(Env e, Term a) {
  Corpus H = e.mem;
  bool arr = term_tag(a) == TAG_ARR;
  Cls cls = blk_span(a);
  Loc src = term_loc(a);
  BLK_ALLOC(dst, cls)
  for (u64 j = 0; j < (1ull << cls); j += 1) {
    H[dst + j] = arr ? blk_keep(e, src + j) : H[src + j];
  }
  return term_blk(arr, blk_cls(a), dst);
}

INLINE Term blk_node(Env e, Term l, Term r) {
  Corpus H = e.mem;
  bool arr = term_tag(l) == TAG_ARR;
  Cls c = blk_cls(l);
  if (c != blk_cls(r) || c + 1 >= NCLS_ALL) {
    err_post(H, ERR_TAGS);
    return l;
  }
  Loc pl = term_loc(l);
  Loc pr = term_loc(r);
  BLK_ALLOC(n, arr ? c + 1 : c)
  if (!arr && c == 0) {
    H[n] = (u64)*blk_ptr(H, pl, 0) | ((u64)*blk_ptr(H, pr, 0) << 32);
  } else {
    u64 cw = 1ull << blk_span(l);
    for (u64 w = 0; w < cw; w += 1) {
      H[n + w]      = H[pl + w];
      H[n + cw + w] = H[pr + w];
    }
  }
  blk_free(e, l);
  blk_free(e, r);
  return term_blk(arr, c + 1, n);
}

INLINE Term blk_half(Env e, Term a, u32 hi) {
  Corpus H = e.mem;
  bool arr = term_tag(a) == TAG_ARR;
  Cls c = blk_cls(a);
  if (c == 0) {
    err_post(H, ERR_TAGS);
    return a;
  }
  c -= 1;
  Cls cw = arr ? c : buf_wcls(c);
  BLK_ALLOC(n, cw)
  if (!arr && c == 0) {
    H[n] = (u64)*blk_ptr(H, term_loc(a), hi);
  } else {
    Loc src = term_loc(a) + ((u64)hi << cw);
    for (u64 w = 0; w < (1ull << cw); w += 1) {
      H[n + w] = H[src + w];
    }
  }
  if (hi) {
    blk_free(e, a);
  }
  return term_blk(arr, c, n);
}

INLINE Term blk_new(Env e, bool arr, Nat d, u32 lgs, u32 n, THR Term* v) {
  Corpus H = e.mem;
  if (d + lgs > 31) {
    err_post(H, ERR_ARRS);
    d = 0;
  }
  Cls c = (u32)d + lgs;
  BLK_ALLOC(l, arr ? c : buf_wcls(c))
  for (u32 j = 0; j < n; j += 1) {
    Term w = v[j];
    if (arr && d > 0 && !term_triv(w)) {
      if (d >= 24) {
        err_post(H, ERR_RFCS);
      } else if (term_rfc(w)) {
        rfc_bump(e, term_loc(w), (1u << d) - 1);
      } else {
        w = rfc_wrap(e, w, 1u << d);
      }
    }
    v[j] = w;
  }
  for (u64 i = 0; i < (1ull << c); i += 1) {
    blk_write(H, arr, l, (u32)i, i % (1u << lgs) < n ? v[i % (1u << lgs)] : 0);
  }
  return term_blk(arr, c, l);
}

// Ring
// ====

// planes LANES wide: a smaller bag has deeper rings in the same region
#define ring_word(H, r, w) ((H) + RING_OFF + (w) * LANES + (r))
#define ring_slot(H, r, p) ring_word(H, r, (p) & (RING_LEN - 1))
#define ring_get(H, r)     ((DEV u32*)ring_word(H, r, RING_LEN))
#define ring_put(H, r)     ((DEV u32*)ring_word(H, r, RING_LEN + 1))

INLINE u32 ring_lap(u32 pos) {
  return ~(u32)(pos / RING_LEN) & 1;
}

INLINE void ring_push(Corpus H, Ring r, Term tsk) {
  u32 pos = a32_add(ring_put(H, r), 1);
  if (pos - a32_load(ring_get(H, r)) >= RING_LEN) {
    err_post(H, ERR_RING);
    return;
  }
  DEV u32* lo = (DEV u32*)ring_slot(H, r, pos);
  a32_store(lo, (u32)tsk);
  a32_store_rel(lo + 1, (u32)(tsk >> 32) | (ring_lap(pos) << 31));
}

INLINE Ring ring_flip(u32 i) {
  return (i % CUBE_T << CUBE_LOG) + i / CUBE_T;
}

#define ring_pick(b, s, c) ((b) + (s) * (g32_add(c, 1) & (CUBE_T - 1)))

// Task
// ====

INLINE Loc task_node(Env e, Fid fid, Term cont, u32 idx, u32 rem) {
  u32 ar  = fid_arity(fid);
  Loc loc = heap_alloc(e, cls_fit(ar + 2));
  for (u32 i = 0; rem && i < ar; i += 1) {
    e.mem[loc + i] = TERM_HOLE;
  }
  e.mem[loc + ar]     = cont;
  e.mem[loc + ar + 1] = ((u64)idx << 32) | rem;
  return loc;
}

INLINE Loc task_tail(Term t) {
  return term_loc(t) + fid_arity((u32)term_aux(t));
}

INLINE Term task_deliver(Corpus H, Term cont, u32 idx, THR Term* v, u32 n) {
  Loc at = cont == TERM_HOLE ? H_ROOT_WORD : term_loc(cont) + idx;
  for (u32 j = 0; j < WL_RESW; j += 1) {
    if (j < n) {
      H[at + j] = v[j];
    }
  }
  if (cont == TERM_HOLE) {
    a32_store_rel(a32_at(H, H_ROOT_DONE), n + 1);
    return 0;
  }
  Loc tl = task_tail(cont);
  if (a32_sub_rel(a32_at(H, tl + 1), 1) == 1) {
    a32_acq(a32_at(H, tl + 1));
    return cont;
  }
  return 0;
}

INLINE void task_deal(Corpus H, Term join, u32 base, u32 stride, Cur cur) {
  Loc loc = term_loc(join);
  u32 ar  = fid_arity((u32)term_aux(join));
  u32 g   = 0;
  if (stride == 0) {
    u32 rem = (u32)H[loc + ar + 1];
    g = a32_add(a32_at(H, H_CURSOR), rem);
  }
  for (u32 i = 0; i < ar; i += 1) {
    Term k = H[loc + i];
    if (term_tag(k) == TAG_TSK) {
      H[loc + i] = TERM_HOLE;
      Ring to;
      if (stride != 0) {
        to = ring_pick(base, stride, cur);
      } else {
        to = ring_flip(g & (u32)(LANES - 1));
        g += 1;
      }
      ring_push(H, to, k);
    }
  }
}

// Root
// ====

INLINE bool root_done(Corpus H) {
  return a32_load_acq(a32_at(H, H_ROOT_DONE)) != 0;
}

static u32 root_take(Corpus H, THR Term* v) {
  u32 n = a32_load_acq(a32_at(H, H_ROOT_DONE)) - 1;
  for (u32 j = 0; j < n; j += 1) {
    v[j] = H[H_ROOT_WORD + j];
  }
  a32_store(a32_at(H, H_ROOT_DONE), 0);
  return n;
}

// Spins
// =====

CONSTV u64 STAT_IMG[] = { 110ull, term_pak(CID_SNIL, 0), 105ull, term_ctr(CID_SCON, STAT_OFF + 0), 98ull, term_ctr(CID_SCON, STAT_OFF + 2), 46ull, term_ctr(CID_SCON, STAT_OFF + 4), 115ull, term_ctr(CID_SCON, STAT_OFF + 6), 101ull, term_ctr(CID_SCON, STAT_OFF + 8), 116ull, term_ctr(CID_SCON, STAT_OFF + 10), 97ull, term_ctr(CID_SCON, STAT_OFF + 12), 116ull, term_ctr(CID_SCON, STAT_OFF + 14), 115ull, term_ctr(CID_SCON, STAT_OFF + 16), 32ull, term_ctr(CID_SCON, STAT_OFF + 18), 101ull, term_ctr(CID_SCON, STAT_OFF + 20), 116ull, term_ctr(CID_SCON, STAT_OFF + 22), 111ull, term_ctr(CID_SCON, STAT_OFF + 24), 114ull, term_ctr(CID_SCON, STAT_OFF + 26), 119ull, term_ctr(CID_SCON, STAT_OFF + 28), 114ull, term_pak(CID_SNIL, 0), term_pak(CID_SNIL, 0), term_pak(CID_NIL, 0), 32ull, term_pak(CID_SNIL, 0), 58ull, term_ctr(CID_SCON, STAT_OFF + 36), 115ull, term_ctr(CID_SCON, STAT_OFF + 38), 109ull, term_ctr(CID_SCON, STAT_OFF + 40), 32ull, term_ctr(CID_SCON, STAT_OFF + 42), 100ull, term_ctr(CID_SCON, STAT_OFF + 44), 108ull, term_ctr(CID_SCON, STAT_OFF + 46), 111ull, term_ctr(CID_SCON, STAT_OFF + 48), 102ull, term_ctr(CID_SCON, STAT_OFF + 50), 47ull, term_ctr(CID_SCON, STAT_OFF + 18), 119ull, term_pak(CID_SNIL, 0), 47ull, term_pak(CID_SNIL, 0), 114ull, term_ctr(CID_SCON, STAT_OFF + 40), 101ull, term_ctr(CID_SCON, STAT_OFF + 60), 107ull, term_ctr(CID_SCON, STAT_OFF + 62), 99ull, term_ctr(CID_SCON, STAT_OFF + 64), 105ull, term_ctr(CID_SCON, STAT_OFF + 66), 116ull, term_ctr(CID_SCON, STAT_OFF + 68), 68ull, term_pak(CID_SNIL, 0), 47ull, term_ctr(CID_SCON, STAT_OFF + 72), 97ull, term_ctr(CID_SCON, STAT_OFF + 74), 116ull, term_ctr(CID_SCON, STAT_OFF + 76), 97ull, term_ctr(CID_SCON, STAT_OFF + 78), 100ull, term_ctr(CID_SCON, STAT_OFF + 80), 47ull, term_ctr(CID_SCON, STAT_OFF + 82), 100ull, term_ctr(CID_SCON, STAT_OFF + 84), 110ull, term_ctr(CID_SCON, STAT_OFF + 86), 101ull, term_ctr(CID_SCON, STAT_OFF + 88), 98ull, term_ctr(CID_SCON, STAT_OFF + 90), 116ull, term_pak(CID_SNIL, 0), 120ull, term_ctr(CID_SCON, STAT_OFF + 94), 116ull, term_ctr(CID_SCON, STAT_OFF + 96), 46ull, term_ctr(CID_SCON, STAT_OFF + 98), 116ull, term_ctr(CID_SCON, STAT_OFF + 100), 115ull, term_ctr(CID_SCON, STAT_OFF + 102), 105ull, term_ctr(CID_SCON, STAT_OFF + 104), 108ull, term_ctr(CID_SCON, STAT_OFF + 106), 47ull, term_ctr(CID_SCON, STAT_OFF + 108) };

INLINE Term spin_1(Env e, THR Term* o, u32 r0, Term r1, Term r2) {
  u32 wpoll = 0;
  Term v_4 = 0;
  u32 c_0 = r0;
  Term a_1 = r1;
  Term b_2 = r2;
  WL_SPIN
    if (c_0 == 0) {
      term_sink(e, a_1);
      v_4 = b_2;
    } else {
      term_sink(e, b_2);
      v_4 = a_1;
    }
  break;
  }
  o[0] = v_4;
  return 1;
}

INLINE Term spin_0(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_2 = 0;
  u32 a_0 = r0;
  u32 b_1 = r1;
  WL_SPIN
    Term v_3 = 0;
    Term o_0[1];
    if (spin_1(e, o_0, ((u64)(f32_unbox(a_0) < f32_unbox(b_1))), b_1, a_0) == 0) {
      return 0;
    }
    v_3 = o_0[0];
    v_2 = v_3;
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_2(Env e, THR Term* o, u32 r0, u32 r1, u32 r2) {
  u32 wpoll = 0;
  u32 v_7 = 0;
  u32 b_3 = r0;
  u32 x_0 = r1;
  u32 y_0 = r2;
  WL_SPIN
    if (b_3 == 1) {
      v_7 = x_0;
    } else {
      v_7 = y_0;
    }
  break;
  }
  o[0] = v_7;
  return 1;
}

INLINE Term spin_3(Env e, THR Term* o, u32 r0, u32 r1, u32 r2) {
  u32 wpoll = 0;
  u32 v_14 = 0;
  u32 b_4 = r0;
  u32 x_1 = r1;
  u32 y_1 = r2;
  WL_SPIN
    if (b_4 == 1) {
      v_14 = x_1;
    } else {
      v_14 = y_1;
    }
  break;
  }
  o[0] = v_14;
  return 1;
}

INLINE Term spin_4(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9) {
  u32 wpoll = 0;
  Term v_6 = 0;
  u32 v_7 = 0;
  u32 v_8 = 0;
  u32 v_9 = 0;
  u32 v_10 = 0;
  u32 v_11 = 0;
  Term r_6 = r0;
  u32 r_7 = r1;
  u32 b_1 = r2;
  u32 lo_t_1 = r3;
  u32 hi_t_1 = r4;
  u32 s_0 = r5;
  u32 s_1 = r6;
  u32 s_2 = r7;
  u32 s_3 = r8;
  u32 s_4 = r9;
  WL_SPIN
    u32 v_12 = 0;
    u32 v_13 = 0;
    Term o_0[1];
    if (spin_0(e, o_0, r_7, 0ull) == 0) {
      return 0;
    }
    v_13 = o_0[0];
    v_12 = v_13;
    u32 is_max_0 = ((u64)(f32_unbox(v_12) > f32_unbox(s_3)));
    u32 v_14 = 0;
    u32 v_15 = 0;
    Term o_1[1];
    if (spin_2(e, o_1, U32_BIN(b_1, <, lo_t_1), v_12, 0ull) == 0) {
      return 0;
    }
    v_15 = o_1[0];
    v_14 = v_15;
    u32 v_16 = 0;
    u32 v_17 = 0;
    Term o_2[1];
    if (spin_2(e, o_2, U32_BIN(b_1, >, hi_t_1), v_12, 0ull) == 0) {
      return 0;
    }
    v_17 = o_2[0];
    v_16 = v_17;
    u32 v_18 = 0;
    u32 v_19 = 0;
    Term o_3[1];
    if (spin_2(e, o_3, is_max_0, v_12, s_3) == 0) {
      return 0;
    }
    v_19 = o_3[0];
    v_18 = v_19;
    u32 v_20 = 0;
    u32 v_21 = 0;
    Term o_4[1];
    if (spin_3(e, o_4, is_max_0, b_1, s_4) == 0) {
      return 0;
    }
    v_21 = o_4[0];
    v_20 = v_21;
    v_6 = r_6;
    v_7 = f32_rewrap(f32_unbox(s_0) + f32_unbox(v_12));
    v_8 = f32_rewrap(f32_unbox(s_1) + f32_unbox(v_14));
    v_9 = f32_rewrap(f32_unbox(s_2) + f32_unbox(v_16));
    v_10 = v_18;
    v_11 = v_20;
  break;
  }
  o[0] = v_6;
  o[1] = v_7;
  o[2] = v_8;
  o[3] = v_9;
  o[4] = v_10;
  o[5] = v_11;
  return 1;
}

INLINE Term spin_5(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  u32 v_2 = 0;
  u32 b_0 = r0;
  WL_SPIN
    if (b_0 == 1) {
      v_2 = 1ull;
    } else {
      v_2 = 0ull;
    }
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_6(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8) {
  u32 wpoll = 0;
  Term v_12 = 0;
  u32 v_13 = 0;
  u32 v_14 = 0;
  u32 v_15 = 0;
  u32 v_16 = 0;
  u32 v_17 = 0;
  Term r_6 = r0;
  u32 r_7 = r1;
  u32 r_8 = r2;
  u32 r_9 = r3;
  u32 r_10 = r4;
  u32 r_11 = r5;
  u32 b_1 = r6;
  u32 lo_t_1 = r7;
  u32 hi_t_1 = r8;
  WL_SPIN
    Term at_0 = blk_at(r_6, b_1, 0);
    u32 c_0 = blk_read(e.mem, 0, term_loc(r_6), at_0 + 0);
    Term v_18 = 0;
    u32 v_19 = 0;
    u32 v_20 = 0;
    u32 v_21 = 0;
    u32 v_22 = 0;
    u32 v_23 = 0;
    Term o_0[6];
    if (spin_4(e, o_0, r_6, c_0, b_1, lo_t_1, hi_t_1, r_7, r_8, r_9, r_10, r_11) == 0) {
      return 0;
    }
    v_18 = o_0[0];
    v_19 = o_0[1];
    v_20 = o_0[2];
    v_21 = o_0[3];
    v_22 = o_0[4];
    v_23 = o_0[5];
    v_12 = v_18;
    v_13 = v_19;
    v_14 = v_20;
    v_15 = v_21;
    v_16 = v_22;
    v_17 = v_23;
  break;
  }
  o[0] = v_12;
  o[1] = v_13;
  o[2] = v_14;
  o[3] = v_15;
  o[4] = v_16;
  o[5] = v_17;
  return 1;
}

INLINE Term spin_7(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_6 = 0;
  u32 a_0 = r0;
  u32 b_0 = r1;
  WL_SPIN
    if (a_0 == 0) {
      v_6 = 0;
    } else {
      v_6 = b_0;
    }
  break;
  }
  o[0] = v_6;
  return 1;
}

INLINE Term spin_8(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_10 = 0;
  u32 b_1 = r0;
  u32 v_9 = r1;
  WL_SPIN
    if (b_1 == 1) {
      v_10 = v_9;
    } else {
      v_10 = 0ull;
    }
  break;
  }
  o[0] = v_10;
  return 1;
}

INLINE Term spin_9(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_15 = 0;
  u32 a_1 = r0;
  u32 b_2 = r1;
  WL_SPIN
    Term v_16 = 0;
    Term o_5[1];
    if (spin_1(e, o_5, ((u64)(f32_unbox(a_1) < f32_unbox(b_2))), a_1, b_2) == 0) {
      return 0;
    }
    v_16 = o_5[0];
    v_15 = v_16;
  break;
  }
  o[0] = v_15;
  return 1;
}

INLINE Term spin_10(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_20 = 0;
  u32 f_0 = r0;
  u32 m_0 = r1;
  WL_SPIN
    v_20 = U32_BIN(U32_BIN(f_0, &, m_0), ==, m_0);
  break;
  }
  o[0] = v_20;
  return 1;
}

INLINE Term spin_11(Env e, THR Term* o, u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
  u32 wpoll = 0;
  u32 v_32 = 0;
  u32 ss_0 = r0;
  u32 rb_0 = r1;
  u32 hu_0 = r2;
  u32 te_0 = r3;
  u32 mr_0 = r4;
  u32 pc_0 = r5;
  u32 wh_0 = r6;
  WL_SPIN
    u32 v_33 = 0;
    u32 v_34 = 0;
    Term o_13[1];
    if (spin_5(e, o_13, ss_0) == 0) {
      return 0;
    }
    v_34 = o_13[0];
    v_33 = v_34;
    u32 v_35 = 0;
    u32 v_36 = 0;
    Term o_14[1];
    if (spin_5(e, o_14, rb_0) == 0) {
      return 0;
    }
    v_36 = o_14[0];
    v_35 = v_36;
    u32 v_37 = 0;
    u32 v_38 = 0;
    Term o_15[1];
    if (spin_5(e, o_15, hu_0) == 0) {
      return 0;
    }
    v_38 = o_15[0];
    v_37 = v_38;
    u32 v_39 = 0;
    u32 v_40 = 0;
    Term o_16[1];
    if (spin_5(e, o_16, te_0) == 0) {
      return 0;
    }
    v_40 = o_16[0];
    v_39 = v_40;
    u32 v_41 = 0;
    u32 v_42 = 0;
    Term o_17[1];
    if (spin_5(e, o_17, mr_0) == 0) {
      return 0;
    }
    v_42 = o_17[0];
    v_41 = v_42;
    u32 v_43 = 0;
    u32 v_44 = 0;
    Term o_18[1];
    if (spin_5(e, o_18, pc_0) == 0) {
      return 0;
    }
    v_44 = o_18[0];
    v_43 = v_44;
    u32 v_45 = 0;
    u32 v_46 = 0;
    Term o_19[1];
    if (spin_5(e, o_19, wh_0) == 0) {
      return 0;
    }
    v_46 = o_19[0];
    v_45 = v_46;
    v_32 = U32_BIN(U32_BIN(U32_BIN(U32_BIN(U32_BIN(U32_BIN(v_33, +, v_35), +, v_37), +, v_39), +, v_41), +, v_43), +, v_45);
  break;
  }
  o[0] = v_32;
  return 1;
}

INLINE Term spin_12(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3) {
  u32 wpoll = 0;
  Term v_2 = 0;
  Term r_0 = r0;
  u32 r_1 = r1;
  u32 b_1 = r2;
  u32 per_1 = r3;
  WL_SPIN
    Term at_1 = blk_at(r_0, b_1, 0);
    u32 c_1 = blk_read(e.mem, 0, term_loc(r_0), at_1 + 0);
    blk_write(e.mem, 0, term_loc(r_0), at_1 + 0, f32_rewrap(f32_unbox(r_1) + f32_unbox(per_1)));
    v_2 = r_0;
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_13(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_1 = 0;
  u32 up_1 = r0;
  u32 dn_1 = r1;
  WL_SPIN
    if (up_1 == 1) {
      v_1 = 1;
    } else {
      if (dn_1 == 1) {
        v_1 = 2;
      } else {
        v_1 = 0;
      }
    }
  break;
  }
  o[0] = v_1;
  return 1;
}

INLINE Term spin_14(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  u32 v_2 = 0;
  u32 p_0 = r0;
  WL_SPIN
    v_2 = f32_to_u32(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap((f32)log(f32_unbox(p_0)))) / f32_unbox(1000566408ull))) + f32_unbox(1157629952ull)));
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_15(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  u32 v_5 = 0;
  u32 p_1 = r0;
  WL_SPIN
    v_5 = f32_to_u32(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap((f32)log(f32_unbox(p_1)))) / f32_unbox(1000566408ull))) + f32_unbox(1157623808ull)));
  break;
  }
  o[0] = v_5;
  return 1;
}

INLINE Term spin_16(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  u32 v_9 = 0;
  u32 p_2 = r0;
  WL_SPIN
    v_9 = f32_to_u32(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap((f32)log(f32_unbox(p_2)))) / f32_unbox(1000566408ull))) + f32_unbox(1157627904ull)));
  break;
  }
  o[0] = v_9;
  return 1;
}

INLINE Term spin_17(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, Term r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9) {
  u32 wpoll = 0;
  Term v_31 = 0;
  u32 v_32 = 0;
  u32 v_33 = 0;
  u32 v_34 = 0;
  u32 v_35 = 0;
  u32 v_36 = 0;
  Term n_0 = r0;
  u32 b_0 = r1;
  u32 lo_t_0 = r2;
  u32 hi_t_0 = r3;
  Term r_0 = r4;
  u32 r_1 = r5;
  u32 r_2 = r6;
  u32 r_3 = r7;
  u32 r_4 = r8;
  u32 r_5 = r9;
  WL_SPIN
    if (n_0 == 0) {
      v_31 = r_0;
      v_32 = r_1;
      v_33 = r_2;
      v_34 = r_3;
      v_35 = r_4;
      v_36 = r_5;
    } else {
      Term p_3 = (n_0 - 1);
      Term v_37 = 0;
      u32 v_38 = 0;
      u32 v_39 = 0;
      u32 v_40 = 0;
      u32 v_41 = 0;
      u32 v_42 = 0;
      Term v_43 = 0;
      u32 v_44 = 0;
      u32 v_45 = 0;
      u32 v_46 = 0;
      u32 v_47 = 0;
      u32 v_48 = 0;
      Term o_8[6];
      if (spin_6(e, o_8, r_0, r_1, r_2, r_3, r_4, r_5, b_0, lo_t_0, hi_t_0) == 0) {
        return 0;
      }
      v_43 = o_8[0];
      v_44 = o_8[1];
      v_45 = o_8[2];
      v_46 = o_8[3];
      v_47 = o_8[4];
      v_48 = o_8[5];
      v_37 = v_43;
      v_38 = v_44;
      v_39 = v_45;
      v_40 = v_46;
      v_41 = v_47;
      v_42 = v_48;
      r0 = p_3;
      r1 = U32_BIN(b_0, +, 1ull);
      r2 = lo_t_0;
      r3 = hi_t_0;
      r4 = v_37;
      r5 = v_38;
      r6 = v_39;
      r7 = v_40;
      r8 = v_41;
      r9 = v_42;
      n_0 = r0;
      b_0 = r1;
      lo_t_0 = r2;
      hi_t_0 = r3;
      r_0 = r4;
      r_1 = r5;
      r_2 = r6;
      r_3 = r7;
      r_4 = r8;
      r_5 = r9;
      WL_AGAIN(spin_17);
    }
  break;
  }
  o[0] = v_31;
  o[1] = v_32;
  o[2] = v_33;
  o[3] = v_34;
  o[4] = v_35;
  o[5] = v_36;
  return 1;
}

INLINE Term spin_18(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, Term r59, Term r60, Term r61, u32 r62, u32 r63, u32 r64, u32 r65, u32 r66, u32 r67, u32 r68) {
  u32 wpoll = 0;
  Term v_106 = 0;
  Term v_107 = 0;
  Term v_108 = 0;
  Term v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  u32 v_128 = 0;
  u32 v_129 = 0;
  u32 v_130 = 0;
  u32 v_131 = 0;
  u32 v_132 = 0;
  u32 v_133 = 0;
  u32 v_134 = 0;
  u32 v_135 = 0;
  u32 v_136 = 0;
  u32 v_137 = 0;
  u32 v_138 = 0;
  u32 v_139 = 0;
  u32 v_140 = 0;
  u32 v_141 = 0;
  u32 v_142 = 0;
  u32 v_143 = 0;
  u32 v_144 = 0;
  u32 v_145 = 0;
  u32 v_146 = 0;
  u32 v_147 = 0;
  u32 v_148 = 0;
  u32 v_149 = 0;
  u32 v_150 = 0;
  u32 v_151 = 0;
  u32 v_152 = 0;
  u32 v_153 = 0;
  u32 v_154 = 0;
  u32 v_155 = 0;
  u32 v_156 = 0;
  u32 v_157 = 0;
  u32 v_158 = 0;
  u32 v_159 = 0;
  u32 v_160 = 0;
  u32 v_161 = 0;
  u32 v_162 = 0;
  Term r_6 = r0;
  u32 r_7 = r1;
  u32 r_8 = r2;
  u32 r_9 = r3;
  u32 r_10 = r4;
  u32 r_11 = r5;
  u32 st_53 = r6;
  u32 st_54 = r7;
  u32 st_55 = r8;
  u32 st_56 = r9;
  u32 st_57 = r10;
  u32 st_58 = r11;
  u32 st_59 = r12;
  u32 st_60 = r13;
  u32 st_61 = r14;
  u32 st_62 = r15;
  u32 st_63 = r16;
  u32 st_64 = r17;
  u32 st_65 = r18;
  u32 st_66 = r19;
  u32 st_67 = r20;
  u32 st_68 = r21;
  u32 st_69 = r22;
  u32 st_70 = r23;
  u32 st_71 = r24;
  u32 st_72 = r25;
  u32 st_73 = r26;
  u32 st_74 = r27;
  u32 st_75 = r28;
  u32 st_76 = r29;
  u32 st_77 = r30;
  u32 st_78 = r31;
  u32 st_79 = r32;
  u32 st_80 = r33;
  u32 st_81 = r34;
  u32 st_82 = r35;
  u32 st_83 = r36;
  u32 st_84 = r37;
  u32 st_85 = r38;
  u32 st_86 = r39;
  u32 st_87 = r40;
  u32 st_88 = r41;
  u32 st_89 = r42;
  u32 st_90 = r43;
  u32 st_91 = r44;
  u32 st_92 = r45;
  u32 st_93 = r46;
  u32 st_94 = r47;
  u32 st_95 = r48;
  u32 st_96 = r49;
  u32 st_97 = r50;
  u32 st_98 = r51;
  u32 st_99 = r52;
  u32 st_100 = r53;
  u32 st_101 = r54;
  u32 st_102 = r55;
  u32 st_103 = r56;
  u32 st_104 = r57;
  u32 st_105 = r58;
  Term bars_1 = r59;
  Term aux_1 = r60;
  Term out_1 = r61;
  u32 trip_1 = r62;
  u32 flags_1 = r63;
  u32 atr_pct_1 = r64;
  u32 pos_in_1 = r65;
  u32 neg_in_1 = r66;
  u32 c_0 = r67;
  u32 i_1 = r68;
  WL_SPIN
    u32 v_163 = 0;
    u32 v_164 = 0;
    Term o_10[1];
    if (spin_2(e, o_10, ((u64)(f32_unbox(r_7) > f32_unbox(0ull))), f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(r_8) / f32_unbox(r_7))) * f32_unbox(1120403456ull)), 0ull) == 0) {
      return 0;
    }
    v_164 = o_10[0];
    v_163 = v_164;
    u32 v_165 = 0;
    u32 v_166 = 0;
    Term o_11[1];
    if (spin_2(e, o_11, ((u64)(f32_unbox(r_7) > f32_unbox(0ull))), f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(r_9) / f32_unbox(r_7))) * f32_unbox(1120403456ull)), 0ull) == 0) {
      return 0;
    }
    v_166 = o_11[0];
    v_165 = v_166;
    u32 whales_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(v_163) + f32_unbox(st_85))) + f32_unbox(st_86))) / f32_unbox(1077936128ull));
    u32 retail_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(v_165) + f32_unbox(st_87))) + f32_unbox(st_88))) / f32_unbox(1077936128ull));
    u32 retail_shrinking_0 = ((u64)(f32_unbox(retail_0) < f32_unbox(st_89)));
    u32 poc_0 = f32_rewrap((f32)exp(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap((f32)(u32)(r_11))) - f32_unbox(1157627904ull))) + f32_unbox(1056964608ull))) * f32_unbox(1000566408ull)))));
    u32 v_167 = 0;
    u32 v_168 = 0;
    Term o_12[1];
    if (spin_7(e, o_12, ((u64)(f32_unbox(c_0) > f32_unbox(poc_0))), U32_BIN(st_104, ==, 1ull)) == 0) {
      return 0;
    }
    v_168 = o_12[0];
    v_167 = v_168;
    u32 mcdx_0 = ((u64)(f32_unbox(whales_0) >= f32_unbox(1112014848ull)));
    u32 v_169 = 0;
    u32 v_170 = 0;
    Term o_13[1];
    if (spin_8(e, o_13, mcdx_0, 4ull) == 0) {
      return 0;
    }
    v_170 = o_13[0];
    v_169 = v_170;
    u32 v_171 = 0;
    u32 v_172 = 0;
    Term o_14[1];
    if (spin_8(e, o_14, v_167, 512ull) == 0) {
      return 0;
    }
    v_172 = o_14[0];
    v_171 = v_172;
    u32 v_173 = 0;
    u32 v_174 = 0;
    Term o_15[1];
    if (spin_9(e, o_15, f32_rewrap(f32_unbox(whales_0) + f32_unbox(1056964608ull)), 1120403456ull) == 0) {
      return 0;
    }
    v_174 = o_15[0];
    v_173 = v_174;
    Term a_0 = f32_to_u32(v_173);
    Term a_1 = 16ull;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    Term o_16[1];
    if (spin_10(e, o_16, flags_1, 128ull) == 0) {
      return 0;
    }
    v_177 = o_16[0];
    v_176 = v_177;
    u32 v_178 = 0;
    u32 v_179 = 0;
    Term o_17[1];
    if (spin_10(e, o_17, flags_1, 256ull) == 0) {
      return 0;
    }
    v_179 = o_17[0];
    v_178 = v_179;
    u32 v_180 = 0;
    u32 v_181 = 0;
    Term o_18[1];
    if (spin_10(e, o_18, flags_1, 1ull) == 0) {
      return 0;
    }
    v_181 = o_18[0];
    v_180 = v_181;
    u32 v_182 = 0;
    u32 v_183 = 0;
    Term o_19[1];
    if (spin_10(e, o_19, flags_1, 8ull) == 0) {
      return 0;
    }
    v_183 = o_19[0];
    v_182 = v_183;
    u32 v_184 = 0;
    u32 v_185 = 0;
    Term o_20[1];
    if (spin_10(e, o_20, flags_1, 1024ull) == 0) {
      return 0;
    }
    v_185 = o_20[0];
    v_184 = v_185;
    u32 v_186 = 0;
    u32 v_187 = 0;
    Term o_21[1];
    if (spin_7(e, o_21, mcdx_0, retail_shrinking_0) == 0) {
      return 0;
    }
    v_187 = o_21[0];
    v_186 = v_187;
    u32 v_188 = 0;
    Term o_22[1];
    if (spin_11(e, o_22, v_176, v_178, v_180, v_182, v_184, v_167, v_186) == 0) {
      return 0;
    }
    v_188 = o_22[0];
    v_175 = v_188;
    Term a_2 = 24ull;
    u32 flags2_0 = U32_BIN(U32_BIN(U32_BIN(U32_BIN(flags_1, |, v_169), |, v_171), |, (a_1 >= 32 ? 0 : U32_BIN(a_0, <<, a_1))), |, (a_2 >= 32 ? 0 : U32_BIN(v_175, <<, a_2)));
    u32 v_189 = 0;
    u32 v_190 = 0;
    Term o_23[1];
    if (spin_5(e, o_23, ((u64)(f32_unbox(c_0) > f32_unbox(poc_0)))) == 0) {
      return 0;
    }
    v_190 = o_23[0];
    v_189 = v_190;
    Term at_0 = blk_at(aux_1, U32_BIN(i_1, *, 4ull), 0);
    u32 c_1 = blk_read(e.mem, 0, term_loc(aux_1), at_0 + 0);
    blk_write(e.mem, 0, term_loc(aux_1), at_0 + 0, st_60);
    Term at_1 = blk_at(aux_1, U32_BIN(U32_BIN(i_1, *, 4ull), +, 1ull), 0);
    u32 c_2 = blk_read(e.mem, 0, term_loc(aux_1), at_1 + 0);
    blk_write(e.mem, 0, term_loc(aux_1), at_1 + 0, trip_1);
    Term at_2 = blk_at(aux_1, U32_BIN(U32_BIN(i_1, *, 4ull), +, 2ull), 0);
    u32 c_3 = blk_read(e.mem, 0, term_loc(aux_1), at_2 + 0);
    blk_write(e.mem, 0, term_loc(aux_1), at_2 + 0, pos_in_1);
    Term at_3 = blk_at(aux_1, U32_BIN(U32_BIN(i_1, *, 4ull), +, 3ull), 0);
    u32 c_4 = blk_read(e.mem, 0, term_loc(aux_1), at_3 + 0);
    blk_write(e.mem, 0, term_loc(aux_1), at_3 + 0, neg_in_1);
    Term at_4 = blk_at(out_1, U32_BIN(i_1, *, 2ull), 0);
    u32 c_5 = blk_read(e.mem, 0, term_loc(out_1), at_4 + 0);
    blk_write(e.mem, 0, term_loc(out_1), at_4 + 0, flags2_0);
    Term at_5 = blk_at(out_1, U32_BIN(U32_BIN(i_1, *, 2ull), +, 1ull), 0);
    u32 c_6 = blk_read(e.mem, 0, term_loc(out_1), at_5 + 0);
    blk_write(e.mem, 0, term_loc(out_1), at_5 + 0, f32_to_u32(f32_rewrap(f32_unbox(atr_pct_1) * f32_unbox(1148846080ull))));
    v_106 = bars_1;
    v_107 = r_6;
    v_108 = aux_1;
    v_109 = out_1;
    v_110 = st_53;
    v_111 = st_54;
    v_112 = st_55;
    v_113 = st_56;
    v_114 = st_57;
    v_115 = st_58;
    v_116 = st_59;
    v_117 = st_60;
    v_118 = st_61;
    v_119 = st_62;
    v_120 = st_63;
    v_121 = st_64;
    v_122 = st_65;
    v_123 = st_66;
    v_124 = st_67;
    v_125 = st_68;
    v_126 = st_69;
    v_127 = st_70;
    v_128 = st_71;
    v_129 = st_72;
    v_130 = st_73;
    v_131 = st_74;
    v_132 = st_75;
    v_133 = st_76;
    v_134 = st_77;
    v_135 = st_78;
    v_136 = st_79;
    v_137 = st_80;
    v_138 = st_81;
    v_139 = st_82;
    v_140 = st_83;
    v_141 = st_84;
    v_142 = v_163;
    v_143 = st_85;
    v_144 = v_165;
    v_145 = st_87;
    v_146 = st_90;
    v_147 = st_91;
    v_148 = retail_0;
    v_149 = st_92;
    v_150 = st_93;
    v_151 = st_94;
    v_152 = st_95;
    v_153 = st_96;
    v_154 = st_97;
    v_155 = st_98;
    v_156 = st_99;
    v_157 = st_100;
    v_158 = st_101;
    v_159 = st_102;
    v_160 = st_103;
    v_161 = v_189;
    v_162 = st_105;
  break;
  }
  o[0] = v_106;
  o[1] = v_107;
  o[2] = v_108;
  o[3] = v_109;
  o[4] = v_110;
  o[5] = v_111;
  o[6] = v_112;
  o[7] = v_113;
  o[8] = v_114;
  o[9] = v_115;
  o[10] = v_116;
  o[11] = v_117;
  o[12] = v_118;
  o[13] = v_119;
  o[14] = v_120;
  o[15] = v_121;
  o[16] = v_122;
  o[17] = v_123;
  o[18] = v_124;
  o[19] = v_125;
  o[20] = v_126;
  o[21] = v_127;
  o[22] = v_128;
  o[23] = v_129;
  o[24] = v_130;
  o[25] = v_131;
  o[26] = v_132;
  o[27] = v_133;
  o[28] = v_134;
  o[29] = v_135;
  o[30] = v_136;
  o[31] = v_137;
  o[32] = v_138;
  o[33] = v_139;
  o[34] = v_140;
  o[35] = v_141;
  o[36] = v_142;
  o[37] = v_143;
  o[38] = v_144;
  o[39] = v_145;
  o[40] = v_146;
  o[41] = v_147;
  o[42] = v_148;
  o[43] = v_149;
  o[44] = v_150;
  o[45] = v_151;
  o[46] = v_152;
  o[47] = v_153;
  o[48] = v_154;
  o[49] = v_155;
  o[50] = v_156;
  o[51] = v_157;
  o[52] = v_158;
  o[53] = v_159;
  o[54] = v_160;
  o[55] = v_161;
  o[56] = v_162;
  return 1;
}

INLINE Term spin_19(Env e, THR Term* o, Term r0, Term r1, u32 r2, u32 r3) {
  u32 wpoll = 0;
  Term v_6 = 0;
  Term n_0 = r0;
  Term chips_1 = r1;
  u32 b_0 = r2;
  u32 per_0 = r3;
  WL_SPIN
    if (n_0 == 0) {
      v_6 = chips_1;
    } else {
      Term p_0 = (n_0 - 1);
      Term v_7 = 0;
      Term at_0 = blk_at(chips_1, b_0, 0);
      u32 c_0 = blk_read(e.mem, 0, term_loc(chips_1), at_0 + 0);
      Term v_8 = 0;
      Term o_2[1];
      if (spin_12(e, o_2, chips_1, c_0, b_0, per_0) == 0) {
        return 0;
      }
      v_8 = o_2[0];
      v_7 = v_8;
      r0 = p_0;
      r1 = v_7;
      r2 = U32_BIN(b_0, +, 1ull);
      r3 = per_0;
      n_0 = r0;
      chips_1 = r1;
      b_0 = r2;
      per_0 = r3;
      WL_AGAIN(spin_19);
    }
  break;
  }
  o[0] = v_6;
  return 1;
}

INLINE Term spin_20(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  u32 v_3 = 0;
  u32 b_0 = r0;
  WL_SPIN
    if (b_0 == 0) {
      v_3 = 1;
    } else {
      v_3 = 0;
    }
  break;
  }
  o[0] = v_3;
  return 1;
}

INLINE Term spin_21(Env e, THR Term* o, u32 r0, u32 r1, u32 r2) {
  u32 wpoll = 0;
  u32 v_27 = 0;
  u32 prev_0 = r0;
  u32 x_0 = r1;
  u32 n_0 = r2;
  WL_SPIN
    v_27 = f32_rewrap(f32_unbox(prev_0) + f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(x_0) - f32_unbox(prev_0))) * f32_unbox(f32_rewrap(f32_unbox(1073741824ull) / f32_unbox(f32_rewrap(f32_unbox(n_0) + f32_unbox(1065353216ull))))))));
  break;
  }
  o[0] = v_27;
  return 1;
}

INLINE Term spin_22(Env e, THR Term* o, u32 r0, u32 r1, u32 r2) {
  u32 wpoll = 0;
  u32 v_46 = 0;
  u32 prev_1 = r0;
  u32 x_1 = r1;
  u32 n_1 = r2;
  WL_SPIN
    v_46 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(prev_1) * f32_unbox(f32_rewrap(f32_unbox(n_1) - f32_unbox(1065353216ull))))) + f32_unbox(x_1))) / f32_unbox(n_1));
  break;
  }
  o[0] = v_46;
  return 1;
}

INLINE Term spin_23(Env e, THR Term* o, u32 r0, u32 r1, u32 r2, u32 r3) {
  u32 wpoll = 0;
  u32 v_62 = 0;
  u32 new_piv_1 = r0;
  u32 up_0 = r1;
  u32 dn_0 = r2;
  u32 s_0 = r3;
  WL_SPIN
    if (new_piv_1 == 1) {
      u32 v_63 = 0;
      Term o_29[1];
      if (spin_13(e, o_29, up_0, dn_0) == 0) {
        return 0;
      }
      v_63 = o_29[0];
      v_62 = v_63;
    } else {
      if (s_0 == 0) {
        u32 v_64 = 0;
        Term o_30[1];
        if (spin_13(e, o_30, up_0, dn_0) == 0) {
          return 0;
        }
        v_64 = o_30[0];
        v_62 = v_64;
      } else if (s_0 == 1) {
        v_62 = 1;
      } else {
        v_62 = 2;
      }
    }
  break;
  }
  o[0] = v_62;
  return 1;
}

INLINE Term spin_24(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  u32 v_67 = 0;
  u32 s_1 = r0;
  WL_SPIN
    if (s_1 == 1) {
      v_67 = 1;
    } else {
      v_67 = 0;
    }
  break;
  }
  o[0] = v_67;
  return 1;
}

INLINE Term spin_25(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  u32 v_70 = 0;
  u32 s_2 = r0;
  WL_SPIN
    if (s_2 == 2) {
      v_70 = 1;
    } else {
      v_70 = 0;
    }
  break;
  }
  o[0] = v_70;
  return 1;
}

INLINE Term spin_26(Env e, THR Term* o, Term r0) {
  u32 wpoll = 0;
  u32 v_91 = 0;
  Term xs_0 = r0;
  WL_SPIN
    if (term_aux(xs_0) == CID_CON) {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, xs_0, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      if (term_aux(f_1) == CID_CON) {
        Term fb_1[2];
        u64 sp_1 = ctr_take(e, f_1, 2, fb_1);
        Term f_2 = fb_1[0];
        Term f_3 = fb_1[1];
        if (term_aux(f_3) == CID_CON) {
          Term fb_2[2];
          u64 sp_2 = ctr_take(e, f_3, 2, fb_2);
          Term f_4 = fb_2[0];
          Term f_5 = fb_2[1];
          if (term_aux(f_5) == CID_CON) {
            Term fb_3[2];
            u64 sp_3 = ctr_take(e, f_5, 2, fb_3);
            Term f_6 = fb_3[0];
            Term f_7 = fb_3[1];
            if (term_aux(f_7) == CID_CON) {
              Term fb_4[2];
              u64 sp_4 = ctr_take(e, f_7, 2, fb_4);
              Term f_8 = fb_4[0];
              Term f_9 = fb_4[1];
              if (term_aux(f_9) == CID_CON) {
                Term fb_5[2];
                u64 sp_5 = ctr_take(e, f_9, 2, fb_5);
                Term f_10 = fb_5[0];
                Term f_11 = fb_5[1];
                term_sink(e, f_11);
                u32 sy_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f_10) + f32_unbox(f_8))) + f32_unbox(f_6))) + f32_unbox(f_4))) + f32_unbox(f_2))) + f32_unbox(f_0));
                u32 sxy_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f_8) + f32_unbox(f32_rewrap(f32_unbox(1073741824ull) * f32_unbox(f_6))))) + f32_unbox(f32_rewrap(f32_unbox(1077936128ull) * f32_unbox(f_4))))) + f32_unbox(f32_rewrap(f32_unbox(1082130432ull) * f32_unbox(f_2))))) + f32_unbox(f32_rewrap(f32_unbox(1084227584ull) * f32_unbox(f_0))));
                u32 slope_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(1086324736ull) * f32_unbox(sxy_0))) - f32_unbox(f32_rewrap(f32_unbox(1097859072ull) * f32_unbox(sy_0))))) / f32_unbox(1121058816ull));
                u32 mean_0 = f32_rewrap(f32_unbox(sy_0) / f32_unbox(1086324736ull));
                v_91 = f32_rewrap(f32_unbox(mean_0) + f32_unbox(f32_rewrap(f32_unbox(slope_0) * f32_unbox(1075838976ull))));
                spare_free(e, cls_fit(2), sp_5);
                spare_free(e, cls_fit(2), sp_4);
                spare_free(e, cls_fit(2), sp_3);
                spare_free(e, cls_fit(2), sp_2);
                spare_free(e, cls_fit(2), sp_1);
                spare_free(e, cls_fit(2), sp_0);
              } else {
                term_sink(e, f_9);
                v_91 = 0ull;
                spare_free(e, cls_fit(2), sp_4);
                spare_free(e, cls_fit(2), sp_3);
                spare_free(e, cls_fit(2), sp_2);
                spare_free(e, cls_fit(2), sp_1);
                spare_free(e, cls_fit(2), sp_0);
              }
            } else {
              term_sink(e, f_7);
              v_91 = 0ull;
              spare_free(e, cls_fit(2), sp_3);
              spare_free(e, cls_fit(2), sp_2);
              spare_free(e, cls_fit(2), sp_1);
              spare_free(e, cls_fit(2), sp_0);
            }
          } else {
            term_sink(e, f_5);
            v_91 = 0ull;
            spare_free(e, cls_fit(2), sp_2);
            spare_free(e, cls_fit(2), sp_1);
            spare_free(e, cls_fit(2), sp_0);
          }
        } else {
          term_sink(e, f_3);
          v_91 = 0ull;
          spare_free(e, cls_fit(2), sp_1);
          spare_free(e, cls_fit(2), sp_0);
        }
      } else {
        term_sink(e, f_1);
        v_91 = 0ull;
        spare_free(e, cls_fit(2), sp_0);
      }
    } else {
      term_sink(e, xs_0);
      v_91 = 0ull;
    }
  break;
  }
  o[0] = v_91;
  return 1;
}

INLINE Term spin_27(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_130 = 0;
  u32 u_0 = r0;
  u32 d_0 = r1;
  WL_SPIN
    u32 v_131 = 0;
    u32 v_132 = 0;
    Term o_62[1];
    if (spin_2(e, o_62, ((u64)(f32_unbox(u_0) > f32_unbox(0ull))), 1120403456ull, 1112014848ull) == 0) {
      return 0;
    }
    v_132 = o_62[0];
    v_131 = v_132;
    u32 v_133 = 0;
    Term o_63[1];
    if (spin_2(e, o_63, ((u64)(f32_unbox(d_0) > f32_unbox(0ull))), f32_rewrap(f32_unbox(1120403456ull) - f32_unbox(f32_rewrap(f32_unbox(1120403456ull) / f32_unbox(f32_rewrap(f32_unbox(1065353216ull) + f32_unbox(f32_rewrap(f32_unbox(u_0) / f32_unbox(d_0)))))))), v_131) == 0) {
      return 0;
    }
    v_133 = o_63[0];
    v_130 = v_133;
  break;
  }
  o[0] = v_130;
  return 1;
}

INLINE Term spin_28(Env e, THR Term* o, u32 r0, u32 r1, u32 r2, Term r3, u32 r4) {
  u32 wpoll = 0;
  Term v_3 = 0;
  u32 bl_0 = r0;
  u32 bh_0 = r1;
  u32 v_2 = r2;
  Term chips_1 = r3;
  u32 sign_0 = r4;
  WL_SPIN
    u32 v_4 = 0;
    u32 v_5 = 0;
    Term o_0[1];
    if (spin_16(e, o_0, bl_0) == 0) {
      return 0;
    }
    v_5 = o_0[0];
    v_4 = v_5;
    u32 v_6 = 0;
    u32 v_7 = 0;
    Term o_1[1];
    if (spin_16(e, o_1, bh_0) == 0) {
      return 0;
    }
    v_7 = o_1[0];
    v_6 = v_7;
    u32 cnt_0 = U32_BIN(U32_BIN(v_6, -, v_4), +, 1ull);
    Term v_8 = 0;
    Term o_2[1];
    if (spin_19(e, o_2, cnt_0, chips_1, v_4, f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(sign_0) * f32_unbox(v_2))) / f32_unbox(f32_rewrap((f32)(u32)(cnt_0))))) == 0) {
      return 0;
    }
    v_8 = o_2[0];
    v_3 = v_8;
  break;
  }
  o[0] = v_3;
  return 1;
}

FAR Term spin_29(Env e, THR Term* o, u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, Term r53, Term r54, Term r55, Term r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62) {
  u32 wpoll = 0;
  Term v_70 = 0;
  Term v_71 = 0;
  Term v_72 = 0;
  Term v_73 = 0;
  u32 v_74 = 0;
  u32 v_75 = 0;
  u32 v_76 = 0;
  u32 v_77 = 0;
  u32 v_78 = 0;
  u32 v_79 = 0;
  u32 v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 st_0 = r0;
  u32 st_1 = r1;
  u32 st_2 = r2;
  u32 st_3 = r3;
  u32 st_4 = r4;
  u32 st_5 = r5;
  u32 st_6 = r6;
  u32 st_7 = r7;
  u32 st_8 = r8;
  u32 st_9 = r9;
  u32 st_10 = r10;
  u32 st_11 = r11;
  u32 st_12 = r12;
  u32 st_13 = r13;
  u32 st_14 = r14;
  u32 st_15 = r15;
  u32 st_16 = r16;
  u32 st_17 = r17;
  u32 st_18 = r18;
  u32 st_19 = r19;
  u32 st_20 = r20;
  u32 st_21 = r21;
  u32 st_22 = r22;
  u32 st_23 = r23;
  u32 st_24 = r24;
  u32 st_25 = r25;
  u32 st_26 = r26;
  u32 st_27 = r27;
  u32 st_28 = r28;
  u32 st_29 = r29;
  u32 st_30 = r30;
  u32 st_31 = r31;
  u32 st_32 = r32;
  u32 st_33 = r33;
  u32 st_34 = r34;
  u32 st_35 = r35;
  u32 st_36 = r36;
  u32 st_37 = r37;
  u32 st_38 = r38;
  u32 st_39 = r39;
  u32 st_40 = r40;
  u32 st_41 = r41;
  u32 st_42 = r42;
  u32 st_43 = r43;
  u32 st_44 = r44;
  u32 st_45 = r45;
  u32 st_46 = r46;
  u32 st_47 = r47;
  u32 st_48 = r48;
  u32 st_49 = r49;
  u32 st_50 = r50;
  u32 st_51 = r51;
  u32 st_52 = r52;
  Term chips_2 = r53;
  Term bars_1 = r54;
  Term aux_1 = r55;
  Term out_1 = r56;
  u32 trip_0 = r57;
  u32 flags_0 = r58;
  u32 atr_pct_0 = r59;
  u32 pos_in_0 = r60;
  u32 neg_in_0 = r61;
  u32 i_1 = r62;
  WL_SPIN
    u32 v_127 = 0;
    u32 v_128 = 0;
    Term o_6[1];
    if (spin_14(e, o_6, f32_rewrap(f32_unbox(st_8) * f32_unbox(1064682127ull))) == 0) {
      return 0;
    }
    v_128 = o_6[0];
    v_127 = v_128;
    u32 v_129 = 0;
    u32 v_130 = 0;
    Term o_7[1];
    if (spin_15(e, o_7, f32_rewrap(f32_unbox(st_8) * f32_unbox(1065688760ull))) == 0) {
      return 0;
    }
    v_130 = o_7[0];
    v_129 = v_130;
    u32 v_131 = 0;
    u32 v_132 = 0;
    u32 v_133 = 0;
    Term o_8[1];
    if (spin_16(e, o_8, f32_rewrap(f32_unbox(st_8) * f32_unbox(1056964608ull))) == 0) {
      return 0;
    }
    v_133 = o_8[0];
    v_132 = v_133;
    u32 v_134 = 0;
    u32 v_135 = 0;
    Term o_9[1];
    if (spin_16(e, o_9, f32_rewrap(f32_unbox(st_8) * f32_unbox(1056964608ull))) == 0) {
      return 0;
    }
    v_135 = o_9[0];
    v_134 = v_135;
    u32 v_136 = 0;
    Term o_10[1];
    if (spin_3(e, o_10, U32_BIN(v_132, <, st_30), v_134, st_30) == 0) {
      return 0;
    }
    v_136 = o_10[0];
    v_131 = v_136;
    u32 v_137 = 0;
    u32 v_138 = 0;
    u32 v_139 = 0;
    Term o_11[1];
    if (spin_16(e, o_11, f32_rewrap(f32_unbox(st_8) * f32_unbox(1073741824ull))) == 0) {
      return 0;
    }
    v_139 = o_11[0];
    v_138 = v_139;
    u32 v_140 = 0;
    u32 v_141 = 0;
    Term o_12[1];
    if (spin_16(e, o_12, f32_rewrap(f32_unbox(st_8) * f32_unbox(1073741824ull))) == 0) {
      return 0;
    }
    v_141 = o_12[0];
    v_140 = v_141;
    u32 v_142 = 0;
    Term o_13[1];
    if (spin_3(e, o_13, U32_BIN(v_138, >, st_31), v_140, st_31) == 0) {
      return 0;
    }
    v_142 = o_13[0];
    v_137 = v_142;
    Term v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    Term v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    Term o_14[6];
    if (spin_17(e, o_14, U32_BIN(U32_BIN(v_137, -, v_131), +, 1ull), v_131, v_127, v_129, chips_2, 0ull, 0ull, 0ull, 0ull, 0ull) == 0) {
      return 0;
    }
    v_149 = o_14[0];
    v_150 = o_14[1];
    v_151 = o_14[2];
    v_152 = o_14[3];
    v_153 = o_14[4];
    v_154 = o_14[5];
    v_143 = v_149;
    v_144 = v_150;
    v_145 = v_151;
    v_146 = v_152;
    v_147 = v_153;
    v_148 = v_154;
    Term v_155 = 0;
    Term v_156 = 0;
    Term v_157 = 0;
    Term v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    u32 v_185 = 0;
    u32 v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    u32 v_191 = 0;
    u32 v_192 = 0;
    u32 v_193 = 0;
    u32 v_194 = 0;
    u32 v_195 = 0;
    u32 v_196 = 0;
    u32 v_197 = 0;
    u32 v_198 = 0;
    u32 v_199 = 0;
    u32 v_200 = 0;
    u32 v_201 = 0;
    u32 v_202 = 0;
    u32 v_203 = 0;
    u32 v_204 = 0;
    u32 v_205 = 0;
    u32 v_206 = 0;
    u32 v_207 = 0;
    u32 v_208 = 0;
    u32 v_209 = 0;
    u32 v_210 = 0;
    u32 v_211 = 0;
    Term o_15[57];
    if (spin_18(e, o_15, v_143, v_144, v_145, v_146, v_147, v_148, st_0, st_1, st_2, st_3, st_4, st_5, st_6, st_7, st_8, st_9, st_10, st_11, st_12, st_13, st_14, st_15, st_16, st_17, st_18, st_19, st_20, st_21, st_22, st_23, st_24, st_25, st_26, st_27, st_28, st_29, v_131, v_137, st_32, st_33, st_34, st_35, st_36, st_37, st_38, st_39, st_40, st_41, st_42, st_43, st_44, st_45, st_46, st_47, st_48, st_49, st_50, st_51, st_52, bars_1, aux_1, out_1, trip_0, flags_0, atr_pct_0, pos_in_0, neg_in_0, st_8, i_1) == 0) {
      return 0;
    }
    v_155 = o_15[0];
    v_156 = o_15[1];
    v_157 = o_15[2];
    v_158 = o_15[3];
    v_159 = o_15[4];
    v_160 = o_15[5];
    v_161 = o_15[6];
    v_162 = o_15[7];
    v_163 = o_15[8];
    v_164 = o_15[9];
    v_165 = o_15[10];
    v_166 = o_15[11];
    v_167 = o_15[12];
    v_168 = o_15[13];
    v_169 = o_15[14];
    v_170 = o_15[15];
    v_171 = o_15[16];
    v_172 = o_15[17];
    v_173 = o_15[18];
    v_174 = o_15[19];
    v_175 = o_15[20];
    v_176 = o_15[21];
    v_177 = o_15[22];
    v_178 = o_15[23];
    v_179 = o_15[24];
    v_180 = o_15[25];
    v_181 = o_15[26];
    v_182 = o_15[27];
    v_183 = o_15[28];
    v_184 = o_15[29];
    v_185 = o_15[30];
    v_186 = o_15[31];
    v_187 = o_15[32];
    v_188 = o_15[33];
    v_189 = o_15[34];
    v_190 = o_15[35];
    v_191 = o_15[36];
    v_192 = o_15[37];
    v_193 = o_15[38];
    v_194 = o_15[39];
    v_195 = o_15[40];
    v_196 = o_15[41];
    v_197 = o_15[42];
    v_198 = o_15[43];
    v_199 = o_15[44];
    v_200 = o_15[45];
    v_201 = o_15[46];
    v_202 = o_15[47];
    v_203 = o_15[48];
    v_204 = o_15[49];
    v_205 = o_15[50];
    v_206 = o_15[51];
    v_207 = o_15[52];
    v_208 = o_15[53];
    v_209 = o_15[54];
    v_210 = o_15[55];
    v_211 = o_15[56];
    v_70 = v_155;
    v_71 = v_156;
    v_72 = v_157;
    v_73 = v_158;
    v_74 = v_159;
    v_75 = v_160;
    v_76 = v_161;
    v_77 = v_162;
    v_78 = v_163;
    v_79 = v_164;
    v_80 = v_165;
    v_81 = v_166;
    v_82 = v_167;
    v_83 = v_168;
    v_84 = v_169;
    v_85 = v_170;
    v_86 = v_171;
    v_87 = v_172;
    v_88 = v_173;
    v_89 = v_174;
    v_90 = v_175;
    v_91 = v_176;
    v_92 = v_177;
    v_93 = v_178;
    v_94 = v_179;
    v_95 = v_180;
    v_96 = v_181;
    v_97 = v_182;
    v_98 = v_183;
    v_99 = v_184;
    v_100 = v_185;
    v_101 = v_186;
    v_102 = v_187;
    v_103 = v_188;
    v_104 = v_189;
    v_105 = v_190;
    v_106 = v_191;
    v_107 = v_192;
    v_108 = v_193;
    v_109 = v_194;
    v_110 = v_195;
    v_111 = v_196;
    v_112 = v_197;
    v_113 = v_198;
    v_114 = v_199;
    v_115 = v_200;
    v_116 = v_201;
    v_117 = v_202;
    v_118 = v_203;
    v_119 = v_204;
    v_120 = v_205;
    v_121 = v_206;
    v_122 = v_207;
    v_123 = v_208;
    v_124 = v_209;
    v_125 = v_210;
    v_126 = v_211;
  break;
  }
  o[0] = v_70;
  o[1] = v_71;
  o[2] = v_72;
  o[3] = v_73;
  o[4] = v_74;
  o[5] = v_75;
  o[6] = v_76;
  o[7] = v_77;
  o[8] = v_78;
  o[9] = v_79;
  o[10] = v_80;
  o[11] = v_81;
  o[12] = v_82;
  o[13] = v_83;
  o[14] = v_84;
  o[15] = v_85;
  o[16] = v_86;
  o[17] = v_87;
  o[18] = v_88;
  o[19] = v_89;
  o[20] = v_90;
  o[21] = v_91;
  o[22] = v_92;
  o[23] = v_93;
  o[24] = v_94;
  o[25] = v_95;
  o[26] = v_96;
  o[27] = v_97;
  o[28] = v_98;
  o[29] = v_99;
  o[30] = v_100;
  o[31] = v_101;
  o[32] = v_102;
  o[33] = v_103;
  o[34] = v_104;
  o[35] = v_105;
  o[36] = v_106;
  o[37] = v_107;
  o[38] = v_108;
  o[39] = v_109;
  o[40] = v_110;
  o[41] = v_111;
  o[42] = v_112;
  o[43] = v_113;
  o[44] = v_114;
  o[45] = v_115;
  o[46] = v_116;
  o[47] = v_117;
  o[48] = v_118;
  o[49] = v_119;
  o[50] = v_120;
  o[51] = v_121;
  o[52] = v_122;
  o[53] = v_123;
  o[54] = v_124;
  o[55] = v_125;
  o[56] = v_126;
  return 1;
}

INLINE Term spin_30(Env e, THR Term* o, Term r0, u32 r1, Term r2) {
  u32 wpoll = 0;
  Term v_2 = 0;
  Term v_3 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  Term acc_0 = r2;
  WL_SPIN
    u64 nd_0 = heap_alloc(e, cls_fit(2));
    e.mem[nd_0 + 0] = r_3;
    e.mem[nd_0 + 1] = acc_0;
    v_2 = r_2;
    v_3 = term_ctr(CID_CON, nd_0);
  break;
  }
  o[0] = v_2;
  o[1] = v_3;
  return 1;
}

FAR Term spin_31(Env e, THR Term* o, u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, Term r63, u32 r64, u32 r65, u32 r66, u32 r67, u32 r68, u32 r69, u32 r70, u32 r71, u32 r72) {
  u32 wpoll = 0;
  u32 v_129 = 0;
  u32 v_130 = 0;
  u32 v_131 = 0;
  u32 v_132 = 0;
  u32 v_133 = 0;
  u32 v_134 = 0;
  u32 v_135 = 0;
  u32 v_136 = 0;
  u32 v_137 = 0;
  u32 v_138 = 0;
  u32 v_139 = 0;
  u32 v_140 = 0;
  u32 v_141 = 0;
  u32 v_142 = 0;
  u32 v_143 = 0;
  u32 v_144 = 0;
  u32 v_145 = 0;
  u32 v_146 = 0;
  u32 v_147 = 0;
  u32 v_148 = 0;
  u32 v_149 = 0;
  u32 v_150 = 0;
  u32 v_151 = 0;
  u32 v_152 = 0;
  u32 v_153 = 0;
  u32 v_154 = 0;
  u32 v_155 = 0;
  u32 v_156 = 0;
  u32 v_157 = 0;
  u32 v_158 = 0;
  u32 v_159 = 0;
  u32 v_160 = 0;
  u32 v_161 = 0;
  u32 v_162 = 0;
  u32 v_163 = 0;
  u32 v_164 = 0;
  u32 v_165 = 0;
  u32 v_166 = 0;
  u32 v_167 = 0;
  u32 v_168 = 0;
  u32 v_169 = 0;
  u32 v_170 = 0;
  u32 v_171 = 0;
  u32 v_172 = 0;
  u32 v_173 = 0;
  u32 v_174 = 0;
  u32 v_175 = 0;
  u32 v_176 = 0;
  u32 v_177 = 0;
  u32 v_178 = 0;
  u32 v_179 = 0;
  u32 v_180 = 0;
  u32 v_181 = 0;
  u32 v_182 = 0;
  u32 v_183 = 0;
  u32 v_184 = 0;
  u32 v_185 = 0;
  u32 v_186 = 0;
  u32 v_187 = 0;
  u32 v_188 = 0;
  u32 v_189 = 0;
  u32 v_190 = 0;
  u32 v_191 = 0;
  u32 v_192 = 0;
  u32 st_53 = r0;
  u32 st_54 = r1;
  u32 st_55 = r2;
  u32 st_56 = r3;
  u32 st_57 = r4;
  u32 st_58 = r5;
  u32 st_59 = r6;
  u32 st_60 = r7;
  u32 st_61 = r8;
  u32 st_62 = r9;
  u32 st_63 = r10;
  u32 st_64 = r11;
  u32 st_65 = r12;
  u32 st_66 = r13;
  u32 st_67 = r14;
  u32 st_68 = r15;
  u32 st_69 = r16;
  u32 st_70 = r17;
  u32 st_71 = r18;
  u32 st_72 = r19;
  u32 st_73 = r20;
  u32 st_74 = r21;
  u32 st_75 = r22;
  u32 st_76 = r23;
  u32 st_77 = r24;
  u32 st_78 = r25;
  u32 st_79 = r26;
  u32 st_80 = r27;
  u32 st_81 = r28;
  u32 st_82 = r29;
  u32 st_83 = r30;
  u32 st_84 = r31;
  u32 st_85 = r32;
  u32 st_86 = r33;
  u32 st_87 = r34;
  u32 st_88 = r35;
  u32 st_89 = r36;
  u32 st_90 = r37;
  u32 st_91 = r38;
  u32 st_92 = r39;
  u32 st_93 = r40;
  u32 st_94 = r41;
  u32 st_95 = r42;
  u32 st_96 = r43;
  u32 st_97 = r44;
  u32 st_98 = r45;
  u32 st_99 = r46;
  u32 st_100 = r47;
  u32 st_101 = r48;
  u32 st_102 = r49;
  u32 st_103 = r50;
  u32 st_104 = r51;
  u32 st_105 = r52;
  u32 o_0 = r53;
  u32 h_0 = r54;
  u32 l_0 = r55;
  u32 c_0 = r56;
  u32 v_128 = r57;
  u32 old_5 = r58;
  u32 old_6 = r59;
  u32 old_7 = r60;
  u32 old_8 = r61;
  u32 old_9 = r62;
  Term trips_1 = r63;
  u32 atr_piv_1 = r64;
  u32 pos_out_1 = r65;
  u32 neg_out_1 = r66;
  u32 is_ph_1 = r67;
  u32 is_pl_1 = r68;
  u32 hh9_1 = r69;
  u32 ll9_1 = r70;
  u32 sma10_1 = r71;
  u32 i_1 = r72;
  WL_SPIN
    u32 v_193 = 0;
    u32 v_194 = 0;
    Term o_1[1];
    if (spin_20(e, o_1, st_105) == 0) {
      return 0;
    }
    v_194 = o_1[0];
    v_193 = v_194;
    u32 rng_0 = f32_rewrap(f32_unbox(hh9_1) - f32_unbox(ll9_1));
    u32 v_195 = 0;
    u32 v_196 = 0;
    Term o_2[1];
    if (spin_2(e, o_2, ((u64)(f32_unbox(rng_0) > f32_unbox(0ull))), f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(c_0) - f32_unbox(ll9_1))) / f32_unbox(rng_0))) * f32_unbox(1120403456ull)), 1112014848ull) == 0) {
      return 0;
    }
    v_196 = o_2[0];
    v_195 = v_196;
    u32 v_197 = 0;
    u32 v_198 = 0;
    Term o_3[1];
    if (spin_2(e, o_3, v_193, v_195, f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(v_195) + f32_unbox(f32_rewrap(f32_unbox(1073741824ull) * f32_unbox(st_53))))) / f32_unbox(1077936128ull))) == 0) {
      return 0;
    }
    v_198 = o_3[0];
    v_197 = v_198;
    u32 v_199 = 0;
    u32 v_200 = 0;
    Term o_4[1];
    if (spin_2(e, o_4, v_193, v_195, f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(v_197) + f32_unbox(f32_rewrap(f32_unbox(1073741824ull) * f32_unbox(st_54))))) / f32_unbox(1077936128ull))) == 0) {
      return 0;
    }
    v_200 = o_4[0];
    v_199 = v_200;
    u32 j_prev_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(1077936128ull) * f32_unbox(st_53))) - f32_unbox(f32_rewrap(f32_unbox(1073741824ull) * f32_unbox(st_54))));
    u32 j_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(1077936128ull) * f32_unbox(v_197))) - f32_unbox(f32_rewrap(f32_unbox(1073741824ull) * f32_unbox(v_199))));
    u32 v_201 = 0;
    u32 v_202 = 0;
    Term o_5[1];
    if (spin_7(e, o_5, ((u64)(f32_unbox(j_0) > f32_unbox(1112014848ull))), ((u64)(f32_unbox(j_prev_0) <= f32_unbox(1112014848ull)))) == 0) {
      return 0;
    }
    v_202 = o_5[0];
    v_201 = v_202;
    u32 v_203 = 0;
    u32 v_204 = 0;
    Term o_6[1];
    if (spin_2(e, o_6, v_201, h_0, st_55) == 0) {
      return 0;
    }
    v_204 = o_6[0];
    v_203 = v_204;
    u32 v_205 = 0;
    u32 v_206 = 0;
    Term o_7[1];
    if (spin_2(e, o_7, v_201, l_0, st_56) == 0) {
      return 0;
    }
    v_206 = o_7[0];
    v_205 = v_206;
    u32 v_207 = 0;
    u32 v_208 = 0;
    u32 v_209 = 0;
    u32 v_210 = 0;
    u32 v_211 = 0;
    Term o_8[1];
    if (spin_20(e, o_8, ((u64)(f32_unbox(c_0) < f32_unbox(v_205)))) == 0) {
      return 0;
    }
    v_211 = o_8[0];
    v_210 = v_211;
    u32 v_212 = 0;
    Term o_9[1];
    if (spin_7(e, o_9, ((u64)(f32_unbox(c_0) > f32_unbox(v_203))), v_210) == 0) {
      return 0;
    }
    v_212 = o_9[0];
    v_209 = v_212;
    u32 v_213 = 0;
    Term o_10[1];
    if (spin_7(e, o_10, ((u64)(f32_unbox(v_203) > f32_unbox(0ull))), v_209) == 0) {
      return 0;
    }
    v_213 = o_10[0];
    v_208 = v_213;
    u32 v_214 = 0;
    Term o_11[1];
    if (spin_7(e, o_11, ((u64)(f32_unbox(j_0) > f32_unbox(1112014848ull))), v_208) == 0) {
      return 0;
    }
    v_214 = o_11[0];
    v_207 = v_214;
    u32 src_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(c_0) + f32_unbox(l_0))) / f32_unbox(1073741824ull));
    u32 v_215 = 0;
    u32 v_216 = 0;
    u32 v_217 = 0;
    Term o_12[1];
    if (spin_21(e, o_12, st_57, src_0, 1090519040ull) == 0) {
      return 0;
    }
    v_217 = o_12[0];
    v_216 = v_217;
    u32 v_218 = 0;
    Term o_13[1];
    if (spin_2(e, o_13, v_193, src_0, v_216) == 0) {
      return 0;
    }
    v_218 = o_13[0];
    v_215 = v_218;
    u32 v_219 = 0;
    u32 v_220 = 0;
    u32 v_221 = 0;
    Term o_14[1];
    if (spin_21(e, o_14, st_58, src_0, 1101004800ull) == 0) {
      return 0;
    }
    v_221 = o_14[0];
    v_220 = v_221;
    u32 v_222 = 0;
    Term o_15[1];
    if (spin_2(e, o_15, v_193, src_0, v_220) == 0) {
      return 0;
    }
    v_222 = o_15[0];
    v_219 = v_222;
    u32 v_223 = 0;
    u32 v_224 = 0;
    u32 v_225 = 0;
    Term o_16[1];
    if (spin_21(e, o_16, st_59, v_219, 1090519040ull) == 0) {
      return 0;
    }
    v_225 = o_16[0];
    v_224 = v_225;
    u32 v_226 = 0;
    Term o_17[1];
    if (spin_2(e, o_17, v_193, src_0, v_224) == 0) {
      return 0;
    }
    v_226 = o_17[0];
    v_223 = v_226;
    u32 ribbon_0 = ((u64)(f32_unbox(v_215) > f32_unbox(v_223)));
    u32 v_227 = 0;
    u32 v_228 = 0;
    u32 v_229 = 0;
    u32 v_230 = 0;
    Term o_18[1];
    if (spin_0(e, o_18, f32_rewrap((f32)fabs(f32_unbox(f32_rewrap(f32_unbox(h_0) - f32_unbox(st_61))))), f32_rewrap((f32)fabs(f32_unbox(f32_rewrap(f32_unbox(l_0) - f32_unbox(st_61)))))) == 0) {
      return 0;
    }
    v_230 = o_18[0];
    v_229 = v_230;
    u32 v_231 = 0;
    Term o_19[1];
    if (spin_0(e, o_19, f32_rewrap(f32_unbox(h_0) - f32_unbox(l_0)), v_229) == 0) {
      return 0;
    }
    v_231 = o_19[0];
    v_228 = v_231;
    u32 v_232 = 0;
    Term o_20[1];
    if (spin_2(e, o_20, v_193, f32_rewrap(f32_unbox(h_0) - f32_unbox(l_0)), v_228) == 0) {
      return 0;
    }
    v_232 = o_20[0];
    v_227 = v_232;
    u32 v_233 = 0;
    u32 v_234 = 0;
    u32 v_235 = 0;
    Term o_21[1];
    if (spin_22(e, o_21, st_60, v_227, 1096810496ull) == 0) {
      return 0;
    }
    v_235 = o_21[0];
    v_234 = v_235;
    u32 v_236 = 0;
    Term o_22[1];
    if (spin_2(e, o_22, v_193, v_227, v_234) == 0) {
      return 0;
    }
    v_236 = o_22[0];
    v_233 = v_236;
    u32 new_piv_0 = ((is_ph_1) | (is_pl_1));
    u32 v_237 = 0;
    u32 v_238 = 0;
    Term o_23[1];
    if (spin_2(e, o_23, new_piv_0, f32_rewrap(f32_unbox(sma10_1) + f32_unbox(f32_rewrap(f32_unbox(1068708659ull) * f32_unbox(atr_piv_1)))), st_62) == 0) {
      return 0;
    }
    v_238 = o_23[0];
    v_237 = v_238;
    u32 v_239 = 0;
    u32 v_240 = 0;
    Term o_24[1];
    if (spin_2(e, o_24, new_piv_0, f32_rewrap(f32_unbox(sma10_1) - f32_unbox(f32_rewrap(f32_unbox(1068708659ull) * f32_unbox(atr_piv_1)))), st_63) == 0) {
      return 0;
    }
    v_240 = o_24[0];
    v_239 = v_240;
    u32 has_level_0 = ((u64)(f32_unbox(v_237) > f32_unbox(0ull)));
    u32 v_241 = 0;
    u32 v_242 = 0;
    u32 v_243 = 0;
    Term o_25[1];
    if (spin_7(e, o_25, ((u64)(f32_unbox(c_0) > f32_unbox(v_237))), ((u64)(f32_unbox(st_61) > f32_unbox(v_237)))) == 0) {
      return 0;
    }
    v_243 = o_25[0];
    v_242 = v_243;
    u32 v_244 = 0;
    Term o_26[1];
    if (spin_7(e, o_26, has_level_0, v_242) == 0) {
      return 0;
    }
    v_244 = o_26[0];
    v_241 = v_244;
    u32 v_245 = 0;
    u32 v_246 = 0;
    u32 v_247 = 0;
    Term o_27[1];
    if (spin_7(e, o_27, ((u64)(f32_unbox(c_0) < f32_unbox(v_239))), ((u64)(f32_unbox(st_61) < f32_unbox(v_239)))) == 0) {
      return 0;
    }
    v_247 = o_27[0];
    v_246 = v_247;
    u32 v_248 = 0;
    Term o_28[1];
    if (spin_7(e, o_28, has_level_0, v_246) == 0) {
      return 0;
    }
    v_248 = o_28[0];
    v_245 = v_248;
    u32 v_249 = 0;
    u32 v_250 = 0;
    Term o_29[1];
    if (spin_23(e, o_29, new_piv_0, v_241, v_245, st_64) == 0) {
      return 0;
    }
    v_250 = o_29[0];
    v_249 = v_250;
    u32 v_251 = 0;
    u32 v_252 = 0;
    Term o_30[1];
    if (spin_24(e, o_30, v_249) == 0) {
      return 0;
    }
    v_252 = o_30[0];
    v_251 = v_252;
    u32 v_253 = 0;
    u32 v_254 = 0;
    Term o_31[1];
    if (spin_25(e, o_31, v_249) == 0) {
      return 0;
    }
    v_254 = o_31[0];
    v_253 = v_254;
    u32 v_255 = 0;
    u32 v_256 = 0;
    u32 v_257 = 0;
    u32 v_258 = 0;
    Term o_32[1];
    if (spin_24(e, o_32, st_67) == 0) {
      return 0;
    }
    v_258 = o_32[0];
    v_257 = v_258;
    u32 v_259 = 0;
    Term o_33[1];
    if (spin_20(e, o_33, v_257) == 0) {
      return 0;
    }
    v_259 = o_33[0];
    v_256 = v_259;
    u32 v_260 = 0;
    Term o_34[1];
    if (spin_7(e, o_34, v_251, v_256) == 0) {
      return 0;
    }
    v_260 = o_34[0];
    v_255 = v_260;
    u32 ohlc4_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(o_0) + f32_unbox(h_0))) + f32_unbox(l_0))) + f32_unbox(c_0))) / f32_unbox(1082130432ull));
    u32 v_261 = 0;
    u32 v_262 = 0;
    u32 v_263 = 0;
    Term o_35[1];
    if (spin_21(e, o_35, st_68, ohlc4_0, 1101529088ull) == 0) {
      return 0;
    }
    v_263 = o_35[0];
    v_262 = v_263;
    u32 v_264 = 0;
    Term o_36[1];
    if (spin_2(e, o_36, v_193, ohlc4_0, v_262) == 0) {
      return 0;
    }
    v_264 = o_36[0];
    v_261 = v_264;
    u32 v_265 = 0;
    u32 v_266 = 0;
    u32 v_267 = 0;
    Term o_37[1];
    if (spin_21(e, o_37, st_69, ohlc4_0, 1107820544ull) == 0) {
      return 0;
    }
    v_267 = o_37[0];
    v_266 = v_267;
    u32 v_268 = 0;
    Term o_38[1];
    if (spin_2(e, o_38, v_193, ohlc4_0, v_266) == 0) {
      return 0;
    }
    v_268 = o_38[0];
    v_265 = v_268;
    u32 v_269 = 0;
    u32 v_270 = 0;
    u32 v_271 = 0;
    Term o_39[1];
    if (spin_21(e, o_39, st_70, ohlc4_0, 1116209152ull) == 0) {
      return 0;
    }
    v_271 = o_39[0];
    v_270 = v_271;
    u32 v_272 = 0;
    Term o_40[1];
    if (spin_2(e, o_40, v_193, ohlc4_0, v_270) == 0) {
      return 0;
    }
    v_272 = o_40[0];
    v_269 = v_272;
    u32 trip_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(v_261) + f32_unbox(v_265))) + f32_unbox(v_269))) / f32_unbox(1077936128ull));
    u32 v_273 = 0;
    u64 nd_0 = heap_alloc(e, cls_fit(2));
    e.mem[nd_0 + 0] = trip_0;
    e.mem[nd_0 + 1] = trips_1;
    u32 v_274 = 0;
    Term o_41[1];
    if (spin_26(e, o_41, term_ctr(CID_CON, nd_0)) == 0) {
      return 0;
    }
    v_274 = o_41[0];
    v_273 = v_274;
    u32 te_bull_0 = ((u64)(f32_unbox(v_273) > f32_unbox(st_71)));
    u32 v_275 = 0;
    u32 v_276 = 0;
    u32 v_277 = 0;
    Term o_42[1];
    if (spin_21(e, o_42, st_72, c_0, 1094713344ull) == 0) {
      return 0;
    }
    v_277 = o_42[0];
    v_276 = v_277;
    u32 v_278 = 0;
    Term o_43[1];
    if (spin_2(e, o_43, v_193, c_0, v_276) == 0) {
      return 0;
    }
    v_278 = o_43[0];
    v_275 = v_278;
    u32 v_279 = 0;
    u32 v_280 = 0;
    u32 v_281 = 0;
    Term o_44[1];
    if (spin_21(e, o_44, st_73, c_0, 1104150528ull) == 0) {
      return 0;
    }
    v_281 = o_44[0];
    v_280 = v_281;
    u32 v_282 = 0;
    Term o_45[1];
    if (spin_2(e, o_45, v_193, c_0, v_280) == 0) {
      return 0;
    }
    v_282 = o_45[0];
    v_279 = v_282;
    u32 macd_bull_0 = ((u64)(f32_unbox(f32_rewrap(f32_unbox(v_275) - f32_unbox(v_279))) > f32_unbox(0ull)));
    u32 chg_0 = f32_rewrap(f32_unbox(c_0) - f32_unbox(st_61));
    u32 v_283 = 0;
    u32 v_284 = 0;
    Term o_46[1];
    if (spin_0(e, o_46, chg_0, 0ull) == 0) {
      return 0;
    }
    v_284 = o_46[0];
    v_283 = v_284;
    u32 v_285 = 0;
    u32 v_286 = 0;
    Term o_47[1];
    if (spin_0(e, o_47, f32_rewrap(f32_unbox(0ull) - f32_unbox(chg_0)), 0ull) == 0) {
      return 0;
    }
    v_286 = o_47[0];
    v_285 = v_286;
    u32 v_287 = 0;
    u32 v_288 = 0;
    u32 v_289 = 0;
    Term o_48[1];
    if (spin_22(e, o_48, st_74, v_283, 1091567616ull) == 0) {
      return 0;
    }
    v_289 = o_48[0];
    v_288 = v_289;
    u32 v_290 = 0;
    Term o_49[1];
    if (spin_2(e, o_49, v_193, 0ull, v_288) == 0) {
      return 0;
    }
    v_290 = o_49[0];
    v_287 = v_290;
    u32 v_291 = 0;
    u32 v_292 = 0;
    u32 v_293 = 0;
    Term o_50[1];
    if (spin_22(e, o_50, st_75, v_285, 1091567616ull) == 0) {
      return 0;
    }
    v_293 = o_50[0];
    v_292 = v_293;
    u32 v_294 = 0;
    Term o_51[1];
    if (spin_2(e, o_51, v_193, 0ull, v_292) == 0) {
      return 0;
    }
    v_294 = o_51[0];
    v_291 = v_294;
    u32 v_295 = 0;
    u32 v_296 = 0;
    u32 v_297 = 0;
    Term o_52[1];
    if (spin_22(e, o_52, st_76, v_283, 1096810496ull) == 0) {
      return 0;
    }
    v_297 = o_52[0];
    v_296 = v_297;
    u32 v_298 = 0;
    Term o_53[1];
    if (spin_2(e, o_53, v_193, 0ull, v_296) == 0) {
      return 0;
    }
    v_298 = o_53[0];
    v_295 = v_298;
    u32 v_299 = 0;
    u32 v_300 = 0;
    u32 v_301 = 0;
    Term o_54[1];
    if (spin_22(e, o_54, st_77, v_285, 1096810496ull) == 0) {
      return 0;
    }
    v_301 = o_54[0];
    v_300 = v_301;
    u32 v_302 = 0;
    Term o_55[1];
    if (spin_2(e, o_55, v_193, 0ull, v_300) == 0) {
      return 0;
    }
    v_302 = o_55[0];
    v_299 = v_302;
    u32 v_303 = 0;
    u32 v_304 = 0;
    u32 v_305 = 0;
    Term o_56[1];
    if (spin_22(e, o_56, st_78, v_283, 1103101952ull) == 0) {
      return 0;
    }
    v_305 = o_56[0];
    v_304 = v_305;
    u32 v_306 = 0;
    Term o_57[1];
    if (spin_2(e, o_57, v_193, 0ull, v_304) == 0) {
      return 0;
    }
    v_306 = o_57[0];
    v_303 = v_306;
    u32 v_307 = 0;
    u32 v_308 = 0;
    u32 v_309 = 0;
    Term o_58[1];
    if (spin_22(e, o_58, st_79, v_285, 1103101952ull) == 0) {
      return 0;
    }
    v_309 = o_58[0];
    v_308 = v_309;
    u32 v_310 = 0;
    Term o_59[1];
    if (spin_2(e, o_59, v_193, 0ull, v_308) == 0) {
      return 0;
    }
    v_310 = o_59[0];
    v_307 = v_310;
    u32 v_311 = 0;
    u32 v_312 = 0;
    Term o_60[1];
    if (spin_27(e, o_60, v_287, v_291) == 0) {
      return 0;
    }
    v_312 = o_60[0];
    v_311 = v_312;
    u32 v_313 = 0;
    u32 v_314 = 0;
    Term o_61[1];
    if (spin_27(e, o_61, v_295, v_299) == 0) {
      return 0;
    }
    v_314 = o_61[0];
    v_313 = v_314;
    u32 v_315 = 0;
    u32 v_316 = 0;
    Term o_62[1];
    if (spin_27(e, o_62, v_303, v_307) == 0) {
      return 0;
    }
    v_316 = o_62[0];
    v_315 = v_316;
    u32 v_317 = 0;
    u32 v_318 = 0;
    u32 v_319 = 0;
    u32 v_320 = 0;
    Term o_63[1];
    if (spin_7(e, o_63, ((u64)(f32_unbox(v_315) > f32_unbox(st_82))), ((u64)(f32_unbox(v_313) > f32_unbox(1112014848ull)))) == 0) {
      return 0;
    }
    v_320 = o_63[0];
    v_319 = v_320;
    u32 v_321 = 0;
    Term o_64[1];
    if (spin_7(e, o_64, ((u64)(f32_unbox(v_313) > f32_unbox(st_81))), v_319) == 0) {
      return 0;
    }
    v_321 = o_64[0];
    v_318 = v_321;
    u32 v_322 = 0;
    Term o_65[1];
    if (spin_7(e, o_65, ((u64)(f32_unbox(v_311) > f32_unbox(st_80))), v_318) == 0) {
      return 0;
    }
    v_322 = o_65[0];
    v_317 = v_322;
    u32 v_323 = 0;
    u32 v_324 = 0;
    u32 v_325 = 0;
    Term o_66[1];
    if (spin_2(e, o_66, ((u64)(f32_unbox(c_0) < f32_unbox(st_61))), f32_rewrap(f32_unbox(0ull) - f32_unbox(v_128)), 0ull) == 0) {
      return 0;
    }
    v_325 = o_66[0];
    v_324 = v_325;
    u32 v_326 = 0;
    Term o_67[1];
    if (spin_2(e, o_67, ((u64)(f32_unbox(c_0) > f32_unbox(st_61))), v_128, v_324) == 0) {
      return 0;
    }
    v_326 = o_67[0];
    v_323 = v_326;
    u32 v_327 = 0;
    u32 v_328 = 0;
    u32 v_329 = 0;
    Term o_68[1];
    if (spin_21(e, o_68, st_92, v_128, 1092616192ull) == 0) {
      return 0;
    }
    v_329 = o_68[0];
    v_328 = v_329;
    u32 v_330 = 0;
    Term o_69[1];
    if (spin_2(e, o_69, v_193, v_128, v_328) == 0) {
      return 0;
    }
    v_330 = o_69[0];
    v_327 = v_330;
    u32 v_331 = 0;
    u32 v_332 = 0;
    u32 v_333 = 0;
    Term o_70[1];
    if (spin_21(e, o_70, st_93, v_323, 1092616192ull) == 0) {
      return 0;
    }
    v_333 = o_70[0];
    v_332 = v_333;
    u32 v_334 = 0;
    Term o_71[1];
    if (spin_2(e, o_71, v_193, v_323, v_332) == 0) {
      return 0;
    }
    v_334 = o_71[0];
    v_331 = v_334;
    u32 v_335 = 0;
    u32 v_336 = 0;
    Term o_72[1];
    if (spin_2(e, o_72, ((u64)(f32_unbox(v_327) > f32_unbox(0ull))), f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(1120403456ull) * f32_unbox(v_331))) / f32_unbox(v_327))) / f32_unbox(1073741824ull)), 0ull) == 0) {
      return 0;
    }
    v_336 = o_72[0];
    v_335 = v_336;
    u32 v_337 = 0;
    u32 v_338 = 0;
    u32 v_339 = 0;
    Term o_73[1];
    if (spin_21(e, o_73, st_94, v_335, 1077936128ull) == 0) {
      return 0;
    }
    v_339 = o_73[0];
    v_338 = v_339;
    u32 v_340 = 0;
    Term o_74[1];
    if (spin_2(e, o_74, v_193, v_335, v_338) == 0) {
      return 0;
    }
    v_340 = o_74[0];
    v_337 = v_340;
    u32 tp_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(h_0) + f32_unbox(l_0))) + f32_unbox(c_0))) / f32_unbox(1077936128ull));
    u32 raw_0 = f32_rewrap(f32_unbox(tp_0) * f32_unbox(v_128));
    u32 v_341 = 0;
    u32 v_342 = 0;
    Term o_75[1];
    if (spin_2(e, o_75, ((u64)(f32_unbox(tp_0) > f32_unbox(st_98))), raw_0, 0ull) == 0) {
      return 0;
    }
    v_342 = o_75[0];
    v_341 = v_342;
    u32 v_343 = 0;
    u32 v_344 = 0;
    Term o_76[1];
    if (spin_2(e, o_76, ((u64)(f32_unbox(tp_0) < f32_unbox(st_98))), raw_0, 0ull) == 0) {
      return 0;
    }
    v_344 = o_76[0];
    v_343 = v_344;
    u32 pos14_2_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(st_96) + f32_unbox(v_341))) - f32_unbox(pos_out_1));
    u32 neg14_2_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(st_97) + f32_unbox(v_343))) - f32_unbox(neg_out_1));
    u32 v_345 = 0;
    u32 v_346 = 0;
    Term o_77[1];
    if (spin_2(e, o_77, ((u64)(f32_unbox(neg14_2_0) > f32_unbox(0ull))), f32_rewrap(f32_unbox(1120403456ull) - f32_unbox(f32_rewrap(f32_unbox(1120403456ull) / f32_unbox(f32_rewrap(f32_unbox(1065353216ull) + f32_unbox(f32_rewrap(f32_unbox(pos14_2_0) / f32_unbox(neg14_2_0)))))))), 1120403456ull) == 0) {
      return 0;
    }
    v_346 = o_77[0];
    v_345 = v_346;
    u32 flow_raw_0 = f32_rewrap(f32_unbox(v_345) - f32_unbox(1112014848ull));
    u32 v_347 = 0;
    u32 v_348 = 0;
    u32 v_349 = 0;
    Term o_78[1];
    if (spin_21(e, o_78, st_95, flow_raw_0, 1077936128ull) == 0) {
      return 0;
    }
    v_349 = o_78[0];
    v_348 = v_349;
    u32 v_350 = 0;
    Term o_79[1];
    if (spin_2(e, o_79, v_193, flow_raw_0, v_348) == 0) {
      return 0;
    }
    v_350 = o_79[0];
    v_347 = v_350;
    u32 v_351 = 0;
    u32 v_352 = 0;
    u32 v_353 = 0;
    Term o_80[1];
    if (spin_21(e, o_80, st_99, c_0, 1101529088ull) == 0) {
      return 0;
    }
    v_353 = o_80[0];
    v_352 = v_353;
    u32 v_354 = 0;
    Term o_81[1];
    if (spin_2(e, o_81, v_193, c_0, v_352) == 0) {
      return 0;
    }
    v_354 = o_81[0];
    v_351 = v_354;
    u32 v_355 = 0;
    u32 v_356 = 0;
    u32 v_357 = 0;
    Term o_82[1];
    if (spin_21(e, o_82, st_100, c_0, 1107820544ull) == 0) {
      return 0;
    }
    v_357 = o_82[0];
    v_356 = v_357;
    u32 v_358 = 0;
    Term o_83[1];
    if (spin_2(e, o_83, v_193, c_0, v_356) == 0) {
      return 0;
    }
    v_358 = o_83[0];
    v_355 = v_358;
    u32 v_359 = 0;
    u32 v_360 = 0;
    u32 v_361 = 0;
    Term o_84[1];
    if (spin_21(e, o_84, st_101, c_0, 1113325568ull) == 0) {
      return 0;
    }
    v_361 = o_84[0];
    v_360 = v_361;
    u32 v_362 = 0;
    Term o_85[1];
    if (spin_2(e, o_85, v_193, c_0, v_360) == 0) {
      return 0;
    }
    v_362 = o_85[0];
    v_359 = v_362;
    u32 v_363 = 0;
    u32 v_364 = 0;
    u32 v_365 = 0;
    Term o_86[1];
    if (spin_21(e, o_86, st_102, c_0, 1128792064ull) == 0) {
      return 0;
    }
    v_365 = o_86[0];
    v_364 = v_365;
    u32 v_366 = 0;
    Term o_87[1];
    if (spin_2(e, o_87, v_193, c_0, v_364) == 0) {
      return 0;
    }
    v_366 = o_87[0];
    v_363 = v_366;
    u32 v_367 = 0;
    u32 v_368 = 0;
    u32 v_369 = 0;
    Term o_88[1];
    if (spin_0(e, o_88, v_355, v_359) == 0) {
      return 0;
    }
    v_369 = o_88[0];
    v_368 = v_369;
    u32 v_370 = 0;
    Term o_89[1];
    if (spin_0(e, o_89, v_351, v_368) == 0) {
      return 0;
    }
    v_370 = o_89[0];
    v_367 = v_370;
    u32 above_gold_0 = ((u64)(f32_unbox(c_0) > f32_unbox(v_367)));
    u32 v_371 = 0;
    u32 v_372 = 0;
    Term o_90[1];
    if (spin_7(e, o_90, ((u64)(f32_unbox(v_351) > f32_unbox(v_359))), ((u64)(f32_unbox(v_351) > f32_unbox(st_103)))) == 0) {
      return 0;
    }
    v_372 = o_90[0];
    v_371 = v_372;
    u32 v_373 = 0;
    u32 v_374 = 0;
    Term o_91[1];
    if (spin_5(e, o_91, ((u64)(f32_unbox(v_337) > f32_unbox(0ull)))) == 0) {
      return 0;
    }
    v_374 = o_91[0];
    v_373 = v_374;
    u32 v_375 = 0;
    u32 v_376 = 0;
    Term o_92[1];
    if (spin_5(e, o_92, ((u64)(f32_unbox(v_347) > f32_unbox(0ull)))) == 0) {
      return 0;
    }
    v_376 = o_92[0];
    v_375 = v_376;
    u32 v_377 = 0;
    u32 v_378 = 0;
    Term o_93[1];
    if (spin_5(e, o_93, above_gold_0) == 0) {
      return 0;
    }
    v_378 = o_93[0];
    v_377 = v_378;
    u32 v_379 = 0;
    u32 v_380 = 0;
    Term o_94[1];
    if (spin_5(e, o_94, v_371) == 0) {
      return 0;
    }
    v_380 = o_94[0];
    v_379 = v_380;
    u32 mountain_0 = U32_BIN(U32_BIN(U32_BIN(v_373, +, v_375), +, v_377), +, v_379);
    u32 v_381 = 0;
    u32 v_382 = 0;
    Term o_95[1];
    if (spin_0(e, o_95, 0ull, f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(c_0) - f32_unbox(v_363))) / f32_unbox(v_233))) == 0) {
      return 0;
    }
    v_382 = o_95[0];
    v_381 = v_382;
    u32 v_383 = 0;
    u32 v_384 = 0;
    Term o_96[1];
    if (spin_2(e, o_96, ((u64)(f32_unbox(v_337) > f32_unbox(1103626240ull))), 1065353216ull, 0ull) == 0) {
      return 0;
    }
    v_384 = o_96[0];
    v_383 = v_384;
    u32 v_385 = 0;
    u32 v_386 = 0;
    Term o_97[1];
    if (spin_2(e, o_97, ((u64)(f32_unbox(v_347) > f32_unbox(1103626240ull))), 1065353216ull, 0ull) == 0) {
      return 0;
    }
    v_386 = o_97[0];
    v_385 = v_386;
    u32 bubble_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(v_381) + f32_unbox(v_383))) + f32_unbox(v_385));
    u32 hot_0 = ((u64)(f32_unbox(bubble_0) >= f32_unbox(1086324736ull)));
    u32 v_387 = 0;
    u32 v_388 = 0;
    Term o_98[1];
    if (spin_8(e, o_98, v_251, 1ull) == 0) {
      return 0;
    }
    v_388 = o_98[0];
    v_387 = v_388;
    u32 v_389 = 0;
    u32 v_390 = 0;
    Term o_99[1];
    if (spin_8(e, o_99, v_253, 2ull) == 0) {
      return 0;
    }
    v_390 = o_99[0];
    v_389 = v_390;
    u32 v_391 = 0;
    u32 v_392 = 0;
    Term o_100[1];
    if (spin_8(e, o_100, te_bull_0, 8ull) == 0) {
      return 0;
    }
    v_392 = o_100[0];
    v_391 = v_392;
    u32 v_393 = 0;
    u32 v_394 = 0;
    Term o_101[1];
    if (spin_8(e, o_101, U32_BIN(mountain_0, >=, 3ull), 16ull) == 0) {
      return 0;
    }
    v_394 = o_101[0];
    v_393 = v_394;
    u32 v_395 = 0;
    u32 v_396 = 0;
    Term o_102[1];
    if (spin_8(e, o_102, above_gold_0, 32ull) == 0) {
      return 0;
    }
    v_396 = o_102[0];
    v_395 = v_396;
    u32 v_397 = 0;
    u32 v_398 = 0;
    Term o_103[1];
    if (spin_8(e, o_103, hot_0, 64ull) == 0) {
      return 0;
    }
    v_398 = o_103[0];
    v_397 = v_398;
    u32 v_399 = 0;
    u32 v_400 = 0;
    Term o_104[1];
    if (spin_8(e, o_104, v_207, 128ull) == 0) {
      return 0;
    }
    v_400 = o_104[0];
    v_399 = v_400;
    u32 v_401 = 0;
    u32 v_402 = 0;
    Term o_105[1];
    if (spin_8(e, o_105, ribbon_0, 256ull) == 0) {
      return 0;
    }
    v_402 = o_105[0];
    v_401 = v_402;
    u32 v_403 = 0;
    u32 v_404 = 0;
    u32 v_405 = 0;
    Term o_106[1];
    if (spin_7(e, o_106, macd_bull_0, v_317) == 0) {
      return 0;
    }
    v_405 = o_106[0];
    v_404 = v_405;
    u32 v_406 = 0;
    Term o_107[1];
    if (spin_8(e, o_107, v_404, 1024ull) == 0) {
      return 0;
    }
    v_406 = o_107[0];
    v_403 = v_406;
    u32 v_407 = 0;
    u32 v_408 = 0;
    Term o_108[1];
    if (spin_8(e, o_108, v_255, 2048ull) == 0) {
      return 0;
    }
    v_408 = o_108[0];
    v_407 = v_408;
    u32 v_409 = 0;
    u32 v_410 = 0;
    Term o_109[1];
    if (spin_8(e, o_109, is_ph_1, 4096ull) == 0) {
      return 0;
    }
    v_410 = o_109[0];
    v_409 = v_410;
    u32 v_411 = 0;
    u32 v_412 = 0;
    Term o_110[1];
    if (spin_8(e, o_110, is_pl_1, 8192ull) == 0) {
      return 0;
    }
    v_412 = o_110[0];
    v_411 = v_412;
    u32 flags_0 = U32_BIN(U32_BIN(U32_BIN(U32_BIN(U32_BIN(U32_BIN(U32_BIN(U32_BIN(U32_BIN(U32_BIN(U32_BIN(v_387, |, v_389), |, v_391), |, v_393), |, v_395), |, v_397), |, v_399), |, v_401), |, v_403), |, v_407), |, v_409), |, v_411);
    u32 st2_0 = 1;
    u32 v_413 = 0;
    u32 v_414 = 0;
    Term o_111[1];
    if (spin_2(e, o_111, U32_BIN(i_1, >=, 120ull), old_9, 0ull) == 0) {
      return 0;
    }
    v_414 = o_111[0];
    v_413 = v_414;
    v_129 = v_197;
    v_130 = v_199;
    v_131 = v_203;
    v_132 = v_205;
    v_133 = v_215;
    v_134 = v_219;
    v_135 = v_223;
    v_136 = v_233;
    v_137 = c_0;
    v_138 = v_237;
    v_139 = v_239;
    v_140 = v_249;
    v_141 = v_249;
    v_142 = st_65;
    v_143 = st_66;
    v_144 = v_261;
    v_145 = v_265;
    v_146 = v_269;
    v_147 = v_273;
    v_148 = v_275;
    v_149 = v_279;
    v_150 = v_287;
    v_151 = v_291;
    v_152 = v_295;
    v_153 = v_299;
    v_154 = v_303;
    v_155 = v_307;
    v_156 = v_311;
    v_157 = v_313;
    v_158 = v_315;
    v_159 = st_83;
    v_160 = st_84;
    v_161 = st_85;
    v_162 = st_86;
    v_163 = st_87;
    v_164 = st_88;
    v_165 = st_89;
    v_166 = st_90;
    v_167 = st_91;
    v_168 = v_327;
    v_169 = v_331;
    v_170 = v_337;
    v_171 = v_347;
    v_172 = pos14_2_0;
    v_173 = neg14_2_0;
    v_174 = tp_0;
    v_175 = v_351;
    v_176 = v_355;
    v_177 = v_359;
    v_178 = v_363;
    v_179 = v_351;
    v_180 = st_104;
    v_181 = st2_0;
    v_182 = trip_0;
    v_183 = v_128;
    v_184 = l_0;
    v_185 = h_0;
    v_186 = v_413;
    v_187 = old_7;
    v_188 = old_6;
    v_189 = flags_0;
    v_190 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(v_233) / f32_unbox(c_0))) * f32_unbox(1120403456ull));
    v_191 = v_341;
    v_192 = v_343;
  break;
  }
  o[0] = v_129;
  o[1] = v_130;
  o[2] = v_131;
  o[3] = v_132;
  o[4] = v_133;
  o[5] = v_134;
  o[6] = v_135;
  o[7] = v_136;
  o[8] = v_137;
  o[9] = v_138;
  o[10] = v_139;
  o[11] = v_140;
  o[12] = v_141;
  o[13] = v_142;
  o[14] = v_143;
  o[15] = v_144;
  o[16] = v_145;
  o[17] = v_146;
  o[18] = v_147;
  o[19] = v_148;
  o[20] = v_149;
  o[21] = v_150;
  o[22] = v_151;
  o[23] = v_152;
  o[24] = v_153;
  o[25] = v_154;
  o[26] = v_155;
  o[27] = v_156;
  o[28] = v_157;
  o[29] = v_158;
  o[30] = v_159;
  o[31] = v_160;
  o[32] = v_161;
  o[33] = v_162;
  o[34] = v_163;
  o[35] = v_164;
  o[36] = v_165;
  o[37] = v_166;
  o[38] = v_167;
  o[39] = v_168;
  o[40] = v_169;
  o[41] = v_170;
  o[42] = v_171;
  o[43] = v_172;
  o[44] = v_173;
  o[45] = v_174;
  o[46] = v_175;
  o[47] = v_176;
  o[48] = v_177;
  o[49] = v_178;
  o[50] = v_179;
  o[51] = v_180;
  o[52] = v_181;
  o[53] = v_182;
  o[54] = v_183;
  o[55] = v_184;
  o[56] = v_185;
  o[57] = v_186;
  o[58] = v_187;
  o[59] = v_188;
  o[60] = v_189;
  o[61] = v_190;
  o[62] = v_191;
  o[63] = v_192;
  return 1;
}

INLINE Term spin_32(Env e, THR Term* o, u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, Term r64, Term r65, Term r66, Term r67, u32 r68) {
  u32 wpoll = 0;
  Term v_472 = 0;
  Term v_473 = 0;
  Term v_474 = 0;
  Term v_475 = 0;
  u32 v_476 = 0;
  u32 v_477 = 0;
  u32 v_478 = 0;
  u32 v_479 = 0;
  u32 v_480 = 0;
  u32 v_481 = 0;
  u32 v_482 = 0;
  u32 v_483 = 0;
  u32 v_484 = 0;
  u32 v_485 = 0;
  u32 v_486 = 0;
  u32 v_487 = 0;
  u32 v_488 = 0;
  u32 v_489 = 0;
  u32 v_490 = 0;
  u32 v_491 = 0;
  u32 v_492 = 0;
  u32 v_493 = 0;
  u32 v_494 = 0;
  u32 v_495 = 0;
  u32 v_496 = 0;
  u32 v_497 = 0;
  u32 v_498 = 0;
  u32 v_499 = 0;
  u32 v_500 = 0;
  u32 v_501 = 0;
  u32 v_502 = 0;
  u32 v_503 = 0;
  u32 v_504 = 0;
  u32 v_505 = 0;
  u32 v_506 = 0;
  u32 v_507 = 0;
  u32 v_508 = 0;
  u32 v_509 = 0;
  u32 v_510 = 0;
  u32 v_511 = 0;
  u32 v_512 = 0;
  u32 v_513 = 0;
  u32 v_514 = 0;
  u32 v_515 = 0;
  u32 v_516 = 0;
  u32 v_517 = 0;
  u32 v_518 = 0;
  u32 v_519 = 0;
  u32 v_520 = 0;
  u32 v_521 = 0;
  u32 v_522 = 0;
  u32 v_523 = 0;
  u32 v_524 = 0;
  u32 v_525 = 0;
  u32 v_526 = 0;
  u32 v_527 = 0;
  u32 v_528 = 0;
  u32 u_0 = r0;
  u32 u_1 = r1;
  u32 u_2 = r2;
  u32 u_3 = r3;
  u32 u_4 = r4;
  u32 u_5 = r5;
  u32 u_6 = r6;
  u32 u_7 = r7;
  u32 u_8 = r8;
  u32 u_9 = r9;
  u32 u_10 = r10;
  u32 u_11 = r11;
  u32 u_12 = r12;
  u32 u_13 = r13;
  u32 u_14 = r14;
  u32 u_15 = r15;
  u32 u_16 = r16;
  u32 u_17 = r17;
  u32 u_18 = r18;
  u32 u_19 = r19;
  u32 u_20 = r20;
  u32 u_21 = r21;
  u32 u_22 = r22;
  u32 u_23 = r23;
  u32 u_24 = r24;
  u32 u_25 = r25;
  u32 u_26 = r26;
  u32 u_27 = r27;
  u32 u_28 = r28;
  u32 u_29 = r29;
  u32 u_30 = r30;
  u32 u_31 = r31;
  u32 u_32 = r32;
  u32 u_33 = r33;
  u32 u_34 = r34;
  u32 u_35 = r35;
  u32 u_36 = r36;
  u32 u_37 = r37;
  u32 u_38 = r38;
  u32 u_39 = r39;
  u32 u_40 = r40;
  u32 u_41 = r41;
  u32 u_42 = r42;
  u32 u_43 = r43;
  u32 u_44 = r44;
  u32 u_45 = r45;
  u32 u_46 = r46;
  u32 u_47 = r47;
  u32 u_48 = r48;
  u32 u_49 = r49;
  u32 u_50 = r50;
  u32 u_51 = r51;
  u32 u_52 = r52;
  u32 u_53 = r53;
  u32 u_54 = r54;
  u32 u_55 = r55;
  u32 u_56 = r56;
  u32 u_57 = r57;
  u32 u_58 = r58;
  u32 u_59 = r59;
  u32 u_60 = r60;
  u32 u_61 = r61;
  u32 u_62 = r62;
  u32 u_63 = r63;
  Term chips_1 = r64;
  Term bars_1 = r65;
  Term aux_1 = r66;
  Term out_1 = r67;
  u32 i_2 = r68;
  WL_SPIN
    Term v_529 = 0;
    Term v_530 = 0;
    Term o_113[1];
    if (spin_28(e, o_113, u_55, u_56, u_54, chips_1, 1065353216ull) == 0) {
      return 0;
    }
    v_530 = o_113[0];
    v_529 = v_530;
    Term v_531 = 0;
    u32 v_532 = 0;
    u32 v_533 = 0;
    Term o_114[1];
    if (spin_2(e, o_114, ((u64)(f32_unbox(u_57) > f32_unbox(0ull))), 1065353216ull, 0ull) == 0) {
      return 0;
    }
    v_533 = o_114[0];
    v_532 = v_533;
    Term v_534 = 0;
    Term o_115[1];
    if (spin_28(e, o_115, u_58, u_59, u_57, v_529, f32_rewrap(f32_unbox(0ull) - f32_unbox(v_532))) == 0) {
      return 0;
    }
    v_534 = o_115[0];
    v_531 = v_534;
    Term v_535 = 0;
    Term v_536 = 0;
    Term v_537 = 0;
    Term v_538 = 0;
    u32 v_539 = 0;
    u32 v_540 = 0;
    u32 v_541 = 0;
    u32 v_542 = 0;
    u32 v_543 = 0;
    u32 v_544 = 0;
    u32 v_545 = 0;
    u32 v_546 = 0;
    u32 v_547 = 0;
    u32 v_548 = 0;
    u32 v_549 = 0;
    u32 v_550 = 0;
    u32 v_551 = 0;
    u32 v_552 = 0;
    u32 v_553 = 0;
    u32 v_554 = 0;
    u32 v_555 = 0;
    u32 v_556 = 0;
    u32 v_557 = 0;
    u32 v_558 = 0;
    u32 v_559 = 0;
    u32 v_560 = 0;
    u32 v_561 = 0;
    u32 v_562 = 0;
    u32 v_563 = 0;
    u32 v_564 = 0;
    u32 v_565 = 0;
    u32 v_566 = 0;
    u32 v_567 = 0;
    u32 v_568 = 0;
    u32 v_569 = 0;
    u32 v_570 = 0;
    u32 v_571 = 0;
    u32 v_572 = 0;
    u32 v_573 = 0;
    u32 v_574 = 0;
    u32 v_575 = 0;
    u32 v_576 = 0;
    u32 v_577 = 0;
    u32 v_578 = 0;
    u32 v_579 = 0;
    u32 v_580 = 0;
    u32 v_581 = 0;
    u32 v_582 = 0;
    u32 v_583 = 0;
    u32 v_584 = 0;
    u32 v_585 = 0;
    u32 v_586 = 0;
    u32 v_587 = 0;
    u32 v_588 = 0;
    u32 v_589 = 0;
    u32 v_590 = 0;
    u32 v_591 = 0;
    Term o_116[57];
    if (spin_29(e, o_116, u_0, u_1, u_2, u_3, u_4, u_5, u_6, u_7, u_8, u_9, u_10, u_11, u_12, u_13, u_14, u_15, u_16, u_17, u_18, u_19, u_20, u_21, u_22, u_23, u_24, u_25, u_26, u_27, u_28, u_29, u_30, u_31, u_32, u_33, u_34, u_35, u_36, u_37, u_38, u_39, u_40, u_41, u_42, u_43, u_44, u_45, u_46, u_47, u_48, u_49, u_50, u_51, u_52, v_531, bars_1, aux_1, out_1, u_53, u_60, u_61, u_62, u_63, i_2) == 0) {
      return 0;
    }
    v_535 = o_116[0];
    v_536 = o_116[1];
    v_537 = o_116[2];
    v_538 = o_116[3];
    v_539 = o_116[4];
    v_540 = o_116[5];
    v_541 = o_116[6];
    v_542 = o_116[7];
    v_543 = o_116[8];
    v_544 = o_116[9];
    v_545 = o_116[10];
    v_546 = o_116[11];
    v_547 = o_116[12];
    v_548 = o_116[13];
    v_549 = o_116[14];
    v_550 = o_116[15];
    v_551 = o_116[16];
    v_552 = o_116[17];
    v_553 = o_116[18];
    v_554 = o_116[19];
    v_555 = o_116[20];
    v_556 = o_116[21];
    v_557 = o_116[22];
    v_558 = o_116[23];
    v_559 = o_116[24];
    v_560 = o_116[25];
    v_561 = o_116[26];
    v_562 = o_116[27];
    v_563 = o_116[28];
    v_564 = o_116[29];
    v_565 = o_116[30];
    v_566 = o_116[31];
    v_567 = o_116[32];
    v_568 = o_116[33];
    v_569 = o_116[34];
    v_570 = o_116[35];
    v_571 = o_116[36];
    v_572 = o_116[37];
    v_573 = o_116[38];
    v_574 = o_116[39];
    v_575 = o_116[40];
    v_576 = o_116[41];
    v_577 = o_116[42];
    v_578 = o_116[43];
    v_579 = o_116[44];
    v_580 = o_116[45];
    v_581 = o_116[46];
    v_582 = o_116[47];
    v_583 = o_116[48];
    v_584 = o_116[49];
    v_585 = o_116[50];
    v_586 = o_116[51];
    v_587 = o_116[52];
    v_588 = o_116[53];
    v_589 = o_116[54];
    v_590 = o_116[55];
    v_591 = o_116[56];
    v_472 = v_535;
    v_473 = v_536;
    v_474 = v_537;
    v_475 = v_538;
    v_476 = v_539;
    v_477 = v_540;
    v_478 = v_541;
    v_479 = v_542;
    v_480 = v_543;
    v_481 = v_544;
    v_482 = v_545;
    v_483 = v_546;
    v_484 = v_547;
    v_485 = v_548;
    v_486 = v_549;
    v_487 = v_550;
    v_488 = v_551;
    v_489 = v_552;
    v_490 = v_553;
    v_491 = v_554;
    v_492 = v_555;
    v_493 = v_556;
    v_494 = v_557;
    v_495 = v_558;
    v_496 = v_559;
    v_497 = v_560;
    v_498 = v_561;
    v_499 = v_562;
    v_500 = v_563;
    v_501 = v_564;
    v_502 = v_565;
    v_503 = v_566;
    v_504 = v_567;
    v_505 = v_568;
    v_506 = v_569;
    v_507 = v_570;
    v_508 = v_571;
    v_509 = v_572;
    v_510 = v_573;
    v_511 = v_574;
    v_512 = v_575;
    v_513 = v_576;
    v_514 = v_577;
    v_515 = v_578;
    v_516 = v_579;
    v_517 = v_580;
    v_518 = v_581;
    v_519 = v_582;
    v_520 = v_583;
    v_521 = v_584;
    v_522 = v_585;
    v_523 = v_586;
    v_524 = v_587;
    v_525 = v_588;
    v_526 = v_589;
    v_527 = v_590;
    v_528 = v_591;
  break;
  }
  o[0] = v_472;
  o[1] = v_473;
  o[2] = v_474;
  o[3] = v_475;
  o[4] = v_476;
  o[5] = v_477;
  o[6] = v_478;
  o[7] = v_479;
  o[8] = v_480;
  o[9] = v_481;
  o[10] = v_482;
  o[11] = v_483;
  o[12] = v_484;
  o[13] = v_485;
  o[14] = v_486;
  o[15] = v_487;
  o[16] = v_488;
  o[17] = v_489;
  o[18] = v_490;
  o[19] = v_491;
  o[20] = v_492;
  o[21] = v_493;
  o[22] = v_494;
  o[23] = v_495;
  o[24] = v_496;
  o[25] = v_497;
  o[26] = v_498;
  o[27] = v_499;
  o[28] = v_500;
  o[29] = v_501;
  o[30] = v_502;
  o[31] = v_503;
  o[32] = v_504;
  o[33] = v_505;
  o[34] = v_506;
  o[35] = v_507;
  o[36] = v_508;
  o[37] = v_509;
  o[38] = v_510;
  o[39] = v_511;
  o[40] = v_512;
  o[41] = v_513;
  o[42] = v_514;
  o[43] = v_515;
  o[44] = v_516;
  o[45] = v_517;
  o[46] = v_518;
  o[47] = v_519;
  o[48] = v_520;
  o[49] = v_521;
  o[50] = v_522;
  o[51] = v_523;
  o[52] = v_524;
  o[53] = v_525;
  o[54] = v_526;
  o[55] = v_527;
  o[56] = v_528;
  return 1;
}

INLINE Term spin_33(Env e, THR Term* o, u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
  u32 wpoll = 0;
  u32 v_2 = 0;
  u32 done_0 = r0;
  u32 hit_s_0 = r1;
  u32 hit_t_0 = r2;
  u32 cur_0 = r3;
  u32 stop_r_0 = r4;
  u32 tgt_r_0 = r5;
  WL_SPIN
    if (done_0 == 1) {
      v_2 = cur_0;
    } else {
      if (hit_s_0 == 1) {
        v_2 = stop_r_0;
      } else {
        if (hit_t_0 == 1) {
          v_2 = tgt_r_0;
        } else {
          v_2 = cur_0;
        }
      }
    }
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_34(Env e, THR Term* o, Term r0, Term r1, u32 r2) {
  u32 wpoll = 0;
  Term v_4 = 0;
  Term v_5 = 0;
  Term r_2 = r0;
  Term r_3 = r1;
  u32 j_1 = r2;
  WL_SPIN
    Term at_0 = blk_at(r_2, U32_BIN(U32_BIN(j_1, *, 4ull), +, 1ull), 0);
    u32 c_0 = blk_read(e.mem, 0, term_loc(r_2), at_0 + 0);
    Term v_6 = 0;
    Term v_7 = 0;
    Term o_0[2];
    if (spin_30(e, o_0, r_2, c_0, r_3) == 0) {
      return 0;
    }
    v_6 = o_0[0];
    v_7 = o_0[1];
    v_4 = v_6;
    v_5 = v_7;
  break;
  }
  o[0] = v_4;
  o[1] = v_5;
  return 1;
}

FAR Term spin_35(Env e, THR Term* o, u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, Term r63, u32 r64, u32 r65, u32 r66, u32 r67, u32 r68, u32 r69, u32 r70, u32 r71, Term r72, Term r73, Term r74, Term r75, u32 r76) {
  u32 wpoll = 0;
  Term v_57 = 0;
  Term v_58 = 0;
  Term v_59 = 0;
  Term v_60 = 0;
  u32 v_61 = 0;
  u32 v_62 = 0;
  u32 v_63 = 0;
  u32 v_64 = 0;
  u32 v_65 = 0;
  u32 v_66 = 0;
  u32 v_67 = 0;
  u32 v_68 = 0;
  u32 v_69 = 0;
  u32 v_70 = 0;
  u32 v_71 = 0;
  u32 v_72 = 0;
  u32 v_73 = 0;
  u32 v_74 = 0;
  u32 v_75 = 0;
  u32 v_76 = 0;
  u32 v_77 = 0;
  u32 v_78 = 0;
  u32 v_79 = 0;
  u32 v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 st_53 = r0;
  u32 st_54 = r1;
  u32 st_55 = r2;
  u32 st_56 = r3;
  u32 st_57 = r4;
  u32 st_58 = r5;
  u32 st_59 = r6;
  u32 st_60 = r7;
  u32 st_61 = r8;
  u32 st_62 = r9;
  u32 st_63 = r10;
  u32 st_64 = r11;
  u32 st_65 = r12;
  u32 st_66 = r13;
  u32 st_67 = r14;
  u32 st_68 = r15;
  u32 st_69 = r16;
  u32 st_70 = r17;
  u32 st_71 = r18;
  u32 st_72 = r19;
  u32 st_73 = r20;
  u32 st_74 = r21;
  u32 st_75 = r22;
  u32 st_76 = r23;
  u32 st_77 = r24;
  u32 st_78 = r25;
  u32 st_79 = r26;
  u32 st_80 = r27;
  u32 st_81 = r28;
  u32 st_82 = r29;
  u32 st_83 = r30;
  u32 st_84 = r31;
  u32 st_85 = r32;
  u32 st_86 = r33;
  u32 st_87 = r34;
  u32 st_88 = r35;
  u32 st_89 = r36;
  u32 st_90 = r37;
  u32 st_91 = r38;
  u32 st_92 = r39;
  u32 st_93 = r40;
  u32 st_94 = r41;
  u32 st_95 = r42;
  u32 st_96 = r43;
  u32 st_97 = r44;
  u32 st_98 = r45;
  u32 st_99 = r46;
  u32 st_100 = r47;
  u32 st_101 = r48;
  u32 st_102 = r49;
  u32 st_103 = r50;
  u32 st_104 = r51;
  u32 st_105 = r52;
  u32 b_5 = r53;
  u32 b_6 = r54;
  u32 b_7 = r55;
  u32 b_8 = r56;
  u32 b_9 = r57;
  u32 old_5 = r58;
  u32 old_6 = r59;
  u32 old_7 = r60;
  u32 old_8 = r61;
  u32 old_9 = r62;
  Term trips_0 = r63;
  u32 atr_piv_1 = r64;
  u32 pos_out_1 = r65;
  u32 neg_out_1 = r66;
  u32 is_ph_1 = r67;
  u32 is_pl_1 = r68;
  u32 hh9_1 = r69;
  u32 ll9_1 = r70;
  u32 sma10_1 = r71;
  Term chips_1 = r72;
  Term bars_1 = r73;
  Term aux_0 = r74;
  Term out_1 = r75;
  u32 i_1 = r76;
  WL_SPIN
    u32 v_114 = 0;
    u32 v_115 = 0;
    u32 v_116 = 0;
    u32 v_117 = 0;
    u32 v_118 = 0;
    u32 v_119 = 0;
    u32 v_120 = 0;
    u32 v_121 = 0;
    u32 v_122 = 0;
    u32 v_123 = 0;
    u32 v_124 = 0;
    u32 v_125 = 0;
    u32 v_126 = 0;
    u32 v_127 = 0;
    u32 v_128 = 0;
    u32 v_129 = 0;
    u32 v_130 = 0;
    u32 v_131 = 0;
    u32 v_132 = 0;
    u32 v_133 = 0;
    u32 v_134 = 0;
    u32 v_135 = 0;
    u32 v_136 = 0;
    u32 v_137 = 0;
    u32 v_138 = 0;
    u32 v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    u32 v_185 = 0;
    u32 v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    u32 v_191 = 0;
    u32 v_192 = 0;
    u32 v_193 = 0;
    u32 v_194 = 0;
    u32 v_195 = 0;
    u32 v_196 = 0;
    u32 v_197 = 0;
    u32 v_198 = 0;
    u32 v_199 = 0;
    u32 v_200 = 0;
    u32 v_201 = 0;
    u32 v_202 = 0;
    u32 v_203 = 0;
    u32 v_204 = 0;
    u32 v_205 = 0;
    u32 v_206 = 0;
    u32 v_207 = 0;
    u32 v_208 = 0;
    u32 v_209 = 0;
    u32 v_210 = 0;
    u32 v_211 = 0;
    u32 v_212 = 0;
    u32 v_213 = 0;
    u32 v_214 = 0;
    u32 v_215 = 0;
    u32 v_216 = 0;
    u32 v_217 = 0;
    u32 v_218 = 0;
    u32 v_219 = 0;
    u32 v_220 = 0;
    u32 v_221 = 0;
    u32 v_222 = 0;
    u32 v_223 = 0;
    u32 v_224 = 0;
    u32 v_225 = 0;
    u32 v_226 = 0;
    u32 v_227 = 0;
    u32 v_228 = 0;
    u32 v_229 = 0;
    u32 v_230 = 0;
    u32 v_231 = 0;
    u32 v_232 = 0;
    u32 v_233 = 0;
    u32 v_234 = 0;
    u32 v_235 = 0;
    u32 v_236 = 0;
    u32 v_237 = 0;
    u32 v_238 = 0;
    u32 v_239 = 0;
    u32 v_240 = 0;
    u32 v_241 = 0;
    Term o_0[64];
    if (spin_31(e, o_0, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, b_5, b_6, b_7, b_8, b_9, old_5, old_6, old_7, old_8, old_9, trips_0, atr_piv_1, pos_out_1, neg_out_1, is_ph_1, is_pl_1, hh9_1, ll9_1, sma10_1, i_1) == 0) {
      return 0;
    }
    v_178 = o_0[0];
    v_179 = o_0[1];
    v_180 = o_0[2];
    v_181 = o_0[3];
    v_182 = o_0[4];
    v_183 = o_0[5];
    v_184 = o_0[6];
    v_185 = o_0[7];
    v_186 = o_0[8];
    v_187 = o_0[9];
    v_188 = o_0[10];
    v_189 = o_0[11];
    v_190 = o_0[12];
    v_191 = o_0[13];
    v_192 = o_0[14];
    v_193 = o_0[15];
    v_194 = o_0[16];
    v_195 = o_0[17];
    v_196 = o_0[18];
    v_197 = o_0[19];
    v_198 = o_0[20];
    v_199 = o_0[21];
    v_200 = o_0[22];
    v_201 = o_0[23];
    v_202 = o_0[24];
    v_203 = o_0[25];
    v_204 = o_0[26];
    v_205 = o_0[27];
    v_206 = o_0[28];
    v_207 = o_0[29];
    v_208 = o_0[30];
    v_209 = o_0[31];
    v_210 = o_0[32];
    v_211 = o_0[33];
    v_212 = o_0[34];
    v_213 = o_0[35];
    v_214 = o_0[36];
    v_215 = o_0[37];
    v_216 = o_0[38];
    v_217 = o_0[39];
    v_218 = o_0[40];
    v_219 = o_0[41];
    v_220 = o_0[42];
    v_221 = o_0[43];
    v_222 = o_0[44];
    v_223 = o_0[45];
    v_224 = o_0[46];
    v_225 = o_0[47];
    v_226 = o_0[48];
    v_227 = o_0[49];
    v_228 = o_0[50];
    v_229 = o_0[51];
    v_230 = o_0[52];
    v_231 = o_0[53];
    v_232 = o_0[54];
    v_233 = o_0[55];
    v_234 = o_0[56];
    v_235 = o_0[57];
    v_236 = o_0[58];
    v_237 = o_0[59];
    v_238 = o_0[60];
    v_239 = o_0[61];
    v_240 = o_0[62];
    v_241 = o_0[63];
    v_114 = v_178;
    v_115 = v_179;
    v_116 = v_180;
    v_117 = v_181;
    v_118 = v_182;
    v_119 = v_183;
    v_120 = v_184;
    v_121 = v_185;
    v_122 = v_186;
    v_123 = v_187;
    v_124 = v_188;
    v_125 = v_189;
    v_126 = v_190;
    v_127 = v_191;
    v_128 = v_192;
    v_129 = v_193;
    v_130 = v_194;
    v_131 = v_195;
    v_132 = v_196;
    v_133 = v_197;
    v_134 = v_198;
    v_135 = v_199;
    v_136 = v_200;
    v_137 = v_201;
    v_138 = v_202;
    v_139 = v_203;
    v_140 = v_204;
    v_141 = v_205;
    v_142 = v_206;
    v_143 = v_207;
    v_144 = v_208;
    v_145 = v_209;
    v_146 = v_210;
    v_147 = v_211;
    v_148 = v_212;
    v_149 = v_213;
    v_150 = v_214;
    v_151 = v_215;
    v_152 = v_216;
    v_153 = v_217;
    v_154 = v_218;
    v_155 = v_219;
    v_156 = v_220;
    v_157 = v_221;
    v_158 = v_222;
    v_159 = v_223;
    v_160 = v_224;
    v_161 = v_225;
    v_162 = v_226;
    v_163 = v_227;
    v_164 = v_228;
    v_165 = v_229;
    v_166 = v_230;
    v_167 = v_231;
    v_168 = v_232;
    v_169 = v_233;
    v_170 = v_234;
    v_171 = v_235;
    v_172 = v_236;
    v_173 = v_237;
    v_174 = v_238;
    v_175 = v_239;
    v_176 = v_240;
    v_177 = v_241;
    Term v_242 = 0;
    Term v_243 = 0;
    Term v_244 = 0;
    Term v_245 = 0;
    u32 v_246 = 0;
    u32 v_247 = 0;
    u32 v_248 = 0;
    u32 v_249 = 0;
    u32 v_250 = 0;
    u32 v_251 = 0;
    u32 v_252 = 0;
    u32 v_253 = 0;
    u32 v_254 = 0;
    u32 v_255 = 0;
    u32 v_256 = 0;
    u32 v_257 = 0;
    u32 v_258 = 0;
    u32 v_259 = 0;
    u32 v_260 = 0;
    u32 v_261 = 0;
    u32 v_262 = 0;
    u32 v_263 = 0;
    u32 v_264 = 0;
    u32 v_265 = 0;
    u32 v_266 = 0;
    u32 v_267 = 0;
    u32 v_268 = 0;
    u32 v_269 = 0;
    u32 v_270 = 0;
    u32 v_271 = 0;
    u32 v_272 = 0;
    u32 v_273 = 0;
    u32 v_274 = 0;
    u32 v_275 = 0;
    u32 v_276 = 0;
    u32 v_277 = 0;
    u32 v_278 = 0;
    u32 v_279 = 0;
    u32 v_280 = 0;
    u32 v_281 = 0;
    u32 v_282 = 0;
    u32 v_283 = 0;
    u32 v_284 = 0;
    u32 v_285 = 0;
    u32 v_286 = 0;
    u32 v_287 = 0;
    u32 v_288 = 0;
    u32 v_289 = 0;
    u32 v_290 = 0;
    u32 v_291 = 0;
    u32 v_292 = 0;
    u32 v_293 = 0;
    u32 v_294 = 0;
    u32 v_295 = 0;
    u32 v_296 = 0;
    u32 v_297 = 0;
    u32 v_298 = 0;
    Term o_1[57];
    if (spin_32(e, o_1, v_114, v_115, v_116, v_117, v_118, v_119, v_120, v_121, v_122, v_123, v_124, v_125, v_126, v_127, v_128, v_129, v_130, v_131, v_132, v_133, v_134, v_135, v_136, v_137, v_138, v_139, v_140, v_141, v_142, v_143, v_144, v_145, v_146, v_147, v_148, v_149, v_150, v_151, v_152, v_153, v_154, v_155, v_156, v_157, v_158, v_159, v_160, v_161, v_162, v_163, v_164, v_165, v_166, v_167, v_168, v_169, v_170, v_171, v_172, v_173, v_174, v_175, v_176, v_177, chips_1, bars_1, aux_0, out_1, i_1) == 0) {
      return 0;
    }
    v_242 = o_1[0];
    v_243 = o_1[1];
    v_244 = o_1[2];
    v_245 = o_1[3];
    v_246 = o_1[4];
    v_247 = o_1[5];
    v_248 = o_1[6];
    v_249 = o_1[7];
    v_250 = o_1[8];
    v_251 = o_1[9];
    v_252 = o_1[10];
    v_253 = o_1[11];
    v_254 = o_1[12];
    v_255 = o_1[13];
    v_256 = o_1[14];
    v_257 = o_1[15];
    v_258 = o_1[16];
    v_259 = o_1[17];
    v_260 = o_1[18];
    v_261 = o_1[19];
    v_262 = o_1[20];
    v_263 = o_1[21];
    v_264 = o_1[22];
    v_265 = o_1[23];
    v_266 = o_1[24];
    v_267 = o_1[25];
    v_268 = o_1[26];
    v_269 = o_1[27];
    v_270 = o_1[28];
    v_271 = o_1[29];
    v_272 = o_1[30];
    v_273 = o_1[31];
    v_274 = o_1[32];
    v_275 = o_1[33];
    v_276 = o_1[34];
    v_277 = o_1[35];
    v_278 = o_1[36];
    v_279 = o_1[37];
    v_280 = o_1[38];
    v_281 = o_1[39];
    v_282 = o_1[40];
    v_283 = o_1[41];
    v_284 = o_1[42];
    v_285 = o_1[43];
    v_286 = o_1[44];
    v_287 = o_1[45];
    v_288 = o_1[46];
    v_289 = o_1[47];
    v_290 = o_1[48];
    v_291 = o_1[49];
    v_292 = o_1[50];
    v_293 = o_1[51];
    v_294 = o_1[52];
    v_295 = o_1[53];
    v_296 = o_1[54];
    v_297 = o_1[55];
    v_298 = o_1[56];
    v_57 = v_242;
    v_58 = v_243;
    v_59 = v_244;
    v_60 = v_245;
    v_61 = v_246;
    v_62 = v_247;
    v_63 = v_248;
    v_64 = v_249;
    v_65 = v_250;
    v_66 = v_251;
    v_67 = v_252;
    v_68 = v_253;
    v_69 = v_254;
    v_70 = v_255;
    v_71 = v_256;
    v_72 = v_257;
    v_73 = v_258;
    v_74 = v_259;
    v_75 = v_260;
    v_76 = v_261;
    v_77 = v_262;
    v_78 = v_263;
    v_79 = v_264;
    v_80 = v_265;
    v_81 = v_266;
    v_82 = v_267;
    v_83 = v_268;
    v_84 = v_269;
    v_85 = v_270;
    v_86 = v_271;
    v_87 = v_272;
    v_88 = v_273;
    v_89 = v_274;
    v_90 = v_275;
    v_91 = v_276;
    v_92 = v_277;
    v_93 = v_278;
    v_94 = v_279;
    v_95 = v_280;
    v_96 = v_281;
    v_97 = v_282;
    v_98 = v_283;
    v_99 = v_284;
    v_100 = v_285;
    v_101 = v_286;
    v_102 = v_287;
    v_103 = v_288;
    v_104 = v_289;
    v_105 = v_290;
    v_106 = v_291;
    v_107 = v_292;
    v_108 = v_293;
    v_109 = v_294;
    v_110 = v_295;
    v_111 = v_296;
    v_112 = v_297;
    v_113 = v_298;
  break;
  }
  o[0] = v_57;
  o[1] = v_58;
  o[2] = v_59;
  o[3] = v_60;
  o[4] = v_61;
  o[5] = v_62;
  o[6] = v_63;
  o[7] = v_64;
  o[8] = v_65;
  o[9] = v_66;
  o[10] = v_67;
  o[11] = v_68;
  o[12] = v_69;
  o[13] = v_70;
  o[14] = v_71;
  o[15] = v_72;
  o[16] = v_73;
  o[17] = v_74;
  o[18] = v_75;
  o[19] = v_76;
  o[20] = v_77;
  o[21] = v_78;
  o[22] = v_79;
  o[23] = v_80;
  o[24] = v_81;
  o[25] = v_82;
  o[26] = v_83;
  o[27] = v_84;
  o[28] = v_85;
  o[29] = v_86;
  o[30] = v_87;
  o[31] = v_88;
  o[32] = v_89;
  o[33] = v_90;
  o[34] = v_91;
  o[35] = v_92;
  o[36] = v_93;
  o[37] = v_94;
  o[38] = v_95;
  o[39] = v_96;
  o[40] = v_97;
  o[41] = v_98;
  o[42] = v_99;
  o[43] = v_100;
  o[44] = v_101;
  o[45] = v_102;
  o[46] = v_103;
  o[47] = v_104;
  o[48] = v_105;
  o[49] = v_106;
  o[50] = v_107;
  o[51] = v_108;
  o[52] = v_109;
  o[53] = v_110;
  o[54] = v_111;
  o[55] = v_112;
  o[56] = v_113;
  return 1;
}

INLINE Term spin_36(Env e, THR Term* o, u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16) {
  u32 wpoll = 0;
  u32 v_18 = 0;
  u32 v_19 = 0;
  u32 v_20 = 0;
  u32 v_21 = 0;
  u32 v_22 = 0;
  u32 v_23 = 0;
  u32 v_24 = 0;
  u32 v_25 = 0;
  u32 v_26 = 0;
  u32 s_9 = r0;
  u32 s_10 = r1;
  u32 s_11 = r2;
  u32 s_12 = r3;
  u32 s_13 = r4;
  u32 s_14 = r5;
  u32 s_15 = r6;
  u32 s_16 = r7;
  u32 s_17 = r8;
  u32 b_0 = r9;
  u32 b_1 = r10;
  u32 b_2 = r11;
  u32 b_3 = r12;
  u32 b_4 = r13;
  u32 ent_1 = r14;
  u32 atr_1 = r15;
  u32 k_rel_1 = r16;
  WL_SPIN
    u32 hit_s3_0 = ((u64)(f32_unbox(b_2) <= f32_unbox(f32_rewrap(f32_unbox(ent_1) - f32_unbox(f32_rewrap(f32_unbox(1077936128ull) * f32_unbox(atr_1)))))));
    u32 hit_s4_0 = ((u64)(f32_unbox(b_2) <= f32_unbox(f32_rewrap(f32_unbox(ent_1) - f32_unbox(f32_rewrap(f32_unbox(1082130432ull) * f32_unbox(atr_1)))))));
    u32 hit_th_0 = ((u64)(f32_unbox(b_1) >= f32_unbox(f32_rewrap(f32_unbox(ent_1) + f32_unbox(f32_rewrap(f32_unbox(1056964608ull) * f32_unbox(atr_1)))))));
    u32 hit_t2_0 = ((u64)(f32_unbox(b_1) >= f32_unbox(f32_rewrap(f32_unbox(ent_1) + f32_unbox(f32_rewrap(f32_unbox(1073741824ull) * f32_unbox(atr_1)))))));
    u32 hit_t3_0 = ((u64)(f32_unbox(b_1) >= f32_unbox(f32_rewrap(f32_unbox(ent_1) + f32_unbox(f32_rewrap(f32_unbox(1077936128ull) * f32_unbox(atr_1)))))));
    u32 d0_0 = U32_BIN(U32_BIN(s_9, &, 1ull), ==, 1ull);
    u32 d1_0 = U32_BIN(U32_BIN(s_9, &, 2ull), ==, 2ull);
    u32 d2_0 = U32_BIN(U32_BIN(s_9, &, 4ull), ==, 4ull);
    u32 d3_0 = U32_BIN(U32_BIN(s_9, &, 8ull), ==, 8ull);
    u32 in10_0 = U32_BIN(k_rel_1, <=, 10ull);
    u32 v_27 = 0;
    u32 v_28 = 0;
    Term o_0[1];
    if (spin_33(e, o_0, d0_0, hit_s4_0, hit_t2_0, s_10, f32_rewrap(f32_unbox(0ull) - f32_unbox(1082130432ull)), 1073741824ull) == 0) {
      return 0;
    }
    v_28 = o_0[0];
    v_27 = v_28;
    u32 v_29 = 0;
    u32 v_30 = 0;
    Term o_1[1];
    if (spin_33(e, o_1, d1_0, hit_s4_0, hit_t3_0, s_11, f32_rewrap(f32_unbox(0ull) - f32_unbox(1082130432ull)), 1077936128ull) == 0) {
      return 0;
    }
    v_30 = o_1[0];
    v_29 = v_30;
    u32 v_31 = 0;
    u32 v_32 = 0;
    u32 v_33 = 0;
    Term o_2[1];
    if (spin_20(e, o_2, in10_0) == 0) {
      return 0;
    }
    v_33 = o_2[0];
    v_32 = v_33;
    u32 v_34 = 0;
    Term o_3[1];
    if (spin_33(e, o_3, ((d2_0) | (v_32)), hit_s3_0, hit_th_0, s_12, f32_rewrap(f32_unbox(0ull) - f32_unbox(1077936128ull)), 1056964608ull) == 0) {
      return 0;
    }
    v_34 = o_3[0];
    v_31 = v_34;
    u32 v_35 = 0;
    u32 v_36 = 0;
    Term o_4[1];
    if (spin_33(e, o_4, d3_0, hit_s4_0, hit_th_0, s_13, f32_rewrap(f32_unbox(0ull) - f32_unbox(1082130432ull)), 1056964608ull) == 0) {
      return 0;
    }
    v_36 = o_4[0];
    v_35 = v_36;
    u32 v_37 = 0;
    u32 v_38 = 0;
    Term o_5[1];
    if (spin_8(e, o_5, ((d0_0) | (((hit_s4_0) | (hit_t2_0)))), 1ull) == 0) {
      return 0;
    }
    v_38 = o_5[0];
    v_37 = v_38;
    u32 v_39 = 0;
    u32 v_40 = 0;
    Term o_6[1];
    if (spin_8(e, o_6, ((d1_0) | (((hit_s4_0) | (hit_t3_0)))), 2ull) == 0) {
      return 0;
    }
    v_40 = o_6[0];
    v_39 = v_40;
    u32 v_41 = 0;
    u32 v_42 = 0;
    u32 v_43 = 0;
    Term o_7[1];
    if (spin_7(e, o_7, in10_0, ((hit_s3_0) | (hit_th_0))) == 0) {
      return 0;
    }
    v_43 = o_7[0];
    v_42 = v_43;
    u32 v_44 = 0;
    Term o_8[1];
    if (spin_8(e, o_8, ((d2_0) | (v_42)), 4ull) == 0) {
      return 0;
    }
    v_44 = o_8[0];
    v_41 = v_44;
    u32 v_45 = 0;
    u32 v_46 = 0;
    Term o_9[1];
    if (spin_8(e, o_9, ((d3_0) | (((hit_s4_0) | (hit_th_0)))), 8ull) == 0) {
      return 0;
    }
    v_46 = o_9[0];
    v_45 = v_46;
    u32 v_47 = 0;
    u32 v_48 = 0;
    Term o_10[1];
    if (spin_0(e, o_10, s_14, b_1) == 0) {
      return 0;
    }
    v_48 = o_10[0];
    v_47 = v_48;
    u32 v_49 = 0;
    u32 v_50 = 0;
    Term o_11[1];
    if (spin_9(e, o_11, s_15, b_2) == 0) {
      return 0;
    }
    v_50 = o_11[0];
    v_49 = v_50;
    u32 v_51 = 0;
    u32 v_52 = 0;
    Term o_12[1];
    if (spin_2(e, o_12, in10_0, b_3, s_16) == 0) {
      return 0;
    }
    v_52 = o_12[0];
    v_51 = v_52;
    v_18 = U32_BIN(U32_BIN(U32_BIN(v_37, |, v_39), |, v_41), |, v_45);
    v_19 = v_27;
    v_20 = v_29;
    v_21 = v_31;
    v_22 = v_35;
    v_23 = v_47;
    v_24 = v_49;
    v_25 = v_51;
    v_26 = b_3;
  break;
  }
  o[0] = v_18;
  o[1] = v_19;
  o[2] = v_20;
  o[3] = v_21;
  o[4] = v_22;
  o[5] = v_23;
  o[6] = v_24;
  o[7] = v_25;
  o[8] = v_26;
  return 1;
}

INLINE Term spin_37(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  Term v_4 = 0;
  u32 i_1 = r0;
  u32 n_0 = r1;
  WL_SPIN
    u32 v_5 = 0;
    u32 v_6 = 0;
    Term o_0[1];
    if (spin_3(e, o_0, U32_BIN(i_1, <, n_0), i_1, n_0) == 0) {
      return 0;
    }
    v_6 = o_0[0];
    v_5 = v_6;
    v_4 = v_5;
  break;
  }
  o[0] = v_4;
  return 1;
}

INLINE Term spin_38(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_9 = 0;
  u32 i_2 = r0;
  u32 n_1 = r1;
  WL_SPIN
    u32 v_10 = 0;
    Term o_2[1];
    if (spin_3(e, o_2, U32_BIN(i_2, <, n_1), 0ull, U32_BIN(i_2, -, n_1)) == 0) {
      return 0;
    }
    v_10 = o_2[0];
    v_9 = v_10;
  break;
  }
  o[0] = v_9;
  return 1;
}

INLINE Term spin_39(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3) {
  u32 wpoll = 0;
  Term v_13 = 0;
  Term v_14 = 0;
  Term n_2 = r0;
  u32 j_0 = r1;
  Term r_2 = r2;
  Term r_3 = r3;
  WL_SPIN
    if (n_2 == 0) {
      v_13 = r_2;
      v_14 = r_3;
    } else {
      Term p_0 = (n_2 - 1);
      Term v_15 = 0;
      Term v_16 = 0;
      Term v_17 = 0;
      Term v_18 = 0;
      Term o_4[2];
      if (spin_34(e, o_4, r_2, r_3, j_0) == 0) {
        return 0;
      }
      v_17 = o_4[0];
      v_18 = o_4[1];
      v_15 = v_17;
      v_16 = v_18;
      r0 = p_0;
      r1 = U32_BIN(j_0, +, 1ull);
      r2 = v_15;
      r3 = v_16;
      n_2 = r0;
      j_0 = r1;
      r_2 = r2;
      r_3 = r3;
      WL_AGAIN(spin_39);
    }
  break;
  }
  o[0] = v_13;
  o[1] = v_14;
  return 1;
}

INLINE Term spin_40(Env e, THR Term* o, Term r0, Term r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, Term r20, Term r21, Term r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64, u32 r65, u32 r66, u32 r67, u32 r68, u32 r69, u32 r70, u32 r71, u32 r72, u32 r73, u32 r74, u32 r75, u32 r76) {
  u32 wpoll = 0;
  Term v_78 = 0;
  Term v_79 = 0;
  Term v_80 = 0;
  Term v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  u32 v_128 = 0;
  u32 v_129 = 0;
  u32 v_130 = 0;
  u32 v_131 = 0;
  u32 v_132 = 0;
  u32 v_133 = 0;
  u32 v_134 = 0;
  Term r_4 = r0;
  Term r_5 = r1;
  u32 pos_out_1 = r2;
  u32 neg_out_0 = r3;
  u32 atr_piv_1 = r4;
  u32 old_5 = r5;
  u32 old_6 = r6;
  u32 old_7 = r7;
  u32 old_8 = r8;
  u32 old_9 = r9;
  u32 is_ph_1 = r10;
  u32 is_pl_1 = r11;
  u32 hh9_1 = r12;
  u32 ll9_1 = r13;
  u32 sma10_1 = r14;
  u32 b_5 = r15;
  u32 b_6 = r16;
  u32 b_7 = r17;
  u32 b_8 = r18;
  u32 b_9 = r19;
  Term chips_1 = r20;
  Term bars_1 = r21;
  Term out_1 = r22;
  u32 st_53 = r23;
  u32 st_54 = r24;
  u32 st_55 = r25;
  u32 st_56 = r26;
  u32 st_57 = r27;
  u32 st_58 = r28;
  u32 st_59 = r29;
  u32 st_60 = r30;
  u32 st_61 = r31;
  u32 st_62 = r32;
  u32 st_63 = r33;
  u32 st_64 = r34;
  u32 st_65 = r35;
  u32 st_66 = r36;
  u32 st_67 = r37;
  u32 st_68 = r38;
  u32 st_69 = r39;
  u32 st_70 = r40;
  u32 st_71 = r41;
  u32 st_72 = r42;
  u32 st_73 = r43;
  u32 st_74 = r44;
  u32 st_75 = r45;
  u32 st_76 = r46;
  u32 st_77 = r47;
  u32 st_78 = r48;
  u32 st_79 = r49;
  u32 st_80 = r50;
  u32 st_81 = r51;
  u32 st_82 = r52;
  u32 st_83 = r53;
  u32 st_84 = r54;
  u32 st_85 = r55;
  u32 st_86 = r56;
  u32 st_87 = r57;
  u32 st_88 = r58;
  u32 st_89 = r59;
  u32 st_90 = r60;
  u32 st_91 = r61;
  u32 st_92 = r62;
  u32 st_93 = r63;
  u32 st_94 = r64;
  u32 st_95 = r65;
  u32 st_96 = r66;
  u32 st_97 = r67;
  u32 st_98 = r68;
  u32 st_99 = r69;
  u32 st_100 = r70;
  u32 st_101 = r71;
  u32 st_102 = r72;
  u32 st_103 = r73;
  u32 st_104 = r74;
  u32 st_105 = r75;
  u32 i_3 = r76;
  WL_SPIN
    Term v_135 = 0;
    Term v_136 = 0;
    Term v_137 = 0;
    Term v_138 = 0;
    u32 v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    u32 v_185 = 0;
    u32 v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    u32 v_191 = 0;
    Term o_7[57];
    if (spin_35(e, o_7, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, b_5, b_6, b_7, b_8, b_9, old_5, old_6, old_7, old_8, old_9, r_5, atr_piv_1, pos_out_1, neg_out_0, is_ph_1, is_pl_1, hh9_1, ll9_1, sma10_1, chips_1, bars_1, r_4, out_1, i_3) == 0) {
      return 0;
    }
    v_135 = o_7[0];
    v_136 = o_7[1];
    v_137 = o_7[2];
    v_138 = o_7[3];
    v_139 = o_7[4];
    v_140 = o_7[5];
    v_141 = o_7[6];
    v_142 = o_7[7];
    v_143 = o_7[8];
    v_144 = o_7[9];
    v_145 = o_7[10];
    v_146 = o_7[11];
    v_147 = o_7[12];
    v_148 = o_7[13];
    v_149 = o_7[14];
    v_150 = o_7[15];
    v_151 = o_7[16];
    v_152 = o_7[17];
    v_153 = o_7[18];
    v_154 = o_7[19];
    v_155 = o_7[20];
    v_156 = o_7[21];
    v_157 = o_7[22];
    v_158 = o_7[23];
    v_159 = o_7[24];
    v_160 = o_7[25];
    v_161 = o_7[26];
    v_162 = o_7[27];
    v_163 = o_7[28];
    v_164 = o_7[29];
    v_165 = o_7[30];
    v_166 = o_7[31];
    v_167 = o_7[32];
    v_168 = o_7[33];
    v_169 = o_7[34];
    v_170 = o_7[35];
    v_171 = o_7[36];
    v_172 = o_7[37];
    v_173 = o_7[38];
    v_174 = o_7[39];
    v_175 = o_7[40];
    v_176 = o_7[41];
    v_177 = o_7[42];
    v_178 = o_7[43];
    v_179 = o_7[44];
    v_180 = o_7[45];
    v_181 = o_7[46];
    v_182 = o_7[47];
    v_183 = o_7[48];
    v_184 = o_7[49];
    v_185 = o_7[50];
    v_186 = o_7[51];
    v_187 = o_7[52];
    v_188 = o_7[53];
    v_189 = o_7[54];
    v_190 = o_7[55];
    v_191 = o_7[56];
    v_78 = v_135;
    v_79 = v_136;
    v_80 = v_137;
    v_81 = v_138;
    v_82 = v_139;
    v_83 = v_140;
    v_84 = v_141;
    v_85 = v_142;
    v_86 = v_143;
    v_87 = v_144;
    v_88 = v_145;
    v_89 = v_146;
    v_90 = v_147;
    v_91 = v_148;
    v_92 = v_149;
    v_93 = v_150;
    v_94 = v_151;
    v_95 = v_152;
    v_96 = v_153;
    v_97 = v_154;
    v_98 = v_155;
    v_99 = v_156;
    v_100 = v_157;
    v_101 = v_158;
    v_102 = v_159;
    v_103 = v_160;
    v_104 = v_161;
    v_105 = v_162;
    v_106 = v_163;
    v_107 = v_164;
    v_108 = v_165;
    v_109 = v_166;
    v_110 = v_167;
    v_111 = v_168;
    v_112 = v_169;
    v_113 = v_170;
    v_114 = v_171;
    v_115 = v_172;
    v_116 = v_173;
    v_117 = v_174;
    v_118 = v_175;
    v_119 = v_176;
    v_120 = v_177;
    v_121 = v_178;
    v_122 = v_179;
    v_123 = v_180;
    v_124 = v_181;
    v_125 = v_182;
    v_126 = v_183;
    v_127 = v_184;
    v_128 = v_185;
    v_129 = v_186;
    v_130 = v_187;
    v_131 = v_188;
    v_132 = v_189;
    v_133 = v_190;
    v_134 = v_191;
  break;
  }
  o[0] = v_78;
  o[1] = v_79;
  o[2] = v_80;
  o[3] = v_81;
  o[4] = v_82;
  o[5] = v_83;
  o[6] = v_84;
  o[7] = v_85;
  o[8] = v_86;
  o[9] = v_87;
  o[10] = v_88;
  o[11] = v_89;
  o[12] = v_90;
  o[13] = v_91;
  o[14] = v_92;
  o[15] = v_93;
  o[16] = v_94;
  o[17] = v_95;
  o[18] = v_96;
  o[19] = v_97;
  o[20] = v_98;
  o[21] = v_99;
  o[22] = v_100;
  o[23] = v_101;
  o[24] = v_102;
  o[25] = v_103;
  o[26] = v_104;
  o[27] = v_105;
  o[28] = v_106;
  o[29] = v_107;
  o[30] = v_108;
  o[31] = v_109;
  o[32] = v_110;
  o[33] = v_111;
  o[34] = v_112;
  o[35] = v_113;
  o[36] = v_114;
  o[37] = v_115;
  o[38] = v_116;
  o[39] = v_117;
  o[40] = v_118;
  o[41] = v_119;
  o[42] = v_120;
  o[43] = v_121;
  o[44] = v_122;
  o[45] = v_123;
  o[46] = v_124;
  o[47] = v_125;
  o[48] = v_126;
  o[49] = v_127;
  o[50] = v_128;
  o[51] = v_129;
  o[52] = v_130;
  o[53] = v_131;
  o[54] = v_132;
  o[55] = v_133;
  o[56] = v_134;
  return 1;
}

INLINE Term spin_42(Env e, THR Term* o, Term r0, u32 r1, u32 r2) {
  u32 wpoll = 0;
  Term v_22 = 0;
  u32 v_23 = 0;
  Term bars_1 = r0;
  u32 i_2 = r1;
  u32 k_1 = r2;
  WL_SPIN
    Term at_0 = blk_at(bars_1, U32_BIN(U32_BIN(i_2, *, 6ull), +, k_1), 0);
    u32 c_0 = blk_read(e.mem, 0, term_loc(bars_1), at_0 + 0);
    v_22 = bars_1;
    v_23 = c_0;
  break;
  }
  o[0] = v_22;
  o[1] = v_23;
  return 1;
}

INLINE Term spin_44(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  u32 v_42 = 0;
  u32 w_0 = r0;
  WL_SPIN
    v_42 = f32_rewrap(f32_unbox(f32_rewrap((f32)(u32)(w_0))) / f32_unbox(1176256512ull));
  break;
  }
  o[0] = v_42;
  return 1;
}

INLINE Term spin_48(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
  u32 wpoll = 0;
  Term v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  Term r_18 = r0;
  u32 r_19 = r1;
  u32 o_12 = r2;
  u32 h_2 = r3;
  u32 l_1 = r4;
  u32 c_1 = r5;
  WL_SPIN
    v_103 = r_18;
    v_104 = o_12;
    v_105 = h_2;
    v_106 = l_1;
    v_107 = c_1;
    v_108 = f32_rewrap((f32)(u32)(r_19));
  break;
  }
  o[0] = v_103;
  o[1] = v_104;
  o[2] = v_105;
  o[3] = v_106;
  o[4] = v_107;
  o[5] = v_108;
  return 1;
}

INLINE Term spin_47(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
  u32 wpoll = 0;
  Term v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  Term r_16 = r0;
  u32 r_17 = r1;
  u32 o_9 = r2;
  u32 h_1 = r3;
  u32 l_0 = r4;
  u32 i_6 = r5;
  WL_SPIN
    Term v_91 = 0;
    u32 v_92 = 0;
    Term v_93 = 0;
    u32 v_94 = 0;
    Term o_10[2];
    if (spin_42(e, o_10, r_16, i_6, 5ull) == 0) {
      return 0;
    }
    v_93 = o_10[0];
    v_94 = o_10[1];
    v_91 = v_93;
    v_92 = v_94;
    u32 v_95 = 0;
    u32 v_96 = 0;
    Term o_11[1];
    if (spin_44(e, o_11, r_17) == 0) {
      return 0;
    }
    v_96 = o_11[0];
    v_95 = v_96;
    Term v_97 = 0;
    u32 v_98 = 0;
    u32 v_99 = 0;
    u32 v_100 = 0;
    u32 v_101 = 0;
    u32 v_102 = 0;
    Term o_13[6];
    if (spin_48(e, o_13, v_91, v_92, o_9, h_1, l_0, v_95) == 0) {
      return 0;
    }
    v_97 = o_13[0];
    v_98 = o_13[1];
    v_99 = o_13[2];
    v_100 = o_13[3];
    v_101 = o_13[4];
    v_102 = o_13[5];
    v_85 = v_97;
    v_86 = v_98;
    v_87 = v_99;
    v_88 = v_100;
    v_89 = v_101;
    v_90 = v_102;
  break;
  }
  o[0] = v_85;
  o[1] = v_86;
  o[2] = v_87;
  o[3] = v_88;
  o[4] = v_89;
  o[5] = v_90;
  return 1;
}

INLINE Term spin_46(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4) {
  u32 wpoll = 0;
  Term v_67 = 0;
  u32 v_68 = 0;
  u32 v_69 = 0;
  u32 v_70 = 0;
  u32 v_71 = 0;
  u32 v_72 = 0;
  Term r_14 = r0;
  u32 r_15 = r1;
  u32 o_6 = r2;
  u32 h_0 = r3;
  u32 i_5 = r4;
  WL_SPIN
    Term v_73 = 0;
    u32 v_74 = 0;
    Term v_75 = 0;
    u32 v_76 = 0;
    Term o_7[2];
    if (spin_42(e, o_7, r_14, i_5, 4ull) == 0) {
      return 0;
    }
    v_75 = o_7[0];
    v_76 = o_7[1];
    v_73 = v_75;
    v_74 = v_76;
    u32 v_77 = 0;
    u32 v_78 = 0;
    Term o_8[1];
    if (spin_44(e, o_8, r_15) == 0) {
      return 0;
    }
    v_78 = o_8[0];
    v_77 = v_78;
    Term v_79 = 0;
    u32 v_80 = 0;
    u32 v_81 = 0;
    u32 v_82 = 0;
    u32 v_83 = 0;
    u32 v_84 = 0;
    Term o_14[6];
    if (spin_47(e, o_14, v_73, v_74, o_6, h_0, v_77, i_5) == 0) {
      return 0;
    }
    v_79 = o_14[0];
    v_80 = o_14[1];
    v_81 = o_14[2];
    v_82 = o_14[3];
    v_83 = o_14[4];
    v_84 = o_14[5];
    v_67 = v_79;
    v_68 = v_80;
    v_69 = v_81;
    v_70 = v_82;
    v_71 = v_83;
    v_72 = v_84;
  break;
  }
  o[0] = v_67;
  o[1] = v_68;
  o[2] = v_69;
  o[3] = v_70;
  o[4] = v_71;
  o[5] = v_72;
  return 1;
}

INLINE Term spin_45(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3) {
  u32 wpoll = 0;
  Term v_49 = 0;
  u32 v_50 = 0;
  u32 v_51 = 0;
  u32 v_52 = 0;
  u32 v_53 = 0;
  u32 v_54 = 0;
  Term r_12 = r0;
  u32 r_13 = r1;
  u32 o_3 = r2;
  u32 i_4 = r3;
  WL_SPIN
    Term v_55 = 0;
    u32 v_56 = 0;
    Term v_57 = 0;
    u32 v_58 = 0;
    Term o_4[2];
    if (spin_42(e, o_4, r_12, i_4, 3ull) == 0) {
      return 0;
    }
    v_57 = o_4[0];
    v_58 = o_4[1];
    v_55 = v_57;
    v_56 = v_58;
    u32 v_59 = 0;
    u32 v_60 = 0;
    Term o_5[1];
    if (spin_44(e, o_5, r_13) == 0) {
      return 0;
    }
    v_60 = o_5[0];
    v_59 = v_60;
    Term v_61 = 0;
    u32 v_62 = 0;
    u32 v_63 = 0;
    u32 v_64 = 0;
    u32 v_65 = 0;
    u32 v_66 = 0;
    Term o_15[6];
    if (spin_46(e, o_15, v_55, v_56, o_3, v_59, i_4) == 0) {
      return 0;
    }
    v_61 = o_15[0];
    v_62 = o_15[1];
    v_63 = o_15[2];
    v_64 = o_15[3];
    v_65 = o_15[4];
    v_66 = o_15[5];
    v_49 = v_61;
    v_50 = v_62;
    v_51 = v_63;
    v_52 = v_64;
    v_53 = v_65;
    v_54 = v_66;
  break;
  }
  o[0] = v_49;
  o[1] = v_50;
  o[2] = v_51;
  o[3] = v_52;
  o[4] = v_53;
  o[5] = v_54;
  return 1;
}

INLINE Term spin_43(Env e, THR Term* o, Term r0, u32 r1, u32 r2) {
  u32 wpoll = 0;
  Term v_30 = 0;
  u32 v_31 = 0;
  u32 v_32 = 0;
  u32 v_33 = 0;
  u32 v_34 = 0;
  u32 v_35 = 0;
  Term r_10 = r0;
  u32 r_11 = r1;
  u32 i_3 = r2;
  WL_SPIN
    Term v_36 = 0;
    u32 v_37 = 0;
    Term v_38 = 0;
    u32 v_39 = 0;
    Term o_1[2];
    if (spin_42(e, o_1, r_10, i_3, 2ull) == 0) {
      return 0;
    }
    v_38 = o_1[0];
    v_39 = o_1[1];
    v_36 = v_38;
    v_37 = v_39;
    u32 v_40 = 0;
    u32 v_41 = 0;
    Term o_2[1];
    if (spin_44(e, o_2, r_11) == 0) {
      return 0;
    }
    v_41 = o_2[0];
    v_40 = v_41;
    Term v_43 = 0;
    u32 v_44 = 0;
    u32 v_45 = 0;
    u32 v_46 = 0;
    u32 v_47 = 0;
    u32 v_48 = 0;
    Term o_16[6];
    if (spin_45(e, o_16, v_36, v_37, v_40, i_3) == 0) {
      return 0;
    }
    v_43 = o_16[0];
    v_44 = o_16[1];
    v_45 = o_16[2];
    v_46 = o_16[3];
    v_47 = o_16[4];
    v_48 = o_16[5];
    v_30 = v_43;
    v_31 = v_44;
    v_32 = v_45;
    v_33 = v_46;
    v_34 = v_47;
    v_35 = v_48;
  break;
  }
  o[0] = v_30;
  o[1] = v_31;
  o[2] = v_32;
  o[3] = v_33;
  o[4] = v_34;
  o[5] = v_35;
  return 1;
}

INLINE Term spin_41(Env e, THR Term* o, Term r0, u32 r1) {
  u32 wpoll = 0;
  Term v_12 = 0;
  u32 v_13 = 0;
  u32 v_14 = 0;
  u32 v_15 = 0;
  u32 v_16 = 0;
  u32 v_17 = 0;
  Term bars_0 = r0;
  u32 i_1 = r1;
  WL_SPIN
    Term v_18 = 0;
    u32 v_19 = 0;
    Term v_20 = 0;
    u32 v_21 = 0;
    Term o_0[2];
    if (spin_42(e, o_0, bars_0, i_1, 1ull) == 0) {
      return 0;
    }
    v_20 = o_0[0];
    v_21 = o_0[1];
    v_18 = v_20;
    v_19 = v_21;
    Term v_24 = 0;
    u32 v_25 = 0;
    u32 v_26 = 0;
    u32 v_27 = 0;
    u32 v_28 = 0;
    u32 v_29 = 0;
    Term o_17[6];
    if (spin_43(e, o_17, v_18, v_19, i_1) == 0) {
      return 0;
    }
    v_24 = o_17[0];
    v_25 = o_17[1];
    v_26 = o_17[2];
    v_27 = o_17[3];
    v_28 = o_17[4];
    v_29 = o_17[5];
    v_12 = v_24;
    v_13 = v_25;
    v_14 = v_26;
    v_15 = v_27;
    v_16 = v_28;
    v_17 = v_29;
  break;
  }
  o[0] = v_12;
  o[1] = v_13;
  o[2] = v_14;
  o[3] = v_15;
  o[4] = v_16;
  o[5] = v_17;
  return 1;
}

INLINE Term spin_49(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17) {
  u32 wpoll = 0;
  Term v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  u32 v_128 = 0;
  Term r_20 = r0;
  u32 r_21 = r1;
  u32 r_22 = r2;
  u32 r_23 = r3;
  u32 r_24 = r4;
  u32 r_25 = r5;
  u32 s_0 = r6;
  u32 s_1 = r7;
  u32 s_2 = r8;
  u32 s_3 = r9;
  u32 s_4 = r10;
  u32 s_5 = r11;
  u32 s_6 = r12;
  u32 s_7 = r13;
  u32 s_8 = r14;
  u32 ent_1 = r15;
  u32 atr_1 = r16;
  u32 k_rel_0 = r17;
  WL_SPIN
    u32 v_129 = 0;
    u32 v_130 = 0;
    u32 v_131 = 0;
    u32 v_132 = 0;
    u32 v_133 = 0;
    u32 v_134 = 0;
    u32 v_135 = 0;
    u32 v_136 = 0;
    u32 v_137 = 0;
    u32 v_138 = 0;
    u32 v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    Term o_19[9];
    if (spin_36(e, o_19, s_0, s_1, s_2, s_3, s_4, s_5, s_6, s_7, s_8, r_21, r_22, r_23, r_24, r_25, ent_1, atr_1, k_rel_0) == 0) {
      return 0;
    }
    v_138 = o_19[0];
    v_139 = o_19[1];
    v_140 = o_19[2];
    v_141 = o_19[3];
    v_142 = o_19[4];
    v_143 = o_19[5];
    v_144 = o_19[6];
    v_145 = o_19[7];
    v_146 = o_19[8];
    v_129 = v_138;
    v_130 = v_139;
    v_131 = v_140;
    v_132 = v_141;
    v_133 = v_142;
    v_134 = v_143;
    v_135 = v_144;
    v_136 = v_145;
    v_137 = v_146;
    v_119 = r_20;
    v_120 = v_129;
    v_121 = v_130;
    v_122 = v_131;
    v_123 = v_132;
    v_124 = v_133;
    v_125 = v_134;
    v_126 = v_135;
    v_127 = v_136;
    v_128 = v_137;
  break;
  }
  o[0] = v_119;
  o[1] = v_120;
  o[2] = v_121;
  o[3] = v_122;
  o[4] = v_123;
  o[5] = v_124;
  o[6] = v_125;
  o[7] = v_126;
  o[8] = v_127;
  o[9] = v_128;
  return 1;
}

INLINE Term spin_50(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, Term r19, Term r20, Term r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64, u32 r65, u32 r66, u32 r67, u32 r68, u32 r69, u32 r70, u32 r71, u32 r72, u32 r73, u32 r74, u32 r75) {
  u32 wpoll = 0;
  Term v_61 = 0;
  Term v_62 = 0;
  Term v_63 = 0;
  Term v_64 = 0;
  u32 v_65 = 0;
  u32 v_66 = 0;
  u32 v_67 = 0;
  u32 v_68 = 0;
  u32 v_69 = 0;
  u32 v_70 = 0;
  u32 v_71 = 0;
  u32 v_72 = 0;
  u32 v_73 = 0;
  u32 v_74 = 0;
  u32 v_75 = 0;
  u32 v_76 = 0;
  u32 v_77 = 0;
  u32 v_78 = 0;
  u32 v_79 = 0;
  u32 v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  u32 pos_out_0 = r2;
  u32 atr_piv_1 = r3;
  u32 old_5 = r4;
  u32 old_6 = r5;
  u32 old_7 = r6;
  u32 old_8 = r7;
  u32 old_9 = r8;
  u32 is_ph_1 = r9;
  u32 is_pl_1 = r10;
  u32 hh9_1 = r11;
  u32 ll9_1 = r12;
  u32 sma10_1 = r13;
  u32 b_5 = r14;
  u32 b_6 = r15;
  u32 b_7 = r16;
  u32 b_8 = r17;
  u32 b_9 = r18;
  Term chips_1 = r19;
  Term bars_1 = r20;
  Term out_1 = r21;
  u32 st_53 = r22;
  u32 st_54 = r23;
  u32 st_55 = r24;
  u32 st_56 = r25;
  u32 st_57 = r26;
  u32 st_58 = r27;
  u32 st_59 = r28;
  u32 st_60 = r29;
  u32 st_61 = r30;
  u32 st_62 = r31;
  u32 st_63 = r32;
  u32 st_64 = r33;
  u32 st_65 = r34;
  u32 st_66 = r35;
  u32 st_67 = r36;
  u32 st_68 = r37;
  u32 st_69 = r38;
  u32 st_70 = r39;
  u32 st_71 = r40;
  u32 st_72 = r41;
  u32 st_73 = r42;
  u32 st_74 = r43;
  u32 st_75 = r44;
  u32 st_76 = r45;
  u32 st_77 = r46;
  u32 st_78 = r47;
  u32 st_79 = r48;
  u32 st_80 = r49;
  u32 st_81 = r50;
  u32 st_82 = r51;
  u32 st_83 = r52;
  u32 st_84 = r53;
  u32 st_85 = r54;
  u32 st_86 = r55;
  u32 st_87 = r56;
  u32 st_88 = r57;
  u32 st_89 = r58;
  u32 st_90 = r59;
  u32 st_91 = r60;
  u32 st_92 = r61;
  u32 st_93 = r62;
  u32 st_94 = r63;
  u32 st_95 = r64;
  u32 st_96 = r65;
  u32 st_97 = r66;
  u32 st_98 = r67;
  u32 st_99 = r68;
  u32 st_100 = r69;
  u32 st_101 = r70;
  u32 st_102 = r71;
  u32 st_103 = r72;
  u32 st_104 = r73;
  u32 st_105 = r74;
  u32 i_1 = r75;
  WL_SPIN
    Term v_118 = 0;
    Term v_119 = 0;
    Term v_120 = 0;
    Term v_121 = 0;
    Term o_2[1];
    if (spin_37(e, o_2, i_1, 5ull) == 0) {
      return 0;
    }
    v_121 = o_2[0];
    v_120 = v_121;
    u32 v_122 = 0;
    u32 v_123 = 0;
    Term o_3[1];
    if (spin_38(e, o_3, i_1, 5ull) == 0) {
      return 0;
    }
    v_123 = o_3[0];
    v_122 = v_123;
    Term v_124 = 0;
    Term v_125 = 0;
    Term o_4[2];
    if (spin_39(e, o_4, v_120, v_122, r_2, term_pak(CID_NIL, 0)) == 0) {
      return 0;
    }
    v_124 = o_4[0];
    v_125 = o_4[1];
    v_118 = v_124;
    v_119 = v_125;
    u32 v_126 = 0;
    u32 v_127 = 0;
    Term o_5[1];
    if (spin_2(e, o_5, U32_BIN(i_1, >=, 14ull), r_3, 0ull) == 0) {
      return 0;
    }
    v_127 = o_5[0];
    v_126 = v_127;
    Term v_128 = 0;
    Term v_129 = 0;
    Term v_130 = 0;
    Term v_131 = 0;
    u32 v_132 = 0;
    u32 v_133 = 0;
    u32 v_134 = 0;
    u32 v_135 = 0;
    u32 v_136 = 0;
    u32 v_137 = 0;
    u32 v_138 = 0;
    u32 v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    Term o_6[57];
    if (spin_40(e, o_6, v_118, v_119, pos_out_0, v_126, atr_piv_1, old_5, old_6, old_7, old_8, old_9, is_ph_1, is_pl_1, hh9_1, ll9_1, sma10_1, b_5, b_6, b_7, b_8, b_9, chips_1, bars_1, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_1) == 0) {
      return 0;
    }
    v_128 = o_6[0];
    v_129 = o_6[1];
    v_130 = o_6[2];
    v_131 = o_6[3];
    v_132 = o_6[4];
    v_133 = o_6[5];
    v_134 = o_6[6];
    v_135 = o_6[7];
    v_136 = o_6[8];
    v_137 = o_6[9];
    v_138 = o_6[10];
    v_139 = o_6[11];
    v_140 = o_6[12];
    v_141 = o_6[13];
    v_142 = o_6[14];
    v_143 = o_6[15];
    v_144 = o_6[16];
    v_145 = o_6[17];
    v_146 = o_6[18];
    v_147 = o_6[19];
    v_148 = o_6[20];
    v_149 = o_6[21];
    v_150 = o_6[22];
    v_151 = o_6[23];
    v_152 = o_6[24];
    v_153 = o_6[25];
    v_154 = o_6[26];
    v_155 = o_6[27];
    v_156 = o_6[28];
    v_157 = o_6[29];
    v_158 = o_6[30];
    v_159 = o_6[31];
    v_160 = o_6[32];
    v_161 = o_6[33];
    v_162 = o_6[34];
    v_163 = o_6[35];
    v_164 = o_6[36];
    v_165 = o_6[37];
    v_166 = o_6[38];
    v_167 = o_6[39];
    v_168 = o_6[40];
    v_169 = o_6[41];
    v_170 = o_6[42];
    v_171 = o_6[43];
    v_172 = o_6[44];
    v_173 = o_6[45];
    v_174 = o_6[46];
    v_175 = o_6[47];
    v_176 = o_6[48];
    v_177 = o_6[49];
    v_178 = o_6[50];
    v_179 = o_6[51];
    v_180 = o_6[52];
    v_181 = o_6[53];
    v_182 = o_6[54];
    v_183 = o_6[55];
    v_184 = o_6[56];
    v_61 = v_128;
    v_62 = v_129;
    v_63 = v_130;
    v_64 = v_131;
    v_65 = v_132;
    v_66 = v_133;
    v_67 = v_134;
    v_68 = v_135;
    v_69 = v_136;
    v_70 = v_137;
    v_71 = v_138;
    v_72 = v_139;
    v_73 = v_140;
    v_74 = v_141;
    v_75 = v_142;
    v_76 = v_143;
    v_77 = v_144;
    v_78 = v_145;
    v_79 = v_146;
    v_80 = v_147;
    v_81 = v_148;
    v_82 = v_149;
    v_83 = v_150;
    v_84 = v_151;
    v_85 = v_152;
    v_86 = v_153;
    v_87 = v_154;
    v_88 = v_155;
    v_89 = v_156;
    v_90 = v_157;
    v_91 = v_158;
    v_92 = v_159;
    v_93 = v_160;
    v_94 = v_161;
    v_95 = v_162;
    v_96 = v_163;
    v_97 = v_164;
    v_98 = v_165;
    v_99 = v_166;
    v_100 = v_167;
    v_101 = v_168;
    v_102 = v_169;
    v_103 = v_170;
    v_104 = v_171;
    v_105 = v_172;
    v_106 = v_173;
    v_107 = v_174;
    v_108 = v_175;
    v_109 = v_176;
    v_110 = v_177;
    v_111 = v_178;
    v_112 = v_179;
    v_113 = v_180;
    v_114 = v_181;
    v_115 = v_182;
    v_116 = v_183;
    v_117 = v_184;
  break;
  }
  o[0] = v_61;
  o[1] = v_62;
  o[2] = v_63;
  o[3] = v_64;
  o[4] = v_65;
  o[5] = v_66;
  o[6] = v_67;
  o[7] = v_68;
  o[8] = v_69;
  o[9] = v_70;
  o[10] = v_71;
  o[11] = v_72;
  o[12] = v_73;
  o[13] = v_74;
  o[14] = v_75;
  o[15] = v_76;
  o[16] = v_77;
  o[17] = v_78;
  o[18] = v_79;
  o[19] = v_80;
  o[20] = v_81;
  o[21] = v_82;
  o[22] = v_83;
  o[23] = v_84;
  o[24] = v_85;
  o[25] = v_86;
  o[26] = v_87;
  o[27] = v_88;
  o[28] = v_89;
  o[29] = v_90;
  o[30] = v_91;
  o[31] = v_92;
  o[32] = v_93;
  o[33] = v_94;
  o[34] = v_95;
  o[35] = v_96;
  o[36] = v_97;
  o[37] = v_98;
  o[38] = v_99;
  o[39] = v_100;
  o[40] = v_101;
  o[41] = v_102;
  o[42] = v_103;
  o[43] = v_104;
  o[44] = v_105;
  o[45] = v_106;
  o[46] = v_107;
  o[47] = v_108;
  o[48] = v_109;
  o[49] = v_110;
  o[50] = v_111;
  o[51] = v_112;
  o[52] = v_113;
  o[53] = v_114;
  o[54] = v_115;
  o[55] = v_116;
  o[56] = v_117;
  return 1;
}

INLINE Term spin_51(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13) {
  u32 wpoll = 0;
  Term v_20 = 0;
  u32 v_21 = 0;
  u32 v_22 = 0;
  u32 v_23 = 0;
  u32 v_24 = 0;
  u32 v_25 = 0;
  u32 v_26 = 0;
  u32 v_27 = 0;
  u32 v_28 = 0;
  u32 v_29 = 0;
  Term r_10 = r0;
  u32 r_11 = r1;
  u32 r_12 = r2;
  u32 r_13 = r3;
  u32 r_14 = r4;
  u32 r_15 = r5;
  u32 r_16 = r6;
  u32 r_17 = r7;
  u32 r_18 = r8;
  u32 r_19 = r9;
  u32 k_1 = r10;
  u32 ent_1 = r11;
  u32 atr_1 = r12;
  u32 i_1 = r13;
  WL_SPIN
    Term v_30 = 0;
    u32 v_31 = 0;
    u32 v_32 = 0;
    u32 v_33 = 0;
    u32 v_34 = 0;
    u32 v_35 = 0;
    Term v_36 = 0;
    u32 v_37 = 0;
    u32 v_38 = 0;
    u32 v_39 = 0;
    u32 v_40 = 0;
    u32 v_41 = 0;
    Term o_0[6];
    if (spin_41(e, o_0, r_10, k_1) == 0) {
      return 0;
    }
    v_36 = o_0[0];
    v_37 = o_0[1];
    v_38 = o_0[2];
    v_39 = o_0[3];
    v_40 = o_0[4];
    v_41 = o_0[5];
    v_30 = v_36;
    v_31 = v_37;
    v_32 = v_38;
    v_33 = v_39;
    v_34 = v_40;
    v_35 = v_41;
    Term v_42 = 0;
    u32 v_43 = 0;
    u32 v_44 = 0;
    u32 v_45 = 0;
    u32 v_46 = 0;
    u32 v_47 = 0;
    u32 v_48 = 0;
    u32 v_49 = 0;
    u32 v_50 = 0;
    u32 v_51 = 0;
    Term o_1[10];
    if (spin_49(e, o_1, v_30, v_31, v_32, v_33, v_34, v_35, r_11, r_12, r_13, r_14, r_15, r_16, r_17, r_18, r_19, ent_1, atr_1, U32_BIN(k_1, -, i_1)) == 0) {
      return 0;
    }
    v_42 = o_1[0];
    v_43 = o_1[1];
    v_44 = o_1[2];
    v_45 = o_1[3];
    v_46 = o_1[4];
    v_47 = o_1[5];
    v_48 = o_1[6];
    v_49 = o_1[7];
    v_50 = o_1[8];
    v_51 = o_1[9];
    v_20 = v_42;
    v_21 = v_43;
    v_22 = v_44;
    v_23 = v_45;
    v_24 = v_46;
    v_25 = v_47;
    v_26 = v_48;
    v_27 = v_49;
    v_28 = v_50;
    v_29 = v_51;
  break;
  }
  o[0] = v_20;
  o[1] = v_21;
  o[2] = v_22;
  o[3] = v_23;
  o[4] = v_24;
  o[5] = v_25;
  o[6] = v_26;
  o[7] = v_27;
  o[8] = v_28;
  o[9] = v_29;
  return 1;
}

INLINE Term spin_52(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  u32 v_10 = 0;
  u32 x_0 = r0;
  WL_SPIN
    v_10 = f32_to_u32(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(x_0) + f32_unbox(1176256512ull))) * f32_unbox(1120403456ull)));
  break;
  }
  o[0] = v_10;
  return 1;
}

INLINE Term spin_53(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, Term r18, Term r19, Term r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64, u32 r65, u32 r66, u32 r67, u32 r68, u32 r69, u32 r70, u32 r71, u32 r72, u32 r73, u32 r74) {
  u32 wpoll = 0;
  Term v_59 = 0;
  Term v_60 = 0;
  Term v_61 = 0;
  Term v_62 = 0;
  u32 v_63 = 0;
  u32 v_64 = 0;
  u32 v_65 = 0;
  u32 v_66 = 0;
  u32 v_67 = 0;
  u32 v_68 = 0;
  u32 v_69 = 0;
  u32 v_70 = 0;
  u32 v_71 = 0;
  u32 v_72 = 0;
  u32 v_73 = 0;
  u32 v_74 = 0;
  u32 v_75 = 0;
  u32 v_76 = 0;
  u32 v_77 = 0;
  u32 v_78 = 0;
  u32 v_79 = 0;
  u32 v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  u32 atr_piv_0 = r2;
  u32 old_5 = r3;
  u32 old_6 = r4;
  u32 old_7 = r5;
  u32 old_8 = r6;
  u32 old_9 = r7;
  u32 is_ph_1 = r8;
  u32 is_pl_1 = r9;
  u32 hh9_1 = r10;
  u32 ll9_1 = r11;
  u32 sma10_1 = r12;
  u32 b_5 = r13;
  u32 b_6 = r14;
  u32 b_7 = r15;
  u32 b_8 = r16;
  u32 b_9 = r17;
  Term chips_1 = r18;
  Term bars_1 = r19;
  Term out_1 = r20;
  u32 st_53 = r21;
  u32 st_54 = r22;
  u32 st_55 = r23;
  u32 st_56 = r24;
  u32 st_57 = r25;
  u32 st_58 = r26;
  u32 st_59 = r27;
  u32 st_60 = r28;
  u32 st_61 = r29;
  u32 st_62 = r30;
  u32 st_63 = r31;
  u32 st_64 = r32;
  u32 st_65 = r33;
  u32 st_66 = r34;
  u32 st_67 = r35;
  u32 st_68 = r36;
  u32 st_69 = r37;
  u32 st_70 = r38;
  u32 st_71 = r39;
  u32 st_72 = r40;
  u32 st_73 = r41;
  u32 st_74 = r42;
  u32 st_75 = r43;
  u32 st_76 = r44;
  u32 st_77 = r45;
  u32 st_78 = r46;
  u32 st_79 = r47;
  u32 st_80 = r48;
  u32 st_81 = r49;
  u32 st_82 = r50;
  u32 st_83 = r51;
  u32 st_84 = r52;
  u32 st_85 = r53;
  u32 st_86 = r54;
  u32 st_87 = r55;
  u32 st_88 = r56;
  u32 st_89 = r57;
  u32 st_90 = r58;
  u32 st_91 = r59;
  u32 st_92 = r60;
  u32 st_93 = r61;
  u32 st_94 = r62;
  u32 st_95 = r63;
  u32 st_96 = r64;
  u32 st_97 = r65;
  u32 st_98 = r66;
  u32 st_99 = r67;
  u32 st_100 = r68;
  u32 st_101 = r69;
  u32 st_102 = r70;
  u32 st_103 = r71;
  u32 st_104 = r72;
  u32 st_105 = r73;
  u32 i_1 = r74;
  WL_SPIN
    u32 v_116 = 0;
    u32 v_117 = 0;
    Term o_1[1];
    if (spin_3(e, o_1, U32_BIN(i_1, <, 14ull), 0ull, U32_BIN(i_1, -, 14ull)) == 0) {
      return 0;
    }
    v_117 = o_1[0];
    v_116 = v_117;
    Term at_1 = blk_at(r_2, U32_BIN(U32_BIN(v_116, *, 4ull), +, 3ull), 0);
    u32 c_1 = blk_read(e.mem, 0, term_loc(r_2), at_1 + 0);
    u32 v_118 = 0;
    u32 v_119 = 0;
    Term o_2[1];
    if (spin_2(e, o_2, U32_BIN(i_1, >=, 14ull), r_3, 0ull) == 0) {
      return 0;
    }
    v_119 = o_2[0];
    v_118 = v_119;
    Term v_120 = 0;
    Term v_121 = 0;
    Term v_122 = 0;
    Term v_123 = 0;
    u32 v_124 = 0;
    u32 v_125 = 0;
    u32 v_126 = 0;
    u32 v_127 = 0;
    u32 v_128 = 0;
    u32 v_129 = 0;
    u32 v_130 = 0;
    u32 v_131 = 0;
    u32 v_132 = 0;
    u32 v_133 = 0;
    u32 v_134 = 0;
    u32 v_135 = 0;
    u32 v_136 = 0;
    u32 v_137 = 0;
    u32 v_138 = 0;
    u32 v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    Term o_3[57];
    if (spin_50(e, o_3, r_2, c_1, v_118, atr_piv_0, old_5, old_6, old_7, old_8, old_9, is_ph_1, is_pl_1, hh9_1, ll9_1, sma10_1, b_5, b_6, b_7, b_8, b_9, chips_1, bars_1, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_1) == 0) {
      return 0;
    }
    v_120 = o_3[0];
    v_121 = o_3[1];
    v_122 = o_3[2];
    v_123 = o_3[3];
    v_124 = o_3[4];
    v_125 = o_3[5];
    v_126 = o_3[6];
    v_127 = o_3[7];
    v_128 = o_3[8];
    v_129 = o_3[9];
    v_130 = o_3[10];
    v_131 = o_3[11];
    v_132 = o_3[12];
    v_133 = o_3[13];
    v_134 = o_3[14];
    v_135 = o_3[15];
    v_136 = o_3[16];
    v_137 = o_3[17];
    v_138 = o_3[18];
    v_139 = o_3[19];
    v_140 = o_3[20];
    v_141 = o_3[21];
    v_142 = o_3[22];
    v_143 = o_3[23];
    v_144 = o_3[24];
    v_145 = o_3[25];
    v_146 = o_3[26];
    v_147 = o_3[27];
    v_148 = o_3[28];
    v_149 = o_3[29];
    v_150 = o_3[30];
    v_151 = o_3[31];
    v_152 = o_3[32];
    v_153 = o_3[33];
    v_154 = o_3[34];
    v_155 = o_3[35];
    v_156 = o_3[36];
    v_157 = o_3[37];
    v_158 = o_3[38];
    v_159 = o_3[39];
    v_160 = o_3[40];
    v_161 = o_3[41];
    v_162 = o_3[42];
    v_163 = o_3[43];
    v_164 = o_3[44];
    v_165 = o_3[45];
    v_166 = o_3[46];
    v_167 = o_3[47];
    v_168 = o_3[48];
    v_169 = o_3[49];
    v_170 = o_3[50];
    v_171 = o_3[51];
    v_172 = o_3[52];
    v_173 = o_3[53];
    v_174 = o_3[54];
    v_175 = o_3[55];
    v_176 = o_3[56];
    v_59 = v_120;
    v_60 = v_121;
    v_61 = v_122;
    v_62 = v_123;
    v_63 = v_124;
    v_64 = v_125;
    v_65 = v_126;
    v_66 = v_127;
    v_67 = v_128;
    v_68 = v_129;
    v_69 = v_130;
    v_70 = v_131;
    v_71 = v_132;
    v_72 = v_133;
    v_73 = v_134;
    v_74 = v_135;
    v_75 = v_136;
    v_76 = v_137;
    v_77 = v_138;
    v_78 = v_139;
    v_79 = v_140;
    v_80 = v_141;
    v_81 = v_142;
    v_82 = v_143;
    v_83 = v_144;
    v_84 = v_145;
    v_85 = v_146;
    v_86 = v_147;
    v_87 = v_148;
    v_88 = v_149;
    v_89 = v_150;
    v_90 = v_151;
    v_91 = v_152;
    v_92 = v_153;
    v_93 = v_154;
    v_94 = v_155;
    v_95 = v_156;
    v_96 = v_157;
    v_97 = v_158;
    v_98 = v_159;
    v_99 = v_160;
    v_100 = v_161;
    v_101 = v_162;
    v_102 = v_163;
    v_103 = v_164;
    v_104 = v_165;
    v_105 = v_166;
    v_106 = v_167;
    v_107 = v_168;
    v_108 = v_169;
    v_109 = v_170;
    v_110 = v_171;
    v_111 = v_172;
    v_112 = v_173;
    v_113 = v_174;
    v_114 = v_175;
    v_115 = v_176;
  break;
  }
  o[0] = v_59;
  o[1] = v_60;
  o[2] = v_61;
  o[3] = v_62;
  o[4] = v_63;
  o[5] = v_64;
  o[6] = v_65;
  o[7] = v_66;
  o[8] = v_67;
  o[9] = v_68;
  o[10] = v_69;
  o[11] = v_70;
  o[12] = v_71;
  o[13] = v_72;
  o[14] = v_73;
  o[15] = v_74;
  o[16] = v_75;
  o[17] = v_76;
  o[18] = v_77;
  o[19] = v_78;
  o[20] = v_79;
  o[21] = v_80;
  o[22] = v_81;
  o[23] = v_82;
  o[24] = v_83;
  o[25] = v_84;
  o[26] = v_85;
  o[27] = v_86;
  o[28] = v_87;
  o[29] = v_88;
  o[30] = v_89;
  o[31] = v_90;
  o[32] = v_91;
  o[33] = v_92;
  o[34] = v_93;
  o[35] = v_94;
  o[36] = v_95;
  o[37] = v_96;
  o[38] = v_97;
  o[39] = v_98;
  o[40] = v_99;
  o[41] = v_100;
  o[42] = v_101;
  o[43] = v_102;
  o[44] = v_103;
  o[45] = v_104;
  o[46] = v_105;
  o[47] = v_106;
  o[48] = v_107;
  o[49] = v_108;
  o[50] = v_109;
  o[51] = v_110;
  o[52] = v_111;
  o[53] = v_112;
  o[54] = v_113;
  o[55] = v_114;
  o[56] = v_115;
  return 1;
}

INLINE Term spin_54(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_6 = 0;
  u32 a_0 = r0;
  u32 b_0 = r1;
  WL_SPIN
    Term v_7 = 0;
    Term o_2[1];
    if (spin_1(e, o_2, U32_BIN(a_0, <, b_0), a_0, b_0) == 0) {
      return 0;
    }
    v_7 = o_2[0];
    v_6 = v_7;
  break;
  }
  o[0] = v_6;
  return 1;
}

INLINE Term spin_55(Env e, THR Term* o, Term r0, u32 r1, Term r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14) {
  u32 wpoll = 0;
  Term v_28 = 0;
  u32 v_29 = 0;
  u32 v_30 = 0;
  u32 v_31 = 0;
  u32 v_32 = 0;
  u32 v_33 = 0;
  u32 v_34 = 0;
  u32 v_35 = 0;
  u32 v_36 = 0;
  u32 v_37 = 0;
  Term fuel_0 = r0;
  u32 k_0 = r1;
  Term r_2 = r2;
  u32 r_3 = r3;
  u32 r_4 = r4;
  u32 r_5 = r5;
  u32 r_6 = r6;
  u32 r_7 = r7;
  u32 r_8 = r8;
  u32 r_9 = r9;
  u32 r_10 = r10;
  u32 r_11 = r11;
  u32 ent_0 = r12;
  u32 atr_0 = r13;
  u32 i_1 = r14;
  WL_SPIN
    if (fuel_0 == 0) {
      v_28 = r_2;
      v_29 = r_3;
      v_30 = r_4;
      v_31 = r_5;
      v_32 = r_6;
      v_33 = r_7;
      v_34 = r_8;
      v_35 = r_9;
      v_36 = r_10;
      v_37 = r_11;
    } else {
      Term p_0 = (fuel_0 - 1);
      Term v_38 = 0;
      u32 v_39 = 0;
      u32 v_40 = 0;
      u32 v_41 = 0;
      u32 v_42 = 0;
      u32 v_43 = 0;
      u32 v_44 = 0;
      u32 v_45 = 0;
      u32 v_46 = 0;
      u32 v_47 = 0;
      Term v_48 = 0;
      u32 v_49 = 0;
      u32 v_50 = 0;
      u32 v_51 = 0;
      u32 v_52 = 0;
      u32 v_53 = 0;
      u32 v_54 = 0;
      u32 v_55 = 0;
      u32 v_56 = 0;
      u32 v_57 = 0;
      Term o_4[10];
      if (spin_51(e, o_4, r_2, r_3, r_4, r_5, r_6, r_7, r_8, r_9, r_10, r_11, k_0, ent_0, atr_0, i_1) == 0) {
        return 0;
      }
      v_48 = o_4[0];
      v_49 = o_4[1];
      v_50 = o_4[2];
      v_51 = o_4[3];
      v_52 = o_4[4];
      v_53 = o_4[5];
      v_54 = o_4[6];
      v_55 = o_4[7];
      v_56 = o_4[8];
      v_57 = o_4[9];
      v_38 = v_48;
      v_39 = v_49;
      v_40 = v_50;
      v_41 = v_51;
      v_42 = v_52;
      v_43 = v_53;
      v_44 = v_54;
      v_45 = v_55;
      v_46 = v_56;
      v_47 = v_57;
      r0 = p_0;
      r1 = U32_BIN(k_0, +, 1ull);
      r2 = v_38;
      r3 = v_39;
      r4 = v_40;
      r5 = v_41;
      r6 = v_42;
      r7 = v_43;
      r8 = v_44;
      r9 = v_45;
      r10 = v_46;
      r11 = v_47;
      r12 = ent_0;
      r13 = atr_0;
      r14 = i_1;
      fuel_0 = r0;
      k_0 = r1;
      r_2 = r2;
      r_3 = r3;
      r_4 = r4;
      r_5 = r5;
      r_6 = r6;
      r_7 = r7;
      r_8 = r8;
      r_9 = r9;
      r_10 = r10;
      r_11 = r11;
      ent_0 = r12;
      atr_0 = r13;
      i_1 = r14;
      WL_AGAIN(spin_55);
    }
  break;
  }
  o[0] = v_28;
  o[1] = v_29;
  o[2] = v_30;
  o[3] = v_31;
  o[4] = v_32;
  o[5] = v_33;
  o[6] = v_34;
  o[7] = v_35;
  o[8] = v_36;
  o[9] = v_37;
  return 1;
}

INLINE Term spin_56(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, Term r10, Term r11, Term r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21) {
  u32 wpoll = 0;
  Term v_64 = 0;
  Term v_65 = 0;
  Term v_66 = 0;
  Term v_67 = 0;
  Term r_12 = r0;
  u32 r_13 = r1;
  u32 r_14 = r2;
  u32 r_15 = r3;
  u32 r_16 = r4;
  u32 r_17 = r5;
  u32 r_18 = r6;
  u32 r_19 = r7;
  u32 r_20 = r8;
  u32 r_21 = r9;
  Term out_1 = r10;
  Term nxt_1 = r11;
  Term acc_1 = r12;
  u32 f_1 = r13;
  u32 atrw_1 = r14;
  u32 ow_1 = r15;
  u32 ent_1 = r16;
  u32 atr_1 = r17;
  u32 cj_0 = r18;
  u32 date_1 = r19;
  u32 i_2 = r20;
  u32 j_1 = r21;
  WL_SPIN
    u32 last_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(r_21) - f32_unbox(ent_1))) / f32_unbox(atr_1));
    u32 last10_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(r_20) - f32_unbox(ent_1))) / f32_unbox(atr_1));
    u32 v_68 = 0;
    u32 v_69 = 0;
    Term o_7[1];
    if (spin_2(e, o_7, U32_BIN(U32_BIN(r_13, &, 1ull), ==, 1ull), r_14, last_0) == 0) {
      return 0;
    }
    v_69 = o_7[0];
    v_68 = v_69;
    u32 v_70 = 0;
    u32 v_71 = 0;
    Term o_8[1];
    if (spin_2(e, o_8, U32_BIN(U32_BIN(r_13, &, 2ull), ==, 2ull), r_15, last_0) == 0) {
      return 0;
    }
    v_71 = o_8[0];
    v_70 = v_71;
    u32 v_72 = 0;
    u32 v_73 = 0;
    Term o_9[1];
    if (spin_2(e, o_9, U32_BIN(U32_BIN(r_13, &, 4ull), ==, 4ull), r_16, last10_0) == 0) {
      return 0;
    }
    v_73 = o_9[0];
    v_72 = v_73;
    u32 v_74 = 0;
    u32 v_75 = 0;
    Term o_10[1];
    if (spin_2(e, o_10, U32_BIN(U32_BIN(r_13, &, 8ull), ==, 8ull), r_17, last_0) == 0) {
      return 0;
    }
    v_75 = o_10[0];
    v_74 = v_75;
    u32 flip_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(cj_0) / f32_unbox(ent_1))) - f32_unbox(1065353216ull))) * f32_unbox(1120403456ull));
    u32 t10_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(r_20) / f32_unbox(ent_1))) - f32_unbox(1065353216ull))) * f32_unbox(1120403456ull));
    u32 mfe_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(r_18) - f32_unbox(ent_1))) / f32_unbox(atr_1));
    u32 mae_0 = f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(r_19) - f32_unbox(ent_1))) / f32_unbox(atr_1));
    u32 v_76 = 0;
    u32 v_77 = 0;
    Term o_11[1];
    if (spin_52(e, o_11, t10_0) == 0) {
      return 0;
    }
    v_77 = o_11[0];
    v_76 = v_77;
    u32 v_78 = 0;
    u32 v_79 = 0;
    Term o_12[1];
    if (spin_52(e, o_12, mae_0) == 0) {
      return 0;
    }
    v_79 = o_12[0];
    v_78 = v_79;
    u32 v_80 = 0;
    u32 v_81 = 0;
    Term o_13[1];
    if (spin_52(e, o_13, mfe_0) == 0) {
      return 0;
    }
    v_81 = o_13[0];
    v_80 = v_81;
    u32 v_82 = 0;
    u32 v_83 = 0;
    Term o_14[1];
    if (spin_52(e, o_14, v_74) == 0) {
      return 0;
    }
    v_83 = o_14[0];
    v_82 = v_83;
    u32 v_84 = 0;
    u32 v_85 = 0;
    Term o_15[1];
    if (spin_52(e, o_15, v_72) == 0) {
      return 0;
    }
    v_85 = o_15[0];
    v_84 = v_85;
    u32 v_86 = 0;
    u32 v_87 = 0;
    Term o_16[1];
    if (spin_52(e, o_16, v_70) == 0) {
      return 0;
    }
    v_87 = o_16[0];
    v_86 = v_87;
    u32 v_88 = 0;
    u32 v_89 = 0;
    Term o_17[1];
    if (spin_52(e, o_17, v_68) == 0) {
      return 0;
    }
    v_89 = o_17[0];
    v_88 = v_89;
    u32 v_90 = 0;
    u32 v_91 = 0;
    Term o_18[1];
    if (spin_52(e, o_18, flip_0) == 0) {
      return 0;
    }
    v_91 = o_18[0];
    v_90 = v_91;
    u64 nd_0 = heap_alloc(e, cls_fit(2));
    e.mem[nd_0 + 0] = i_2;
    e.mem[nd_0 + 1] = acc_1;
    u64 nd_1 = heap_alloc(e, cls_fit(2));
    e.mem[nd_1 + 0] = date_1;
    e.mem[nd_1 + 1] = term_ctr(CID_CON, nd_0);
    u64 nd_2 = heap_alloc(e, cls_fit(2));
    e.mem[nd_2 + 0] = f_1;
    e.mem[nd_2 + 1] = term_ctr(CID_CON, nd_1);
    u64 nd_3 = heap_alloc(e, cls_fit(2));
    e.mem[nd_3 + 0] = atrw_1;
    e.mem[nd_3 + 1] = term_ctr(CID_CON, nd_2);
    u64 nd_4 = heap_alloc(e, cls_fit(2));
    e.mem[nd_4 + 0] = ow_1;
    e.mem[nd_4 + 1] = term_ctr(CID_CON, nd_3);
    u64 nd_5 = heap_alloc(e, cls_fit(2));
    e.mem[nd_5 + 0] = U32_BIN(j_1, -, i_2);
    e.mem[nd_5 + 1] = term_ctr(CID_CON, nd_4);
    u64 nd_6 = heap_alloc(e, cls_fit(2));
    e.mem[nd_6 + 0] = v_90;
    e.mem[nd_6 + 1] = term_ctr(CID_CON, nd_5);
    u64 nd_7 = heap_alloc(e, cls_fit(2));
    e.mem[nd_7 + 0] = v_88;
    e.mem[nd_7 + 1] = term_ctr(CID_CON, nd_6);
    u64 nd_8 = heap_alloc(e, cls_fit(2));
    e.mem[nd_8 + 0] = v_86;
    e.mem[nd_8 + 1] = term_ctr(CID_CON, nd_7);
    u64 nd_9 = heap_alloc(e, cls_fit(2));
    e.mem[nd_9 + 0] = v_84;
    e.mem[nd_9 + 1] = term_ctr(CID_CON, nd_8);
    u64 nd_10 = heap_alloc(e, cls_fit(2));
    e.mem[nd_10 + 0] = v_82;
    e.mem[nd_10 + 1] = term_ctr(CID_CON, nd_9);
    u64 nd_11 = heap_alloc(e, cls_fit(2));
    e.mem[nd_11 + 0] = v_80;
    e.mem[nd_11 + 1] = term_ctr(CID_CON, nd_10);
    u64 nd_12 = heap_alloc(e, cls_fit(2));
    e.mem[nd_12 + 0] = v_78;
    e.mem[nd_12 + 1] = term_ctr(CID_CON, nd_11);
    u64 nd_13 = heap_alloc(e, cls_fit(2));
    e.mem[nd_13 + 0] = v_76;
    e.mem[nd_13 + 1] = term_ctr(CID_CON, nd_12);
    u64 nd_14 = heap_alloc(e, cls_fit(2));
    e.mem[nd_14 + 0] = j_1;
    e.mem[nd_14 + 1] = term_ctr(CID_CON, nd_13);
    u64 nd_15 = heap_alloc(e, cls_fit(2));
    e.mem[nd_15 + 0] = 0ull;
    e.mem[nd_15 + 1] = term_ctr(CID_CON, nd_14);
    v_64 = r_12;
    v_65 = out_1;
    v_66 = nxt_1;
    v_67 = term_ctr(CID_CON, nd_15);
  break;
  }
  o[0] = v_64;
  o[1] = v_65;
  o[2] = v_66;
  o[3] = v_67;
  return 1;
}

INLINE Term spin_57(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, Term r17, Term r18, Term r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64, u32 r65, u32 r66, u32 r67, u32 r68, u32 r69, u32 r70, u32 r71, u32 r72, u32 r73) {
  u32 wpoll = 0;
  Term v_59 = 0;
  Term v_60 = 0;
  Term v_61 = 0;
  Term v_62 = 0;
  u32 v_63 = 0;
  u32 v_64 = 0;
  u32 v_65 = 0;
  u32 v_66 = 0;
  u32 v_67 = 0;
  u32 v_68 = 0;
  u32 v_69 = 0;
  u32 v_70 = 0;
  u32 v_71 = 0;
  u32 v_72 = 0;
  u32 v_73 = 0;
  u32 v_74 = 0;
  u32 v_75 = 0;
  u32 v_76 = 0;
  u32 v_77 = 0;
  u32 v_78 = 0;
  u32 v_79 = 0;
  u32 v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  Term r_6 = r0;
  u32 r_7 = r1;
  u32 old_0 = r2;
  u32 old_1 = r3;
  u32 old_2 = r4;
  u32 old_3 = r5;
  u32 old_4 = r6;
  u32 is_ph_1 = r7;
  u32 is_pl_1 = r8;
  u32 hh9_1 = r9;
  u32 ll9_1 = r10;
  u32 sma10_1 = r11;
  u32 b_5 = r12;
  u32 b_6 = r13;
  u32 b_7 = r14;
  u32 b_8 = r15;
  u32 b_9 = r16;
  Term chips_1 = r17;
  Term bars_0 = r18;
  Term out_1 = r19;
  u32 st_53 = r20;
  u32 st_54 = r21;
  u32 st_55 = r22;
  u32 st_56 = r23;
  u32 st_57 = r24;
  u32 st_58 = r25;
  u32 st_59 = r26;
  u32 st_60 = r27;
  u32 st_61 = r28;
  u32 st_62 = r29;
  u32 st_63 = r30;
  u32 st_64 = r31;
  u32 st_65 = r32;
  u32 st_66 = r33;
  u32 st_67 = r34;
  u32 st_68 = r35;
  u32 st_69 = r36;
  u32 st_70 = r37;
  u32 st_71 = r38;
  u32 st_72 = r39;
  u32 st_73 = r40;
  u32 st_74 = r41;
  u32 st_75 = r42;
  u32 st_76 = r43;
  u32 st_77 = r44;
  u32 st_78 = r45;
  u32 st_79 = r46;
  u32 st_80 = r47;
  u32 st_81 = r48;
  u32 st_82 = r49;
  u32 st_83 = r50;
  u32 st_84 = r51;
  u32 st_85 = r52;
  u32 st_86 = r53;
  u32 st_87 = r54;
  u32 st_88 = r55;
  u32 st_89 = r56;
  u32 st_90 = r57;
  u32 st_91 = r58;
  u32 st_92 = r59;
  u32 st_93 = r60;
  u32 st_94 = r61;
  u32 st_95 = r62;
  u32 st_96 = r63;
  u32 st_97 = r64;
  u32 st_98 = r65;
  u32 st_99 = r66;
  u32 st_100 = r67;
  u32 st_101 = r68;
  u32 st_102 = r69;
  u32 st_103 = r70;
  u32 st_104 = r71;
  u32 st_105 = r72;
  u32 i_1 = r73;
  WL_SPIN
    u32 v_116 = 0;
    u32 v_117 = 0;
    Term o_1[1];
    if (spin_3(e, o_1, U32_BIN(i_1, <, 14ull), 0ull, U32_BIN(i_1, -, 14ull)) == 0) {
      return 0;
    }
    v_117 = o_1[0];
    v_116 = v_117;
    Term at_1 = blk_at(r_6, U32_BIN(U32_BIN(v_116, *, 4ull), +, 2ull), 0);
    u32 c_1 = blk_read(e.mem, 0, term_loc(r_6), at_1 + 0);
    Term v_118 = 0;
    Term v_119 = 0;
    Term v_120 = 0;
    Term v_121 = 0;
    u32 v_122 = 0;
    u32 v_123 = 0;
    u32 v_124 = 0;
    u32 v_125 = 0;
    u32 v_126 = 0;
    u32 v_127 = 0;
    u32 v_128 = 0;
    u32 v_129 = 0;
    u32 v_130 = 0;
    u32 v_131 = 0;
    u32 v_132 = 0;
    u32 v_133 = 0;
    u32 v_134 = 0;
    u32 v_135 = 0;
    u32 v_136 = 0;
    u32 v_137 = 0;
    u32 v_138 = 0;
    u32 v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    Term o_2[57];
    if (spin_53(e, o_2, r_6, c_1, r_7, old_0, old_1, old_2, old_3, old_4, is_ph_1, is_pl_1, hh9_1, ll9_1, sma10_1, b_5, b_6, b_7, b_8, b_9, chips_1, bars_0, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_1) == 0) {
      return 0;
    }
    v_118 = o_2[0];
    v_119 = o_2[1];
    v_120 = o_2[2];
    v_121 = o_2[3];
    v_122 = o_2[4];
    v_123 = o_2[5];
    v_124 = o_2[6];
    v_125 = o_2[7];
    v_126 = o_2[8];
    v_127 = o_2[9];
    v_128 = o_2[10];
    v_129 = o_2[11];
    v_130 = o_2[12];
    v_131 = o_2[13];
    v_132 = o_2[14];
    v_133 = o_2[15];
    v_134 = o_2[16];
    v_135 = o_2[17];
    v_136 = o_2[18];
    v_137 = o_2[19];
    v_138 = o_2[20];
    v_139 = o_2[21];
    v_140 = o_2[22];
    v_141 = o_2[23];
    v_142 = o_2[24];
    v_143 = o_2[25];
    v_144 = o_2[26];
    v_145 = o_2[27];
    v_146 = o_2[28];
    v_147 = o_2[29];
    v_148 = o_2[30];
    v_149 = o_2[31];
    v_150 = o_2[32];
    v_151 = o_2[33];
    v_152 = o_2[34];
    v_153 = o_2[35];
    v_154 = o_2[36];
    v_155 = o_2[37];
    v_156 = o_2[38];
    v_157 = o_2[39];
    v_158 = o_2[40];
    v_159 = o_2[41];
    v_160 = o_2[42];
    v_161 = o_2[43];
    v_162 = o_2[44];
    v_163 = o_2[45];
    v_164 = o_2[46];
    v_165 = o_2[47];
    v_166 = o_2[48];
    v_167 = o_2[49];
    v_168 = o_2[50];
    v_169 = o_2[51];
    v_170 = o_2[52];
    v_171 = o_2[53];
    v_172 = o_2[54];
    v_173 = o_2[55];
    v_174 = o_2[56];
    v_59 = v_118;
    v_60 = v_119;
    v_61 = v_120;
    v_62 = v_121;
    v_63 = v_122;
    v_64 = v_123;
    v_65 = v_124;
    v_66 = v_125;
    v_67 = v_126;
    v_68 = v_127;
    v_69 = v_128;
    v_70 = v_129;
    v_71 = v_130;
    v_72 = v_131;
    v_73 = v_132;
    v_74 = v_133;
    v_75 = v_134;
    v_76 = v_135;
    v_77 = v_136;
    v_78 = v_137;
    v_79 = v_138;
    v_80 = v_139;
    v_81 = v_140;
    v_82 = v_141;
    v_83 = v_142;
    v_84 = v_143;
    v_85 = v_144;
    v_86 = v_145;
    v_87 = v_146;
    v_88 = v_147;
    v_89 = v_148;
    v_90 = v_149;
    v_91 = v_150;
    v_92 = v_151;
    v_93 = v_152;
    v_94 = v_153;
    v_95 = v_154;
    v_96 = v_155;
    v_97 = v_156;
    v_98 = v_157;
    v_99 = v_158;
    v_100 = v_159;
    v_101 = v_160;
    v_102 = v_161;
    v_103 = v_162;
    v_104 = v_163;
    v_105 = v_164;
    v_106 = v_165;
    v_107 = v_166;
    v_108 = v_167;
    v_109 = v_168;
    v_110 = v_169;
    v_111 = v_170;
    v_112 = v_171;
    v_113 = v_172;
    v_114 = v_173;
    v_115 = v_174;
  break;
  }
  o[0] = v_59;
  o[1] = v_60;
  o[2] = v_61;
  o[3] = v_62;
  o[4] = v_63;
  o[5] = v_64;
  o[6] = v_65;
  o[7] = v_66;
  o[8] = v_67;
  o[9] = v_68;
  o[10] = v_69;
  o[11] = v_70;
  o[12] = v_71;
  o[13] = v_72;
  o[14] = v_73;
  o[15] = v_74;
  o[16] = v_75;
  o[17] = v_76;
  o[18] = v_77;
  o[19] = v_78;
  o[20] = v_79;
  o[21] = v_80;
  o[22] = v_81;
  o[23] = v_82;
  o[24] = v_83;
  o[25] = v_84;
  o[26] = v_85;
  o[27] = v_86;
  o[28] = v_87;
  o[29] = v_88;
  o[30] = v_89;
  o[31] = v_90;
  o[32] = v_91;
  o[33] = v_92;
  o[34] = v_93;
  o[35] = v_94;
  o[36] = v_95;
  o[37] = v_96;
  o[38] = v_97;
  o[39] = v_98;
  o[40] = v_99;
  o[41] = v_100;
  o[42] = v_101;
  o[43] = v_102;
  o[44] = v_103;
  o[45] = v_104;
  o[46] = v_105;
  o[47] = v_106;
  o[48] = v_107;
  o[49] = v_108;
  o[50] = v_109;
  o[51] = v_110;
  o[52] = v_111;
  o[53] = v_112;
  o[54] = v_113;
  o[55] = v_114;
  o[56] = v_115;
  return 1;
}

INLINE Term spin_58(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3, Term r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11) {
  u32 wpoll = 0;
  Term v_8 = 0;
  Term v_9 = 0;
  Term v_10 = 0;
  Term v_11 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  Term out_1 = r2;
  Term nxt_0 = r3;
  Term acc_1 = r4;
  u32 f_1 = r5;
  u32 atrw_1 = r6;
  u32 ow_1 = r7;
  u32 date_1 = r8;
  u32 i_1 = r9;
  u32 j_0 = r10;
  u32 n_1 = r11;
  WL_SPIN
    u32 v_12 = 0;
    u32 v_13 = 0;
    Term o_1[1];
    if (spin_44(e, o_1, ow_1) == 0) {
      return 0;
    }
    v_13 = o_1[0];
    v_12 = v_13;
    u32 v_14 = 0;
    u32 v_15 = 0;
    Term o_2[1];
    if (spin_0(e, o_2, f32_rewrap(f32_unbox(f32_rewrap(f32_unbox(v_12) * f32_unbox(f32_rewrap((f32)(u32)(atrw_1))))) / f32_unbox(1203982336ull)), f32_rewrap(f32_unbox(v_12) * f32_unbox(953267991ull))) == 0) {
      return 0;
    }
    v_15 = o_2[0];
    v_14 = v_15;
    u32 v_16 = 0;
    u32 v_17 = 0;
    Term o_3[1];
    if (spin_54(e, o_3, U32_BIN(i_1, +, 20ull), U32_BIN(n_1, -, 1ull)) == 0) {
      return 0;
    }
    v_17 = o_3[0];
    v_16 = v_17;
    Term v_18 = 0;
    u32 v_19 = 0;
    u32 v_20 = 0;
    u32 v_21 = 0;
    u32 v_22 = 0;
    u32 v_23 = 0;
    u32 v_24 = 0;
    u32 v_25 = 0;
    u32 v_26 = 0;
    u32 v_27 = 0;
    Term v_28 = 0;
    u32 v_29 = 0;
    u32 v_30 = 0;
    u32 v_31 = 0;
    u32 v_32 = 0;
    u32 v_33 = 0;
    u32 v_34 = 0;
    u32 v_35 = 0;
    u32 v_36 = 0;
    u32 v_37 = 0;
    Term o_4[10];
    if (spin_55(e, o_4, U32_BIN(v_16, -, i_1), U32_BIN(i_1, +, 1ull), r_2, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 1315859240ull, 0ull, 0ull, v_12, v_14, i_1) == 0) {
      return 0;
    }
    v_28 = o_4[0];
    v_29 = o_4[1];
    v_30 = o_4[2];
    v_31 = o_4[3];
    v_32 = o_4[4];
    v_33 = o_4[5];
    v_34 = o_4[6];
    v_35 = o_4[7];
    v_36 = o_4[8];
    v_37 = o_4[9];
    v_18 = v_28;
    v_19 = v_29;
    v_20 = v_30;
    v_21 = v_31;
    v_22 = v_32;
    v_23 = v_33;
    v_24 = v_34;
    v_25 = v_35;
    v_26 = v_36;
    v_27 = v_37;
    u32 v_38 = 0;
    u32 v_39 = 0;
    Term o_5[1];
    if (spin_44(e, o_5, r_3) == 0) {
      return 0;
    }
    v_39 = o_5[0];
    v_38 = v_39;
    Term v_40 = 0;
    Term v_41 = 0;
    Term v_42 = 0;
    Term v_43 = 0;
    Term o_6[4];
    if (spin_56(e, o_6, v_18, v_19, v_20, v_21, v_22, v_23, v_24, v_25, v_26, v_27, out_1, nxt_0, acc_1, f_1, atrw_1, ow_1, v_12, v_14, v_38, date_1, i_1, j_0) == 0) {
      return 0;
    }
    v_40 = o_6[0];
    v_41 = o_6[1];
    v_42 = o_6[2];
    v_43 = o_6[3];
    v_8 = v_40;
    v_9 = v_41;
    v_10 = v_42;
    v_11 = v_43;
  break;
  }
  o[0] = v_8;
  o[1] = v_9;
  o[2] = v_10;
  o[3] = v_11;
  return 1;
}

INLINE Term spin_59(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, Term r16, Term r17, Term r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64, u32 r65, u32 r66, u32 r67, u32 r68, u32 r69, u32 r70, u32 r71, u32 r72) {
  u32 wpoll = 0;
  Term v_77 = 0;
  Term v_78 = 0;
  Term v_79 = 0;
  Term v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  u32 v_128 = 0;
  u32 v_129 = 0;
  u32 v_130 = 0;
  u32 v_131 = 0;
  u32 v_132 = 0;
  u32 v_133 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  u32 r_4 = r2;
  u32 r_5 = r3;
  u32 r_6 = r4;
  u32 r_7 = r5;
  u32 is_ph_0 = r6;
  u32 is_pl_0 = r7;
  u32 hh9_1 = r8;
  u32 ll9_1 = r9;
  u32 sma10_1 = r10;
  u32 b_5 = r11;
  u32 b_6 = r12;
  u32 b_7 = r13;
  u32 b_8 = r14;
  u32 b_9 = r15;
  Term chips_1 = r16;
  Term aux_1 = r17;
  Term out_1 = r18;
  u32 st_53 = r19;
  u32 st_54 = r20;
  u32 st_55 = r21;
  u32 st_56 = r22;
  u32 st_57 = r23;
  u32 st_58 = r24;
  u32 st_59 = r25;
  u32 st_60 = r26;
  u32 st_61 = r27;
  u32 st_62 = r28;
  u32 st_63 = r29;
  u32 st_64 = r30;
  u32 st_65 = r31;
  u32 st_66 = r32;
  u32 st_67 = r33;
  u32 st_68 = r34;
  u32 st_69 = r35;
  u32 st_70 = r36;
  u32 st_71 = r37;
  u32 st_72 = r38;
  u32 st_73 = r39;
  u32 st_74 = r40;
  u32 st_75 = r41;
  u32 st_76 = r42;
  u32 st_77 = r43;
  u32 st_78 = r44;
  u32 st_79 = r45;
  u32 st_80 = r46;
  u32 st_81 = r47;
  u32 st_82 = r48;
  u32 st_83 = r49;
  u32 st_84 = r50;
  u32 st_85 = r51;
  u32 st_86 = r52;
  u32 st_87 = r53;
  u32 st_88 = r54;
  u32 st_89 = r55;
  u32 st_90 = r56;
  u32 st_91 = r57;
  u32 st_92 = r58;
  u32 st_93 = r59;
  u32 st_94 = r60;
  u32 st_95 = r61;
  u32 st_96 = r62;
  u32 st_97 = r63;
  u32 st_98 = r64;
  u32 st_99 = r65;
  u32 st_100 = r66;
  u32 st_101 = r67;
  u32 st_102 = r68;
  u32 st_103 = r69;
  u32 st_104 = r70;
  u32 st_105 = r71;
  u32 i_1 = r72;
  WL_SPIN
    u32 v_134 = 0;
    u32 v_135 = 0;
    Term o_5[1];
    if (spin_3(e, o_5, U32_BIN(i_1, <, 4ull), 0ull, U32_BIN(i_1, -, 4ull)) == 0) {
      return 0;
    }
    v_135 = o_5[0];
    v_134 = v_135;
    Term at_0 = blk_at(aux_1, U32_BIN(v_134, *, 4ull), 0);
    u32 c_0 = blk_read(e.mem, 0, term_loc(aux_1), at_0 + 0);
    Term v_136 = 0;
    Term v_137 = 0;
    Term v_138 = 0;
    Term v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    u32 v_185 = 0;
    u32 v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    u32 v_191 = 0;
    u32 v_192 = 0;
    Term o_6[57];
    if (spin_57(e, o_6, aux_1, c_0, r_3, r_4, r_5, r_6, r_7, is_ph_0, is_pl_0, hh9_1, ll9_1, sma10_1, b_5, b_6, b_7, b_8, b_9, chips_1, r_2, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_1) == 0) {
      return 0;
    }
    v_136 = o_6[0];
    v_137 = o_6[1];
    v_138 = o_6[2];
    v_139 = o_6[3];
    v_140 = o_6[4];
    v_141 = o_6[5];
    v_142 = o_6[6];
    v_143 = o_6[7];
    v_144 = o_6[8];
    v_145 = o_6[9];
    v_146 = o_6[10];
    v_147 = o_6[11];
    v_148 = o_6[12];
    v_149 = o_6[13];
    v_150 = o_6[14];
    v_151 = o_6[15];
    v_152 = o_6[16];
    v_153 = o_6[17];
    v_154 = o_6[18];
    v_155 = o_6[19];
    v_156 = o_6[20];
    v_157 = o_6[21];
    v_158 = o_6[22];
    v_159 = o_6[23];
    v_160 = o_6[24];
    v_161 = o_6[25];
    v_162 = o_6[26];
    v_163 = o_6[27];
    v_164 = o_6[28];
    v_165 = o_6[29];
    v_166 = o_6[30];
    v_167 = o_6[31];
    v_168 = o_6[32];
    v_169 = o_6[33];
    v_170 = o_6[34];
    v_171 = o_6[35];
    v_172 = o_6[36];
    v_173 = o_6[37];
    v_174 = o_6[38];
    v_175 = o_6[39];
    v_176 = o_6[40];
    v_177 = o_6[41];
    v_178 = o_6[42];
    v_179 = o_6[43];
    v_180 = o_6[44];
    v_181 = o_6[45];
    v_182 = o_6[46];
    v_183 = o_6[47];
    v_184 = o_6[48];
    v_185 = o_6[49];
    v_186 = o_6[50];
    v_187 = o_6[51];
    v_188 = o_6[52];
    v_189 = o_6[53];
    v_190 = o_6[54];
    v_191 = o_6[55];
    v_192 = o_6[56];
    v_77 = v_136;
    v_78 = v_137;
    v_79 = v_138;
    v_80 = v_139;
    v_81 = v_140;
    v_82 = v_141;
    v_83 = v_142;
    v_84 = v_143;
    v_85 = v_144;
    v_86 = v_145;
    v_87 = v_146;
    v_88 = v_147;
    v_89 = v_148;
    v_90 = v_149;
    v_91 = v_150;
    v_92 = v_151;
    v_93 = v_152;
    v_94 = v_153;
    v_95 = v_154;
    v_96 = v_155;
    v_97 = v_156;
    v_98 = v_157;
    v_99 = v_158;
    v_100 = v_159;
    v_101 = v_160;
    v_102 = v_161;
    v_103 = v_162;
    v_104 = v_163;
    v_105 = v_164;
    v_106 = v_165;
    v_107 = v_166;
    v_108 = v_167;
    v_109 = v_168;
    v_110 = v_169;
    v_111 = v_170;
    v_112 = v_171;
    v_113 = v_172;
    v_114 = v_173;
    v_115 = v_174;
    v_116 = v_175;
    v_117 = v_176;
    v_118 = v_177;
    v_119 = v_178;
    v_120 = v_179;
    v_121 = v_180;
    v_122 = v_181;
    v_123 = v_182;
    v_124 = v_183;
    v_125 = v_184;
    v_126 = v_185;
    v_127 = v_186;
    v_128 = v_187;
    v_129 = v_188;
    v_130 = v_189;
    v_131 = v_190;
    v_132 = v_191;
    v_133 = v_192;
  break;
  }
  o[0] = v_77;
  o[1] = v_78;
  o[2] = v_79;
  o[3] = v_80;
  o[4] = v_81;
  o[5] = v_82;
  o[6] = v_83;
  o[7] = v_84;
  o[8] = v_85;
  o[9] = v_86;
  o[10] = v_87;
  o[11] = v_88;
  o[12] = v_89;
  o[13] = v_90;
  o[14] = v_91;
  o[15] = v_92;
  o[16] = v_93;
  o[17] = v_94;
  o[18] = v_95;
  o[19] = v_96;
  o[20] = v_97;
  o[21] = v_98;
  o[22] = v_99;
  o[23] = v_100;
  o[24] = v_101;
  o[25] = v_102;
  o[26] = v_103;
  o[27] = v_104;
  o[28] = v_105;
  o[29] = v_106;
  o[30] = v_107;
  o[31] = v_108;
  o[32] = v_109;
  o[33] = v_110;
  o[34] = v_111;
  o[35] = v_112;
  o[36] = v_113;
  o[37] = v_114;
  o[38] = v_115;
  o[39] = v_116;
  o[40] = v_117;
  o[41] = v_118;
  o[42] = v_119;
  o[43] = v_120;
  o[44] = v_121;
  o[45] = v_122;
  o[46] = v_123;
  o[47] = v_124;
  o[48] = v_125;
  o[49] = v_126;
  o[50] = v_127;
  o[51] = v_128;
  o[52] = v_129;
  o[53] = v_130;
  o[54] = v_131;
  o[55] = v_132;
  o[56] = v_133;
  return 1;
}

INLINE Term spin_60(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3, Term r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10) {
  u32 wpoll = 0;
  Term v_4 = 0;
  Term v_5 = 0;
  Term v_6 = 0;
  Term v_7 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  Term bars_0 = r2;
  Term out_1 = r3;
  Term acc_1 = r4;
  u32 f_1 = r5;
  u32 atrw_1 = r6;
  u32 ow_0 = r7;
  u32 date_1 = r8;
  u32 i_1 = r9;
  u32 n_1 = r10;
  WL_SPIN
    Term v_8 = 0;
    u32 v_9 = 0;
    Term v_10 = 0;
    u32 v_11 = 0;
    Term o_0[2];
    if (spin_42(e, o_0, bars_0, r_3, 4ull) == 0) {
      return 0;
    }
    v_10 = o_0[0];
    v_11 = o_0[1];
    v_8 = v_10;
    v_9 = v_11;
    Term v_12 = 0;
    Term v_13 = 0;
    Term v_14 = 0;
    Term v_15 = 0;
    Term o_1[4];
    if (spin_58(e, o_1, v_8, v_9, out_1, r_2, acc_1, f_1, atrw_1, ow_0, date_1, i_1, r_3, n_1) == 0) {
      return 0;
    }
    v_12 = o_1[0];
    v_13 = o_1[1];
    v_14 = o_1[2];
    v_15 = o_1[3];
    v_4 = v_12;
    v_5 = v_13;
    v_6 = v_14;
    v_7 = v_15;
  break;
  }
  o[0] = v_4;
  o[1] = v_5;
  o[2] = v_6;
  o[3] = v_7;
  return 1;
}

INLINE Term spin_61(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, Term r13, Term r14, Term r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64, u32 r65, u32 r66, u32 r67, u32 r68, u32 r69) {
  u32 wpoll = 0;
  Term v_65 = 0;
  Term v_66 = 0;
  Term v_67 = 0;
  Term v_68 = 0;
  u32 v_69 = 0;
  u32 v_70 = 0;
  u32 v_71 = 0;
  u32 v_72 = 0;
  u32 v_73 = 0;
  u32 v_74 = 0;
  u32 v_75 = 0;
  u32 v_76 = 0;
  u32 v_77 = 0;
  u32 v_78 = 0;
  u32 v_79 = 0;
  u32 v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  u32 ch_0 = r2;
  u32 wh_1 = r3;
  u32 wl_1 = r4;
  u32 hh9_1 = r5;
  u32 ll9_1 = r6;
  u32 sma10_1 = r7;
  u32 b_5 = r8;
  u32 b_6 = r9;
  u32 b_7 = r10;
  u32 b_8 = r11;
  u32 b_9 = r12;
  Term chips_1 = r13;
  Term aux_1 = r14;
  Term out_1 = r15;
  u32 st_53 = r16;
  u32 st_54 = r17;
  u32 st_55 = r18;
  u32 st_56 = r19;
  u32 st_57 = r20;
  u32 st_58 = r21;
  u32 st_59 = r22;
  u32 st_60 = r23;
  u32 st_61 = r24;
  u32 st_62 = r25;
  u32 st_63 = r26;
  u32 st_64 = r27;
  u32 st_65 = r28;
  u32 st_66 = r29;
  u32 st_67 = r30;
  u32 st_68 = r31;
  u32 st_69 = r32;
  u32 st_70 = r33;
  u32 st_71 = r34;
  u32 st_72 = r35;
  u32 st_73 = r36;
  u32 st_74 = r37;
  u32 st_75 = r38;
  u32 st_76 = r39;
  u32 st_77 = r40;
  u32 st_78 = r41;
  u32 st_79 = r42;
  u32 st_80 = r43;
  u32 st_81 = r44;
  u32 st_82 = r45;
  u32 st_83 = r46;
  u32 st_84 = r47;
  u32 st_85 = r48;
  u32 st_86 = r49;
  u32 st_87 = r50;
  u32 st_88 = r51;
  u32 st_89 = r52;
  u32 st_90 = r53;
  u32 st_91 = r54;
  u32 st_92 = r55;
  u32 st_93 = r56;
  u32 st_94 = r57;
  u32 st_95 = r58;
  u32 st_96 = r59;
  u32 st_97 = r60;
  u32 st_98 = r61;
  u32 st_99 = r62;
  u32 st_100 = r63;
  u32 st_101 = r64;
  u32 st_102 = r65;
  u32 st_103 = r66;
  u32 st_104 = r67;
  u32 st_105 = r68;
  u32 i_1 = r69;
  WL_SPIN
    u32 v_122 = 0;
    u32 v_123 = 0;
    Term o_3[1];
    if (spin_44(e, o_3, r_3) == 0) {
      return 0;
    }
    v_123 = o_3[0];
    v_122 = v_123;
    u32 piv_ok_0 = U32_BIN(i_1, >=, 8ull);
    u32 v_124 = 0;
    u32 v_125 = 0;
    Term o_4[1];
    if (spin_7(e, o_4, piv_ok_0, ((u64)(f32_unbox(ch_0) > f32_unbox(wh_1)))) == 0) {
      return 0;
    }
    v_125 = o_4[0];
    v_124 = v_125;
    u32 v_126 = 0;
    u32 v_127 = 0;
    Term o_5[1];
    if (spin_7(e, o_5, piv_ok_0, ((u64)(f32_unbox(v_122) < f32_unbox(wl_1)))) == 0) {
      return 0;
    }
    v_127 = o_5[0];
    v_126 = v_127;
    Term v_128 = 0;
    u32 v_129 = 0;
    u32 v_130 = 0;
    u32 v_131 = 0;
    u32 v_132 = 0;
    u32 v_133 = 0;
    u32 v_134 = 0;
    u32 v_135 = 0;
    Term o_6[1];
    if (spin_3(e, o_6, U32_BIN(i_1, <, 120ull), i_1, 120ull) == 0) {
      return 0;
    }
    v_135 = o_6[0];
    v_134 = v_135;
    Term v_136 = 0;
    u32 v_137 = 0;
    u32 v_138 = 0;
    u32 v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    Term o_7[6];
    if (spin_41(e, o_7, r_2, U32_BIN(i_1, -, v_134)) == 0) {
      return 0;
    }
    v_136 = o_7[0];
    v_137 = o_7[1];
    v_138 = o_7[2];
    v_139 = o_7[3];
    v_140 = o_7[4];
    v_141 = o_7[5];
    v_128 = v_136;
    v_129 = v_137;
    v_130 = v_138;
    v_131 = v_139;
    v_132 = v_140;
    v_133 = v_141;
    Term v_142 = 0;
    Term v_143 = 0;
    Term v_144 = 0;
    Term v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    u32 v_185 = 0;
    u32 v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    u32 v_191 = 0;
    u32 v_192 = 0;
    u32 v_193 = 0;
    u32 v_194 = 0;
    u32 v_195 = 0;
    u32 v_196 = 0;
    u32 v_197 = 0;
    u32 v_198 = 0;
    Term o_8[57];
    if (spin_59(e, o_8, v_128, v_129, v_130, v_131, v_132, v_133, v_124, v_126, hh9_1, ll9_1, sma10_1, b_5, b_6, b_7, b_8, b_9, chips_1, aux_1, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_1) == 0) {
      return 0;
    }
    v_142 = o_8[0];
    v_143 = o_8[1];
    v_144 = o_8[2];
    v_145 = o_8[3];
    v_146 = o_8[4];
    v_147 = o_8[5];
    v_148 = o_8[6];
    v_149 = o_8[7];
    v_150 = o_8[8];
    v_151 = o_8[9];
    v_152 = o_8[10];
    v_153 = o_8[11];
    v_154 = o_8[12];
    v_155 = o_8[13];
    v_156 = o_8[14];
    v_157 = o_8[15];
    v_158 = o_8[16];
    v_159 = o_8[17];
    v_160 = o_8[18];
    v_161 = o_8[19];
    v_162 = o_8[20];
    v_163 = o_8[21];
    v_164 = o_8[22];
    v_165 = o_8[23];
    v_166 = o_8[24];
    v_167 = o_8[25];
    v_168 = o_8[26];
    v_169 = o_8[27];
    v_170 = o_8[28];
    v_171 = o_8[29];
    v_172 = o_8[30];
    v_173 = o_8[31];
    v_174 = o_8[32];
    v_175 = o_8[33];
    v_176 = o_8[34];
    v_177 = o_8[35];
    v_178 = o_8[36];
    v_179 = o_8[37];
    v_180 = o_8[38];
    v_181 = o_8[39];
    v_182 = o_8[40];
    v_183 = o_8[41];
    v_184 = o_8[42];
    v_185 = o_8[43];
    v_186 = o_8[44];
    v_187 = o_8[45];
    v_188 = o_8[46];
    v_189 = o_8[47];
    v_190 = o_8[48];
    v_191 = o_8[49];
    v_192 = o_8[50];
    v_193 = o_8[51];
    v_194 = o_8[52];
    v_195 = o_8[53];
    v_196 = o_8[54];
    v_197 = o_8[55];
    v_198 = o_8[56];
    v_65 = v_142;
    v_66 = v_143;
    v_67 = v_144;
    v_68 = v_145;
    v_69 = v_146;
    v_70 = v_147;
    v_71 = v_148;
    v_72 = v_149;
    v_73 = v_150;
    v_74 = v_151;
    v_75 = v_152;
    v_76 = v_153;
    v_77 = v_154;
    v_78 = v_155;
    v_79 = v_156;
    v_80 = v_157;
    v_81 = v_158;
    v_82 = v_159;
    v_83 = v_160;
    v_84 = v_161;
    v_85 = v_162;
    v_86 = v_163;
    v_87 = v_164;
    v_88 = v_165;
    v_89 = v_166;
    v_90 = v_167;
    v_91 = v_168;
    v_92 = v_169;
    v_93 = v_170;
    v_94 = v_171;
    v_95 = v_172;
    v_96 = v_173;
    v_97 = v_174;
    v_98 = v_175;
    v_99 = v_176;
    v_100 = v_177;
    v_101 = v_178;
    v_102 = v_179;
    v_103 = v_180;
    v_104 = v_181;
    v_105 = v_182;
    v_106 = v_183;
    v_107 = v_184;
    v_108 = v_185;
    v_109 = v_186;
    v_110 = v_187;
    v_111 = v_188;
    v_112 = v_189;
    v_113 = v_190;
    v_114 = v_191;
    v_115 = v_192;
    v_116 = v_193;
    v_117 = v_194;
    v_118 = v_195;
    v_119 = v_196;
    v_120 = v_197;
    v_121 = v_198;
  break;
  }
  o[0] = v_65;
  o[1] = v_66;
  o[2] = v_67;
  o[3] = v_68;
  o[4] = v_69;
  o[5] = v_70;
  o[6] = v_71;
  o[7] = v_72;
  o[8] = v_73;
  o[9] = v_74;
  o[10] = v_75;
  o[11] = v_76;
  o[12] = v_77;
  o[13] = v_78;
  o[14] = v_79;
  o[15] = v_80;
  o[16] = v_81;
  o[17] = v_82;
  o[18] = v_83;
  o[19] = v_84;
  o[20] = v_85;
  o[21] = v_86;
  o[22] = v_87;
  o[23] = v_88;
  o[24] = v_89;
  o[25] = v_90;
  o[26] = v_91;
  o[27] = v_92;
  o[28] = v_93;
  o[29] = v_94;
  o[30] = v_95;
  o[31] = v_96;
  o[32] = v_97;
  o[33] = v_98;
  o[34] = v_99;
  o[35] = v_100;
  o[36] = v_101;
  o[37] = v_102;
  o[38] = v_103;
  o[39] = v_104;
  o[40] = v_105;
  o[41] = v_106;
  o[42] = v_107;
  o[43] = v_108;
  o[44] = v_109;
  o[45] = v_110;
  o[46] = v_111;
  o[47] = v_112;
  o[48] = v_113;
  o[49] = v_114;
  o[50] = v_115;
  o[51] = v_116;
  o[52] = v_117;
  o[53] = v_118;
  o[54] = v_119;
  o[55] = v_120;
  o[56] = v_121;
  return 1;
}

INLINE Term spin_62(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3, Term r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9) {
  u32 wpoll = 0;
  Term v_8 = 0;
  Term v_9 = 0;
  Term v_10 = 0;
  Term v_11 = 0;
  Term r_0 = r0;
  u32 r_1 = r1;
  Term out_1 = r2;
  Term nxt_1 = r3;
  Term acc_1 = r4;
  u32 f_1 = r5;
  u32 atrw_1 = r6;
  u32 date_1 = r7;
  u32 i_1 = r8;
  u32 n_1 = r9;
  WL_SPIN
    Term at_0 = blk_at(nxt_1, U32_BIN(i_1, +, 1ull), 0);
    u32 c_0 = blk_read(e.mem, 0, term_loc(nxt_1), at_0 + 0);
    Term v_12 = 0;
    Term v_13 = 0;
    Term v_14 = 0;
    Term v_15 = 0;
    Term o_1[4];
    if (spin_60(e, o_1, nxt_1, c_0, r_0, out_1, acc_1, f_1, atrw_1, r_1, date_1, i_1, n_1) == 0) {
      return 0;
    }
    v_12 = o_1[0];
    v_13 = o_1[1];
    v_14 = o_1[2];
    v_15 = o_1[3];
    v_8 = v_12;
    v_9 = v_13;
    v_10 = v_14;
    v_11 = v_15;
  break;
  }
  o[0] = v_8;
  o[1] = v_9;
  o[2] = v_10;
  o[3] = v_11;
  return 1;
}

INLINE Term spin_63(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_4 = 0;
  u32 a_0 = r0;
  u32 b_0 = r1;
  WL_SPIN
    v_4 = f32_rewrap(f32_unbox(a_0) + f32_unbox(b_0));
  break;
  }
  o[0] = v_4;
  return 1;
}

INLINE Term spin_64(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, Term r12, Term r13, Term r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64, u32 r65, u32 r66, u32 r67, u32 r68) {
  u32 wpoll = 0;
  Term v_63 = 0;
  Term v_64 = 0;
  Term v_65 = 0;
  Term v_66 = 0;
  u32 v_67 = 0;
  u32 v_68 = 0;
  u32 v_69 = 0;
  u32 v_70 = 0;
  u32 v_71 = 0;
  u32 v_72 = 0;
  u32 v_73 = 0;
  u32 v_74 = 0;
  u32 v_75 = 0;
  u32 v_76 = 0;
  u32 v_77 = 0;
  u32 v_78 = 0;
  u32 v_79 = 0;
  u32 v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  u32 wh_1 = r2;
  u32 wl_0 = r3;
  u32 hh9_1 = r4;
  u32 ll9_1 = r5;
  u32 sma10_1 = r6;
  u32 b_5 = r7;
  u32 b_6 = r8;
  u32 b_7 = r9;
  u32 b_8 = r10;
  u32 b_9 = r11;
  Term chips_1 = r12;
  Term aux_1 = r13;
  Term out_1 = r14;
  u32 st_53 = r15;
  u32 st_54 = r16;
  u32 st_55 = r17;
  u32 st_56 = r18;
  u32 st_57 = r19;
  u32 st_58 = r20;
  u32 st_59 = r21;
  u32 st_60 = r22;
  u32 st_61 = r23;
  u32 st_62 = r24;
  u32 st_63 = r25;
  u32 st_64 = r26;
  u32 st_65 = r27;
  u32 st_66 = r28;
  u32 st_67 = r29;
  u32 st_68 = r30;
  u32 st_69 = r31;
  u32 st_70 = r32;
  u32 st_71 = r33;
  u32 st_72 = r34;
  u32 st_73 = r35;
  u32 st_74 = r36;
  u32 st_75 = r37;
  u32 st_76 = r38;
  u32 st_77 = r39;
  u32 st_78 = r40;
  u32 st_79 = r41;
  u32 st_80 = r42;
  u32 st_81 = r43;
  u32 st_82 = r44;
  u32 st_83 = r45;
  u32 st_84 = r46;
  u32 st_85 = r47;
  u32 st_86 = r48;
  u32 st_87 = r49;
  u32 st_88 = r50;
  u32 st_89 = r51;
  u32 st_90 = r52;
  u32 st_91 = r53;
  u32 st_92 = r54;
  u32 st_93 = r55;
  u32 st_94 = r56;
  u32 st_95 = r57;
  u32 st_96 = r58;
  u32 st_97 = r59;
  u32 st_98 = r60;
  u32 st_99 = r61;
  u32 st_100 = r62;
  u32 st_101 = r63;
  u32 st_102 = r64;
  u32 st_103 = r65;
  u32 st_104 = r66;
  u32 st_105 = r67;
  u32 i_1 = r68;
  WL_SPIN
    Term v_120 = 0;
    u32 v_121 = 0;
    u32 v_122 = 0;
    u32 v_123 = 0;
    Term o_2[1];
    if (spin_3(e, o_2, U32_BIN(i_1, <, 8ull), i_1, 4ull) == 0) {
      return 0;
    }
    v_123 = o_2[0];
    v_122 = v_123;
    Term v_124 = 0;
    u32 v_125 = 0;
    Term o_3[2];
    if (spin_42(e, o_3, r_2, U32_BIN(i_1, -, v_122), 3ull) == 0) {
      return 0;
    }
    v_124 = o_3[0];
    v_125 = o_3[1];
    v_120 = v_124;
    v_121 = v_125;
    u32 v_126 = 0;
    u32 v_127 = 0;
    Term o_4[1];
    if (spin_44(e, o_4, r_3) == 0) {
      return 0;
    }
    v_127 = o_4[0];
    v_126 = v_127;
    Term v_128 = 0;
    Term v_129 = 0;
    Term v_130 = 0;
    Term v_131 = 0;
    u32 v_132 = 0;
    u32 v_133 = 0;
    u32 v_134 = 0;
    u32 v_135 = 0;
    u32 v_136 = 0;
    u32 v_137 = 0;
    u32 v_138 = 0;
    u32 v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    Term o_5[57];
    if (spin_61(e, o_5, v_120, v_121, v_126, wh_1, wl_0, hh9_1, ll9_1, sma10_1, b_5, b_6, b_7, b_8, b_9, chips_1, aux_1, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_1) == 0) {
      return 0;
    }
    v_128 = o_5[0];
    v_129 = o_5[1];
    v_130 = o_5[2];
    v_131 = o_5[3];
    v_132 = o_5[4];
    v_133 = o_5[5];
    v_134 = o_5[6];
    v_135 = o_5[7];
    v_136 = o_5[8];
    v_137 = o_5[9];
    v_138 = o_5[10];
    v_139 = o_5[11];
    v_140 = o_5[12];
    v_141 = o_5[13];
    v_142 = o_5[14];
    v_143 = o_5[15];
    v_144 = o_5[16];
    v_145 = o_5[17];
    v_146 = o_5[18];
    v_147 = o_5[19];
    v_148 = o_5[20];
    v_149 = o_5[21];
    v_150 = o_5[22];
    v_151 = o_5[23];
    v_152 = o_5[24];
    v_153 = o_5[25];
    v_154 = o_5[26];
    v_155 = o_5[27];
    v_156 = o_5[28];
    v_157 = o_5[29];
    v_158 = o_5[30];
    v_159 = o_5[31];
    v_160 = o_5[32];
    v_161 = o_5[33];
    v_162 = o_5[34];
    v_163 = o_5[35];
    v_164 = o_5[36];
    v_165 = o_5[37];
    v_166 = o_5[38];
    v_167 = o_5[39];
    v_168 = o_5[40];
    v_169 = o_5[41];
    v_170 = o_5[42];
    v_171 = o_5[43];
    v_172 = o_5[44];
    v_173 = o_5[45];
    v_174 = o_5[46];
    v_175 = o_5[47];
    v_176 = o_5[48];
    v_177 = o_5[49];
    v_178 = o_5[50];
    v_179 = o_5[51];
    v_180 = o_5[52];
    v_181 = o_5[53];
    v_182 = o_5[54];
    v_183 = o_5[55];
    v_184 = o_5[56];
    v_63 = v_128;
    v_64 = v_129;
    v_65 = v_130;
    v_66 = v_131;
    v_67 = v_132;
    v_68 = v_133;
    v_69 = v_134;
    v_70 = v_135;
    v_71 = v_136;
    v_72 = v_137;
    v_73 = v_138;
    v_74 = v_139;
    v_75 = v_140;
    v_76 = v_141;
    v_77 = v_142;
    v_78 = v_143;
    v_79 = v_144;
    v_80 = v_145;
    v_81 = v_146;
    v_82 = v_147;
    v_83 = v_148;
    v_84 = v_149;
    v_85 = v_150;
    v_86 = v_151;
    v_87 = v_152;
    v_88 = v_153;
    v_89 = v_154;
    v_90 = v_155;
    v_91 = v_156;
    v_92 = v_157;
    v_93 = v_158;
    v_94 = v_159;
    v_95 = v_160;
    v_96 = v_161;
    v_97 = v_162;
    v_98 = v_163;
    v_99 = v_164;
    v_100 = v_165;
    v_101 = v_166;
    v_102 = v_167;
    v_103 = v_168;
    v_104 = v_169;
    v_105 = v_170;
    v_106 = v_171;
    v_107 = v_172;
    v_108 = v_173;
    v_109 = v_174;
    v_110 = v_175;
    v_111 = v_176;
    v_112 = v_177;
    v_113 = v_178;
    v_114 = v_179;
    v_115 = v_180;
    v_116 = v_181;
    v_117 = v_182;
    v_118 = v_183;
    v_119 = v_184;
  break;
  }
  o[0] = v_63;
  o[1] = v_64;
  o[2] = v_65;
  o[3] = v_66;
  o[4] = v_67;
  o[5] = v_68;
  o[6] = v_69;
  o[7] = v_70;
  o[8] = v_71;
  o[9] = v_72;
  o[10] = v_73;
  o[11] = v_74;
  o[12] = v_75;
  o[13] = v_76;
  o[14] = v_77;
  o[15] = v_78;
  o[16] = v_79;
  o[17] = v_80;
  o[18] = v_81;
  o[19] = v_82;
  o[20] = v_83;
  o[21] = v_84;
  o[22] = v_85;
  o[23] = v_86;
  o[24] = v_87;
  o[25] = v_88;
  o[26] = v_89;
  o[27] = v_90;
  o[28] = v_91;
  o[29] = v_92;
  o[30] = v_93;
  o[31] = v_94;
  o[32] = v_95;
  o[33] = v_96;
  o[34] = v_97;
  o[35] = v_98;
  o[36] = v_99;
  o[37] = v_100;
  o[38] = v_101;
  o[39] = v_102;
  o[40] = v_103;
  o[41] = v_104;
  o[42] = v_105;
  o[43] = v_106;
  o[44] = v_107;
  o[45] = v_108;
  o[46] = v_109;
  o[47] = v_110;
  o[48] = v_111;
  o[49] = v_112;
  o[50] = v_113;
  o[51] = v_114;
  o[52] = v_115;
  o[53] = v_116;
  o[54] = v_117;
  o[55] = v_118;
  o[56] = v_119;
  return 1;
}

INLINE Term spin_65(Env e, THR Term* o, Term r0, Term r1, Term r2, Term r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8) {
  u32 wpoll = 0;
  Term v_4 = 0;
  Term v_5 = 0;
  Term v_6 = 0;
  Term v_7 = 0;
  Term bars_0 = r0;
  Term out_0 = r1;
  Term nxt_0 = r2;
  Term acc_0 = r3;
  u32 f_1 = r4;
  u32 atrw_1 = r5;
  u32 date_1 = r6;
  u32 i_1 = r7;
  u32 n_1 = r8;
  WL_SPIN
    Term v_8 = 0;
    u32 v_9 = 0;
    Term v_10 = 0;
    u32 v_11 = 0;
    Term o_0[2];
    if (spin_42(e, o_0, bars_0, U32_BIN(i_1, +, 1ull), 1ull) == 0) {
      return 0;
    }
    v_10 = o_0[0];
    v_11 = o_0[1];
    v_8 = v_10;
    v_9 = v_11;
    Term v_12 = 0;
    Term v_13 = 0;
    Term v_14 = 0;
    Term v_15 = 0;
    Term o_1[4];
    if (spin_62(e, o_1, v_8, v_9, out_0, nxt_0, acc_0, f_1, atrw_1, date_1, i_1, n_1) == 0) {
      return 0;
    }
    v_12 = o_1[0];
    v_13 = o_1[1];
    v_14 = o_1[2];
    v_15 = o_1[3];
    v_4 = v_12;
    v_5 = v_13;
    v_6 = v_14;
    v_7 = v_15;
  break;
  }
  o[0] = v_4;
  o[1] = v_5;
  o[2] = v_6;
  o[3] = v_7;
  return 1;
}

INLINE Term spin_66(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_4 = 0;
  u32 a_0 = r0;
  u32 b_0 = r1;
  WL_SPIN
    u32 v_5 = 0;
    Term o_1[1];
    if (spin_9(e, o_1, a_0, b_0) == 0) {
      return 0;
    }
    v_5 = o_1[0];
    v_4 = v_5;
  break;
  }
  o[0] = v_4;
  return 1;
}

INLINE Term spin_67(Env e, THR Term* o, Term r0, u32 r1, u32 r2) {
  u32 wpoll = 0;
  Term v_6 = 0;
  u32 v_7 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  u32 acc_0 = r2;
  WL_SPIN
    u32 v_8 = 0;
    u32 v_9 = 0;
    u32 v_10 = 0;
    Term o_1[1];
    if (spin_44(e, o_1, r_3) == 0) {
      return 0;
    }
    v_10 = o_1[0];
    v_9 = v_10;
    u32 v_11 = 0;
    Term o_2[1];
    if (spin_63(e, o_2, acc_0, v_9) == 0) {
      return 0;
    }
    v_11 = o_2[0];
    v_8 = v_11;
    v_6 = r_2;
    v_7 = v_8;
  break;
  }
  o[0] = v_6;
  o[1] = v_7;
  return 1;
}

INLINE Term spin_69(Env e, THR Term* o, Term r0, u32 r1, u32 r2) {
  u32 wpoll = 0;
  Term v_14 = 0;
  u32 v_15 = 0;
  Term r_4 = r0;
  u32 r_5 = r1;
  u32 acc_0 = r2;
  WL_SPIN
    u32 v_16 = 0;
    u32 v_17 = 0;
    u32 v_18 = 0;
    Term o_2[1];
    if (spin_44(e, o_2, r_5) == 0) {
      return 0;
    }
    v_18 = o_2[0];
    v_17 = v_18;
    u32 v_19 = 0;
    Term o_3[1];
    if (spin_66(e, o_3, acc_0, v_17) == 0) {
      return 0;
    }
    v_19 = o_3[0];
    v_16 = v_19;
    v_14 = r_4;
    v_15 = v_16;
  break;
  }
  o[0] = v_14;
  o[1] = v_15;
  return 1;
}

INLINE Term spin_68(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3) {
  u32 wpoll = 0;
  Term v_6 = 0;
  u32 v_7 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  u32 idx_0 = r2;
  u32 k_0 = r3;
  WL_SPIN
    Term v_8 = 0;
    u32 v_9 = 0;
    Term v_10 = 0;
    u32 v_11 = 0;
    Term o_1[2];
    if (spin_42(e, o_1, r_2, idx_0, k_0) == 0) {
      return 0;
    }
    v_10 = o_1[0];
    v_11 = o_1[1];
    v_8 = v_10;
    v_9 = v_11;
    Term v_12 = 0;
    u32 v_13 = 0;
    Term o_4[2];
    if (spin_69(e, o_4, v_8, v_9, r_3) == 0) {
      return 0;
    }
    v_12 = o_4[0];
    v_13 = o_4[1];
    v_6 = v_12;
    v_7 = v_13;
  break;
  }
  o[0] = v_6;
  o[1] = v_7;
  return 1;
}

INLINE Term spin_70(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, Term r11, Term r12, Term r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64, u32 r65, u32 r66, u32 r67) {
  u32 wpoll = 0;
  Term v_105 = 0;
  Term v_106 = 0;
  Term v_107 = 0;
  Term v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  u32 v_128 = 0;
  u32 v_129 = 0;
  u32 v_130 = 0;
  u32 v_131 = 0;
  u32 v_132 = 0;
  u32 v_133 = 0;
  u32 v_134 = 0;
  u32 v_135 = 0;
  u32 v_136 = 0;
  u32 v_137 = 0;
  u32 v_138 = 0;
  u32 v_139 = 0;
  u32 v_140 = 0;
  u32 v_141 = 0;
  u32 v_142 = 0;
  u32 v_143 = 0;
  u32 v_144 = 0;
  u32 v_145 = 0;
  u32 v_146 = 0;
  u32 v_147 = 0;
  u32 v_148 = 0;
  u32 v_149 = 0;
  u32 v_150 = 0;
  u32 v_151 = 0;
  u32 v_152 = 0;
  u32 v_153 = 0;
  u32 v_154 = 0;
  u32 v_155 = 0;
  u32 v_156 = 0;
  u32 v_157 = 0;
  u32 v_158 = 0;
  u32 v_159 = 0;
  u32 v_160 = 0;
  u32 v_161 = 0;
  Term r_6 = r0;
  u32 r_7 = r1;
  u32 wh_0 = r2;
  u32 hh9_1 = r3;
  u32 ll9_1 = r4;
  u32 sma10_1 = r5;
  u32 b_5 = r6;
  u32 b_6 = r7;
  u32 b_7 = r8;
  u32 b_8 = r9;
  u32 b_9 = r10;
  Term chips_1 = r11;
  Term aux_1 = r12;
  Term out_1 = r13;
  u32 st_53 = r14;
  u32 st_54 = r15;
  u32 st_55 = r16;
  u32 st_56 = r17;
  u32 st_57 = r18;
  u32 st_58 = r19;
  u32 st_59 = r20;
  u32 st_60 = r21;
  u32 st_61 = r22;
  u32 st_62 = r23;
  u32 st_63 = r24;
  u32 st_64 = r25;
  u32 st_65 = r26;
  u32 st_66 = r27;
  u32 st_67 = r28;
  u32 st_68 = r29;
  u32 st_69 = r30;
  u32 st_70 = r31;
  u32 st_71 = r32;
  u32 st_72 = r33;
  u32 st_73 = r34;
  u32 st_74 = r35;
  u32 st_75 = r36;
  u32 st_76 = r37;
  u32 st_77 = r38;
  u32 st_78 = r39;
  u32 st_79 = r40;
  u32 st_80 = r41;
  u32 st_81 = r42;
  u32 st_82 = r43;
  u32 st_83 = r44;
  u32 st_84 = r45;
  u32 st_85 = r46;
  u32 st_86 = r47;
  u32 st_87 = r48;
  u32 st_88 = r49;
  u32 st_89 = r50;
  u32 st_90 = r51;
  u32 st_91 = r52;
  u32 st_92 = r53;
  u32 st_93 = r54;
  u32 st_94 = r55;
  u32 st_95 = r56;
  u32 st_96 = r57;
  u32 st_97 = r58;
  u32 st_98 = r59;
  u32 st_99 = r60;
  u32 st_100 = r61;
  u32 st_101 = r62;
  u32 st_102 = r63;
  u32 st_103 = r64;
  u32 st_104 = r65;
  u32 st_105 = r66;
  u32 i_1 = r67;
  WL_SPIN
    Term v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    Term o_13[1];
    if (spin_3(e, o_13, U32_BIN(i_1, <, 8ull), i_1, 4ull) == 0) {
      return 0;
    }
    v_165 = o_13[0];
    v_164 = v_165;
    Term v_166 = 0;
    u32 v_167 = 0;
    Term o_14[2];
    if (spin_42(e, o_14, r_6, U32_BIN(i_1, -, v_164), 2ull) == 0) {
      return 0;
    }
    v_166 = o_14[0];
    v_167 = o_14[1];
    v_162 = v_166;
    v_163 = v_167;
    Term v_168 = 0;
    Term v_169 = 0;
    Term v_170 = 0;
    Term v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    u32 v_185 = 0;
    u32 v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    u32 v_191 = 0;
    u32 v_192 = 0;
    u32 v_193 = 0;
    u32 v_194 = 0;
    u32 v_195 = 0;
    u32 v_196 = 0;
    u32 v_197 = 0;
    u32 v_198 = 0;
    u32 v_199 = 0;
    u32 v_200 = 0;
    u32 v_201 = 0;
    u32 v_202 = 0;
    u32 v_203 = 0;
    u32 v_204 = 0;
    u32 v_205 = 0;
    u32 v_206 = 0;
    u32 v_207 = 0;
    u32 v_208 = 0;
    u32 v_209 = 0;
    u32 v_210 = 0;
    u32 v_211 = 0;
    u32 v_212 = 0;
    u32 v_213 = 0;
    u32 v_214 = 0;
    u32 v_215 = 0;
    u32 v_216 = 0;
    u32 v_217 = 0;
    u32 v_218 = 0;
    u32 v_219 = 0;
    u32 v_220 = 0;
    u32 v_221 = 0;
    u32 v_222 = 0;
    u32 v_223 = 0;
    u32 v_224 = 0;
    Term o_15[57];
    if (spin_64(e, o_15, v_162, v_163, wh_0, r_7, hh9_1, ll9_1, sma10_1, b_5, b_6, b_7, b_8, b_9, chips_1, aux_1, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_1) == 0) {
      return 0;
    }
    v_168 = o_15[0];
    v_169 = o_15[1];
    v_170 = o_15[2];
    v_171 = o_15[3];
    v_172 = o_15[4];
    v_173 = o_15[5];
    v_174 = o_15[6];
    v_175 = o_15[7];
    v_176 = o_15[8];
    v_177 = o_15[9];
    v_178 = o_15[10];
    v_179 = o_15[11];
    v_180 = o_15[12];
    v_181 = o_15[13];
    v_182 = o_15[14];
    v_183 = o_15[15];
    v_184 = o_15[16];
    v_185 = o_15[17];
    v_186 = o_15[18];
    v_187 = o_15[19];
    v_188 = o_15[20];
    v_189 = o_15[21];
    v_190 = o_15[22];
    v_191 = o_15[23];
    v_192 = o_15[24];
    v_193 = o_15[25];
    v_194 = o_15[26];
    v_195 = o_15[27];
    v_196 = o_15[28];
    v_197 = o_15[29];
    v_198 = o_15[30];
    v_199 = o_15[31];
    v_200 = o_15[32];
    v_201 = o_15[33];
    v_202 = o_15[34];
    v_203 = o_15[35];
    v_204 = o_15[36];
    v_205 = o_15[37];
    v_206 = o_15[38];
    v_207 = o_15[39];
    v_208 = o_15[40];
    v_209 = o_15[41];
    v_210 = o_15[42];
    v_211 = o_15[43];
    v_212 = o_15[44];
    v_213 = o_15[45];
    v_214 = o_15[46];
    v_215 = o_15[47];
    v_216 = o_15[48];
    v_217 = o_15[49];
    v_218 = o_15[50];
    v_219 = o_15[51];
    v_220 = o_15[52];
    v_221 = o_15[53];
    v_222 = o_15[54];
    v_223 = o_15[55];
    v_224 = o_15[56];
    v_105 = v_168;
    v_106 = v_169;
    v_107 = v_170;
    v_108 = v_171;
    v_109 = v_172;
    v_110 = v_173;
    v_111 = v_174;
    v_112 = v_175;
    v_113 = v_176;
    v_114 = v_177;
    v_115 = v_178;
    v_116 = v_179;
    v_117 = v_180;
    v_118 = v_181;
    v_119 = v_182;
    v_120 = v_183;
    v_121 = v_184;
    v_122 = v_185;
    v_123 = v_186;
    v_124 = v_187;
    v_125 = v_188;
    v_126 = v_189;
    v_127 = v_190;
    v_128 = v_191;
    v_129 = v_192;
    v_130 = v_193;
    v_131 = v_194;
    v_132 = v_195;
    v_133 = v_196;
    v_134 = v_197;
    v_135 = v_198;
    v_136 = v_199;
    v_137 = v_200;
    v_138 = v_201;
    v_139 = v_202;
    v_140 = v_203;
    v_141 = v_204;
    v_142 = v_205;
    v_143 = v_206;
    v_144 = v_207;
    v_145 = v_208;
    v_146 = v_209;
    v_147 = v_210;
    v_148 = v_211;
    v_149 = v_212;
    v_150 = v_213;
    v_151 = v_214;
    v_152 = v_215;
    v_153 = v_216;
    v_154 = v_217;
    v_155 = v_218;
    v_156 = v_219;
    v_157 = v_220;
    v_158 = v_221;
    v_159 = v_222;
    v_160 = v_223;
    v_161 = v_224;
  break;
  }
  o[0] = v_105;
  o[1] = v_106;
  o[2] = v_107;
  o[3] = v_108;
  o[4] = v_109;
  o[5] = v_110;
  o[6] = v_111;
  o[7] = v_112;
  o[8] = v_113;
  o[9] = v_114;
  o[10] = v_115;
  o[11] = v_116;
  o[12] = v_117;
  o[13] = v_118;
  o[14] = v_119;
  o[15] = v_120;
  o[16] = v_121;
  o[17] = v_122;
  o[18] = v_123;
  o[19] = v_124;
  o[20] = v_125;
  o[21] = v_126;
  o[22] = v_127;
  o[23] = v_128;
  o[24] = v_129;
  o[25] = v_130;
  o[26] = v_131;
  o[27] = v_132;
  o[28] = v_133;
  o[29] = v_134;
  o[30] = v_135;
  o[31] = v_136;
  o[32] = v_137;
  o[33] = v_138;
  o[34] = v_139;
  o[35] = v_140;
  o[36] = v_141;
  o[37] = v_142;
  o[38] = v_143;
  o[39] = v_144;
  o[40] = v_145;
  o[41] = v_146;
  o[42] = v_147;
  o[43] = v_148;
  o[44] = v_149;
  o[45] = v_150;
  o[46] = v_151;
  o[47] = v_152;
  o[48] = v_153;
  o[49] = v_154;
  o[50] = v_155;
  o[51] = v_156;
  o[52] = v_157;
  o[53] = v_158;
  o[54] = v_159;
  o[55] = v_160;
  o[56] = v_161;
  return 1;
}

INLINE Term spin_71(Env e, THR Term* o, u32 r0, Term r1, Term r2, Term r3, Term r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9) {
  u32 wpoll = 0;
  Term v_12 = 0;
  Term v_13 = 0;
  Term v_14 = 0;
  Term v_15 = 0;
  u32 go_0 = r0;
  Term q_0 = r1;
  Term q_1 = r2;
  Term q_2 = r3;
  Term q_3 = r4;
  u32 f_1 = r5;
  u32 atrw_0 = r6;
  u32 date_1 = r7;
  u32 i_1 = r8;
  u32 n_1 = r9;
  WL_SPIN
    if (go_0 == 1) {
      Term v_16 = 0;
      Term v_17 = 0;
      Term v_18 = 0;
      Term v_19 = 0;
      Term o_0[4];
      if (spin_65(e, o_0, q_0, q_1, q_2, q_3, f_1, atrw_0, date_1, i_1, n_1) == 0) {
        return 0;
      }
      v_16 = o_0[0];
      v_17 = o_0[1];
      v_18 = o_0[2];
      v_19 = o_0[3];
      v_12 = v_16;
      v_13 = v_17;
      v_14 = v_18;
      v_15 = v_19;
    } else {
      v_12 = q_0;
      v_13 = q_1;
      v_14 = q_2;
      v_15 = q_3;
    }
  break;
  }
  o[0] = v_12;
  o[1] = v_13;
  o[2] = v_14;
  o[3] = v_15;
  return 1;
}

INLINE Term spin_72(Env e, THR Term* o, Term r0, Term r1, Term r2, Term r3, u32 r4, u32 r5, u32 r6) {
  u32 wpoll = 0;
  Term v_35 = 0;
  Term v_36 = 0;
  Term v_37 = 0;
  Term v_38 = 0;
  u32 v_39 = 0;
  u32 v_40 = 0;
  u32 v_41 = 0;
  Term q_4 = r0;
  Term q_5 = r1;
  Term q_6 = r2;
  Term q_7 = r3;
  u32 c_on_1 = r4;
  u32 r_on_1 = r5;
  u32 cnt_1 = r6;
  WL_SPIN
    v_35 = q_4;
    v_36 = q_5;
    v_37 = q_6;
    v_38 = q_7;
    v_39 = c_on_1;
    v_40 = r_on_1;
    v_41 = cnt_1;
  break;
  }
  o[0] = v_35;
  o[1] = v_36;
  o[2] = v_37;
  o[3] = v_38;
  o[4] = v_39;
  o[5] = v_40;
  o[6] = v_41;
  return 1;
}

INLINE Term spin_73(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_4 = 0;
  u32 a_0 = r0;
  u32 b_0 = r1;
  WL_SPIN
    u32 v_5 = 0;
    Term o_1[1];
    if (spin_0(e, o_1, a_0, b_0) == 0) {
      return 0;
    }
    v_5 = o_1[0];
    v_4 = v_5;
  break;
  }
  o[0] = v_4;
  return 1;
}

INLINE Term spin_74(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3) {
  u32 wpoll = 0;
  Term v_4 = 0;
  u32 v_5 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  u32 idx_1 = r2;
  u32 k_1 = r3;
  WL_SPIN
    Term v_6 = 0;
    u32 v_7 = 0;
    Term v_8 = 0;
    u32 v_9 = 0;
    Term o_0[2];
    if (spin_42(e, o_0, r_2, idx_1, k_1) == 0) {
      return 0;
    }
    v_8 = o_0[0];
    v_9 = o_0[1];
    v_6 = v_8;
    v_7 = v_9;
    Term v_10 = 0;
    u32 v_11 = 0;
    Term o_1[2];
    if (spin_67(e, o_1, v_6, v_7, r_3) == 0) {
      return 0;
    }
    v_10 = o_1[0];
    v_11 = o_1[1];
    v_4 = v_10;
    v_5 = v_11;
  break;
  }
  o[0] = v_4;
  o[1] = v_5;
  return 1;
}

INLINE Term spin_76(Env e, THR Term* o, Term r0, u32 r1, u32 r2) {
  u32 wpoll = 0;
  Term v_18 = 0;
  u32 v_19 = 0;
  Term r_4 = r0;
  u32 r_5 = r1;
  u32 acc_0 = r2;
  WL_SPIN
    u32 v_20 = 0;
    u32 v_21 = 0;
    u32 v_22 = 0;
    Term o_4[1];
    if (spin_44(e, o_4, r_5) == 0) {
      return 0;
    }
    v_22 = o_4[0];
    v_21 = v_22;
    u32 v_23 = 0;
    Term o_5[1];
    if (spin_73(e, o_5, acc_0, v_21) == 0) {
      return 0;
    }
    v_23 = o_5[0];
    v_20 = v_23;
    v_18 = r_4;
    v_19 = v_20;
  break;
  }
  o[0] = v_18;
  o[1] = v_19;
  return 1;
}

INLINE Term spin_75(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3) {
  u32 wpoll = 0;
  Term v_10 = 0;
  u32 v_11 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  u32 idx_0 = r2;
  u32 k_0 = r3;
  WL_SPIN
    Term v_12 = 0;
    u32 v_13 = 0;
    Term v_14 = 0;
    u32 v_15 = 0;
    Term o_3[2];
    if (spin_42(e, o_3, r_2, idx_0, k_0) == 0) {
      return 0;
    }
    v_14 = o_3[0];
    v_15 = o_3[1];
    v_12 = v_14;
    v_13 = v_15;
    Term v_16 = 0;
    u32 v_17 = 0;
    Term o_6[2];
    if (spin_76(e, o_6, v_12, v_13, r_3) == 0) {
      return 0;
    }
    v_16 = o_6[0];
    v_17 = o_6[1];
    v_10 = v_16;
    v_11 = v_17;
  break;
  }
  o[0] = v_10;
  o[1] = v_11;
  return 1;
}

FAR Term spin_77(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, Term r10, Term r11, Term r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64, u32 r65, u32 r66) {
  u32 wpoll = 0;
  Term v_109 = 0;
  Term v_110 = 0;
  Term v_111 = 0;
  Term v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  u32 v_128 = 0;
  u32 v_129 = 0;
  u32 v_130 = 0;
  u32 v_131 = 0;
  u32 v_132 = 0;
  u32 v_133 = 0;
  u32 v_134 = 0;
  u32 v_135 = 0;
  u32 v_136 = 0;
  u32 v_137 = 0;
  u32 v_138 = 0;
  u32 v_139 = 0;
  u32 v_140 = 0;
  u32 v_141 = 0;
  u32 v_142 = 0;
  u32 v_143 = 0;
  u32 v_144 = 0;
  u32 v_145 = 0;
  u32 v_146 = 0;
  u32 v_147 = 0;
  u32 v_148 = 0;
  u32 v_149 = 0;
  u32 v_150 = 0;
  u32 v_151 = 0;
  u32 v_152 = 0;
  u32 v_153 = 0;
  u32 v_154 = 0;
  u32 v_155 = 0;
  u32 v_156 = 0;
  u32 v_157 = 0;
  u32 v_158 = 0;
  u32 v_159 = 0;
  u32 v_160 = 0;
  u32 v_161 = 0;
  u32 v_162 = 0;
  u32 v_163 = 0;
  u32 v_164 = 0;
  u32 v_165 = 0;
  Term r_6 = r0;
  u32 r_7 = r1;
  u32 hh9_1 = r2;
  u32 ll9_1 = r3;
  u32 sma10_1 = r4;
  u32 b_5 = r5;
  u32 b_6 = r6;
  u32 b_7 = r7;
  u32 b_8 = r8;
  u32 b_9 = r9;
  Term chips_1 = r10;
  Term aux_1 = r11;
  Term out_1 = r12;
  u32 st_53 = r13;
  u32 st_54 = r14;
  u32 st_55 = r15;
  u32 st_56 = r16;
  u32 st_57 = r17;
  u32 st_58 = r18;
  u32 st_59 = r19;
  u32 st_60 = r20;
  u32 st_61 = r21;
  u32 st_62 = r22;
  u32 st_63 = r23;
  u32 st_64 = r24;
  u32 st_65 = r25;
  u32 st_66 = r26;
  u32 st_67 = r27;
  u32 st_68 = r28;
  u32 st_69 = r29;
  u32 st_70 = r30;
  u32 st_71 = r31;
  u32 st_72 = r32;
  u32 st_73 = r33;
  u32 st_74 = r34;
  u32 st_75 = r35;
  u32 st_76 = r36;
  u32 st_77 = r37;
  u32 st_78 = r38;
  u32 st_79 = r39;
  u32 st_80 = r40;
  u32 st_81 = r41;
  u32 st_82 = r42;
  u32 st_83 = r43;
  u32 st_84 = r44;
  u32 st_85 = r45;
  u32 st_86 = r46;
  u32 st_87 = r47;
  u32 st_88 = r48;
  u32 st_89 = r49;
  u32 st_90 = r50;
  u32 st_91 = r51;
  u32 st_92 = r52;
  u32 st_93 = r53;
  u32 st_94 = r54;
  u32 st_95 = r55;
  u32 st_96 = r56;
  u32 st_97 = r57;
  u32 st_98 = r58;
  u32 st_99 = r59;
  u32 st_100 = r60;
  u32 st_101 = r61;
  u32 st_102 = r62;
  u32 st_103 = r63;
  u32 st_104 = r64;
  u32 st_105 = r65;
  u32 i_1 = r66;
  WL_SPIN
    u32 v_166 = 0;
    u32 v_167 = 0;
    Term o_15[1];
    if (spin_3(e, o_15, U32_BIN(i_1, <, 8ull), 0ull, U32_BIN(i_1, -, 8ull)) == 0) {
      return 0;
    }
    v_167 = o_15[0];
    v_166 = v_167;
    u32 a_8 = U32_BIN(v_166, +, 5ull);
    u32 a_9 = 1315859240ull;
    u32 a_10 = U32_BIN(v_166, +, 1ull);
    Term v_168 = 0;
    u32 v_169 = 0;
    Term v_170 = 0;
    u32 v_171 = 0;
    Term o_16[2];
    if (spin_68(e, o_16, r_6, a_9, v_166, 3ull) == 0) {
      return 0;
    }
    v_170 = o_16[0];
    v_171 = o_16[1];
    v_168 = v_170;
    v_169 = v_171;
    u32 a_11 = U32_BIN(a_10, +, 1ull);
    Term v_172 = 0;
    u32 v_173 = 0;
    Term v_174 = 0;
    u32 v_175 = 0;
    Term o_17[2];
    if (spin_68(e, o_17, v_168, v_169, a_10, 3ull) == 0) {
      return 0;
    }
    v_174 = o_17[0];
    v_175 = o_17[1];
    v_172 = v_174;
    v_173 = v_175;
    u32 a_12 = U32_BIN(a_11, +, 1ull);
    Term v_176 = 0;
    u32 v_177 = 0;
    Term v_178 = 0;
    u32 v_179 = 0;
    Term o_18[2];
    if (spin_68(e, o_18, v_172, v_173, a_11, 3ull) == 0) {
      return 0;
    }
    v_178 = o_18[0];
    v_179 = o_18[1];
    v_176 = v_178;
    v_177 = v_179;
    Term v_180 = 0;
    u32 v_181 = 0;
    Term v_182 = 0;
    u32 v_183 = 0;
    Term o_19[2];
    if (spin_68(e, o_19, v_176, v_177, a_12, 3ull) == 0) {
      return 0;
    }
    v_182 = o_19[0];
    v_183 = o_19[1];
    v_180 = v_182;
    v_181 = v_183;
    u32 a_13 = U32_BIN(a_8, +, 1ull);
    Term v_184 = 0;
    u32 v_185 = 0;
    Term v_186 = 0;
    u32 v_187 = 0;
    Term o_20[2];
    if (spin_68(e, o_20, v_180, v_181, a_8, 3ull) == 0) {
      return 0;
    }
    v_186 = o_20[0];
    v_187 = o_20[1];
    v_184 = v_186;
    v_185 = v_187;
    u32 a_14 = U32_BIN(a_13, +, 1ull);
    Term v_188 = 0;
    u32 v_189 = 0;
    Term v_190 = 0;
    u32 v_191 = 0;
    Term o_21[2];
    if (spin_68(e, o_21, v_184, v_185, a_13, 3ull) == 0) {
      return 0;
    }
    v_190 = o_21[0];
    v_191 = o_21[1];
    v_188 = v_190;
    v_189 = v_191;
    u32 a_15 = U32_BIN(a_14, +, 1ull);
    Term v_192 = 0;
    u32 v_193 = 0;
    Term v_194 = 0;
    u32 v_195 = 0;
    Term o_22[2];
    if (spin_68(e, o_22, v_188, v_189, a_14, 3ull) == 0) {
      return 0;
    }
    v_194 = o_22[0];
    v_195 = o_22[1];
    v_192 = v_194;
    v_193 = v_195;
    Term v_196 = 0;
    u32 v_197 = 0;
    Term v_198 = 0;
    u32 v_199 = 0;
    Term o_23[2];
    if (spin_68(e, o_23, v_192, v_193, a_15, 3ull) == 0) {
      return 0;
    }
    v_198 = o_23[0];
    v_199 = o_23[1];
    v_196 = v_198;
    v_197 = v_199;
    Term v_200 = 0;
    Term v_201 = 0;
    Term v_202 = 0;
    Term v_203 = 0;
    u32 v_204 = 0;
    u32 v_205 = 0;
    u32 v_206 = 0;
    u32 v_207 = 0;
    u32 v_208 = 0;
    u32 v_209 = 0;
    u32 v_210 = 0;
    u32 v_211 = 0;
    u32 v_212 = 0;
    u32 v_213 = 0;
    u32 v_214 = 0;
    u32 v_215 = 0;
    u32 v_216 = 0;
    u32 v_217 = 0;
    u32 v_218 = 0;
    u32 v_219 = 0;
    u32 v_220 = 0;
    u32 v_221 = 0;
    u32 v_222 = 0;
    u32 v_223 = 0;
    u32 v_224 = 0;
    u32 v_225 = 0;
    u32 v_226 = 0;
    u32 v_227 = 0;
    u32 v_228 = 0;
    u32 v_229 = 0;
    u32 v_230 = 0;
    u32 v_231 = 0;
    u32 v_232 = 0;
    u32 v_233 = 0;
    u32 v_234 = 0;
    u32 v_235 = 0;
    u32 v_236 = 0;
    u32 v_237 = 0;
    u32 v_238 = 0;
    u32 v_239 = 0;
    u32 v_240 = 0;
    u32 v_241 = 0;
    u32 v_242 = 0;
    u32 v_243 = 0;
    u32 v_244 = 0;
    u32 v_245 = 0;
    u32 v_246 = 0;
    u32 v_247 = 0;
    u32 v_248 = 0;
    u32 v_249 = 0;
    u32 v_250 = 0;
    u32 v_251 = 0;
    u32 v_252 = 0;
    u32 v_253 = 0;
    u32 v_254 = 0;
    u32 v_255 = 0;
    u32 v_256 = 0;
    Term o_24[57];
    if (spin_70(e, o_24, v_196, v_197, r_7, hh9_1, ll9_1, sma10_1, b_5, b_6, b_7, b_8, b_9, chips_1, aux_1, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_1) == 0) {
      return 0;
    }
    v_200 = o_24[0];
    v_201 = o_24[1];
    v_202 = o_24[2];
    v_203 = o_24[3];
    v_204 = o_24[4];
    v_205 = o_24[5];
    v_206 = o_24[6];
    v_207 = o_24[7];
    v_208 = o_24[8];
    v_209 = o_24[9];
    v_210 = o_24[10];
    v_211 = o_24[11];
    v_212 = o_24[12];
    v_213 = o_24[13];
    v_214 = o_24[14];
    v_215 = o_24[15];
    v_216 = o_24[16];
    v_217 = o_24[17];
    v_218 = o_24[18];
    v_219 = o_24[19];
    v_220 = o_24[20];
    v_221 = o_24[21];
    v_222 = o_24[22];
    v_223 = o_24[23];
    v_224 = o_24[24];
    v_225 = o_24[25];
    v_226 = o_24[26];
    v_227 = o_24[27];
    v_228 = o_24[28];
    v_229 = o_24[29];
    v_230 = o_24[30];
    v_231 = o_24[31];
    v_232 = o_24[32];
    v_233 = o_24[33];
    v_234 = o_24[34];
    v_235 = o_24[35];
    v_236 = o_24[36];
    v_237 = o_24[37];
    v_238 = o_24[38];
    v_239 = o_24[39];
    v_240 = o_24[40];
    v_241 = o_24[41];
    v_242 = o_24[42];
    v_243 = o_24[43];
    v_244 = o_24[44];
    v_245 = o_24[45];
    v_246 = o_24[46];
    v_247 = o_24[47];
    v_248 = o_24[48];
    v_249 = o_24[49];
    v_250 = o_24[50];
    v_251 = o_24[51];
    v_252 = o_24[52];
    v_253 = o_24[53];
    v_254 = o_24[54];
    v_255 = o_24[55];
    v_256 = o_24[56];
    v_109 = v_200;
    v_110 = v_201;
    v_111 = v_202;
    v_112 = v_203;
    v_113 = v_204;
    v_114 = v_205;
    v_115 = v_206;
    v_116 = v_207;
    v_117 = v_208;
    v_118 = v_209;
    v_119 = v_210;
    v_120 = v_211;
    v_121 = v_212;
    v_122 = v_213;
    v_123 = v_214;
    v_124 = v_215;
    v_125 = v_216;
    v_126 = v_217;
    v_127 = v_218;
    v_128 = v_219;
    v_129 = v_220;
    v_130 = v_221;
    v_131 = v_222;
    v_132 = v_223;
    v_133 = v_224;
    v_134 = v_225;
    v_135 = v_226;
    v_136 = v_227;
    v_137 = v_228;
    v_138 = v_229;
    v_139 = v_230;
    v_140 = v_231;
    v_141 = v_232;
    v_142 = v_233;
    v_143 = v_234;
    v_144 = v_235;
    v_145 = v_236;
    v_146 = v_237;
    v_147 = v_238;
    v_148 = v_239;
    v_149 = v_240;
    v_150 = v_241;
    v_151 = v_242;
    v_152 = v_243;
    v_153 = v_244;
    v_154 = v_245;
    v_155 = v_246;
    v_156 = v_247;
    v_157 = v_248;
    v_158 = v_249;
    v_159 = v_250;
    v_160 = v_251;
    v_161 = v_252;
    v_162 = v_253;
    v_163 = v_254;
    v_164 = v_255;
    v_165 = v_256;
  break;
  }
  o[0] = v_109;
  o[1] = v_110;
  o[2] = v_111;
  o[3] = v_112;
  o[4] = v_113;
  o[5] = v_114;
  o[6] = v_115;
  o[7] = v_116;
  o[8] = v_117;
  o[9] = v_118;
  o[10] = v_119;
  o[11] = v_120;
  o[12] = v_121;
  o[13] = v_122;
  o[14] = v_123;
  o[15] = v_124;
  o[16] = v_125;
  o[17] = v_126;
  o[18] = v_127;
  o[19] = v_128;
  o[20] = v_129;
  o[21] = v_130;
  o[22] = v_131;
  o[23] = v_132;
  o[24] = v_133;
  o[25] = v_134;
  o[26] = v_135;
  o[27] = v_136;
  o[28] = v_137;
  o[29] = v_138;
  o[30] = v_139;
  o[31] = v_140;
  o[32] = v_141;
  o[33] = v_142;
  o[34] = v_143;
  o[35] = v_144;
  o[36] = v_145;
  o[37] = v_146;
  o[38] = v_147;
  o[39] = v_148;
  o[40] = v_149;
  o[41] = v_150;
  o[42] = v_151;
  o[43] = v_152;
  o[44] = v_153;
  o[45] = v_154;
  o[46] = v_155;
  o[47] = v_156;
  o[48] = v_157;
  o[49] = v_158;
  o[50] = v_159;
  o[51] = v_160;
  o[52] = v_161;
  o[53] = v_162;
  o[54] = v_163;
  o[55] = v_164;
  o[56] = v_165;
  return 1;
}

INLINE Term spin_78(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  u32 v_2 = 0;
  u32 f_0 = r0;
  WL_SPIN
    v_2 = U32_BIN(U32_BIN(f_0, &, 125ull), ==, 61ull);
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_79(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_5 = 0;
  u32 d_0 = r0;
  u32 n_1 = r1;
  WL_SPIN
    u32 h_0 = U32_BIN(U32_BIN(d_0, *, 2654435761ull), +, U32_BIN(n_1, *, 40503ull));
    Term a_0 = 13ull;
    u32 h3_0 = U32_BIN(U32_BIN(h_0, ^, (a_0 >= 32 ? 0 : U32_BIN(h_0, >>, a_0))), *, 1274126177ull);
    Term a_1 = 8ull;
    v_5 = U32_BIN(U32_BIN((a_1 >= 32 ? 0 : U32_BIN(h3_0, >>, a_1)), &, 255ull), <, 31ull);
  break;
  }
  o[0] = v_5;
  return 1;
}

INLINE Term spin_80(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3, Term r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13) {
  u32 wpoll = 0;
  Term v_25 = 0;
  Term v_26 = 0;
  Term v_27 = 0;
  Term v_28 = 0;
  u32 v_29 = 0;
  u32 v_30 = 0;
  u32 v_31 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  Term bars_1 = r2;
  Term nxt_1 = r3;
  Term acc_1 = r4;
  u32 f_1 = r5;
  u32 go_c_0 = r6;
  u32 go_r_0 = r7;
  u32 c_on_0 = r8;
  u32 r_on_0 = r9;
  u32 cnt_1 = r10;
  u32 date_1 = r11;
  u32 i_1 = r12;
  u32 n_2 = r13;
  WL_SPIN
    Term v_32 = 0;
    Term v_33 = 0;
    Term v_34 = 0;
    Term v_35 = 0;
    Term v_36 = 0;
    Term v_37 = 0;
    Term v_38 = 0;
    Term v_39 = 0;
    Term v_40 = 0;
    Term v_41 = 0;
    Term v_42 = 0;
    Term v_43 = 0;
    Term o_8[4];
    if (spin_71(e, o_8, go_c_0, bars_1, r_2, nxt_1, acc_1, f_1, r_3, date_1, i_1, n_2) == 0) {
      return 0;
    }
    v_40 = o_8[0];
    v_41 = o_8[1];
    v_42 = o_8[2];
    v_43 = o_8[3];
    v_36 = v_40;
    v_37 = v_41;
    v_38 = v_42;
    v_39 = v_43;
    Term v_44 = 0;
    Term v_45 = 0;
    Term v_46 = 0;
    Term v_47 = 0;
    Term o_9[4];
    if (spin_71(e, o_9, go_r_0, v_36, v_37, v_38, v_39, U32_BIN(f_1, |, 1073741824ull), r_3, date_1, i_1, n_2) == 0) {
      return 0;
    }
    v_44 = o_9[0];
    v_45 = o_9[1];
    v_46 = o_9[2];
    v_47 = o_9[3];
    v_32 = v_44;
    v_33 = v_45;
    v_34 = v_46;
    v_35 = v_47;
    u32 v_48 = 0;
    u32 v_49 = 0;
    Term o_10[1];
    if (spin_5(e, o_10, go_c_0) == 0) {
      return 0;
    }
    v_49 = o_10[0];
    v_48 = v_49;
    u32 v_50 = 0;
    u32 v_51 = 0;
    Term o_11[1];
    if (spin_5(e, o_11, go_r_0) == 0) {
      return 0;
    }
    v_51 = o_11[0];
    v_50 = v_51;
    Term v_52 = 0;
    Term v_53 = 0;
    Term v_54 = 0;
    Term v_55 = 0;
    u32 v_56 = 0;
    u32 v_57 = 0;
    u32 v_58 = 0;
    Term o_12[7];
    if (spin_72(e, o_12, v_32, v_33, v_34, v_35, c_on_0, r_on_0, U32_BIN(U32_BIN(cnt_1, +, v_48), +, v_50)) == 0) {
      return 0;
    }
    v_52 = o_12[0];
    v_53 = o_12[1];
    v_54 = o_12[2];
    v_55 = o_12[3];
    v_56 = o_12[4];
    v_57 = o_12[5];
    v_58 = o_12[6];
    v_25 = v_52;
    v_26 = v_53;
    v_27 = v_54;
    v_28 = v_55;
    v_29 = v_56;
    v_30 = v_57;
    v_31 = v_58;
  break;
  }
  o[0] = v_25;
  o[1] = v_26;
  o[2] = v_27;
  o[3] = v_28;
  o[4] = v_29;
  o[5] = v_30;
  o[6] = v_31;
  return 1;
}

INLINE Term spin_81(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  Term v_6 = 0;
  u32 i_1 = r0;
  u32 n_0 = r1;
  WL_SPIN
    u32 v_7 = 0;
    u32 v_8 = 0;
    Term o_1[1];
    if (spin_3(e, o_1, U32_BIN(i_1, <, n_0), U32_BIN(i_1, +, 1ull), n_0) == 0) {
      return 0;
    }
    v_8 = o_1[0];
    v_7 = v_8;
    v_6 = v_7;
  break;
  }
  o[0] = v_6;
  return 1;
}

INLINE Term spin_82(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_11 = 0;
  u32 i_2 = r0;
  u32 n_1 = r1;
  WL_SPIN
    u32 v_12 = 0;
    Term o_3[1];
    if (spin_3(e, o_3, U32_BIN(i_2, <, n_1), 0ull, U32_BIN(U32_BIN(i_2, +, 1ull), -, n_1)) == 0) {
      return 0;
    }
    v_12 = o_3[0];
    v_11 = v_12;
  break;
  }
  o[0] = v_11;
  return 1;
}

INLINE Term spin_83(Env e, THR Term* o, Term r0, u32 r1, u32 r2, Term r3, u32 r4) {
  u32 wpoll = 0;
  Term v_15 = 0;
  u32 v_16 = 0;
  Term n_2 = r0;
  u32 idx_0 = r1;
  u32 k_0 = r2;
  Term r_2 = r3;
  u32 r_3 = r4;
  WL_SPIN
    if (n_2 == 0) {
      v_15 = r_2;
      v_16 = r_3;
    } else {
      Term p_0 = (n_2 - 1);
      Term v_17 = 0;
      u32 v_18 = 0;
      Term v_19 = 0;
      u32 v_20 = 0;
      Term o_5[2];
      if (spin_74(e, o_5, r_2, r_3, idx_0, k_0) == 0) {
        return 0;
      }
      v_19 = o_5[0];
      v_20 = o_5[1];
      v_17 = v_19;
      v_18 = v_20;
      r0 = p_0;
      r1 = U32_BIN(idx_0, +, 1ull);
      r2 = k_0;
      r3 = v_17;
      r4 = v_18;
      n_2 = r0;
      idx_0 = r1;
      k_0 = r2;
      r_2 = r3;
      r_3 = r4;
      WL_AGAIN(spin_83);
    }
  break;
  }
  o[0] = v_15;
  o[1] = v_16;
  return 1;
}

FAR Term spin_84(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, Term r9, Term r10, Term r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64, u32 r65) {
  u32 wpoll = 0;
  Term v_78 = 0;
  Term v_79 = 0;
  Term v_80 = 0;
  Term v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  u32 v_128 = 0;
  u32 v_129 = 0;
  u32 v_130 = 0;
  u32 v_131 = 0;
  u32 v_132 = 0;
  u32 v_133 = 0;
  u32 v_134 = 0;
  Term r_4 = r0;
  u32 r_5 = r1;
  u32 hh9_1 = r2;
  u32 ll9_0 = r3;
  u32 b_5 = r4;
  u32 b_6 = r5;
  u32 b_7 = r6;
  u32 b_8 = r7;
  u32 b_9 = r8;
  Term chips_1 = r9;
  Term aux_1 = r10;
  Term out_1 = r11;
  u32 st_53 = r12;
  u32 st_54 = r13;
  u32 st_55 = r14;
  u32 st_56 = r15;
  u32 st_57 = r16;
  u32 st_58 = r17;
  u32 st_59 = r18;
  u32 st_60 = r19;
  u32 st_61 = r20;
  u32 st_62 = r21;
  u32 st_63 = r22;
  u32 st_64 = r23;
  u32 st_65 = r24;
  u32 st_66 = r25;
  u32 st_67 = r26;
  u32 st_68 = r27;
  u32 st_69 = r28;
  u32 st_70 = r29;
  u32 st_71 = r30;
  u32 st_72 = r31;
  u32 st_73 = r32;
  u32 st_74 = r33;
  u32 st_75 = r34;
  u32 st_76 = r35;
  u32 st_77 = r36;
  u32 st_78 = r37;
  u32 st_79 = r38;
  u32 st_80 = r39;
  u32 st_81 = r40;
  u32 st_82 = r41;
  u32 st_83 = r42;
  u32 st_84 = r43;
  u32 st_85 = r44;
  u32 st_86 = r45;
  u32 st_87 = r46;
  u32 st_88 = r47;
  u32 st_89 = r48;
  u32 st_90 = r49;
  u32 st_91 = r50;
  u32 st_92 = r51;
  u32 st_93 = r52;
  u32 st_94 = r53;
  u32 st_95 = r54;
  u32 st_96 = r55;
  u32 st_97 = r56;
  u32 st_98 = r57;
  u32 st_99 = r58;
  u32 st_100 = r59;
  u32 st_101 = r60;
  u32 st_102 = r61;
  u32 st_103 = r62;
  u32 st_104 = r63;
  u32 st_105 = r64;
  u32 i_3 = r65;
  WL_SPIN
    u32 v_135 = 0;
    u32 v_136 = 0;
    Term o_7[1];
    if (spin_3(e, o_7, U32_BIN(i_3, <, 4ull), 0ull, U32_BIN(i_3, -, 4ull)) == 0) {
      return 0;
    }
    v_136 = o_7[0];
    v_135 = v_136;
    u32 v_137 = 0;
    u32 v_138 = 0;
    Term o_8[1];
    if (spin_3(e, o_8, U32_BIN(v_135, <, 10ull), U32_BIN(v_135, +, 1ull), 10ull) == 0) {
      return 0;
    }
    v_138 = o_8[0];
    v_137 = v_138;
    u32 sma10_0 = f32_rewrap(f32_unbox(r_5) / f32_unbox(f32_rewrap((f32)(u32)(v_137))));
    u32 v_139 = 0;
    u32 v_140 = 0;
    Term o_9[1];
    if (spin_3(e, o_9, U32_BIN(i_3, <, 8ull), 0ull, U32_BIN(i_3, -, 8ull)) == 0) {
      return 0;
    }
    v_140 = o_9[0];
    v_139 = v_140;
    u32 a_0 = U32_BIN(v_139, +, 5ull);
    u32 a_1 = 0ull;
    u32 a_2 = U32_BIN(v_139, +, 1ull);
    Term v_141 = 0;
    u32 v_142 = 0;
    Term v_143 = 0;
    u32 v_144 = 0;
    Term o_10[2];
    if (spin_75(e, o_10, r_4, a_1, v_139, 2ull) == 0) {
      return 0;
    }
    v_143 = o_10[0];
    v_144 = o_10[1];
    v_141 = v_143;
    v_142 = v_144;
    u32 a_3 = U32_BIN(a_2, +, 1ull);
    Term v_145 = 0;
    u32 v_146 = 0;
    Term v_147 = 0;
    u32 v_148 = 0;
    Term o_11[2];
    if (spin_75(e, o_11, v_141, v_142, a_2, 2ull) == 0) {
      return 0;
    }
    v_147 = o_11[0];
    v_148 = o_11[1];
    v_145 = v_147;
    v_146 = v_148;
    u32 a_4 = U32_BIN(a_3, +, 1ull);
    Term v_149 = 0;
    u32 v_150 = 0;
    Term v_151 = 0;
    u32 v_152 = 0;
    Term o_12[2];
    if (spin_75(e, o_12, v_145, v_146, a_3, 2ull) == 0) {
      return 0;
    }
    v_151 = o_12[0];
    v_152 = o_12[1];
    v_149 = v_151;
    v_150 = v_152;
    Term v_153 = 0;
    u32 v_154 = 0;
    Term v_155 = 0;
    u32 v_156 = 0;
    Term o_13[2];
    if (spin_75(e, o_13, v_149, v_150, a_4, 2ull) == 0) {
      return 0;
    }
    v_155 = o_13[0];
    v_156 = o_13[1];
    v_153 = v_155;
    v_154 = v_156;
    u32 a_5 = U32_BIN(a_0, +, 1ull);
    Term v_157 = 0;
    u32 v_158 = 0;
    Term v_159 = 0;
    u32 v_160 = 0;
    Term o_14[2];
    if (spin_75(e, o_14, v_153, v_154, a_0, 2ull) == 0) {
      return 0;
    }
    v_159 = o_14[0];
    v_160 = o_14[1];
    v_157 = v_159;
    v_158 = v_160;
    u32 a_6 = U32_BIN(a_5, +, 1ull);
    Term v_161 = 0;
    u32 v_162 = 0;
    Term v_163 = 0;
    u32 v_164 = 0;
    Term o_15[2];
    if (spin_75(e, o_15, v_157, v_158, a_5, 2ull) == 0) {
      return 0;
    }
    v_163 = o_15[0];
    v_164 = o_15[1];
    v_161 = v_163;
    v_162 = v_164;
    u32 a_7 = U32_BIN(a_6, +, 1ull);
    Term v_165 = 0;
    u32 v_166 = 0;
    Term v_167 = 0;
    u32 v_168 = 0;
    Term o_16[2];
    if (spin_75(e, o_16, v_161, v_162, a_6, 2ull) == 0) {
      return 0;
    }
    v_167 = o_16[0];
    v_168 = o_16[1];
    v_165 = v_167;
    v_166 = v_168;
    Term v_169 = 0;
    u32 v_170 = 0;
    Term v_171 = 0;
    u32 v_172 = 0;
    Term o_17[2];
    if (spin_75(e, o_17, v_165, v_166, a_7, 2ull) == 0) {
      return 0;
    }
    v_171 = o_17[0];
    v_172 = o_17[1];
    v_169 = v_171;
    v_170 = v_172;
    Term v_173 = 0;
    Term v_174 = 0;
    Term v_175 = 0;
    Term v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    u32 v_185 = 0;
    u32 v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    u32 v_191 = 0;
    u32 v_192 = 0;
    u32 v_193 = 0;
    u32 v_194 = 0;
    u32 v_195 = 0;
    u32 v_196 = 0;
    u32 v_197 = 0;
    u32 v_198 = 0;
    u32 v_199 = 0;
    u32 v_200 = 0;
    u32 v_201 = 0;
    u32 v_202 = 0;
    u32 v_203 = 0;
    u32 v_204 = 0;
    u32 v_205 = 0;
    u32 v_206 = 0;
    u32 v_207 = 0;
    u32 v_208 = 0;
    u32 v_209 = 0;
    u32 v_210 = 0;
    u32 v_211 = 0;
    u32 v_212 = 0;
    u32 v_213 = 0;
    u32 v_214 = 0;
    u32 v_215 = 0;
    u32 v_216 = 0;
    u32 v_217 = 0;
    u32 v_218 = 0;
    u32 v_219 = 0;
    u32 v_220 = 0;
    u32 v_221 = 0;
    u32 v_222 = 0;
    u32 v_223 = 0;
    u32 v_224 = 0;
    u32 v_225 = 0;
    u32 v_226 = 0;
    u32 v_227 = 0;
    u32 v_228 = 0;
    u32 v_229 = 0;
    Term o_18[57];
    if (spin_77(e, o_18, v_169, v_170, hh9_1, ll9_0, sma10_0, b_5, b_6, b_7, b_8, b_9, chips_1, aux_1, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_3) == 0) {
      return 0;
    }
    v_173 = o_18[0];
    v_174 = o_18[1];
    v_175 = o_18[2];
    v_176 = o_18[3];
    v_177 = o_18[4];
    v_178 = o_18[5];
    v_179 = o_18[6];
    v_180 = o_18[7];
    v_181 = o_18[8];
    v_182 = o_18[9];
    v_183 = o_18[10];
    v_184 = o_18[11];
    v_185 = o_18[12];
    v_186 = o_18[13];
    v_187 = o_18[14];
    v_188 = o_18[15];
    v_189 = o_18[16];
    v_190 = o_18[17];
    v_191 = o_18[18];
    v_192 = o_18[19];
    v_193 = o_18[20];
    v_194 = o_18[21];
    v_195 = o_18[22];
    v_196 = o_18[23];
    v_197 = o_18[24];
    v_198 = o_18[25];
    v_199 = o_18[26];
    v_200 = o_18[27];
    v_201 = o_18[28];
    v_202 = o_18[29];
    v_203 = o_18[30];
    v_204 = o_18[31];
    v_205 = o_18[32];
    v_206 = o_18[33];
    v_207 = o_18[34];
    v_208 = o_18[35];
    v_209 = o_18[36];
    v_210 = o_18[37];
    v_211 = o_18[38];
    v_212 = o_18[39];
    v_213 = o_18[40];
    v_214 = o_18[41];
    v_215 = o_18[42];
    v_216 = o_18[43];
    v_217 = o_18[44];
    v_218 = o_18[45];
    v_219 = o_18[46];
    v_220 = o_18[47];
    v_221 = o_18[48];
    v_222 = o_18[49];
    v_223 = o_18[50];
    v_224 = o_18[51];
    v_225 = o_18[52];
    v_226 = o_18[53];
    v_227 = o_18[54];
    v_228 = o_18[55];
    v_229 = o_18[56];
    v_78 = v_173;
    v_79 = v_174;
    v_80 = v_175;
    v_81 = v_176;
    v_82 = v_177;
    v_83 = v_178;
    v_84 = v_179;
    v_85 = v_180;
    v_86 = v_181;
    v_87 = v_182;
    v_88 = v_183;
    v_89 = v_184;
    v_90 = v_185;
    v_91 = v_186;
    v_92 = v_187;
    v_93 = v_188;
    v_94 = v_189;
    v_95 = v_190;
    v_96 = v_191;
    v_97 = v_192;
    v_98 = v_193;
    v_99 = v_194;
    v_100 = v_195;
    v_101 = v_196;
    v_102 = v_197;
    v_103 = v_198;
    v_104 = v_199;
    v_105 = v_200;
    v_106 = v_201;
    v_107 = v_202;
    v_108 = v_203;
    v_109 = v_204;
    v_110 = v_205;
    v_111 = v_206;
    v_112 = v_207;
    v_113 = v_208;
    v_114 = v_209;
    v_115 = v_210;
    v_116 = v_211;
    v_117 = v_212;
    v_118 = v_213;
    v_119 = v_214;
    v_120 = v_215;
    v_121 = v_216;
    v_122 = v_217;
    v_123 = v_218;
    v_124 = v_219;
    v_125 = v_220;
    v_126 = v_221;
    v_127 = v_222;
    v_128 = v_223;
    v_129 = v_224;
    v_130 = v_225;
    v_131 = v_226;
    v_132 = v_227;
    v_133 = v_228;
    v_134 = v_229;
  break;
  }
  o[0] = v_78;
  o[1] = v_79;
  o[2] = v_80;
  o[3] = v_81;
  o[4] = v_82;
  o[5] = v_83;
  o[6] = v_84;
  o[7] = v_85;
  o[8] = v_86;
  o[9] = v_87;
  o[10] = v_88;
  o[11] = v_89;
  o[12] = v_90;
  o[13] = v_91;
  o[14] = v_92;
  o[15] = v_93;
  o[16] = v_94;
  o[17] = v_95;
  o[18] = v_96;
  o[19] = v_97;
  o[20] = v_98;
  o[21] = v_99;
  o[22] = v_100;
  o[23] = v_101;
  o[24] = v_102;
  o[25] = v_103;
  o[26] = v_104;
  o[27] = v_105;
  o[28] = v_106;
  o[29] = v_107;
  o[30] = v_108;
  o[31] = v_109;
  o[32] = v_110;
  o[33] = v_111;
  o[34] = v_112;
  o[35] = v_113;
  o[36] = v_114;
  o[37] = v_115;
  o[38] = v_116;
  o[39] = v_117;
  o[40] = v_118;
  o[41] = v_119;
  o[42] = v_120;
  o[43] = v_121;
  o[44] = v_122;
  o[45] = v_123;
  o[46] = v_124;
  o[47] = v_125;
  o[48] = v_126;
  o[49] = v_127;
  o[50] = v_128;
  o[51] = v_129;
  o[52] = v_130;
  o[53] = v_131;
  o[54] = v_132;
  o[55] = v_133;
  o[56] = v_134;
  return 1;
}

INLINE Term spin_85(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3, Term r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10) {
  u32 wpoll = 0;
  Term v_7 = 0;
  Term v_8 = 0;
  Term v_9 = 0;
  Term v_10 = 0;
  u32 v_11 = 0;
  u32 v_12 = 0;
  u32 v_13 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  Term bars_0 = r2;
  Term nxt_1 = r3;
  Term acc_1 = r4;
  u32 prev_c_1 = r5;
  u32 prev_r_1 = r6;
  u32 cnt_1 = r7;
  u32 date_0 = r8;
  u32 i_1 = r9;
  u32 n_1 = r10;
  WL_SPIN
    u32 v_14 = 0;
    u32 v_15 = 0;
    Term o_0[1];
    if (spin_78(e, o_0, r_3) == 0) {
      return 0;
    }
    v_15 = o_0[0];
    v_14 = v_15;
    u32 v_16 = 0;
    u32 v_17 = 0;
    Term o_1[1];
    if (spin_79(e, o_1, date_0, n_1) == 0) {
      return 0;
    }
    v_17 = o_1[0];
    v_16 = v_17;
    u32 room_0 = U32_BIN(U32_BIN(i_1, +, 2ull), <, n_1);
    Term at_1 = blk_at(r_2, U32_BIN(U32_BIN(i_1, *, 2ull), +, 1ull), 0);
    u32 c_1 = blk_read(e.mem, 0, term_loc(r_2), at_1 + 0);
    u32 v_18 = 0;
    u32 v_19 = 0;
    u32 v_20 = 0;
    u32 v_21 = 0;
    Term o_2[1];
    if (spin_20(e, o_2, prev_c_1) == 0) {
      return 0;
    }
    v_21 = o_2[0];
    v_20 = v_21;
    u32 v_22 = 0;
    Term o_3[1];
    if (spin_7(e, o_3, v_14, v_20) == 0) {
      return 0;
    }
    v_22 = o_3[0];
    v_19 = v_22;
    u32 v_23 = 0;
    Term o_4[1];
    if (spin_7(e, o_4, room_0, v_19) == 0) {
      return 0;
    }
    v_23 = o_4[0];
    v_18 = v_23;
    u32 v_24 = 0;
    u32 v_25 = 0;
    u32 v_26 = 0;
    u32 v_27 = 0;
    Term o_5[1];
    if (spin_20(e, o_5, prev_r_1) == 0) {
      return 0;
    }
    v_27 = o_5[0];
    v_26 = v_27;
    u32 v_28 = 0;
    Term o_6[1];
    if (spin_7(e, o_6, v_16, v_26) == 0) {
      return 0;
    }
    v_28 = o_6[0];
    v_25 = v_28;
    u32 v_29 = 0;
    Term o_7[1];
    if (spin_7(e, o_7, room_0, v_25) == 0) {
      return 0;
    }
    v_29 = o_7[0];
    v_24 = v_29;
    Term v_30 = 0;
    Term v_31 = 0;
    Term v_32 = 0;
    Term v_33 = 0;
    u32 v_34 = 0;
    u32 v_35 = 0;
    u32 v_36 = 0;
    Term o_8[7];
    if (spin_80(e, o_8, r_2, c_1, bars_0, nxt_1, acc_1, r_3, v_18, v_24, v_14, v_16, cnt_1, date_0, i_1, n_1) == 0) {
      return 0;
    }
    v_30 = o_8[0];
    v_31 = o_8[1];
    v_32 = o_8[2];
    v_33 = o_8[3];
    v_34 = o_8[4];
    v_35 = o_8[5];
    v_36 = o_8[6];
    v_7 = v_30;
    v_8 = v_31;
    v_9 = v_32;
    v_10 = v_33;
    v_11 = v_34;
    v_12 = v_35;
    v_13 = v_36;
  break;
  }
  o[0] = v_7;
  o[1] = v_8;
  o[2] = v_9;
  o[3] = v_10;
  o[4] = v_11;
  o[5] = v_12;
  o[6] = v_13;
  return 1;
}

INLINE Term spin_86(Env e, THR Term* o, Term r0, u32 r1, Term r2) {
  u32 wpoll = 0;
  Term v_2 = 0;
  Term v_3 = 0;
  Term r_2 = r0;
  u32 r_3 = r1;
  Term acc_0 = r2;
  WL_SPIN
    u64 nd_0 = heap_alloc(e, cls_fit(2));
    e.mem[nd_0 + 0] = r_3;
    e.mem[nd_0 + 1] = acc_0;
    v_2 = r_2;
    v_3 = term_ctr(CID_CON, nd_0);
  break;
  }
  o[0] = v_2;
  o[1] = v_3;
  return 1;
}

INLINE Term spin_87(Env e, THR Term* o, Term r0, u32 r1, u32 r2, Term r3, u32 r4) {
  u32 wpoll = 0;
  Term v_8 = 0;
  u32 v_9 = 0;
  Term n_0 = r0;
  u32 idx_0 = r1;
  u32 k_0 = r2;
  Term r_2 = r3;
  u32 r_3 = r4;
  WL_SPIN
    if (n_0 == 0) {
      v_8 = r_2;
      v_9 = r_3;
    } else {
      Term p_0 = (n_0 - 1);
      Term v_10 = 0;
      u32 v_11 = 0;
      Term v_12 = 0;
      u32 v_13 = 0;
      Term o_2[2];
      if (spin_68(e, o_2, r_2, r_3, idx_0, k_0) == 0) {
        return 0;
      }
      v_12 = o_2[0];
      v_13 = o_2[1];
      v_10 = v_12;
      v_11 = v_13;
      r0 = p_0;
      r1 = U32_BIN(idx_0, +, 1ull);
      r2 = k_0;
      r3 = v_10;
      r4 = v_11;
      n_0 = r0;
      idx_0 = r1;
      k_0 = r2;
      r_2 = r3;
      r_3 = r4;
      WL_AGAIN(spin_87);
    }
  break;
  }
  o[0] = v_8;
  o[1] = v_9;
  return 1;
}

INLINE Term spin_88(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, Term r8, Term r9, Term r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63, u32 r64) {
  u32 wpoll = 0;
  Term v_71 = 0;
  Term v_72 = 0;
  Term v_73 = 0;
  Term v_74 = 0;
  u32 v_75 = 0;
  u32 v_76 = 0;
  u32 v_77 = 0;
  u32 v_78 = 0;
  u32 v_79 = 0;
  u32 v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  Term r_4 = r0;
  u32 r_5 = r1;
  u32 hh9_0 = r2;
  u32 b_5 = r3;
  u32 b_6 = r4;
  u32 b_7 = r5;
  u32 b_8 = r6;
  u32 b_9 = r7;
  Term chips_1 = r8;
  Term aux_1 = r9;
  Term out_1 = r10;
  u32 st_53 = r11;
  u32 st_54 = r12;
  u32 st_55 = r13;
  u32 st_56 = r14;
  u32 st_57 = r15;
  u32 st_58 = r16;
  u32 st_59 = r17;
  u32 st_60 = r18;
  u32 st_61 = r19;
  u32 st_62 = r20;
  u32 st_63 = r21;
  u32 st_64 = r22;
  u32 st_65 = r23;
  u32 st_66 = r24;
  u32 st_67 = r25;
  u32 st_68 = r26;
  u32 st_69 = r27;
  u32 st_70 = r28;
  u32 st_71 = r29;
  u32 st_72 = r30;
  u32 st_73 = r31;
  u32 st_74 = r32;
  u32 st_75 = r33;
  u32 st_76 = r34;
  u32 st_77 = r35;
  u32 st_78 = r36;
  u32 st_79 = r37;
  u32 st_80 = r38;
  u32 st_81 = r39;
  u32 st_82 = r40;
  u32 st_83 = r41;
  u32 st_84 = r42;
  u32 st_85 = r43;
  u32 st_86 = r44;
  u32 st_87 = r45;
  u32 st_88 = r46;
  u32 st_89 = r47;
  u32 st_90 = r48;
  u32 st_91 = r49;
  u32 st_92 = r50;
  u32 st_93 = r51;
  u32 st_94 = r52;
  u32 st_95 = r53;
  u32 st_96 = r54;
  u32 st_97 = r55;
  u32 st_98 = r56;
  u32 st_99 = r57;
  u32 st_100 = r58;
  u32 st_101 = r59;
  u32 st_102 = r60;
  u32 st_103 = r61;
  u32 st_104 = r62;
  u32 st_105 = r63;
  u32 i_1 = r64;
  WL_SPIN
    u32 v_128 = 0;
    u32 v_129 = 0;
    Term o_4[1];
    if (spin_3(e, o_4, U32_BIN(i_1, <, 4ull), 0ull, U32_BIN(i_1, -, 4ull)) == 0) {
      return 0;
    }
    v_129 = o_4[0];
    v_128 = v_129;
    Term v_130 = 0;
    u32 v_131 = 0;
    Term v_132 = 0;
    Term v_133 = 0;
    Term o_5[1];
    if (spin_81(e, o_5, v_128, 10ull) == 0) {
      return 0;
    }
    v_133 = o_5[0];
    v_132 = v_133;
    u32 v_134 = 0;
    u32 v_135 = 0;
    Term o_6[1];
    if (spin_82(e, o_6, v_128, 10ull) == 0) {
      return 0;
    }
    v_135 = o_6[0];
    v_134 = v_135;
    Term v_136 = 0;
    u32 v_137 = 0;
    Term o_7[2];
    if (spin_83(e, o_7, v_132, v_134, 4ull, r_4, 0ull) == 0) {
      return 0;
    }
    v_136 = o_7[0];
    v_137 = o_7[1];
    v_130 = v_136;
    v_131 = v_137;
    Term v_138 = 0;
    Term v_139 = 0;
    Term v_140 = 0;
    Term v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    u32 v_185 = 0;
    u32 v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    u32 v_191 = 0;
    u32 v_192 = 0;
    u32 v_193 = 0;
    u32 v_194 = 0;
    Term o_8[57];
    if (spin_84(e, o_8, v_130, v_131, hh9_0, r_5, b_5, b_6, b_7, b_8, b_9, chips_1, aux_1, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_1) == 0) {
      return 0;
    }
    v_138 = o_8[0];
    v_139 = o_8[1];
    v_140 = o_8[2];
    v_141 = o_8[3];
    v_142 = o_8[4];
    v_143 = o_8[5];
    v_144 = o_8[6];
    v_145 = o_8[7];
    v_146 = o_8[8];
    v_147 = o_8[9];
    v_148 = o_8[10];
    v_149 = o_8[11];
    v_150 = o_8[12];
    v_151 = o_8[13];
    v_152 = o_8[14];
    v_153 = o_8[15];
    v_154 = o_8[16];
    v_155 = o_8[17];
    v_156 = o_8[18];
    v_157 = o_8[19];
    v_158 = o_8[20];
    v_159 = o_8[21];
    v_160 = o_8[22];
    v_161 = o_8[23];
    v_162 = o_8[24];
    v_163 = o_8[25];
    v_164 = o_8[26];
    v_165 = o_8[27];
    v_166 = o_8[28];
    v_167 = o_8[29];
    v_168 = o_8[30];
    v_169 = o_8[31];
    v_170 = o_8[32];
    v_171 = o_8[33];
    v_172 = o_8[34];
    v_173 = o_8[35];
    v_174 = o_8[36];
    v_175 = o_8[37];
    v_176 = o_8[38];
    v_177 = o_8[39];
    v_178 = o_8[40];
    v_179 = o_8[41];
    v_180 = o_8[42];
    v_181 = o_8[43];
    v_182 = o_8[44];
    v_183 = o_8[45];
    v_184 = o_8[46];
    v_185 = o_8[47];
    v_186 = o_8[48];
    v_187 = o_8[49];
    v_188 = o_8[50];
    v_189 = o_8[51];
    v_190 = o_8[52];
    v_191 = o_8[53];
    v_192 = o_8[54];
    v_193 = o_8[55];
    v_194 = o_8[56];
    v_71 = v_138;
    v_72 = v_139;
    v_73 = v_140;
    v_74 = v_141;
    v_75 = v_142;
    v_76 = v_143;
    v_77 = v_144;
    v_78 = v_145;
    v_79 = v_146;
    v_80 = v_147;
    v_81 = v_148;
    v_82 = v_149;
    v_83 = v_150;
    v_84 = v_151;
    v_85 = v_152;
    v_86 = v_153;
    v_87 = v_154;
    v_88 = v_155;
    v_89 = v_156;
    v_90 = v_157;
    v_91 = v_158;
    v_92 = v_159;
    v_93 = v_160;
    v_94 = v_161;
    v_95 = v_162;
    v_96 = v_163;
    v_97 = v_164;
    v_98 = v_165;
    v_99 = v_166;
    v_100 = v_167;
    v_101 = v_168;
    v_102 = v_169;
    v_103 = v_170;
    v_104 = v_171;
    v_105 = v_172;
    v_106 = v_173;
    v_107 = v_174;
    v_108 = v_175;
    v_109 = v_176;
    v_110 = v_177;
    v_111 = v_178;
    v_112 = v_179;
    v_113 = v_180;
    v_114 = v_181;
    v_115 = v_182;
    v_116 = v_183;
    v_117 = v_184;
    v_118 = v_185;
    v_119 = v_186;
    v_120 = v_187;
    v_121 = v_188;
    v_122 = v_189;
    v_123 = v_190;
    v_124 = v_191;
    v_125 = v_192;
    v_126 = v_193;
    v_127 = v_194;
  break;
  }
  o[0] = v_71;
  o[1] = v_72;
  o[2] = v_73;
  o[3] = v_74;
  o[4] = v_75;
  o[5] = v_76;
  o[6] = v_77;
  o[7] = v_78;
  o[8] = v_79;
  o[9] = v_80;
  o[10] = v_81;
  o[11] = v_82;
  o[12] = v_83;
  o[13] = v_84;
  o[14] = v_85;
  o[15] = v_86;
  o[16] = v_87;
  o[17] = v_88;
  o[18] = v_89;
  o[19] = v_90;
  o[20] = v_91;
  o[21] = v_92;
  o[22] = v_93;
  o[23] = v_94;
  o[24] = v_95;
  o[25] = v_96;
  o[26] = v_97;
  o[27] = v_98;
  o[28] = v_99;
  o[29] = v_100;
  o[30] = v_101;
  o[31] = v_102;
  o[32] = v_103;
  o[33] = v_104;
  o[34] = v_105;
  o[35] = v_106;
  o[36] = v_107;
  o[37] = v_108;
  o[38] = v_109;
  o[39] = v_110;
  o[40] = v_111;
  o[41] = v_112;
  o[42] = v_113;
  o[43] = v_114;
  o[44] = v_115;
  o[45] = v_116;
  o[46] = v_117;
  o[47] = v_118;
  o[48] = v_119;
  o[49] = v_120;
  o[50] = v_121;
  o[51] = v_122;
  o[52] = v_123;
  o[53] = v_124;
  o[54] = v_125;
  o[55] = v_126;
  o[56] = v_127;
  return 1;
}

INLINE Term spin_89(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3, Term r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9) {
  u32 wpoll = 0;
  Term v_11 = 0;
  Term v_12 = 0;
  Term v_13 = 0;
  Term v_14 = 0;
  u32 v_15 = 0;
  u32 v_16 = 0;
  u32 v_17 = 0;
  Term r_0 = r0;
  u32 r_1 = r1;
  Term out_0 = r2;
  Term nxt_0 = r3;
  Term acc_0 = r4;
  u32 prev_c_0 = r5;
  u32 prev_r_0 = r6;
  u32 cnt_0 = r7;
  u32 i_1 = r8;
  u32 n_1 = r9;
  WL_SPIN
    Term at_0 = blk_at(out_0, U32_BIN(i_1, *, 2ull), 0);
    u32 c_0 = blk_read(e.mem, 0, term_loc(out_0), at_0 + 0);
    Term v_18 = 0;
    Term v_19 = 0;
    Term v_20 = 0;
    Term v_21 = 0;
    u32 v_22 = 0;
    u32 v_23 = 0;
    u32 v_24 = 0;
    Term o_1[7];
    if (spin_85(e, o_1, out_0, c_0, r_0, nxt_0, acc_0, prev_c_0, prev_r_0, cnt_0, r_1, i_1, n_1) == 0) {
      return 0;
    }
    v_18 = o_1[0];
    v_19 = o_1[1];
    v_20 = o_1[2];
    v_21 = o_1[3];
    v_22 = o_1[4];
    v_23 = o_1[5];
    v_24 = o_1[6];
    v_11 = v_18;
    v_12 = v_19;
    v_13 = v_20;
    v_14 = v_21;
    v_15 = v_22;
    v_16 = v_23;
    v_17 = v_24;
  break;
  }
  o[0] = v_11;
  o[1] = v_12;
  o[2] = v_13;
  o[3] = v_14;
  o[4] = v_15;
  o[5] = v_16;
  o[6] = v_17;
  return 1;
}

INLINE Term spin_90(Env e, THR Term* o, Term r0, Term r1, u32 r2) {
  u32 wpoll = 0;
  Term v_4 = 0;
  Term v_5 = 0;
  Term r_2 = r0;
  Term r_3 = r1;
  u32 i_1 = r2;
  WL_SPIN
    Term at_0 = blk_at(r_2, i_1, 0);
    u32 c_0 = blk_read(e.mem, 0, term_loc(r_2), at_0 + 0);
    Term v_6 = 0;
    Term v_7 = 0;
    Term o_0[2];
    if (spin_86(e, o_0, r_2, c_0, r_3) == 0) {
      return 0;
    }
    v_6 = o_0[0];
    v_7 = o_0[1];
    v_4 = v_6;
    v_5 = v_7;
  break;
  }
  o[0] = v_4;
  o[1] = v_5;
  return 1;
}

INLINE Term spin_92(Env e, THR Term* o, Term r0, Term r1) {
  u32 wpoll = 0;
  Term v_4 = 0;
  Term xs_1 = r0;
  Term acc_0 = r1;
  WL_SPIN
    if (term_aux(xs_1) == CID_NIL) {
      v_4 = acc_0;
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, xs_1, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      u64 nd_0 = sp_0 >= HEAP_OFF ? sp_0 : heap_alloc(e, cls_fit(2));
      e.mem[nd_0 + 0] = f_0;
      e.mem[nd_0 + 1] = acc_0;
      r0 = f_1;
      r1 = term_ctr(CID_CON, nd_0);
      xs_1 = r0;
      acc_0 = r1;
      WL_AGAIN(spin_92);
    }
  break;
  }
  o[0] = v_4;
  return 1;
}

INLINE Term spin_91(Env e, THR Term* o, Term r0) {
  u32 wpoll = 0;
  Term v_2 = 0;
  Term xs_0 = r0;
  WL_SPIN
    Term v_3 = 0;
    Term o_0[1];
    if (spin_92(e, o_0, xs_0, term_pak(CID_NIL, 0)) == 0) {
      return 0;
    }
    v_3 = o_0[0];
    v_2 = v_3;
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_93(Env e, THR Term* o, Term r0, u32 r1, u32 r2, Term r3, u32 r4) {
  u32 wpoll = 0;
  Term v_8 = 0;
  u32 v_9 = 0;
  Term n_0 = r0;
  u32 idx_0 = r1;
  u32 k_0 = r2;
  Term r_6 = r3;
  u32 r_7 = r4;
  WL_SPIN
    if (n_0 == 0) {
      v_8 = r_6;
      v_9 = r_7;
    } else {
      Term p_0 = (n_0 - 1);
      Term v_10 = 0;
      u32 v_11 = 0;
      Term v_12 = 0;
      u32 v_13 = 0;
      Term o_2[2];
      if (spin_75(e, o_2, r_6, r_7, idx_0, k_0) == 0) {
        return 0;
      }
      v_12 = o_2[0];
      v_13 = o_2[1];
      v_10 = v_12;
      v_11 = v_13;
      r0 = p_0;
      r1 = U32_BIN(idx_0, +, 1ull);
      r2 = k_0;
      r3 = v_10;
      r4 = v_11;
      n_0 = r0;
      idx_0 = r1;
      k_0 = r2;
      r_6 = r3;
      r_7 = r4;
      WL_AGAIN(spin_93);
    }
  break;
  }
  o[0] = v_8;
  o[1] = v_9;
  return 1;
}

INLINE Term spin_94(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, Term r7, Term r8, Term r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62, u32 r63) {
  u32 wpoll = 0;
  Term v_71 = 0;
  Term v_72 = 0;
  Term v_73 = 0;
  Term v_74 = 0;
  u32 v_75 = 0;
  u32 v_76 = 0;
  u32 v_77 = 0;
  u32 v_78 = 0;
  u32 v_79 = 0;
  u32 v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  Term r_8 = r0;
  u32 r_9 = r1;
  u32 b_0 = r2;
  u32 b_1 = r3;
  u32 b_2 = r4;
  u32 b_3 = r5;
  u32 b_4 = r6;
  Term chips_1 = r7;
  Term aux_1 = r8;
  Term out_1 = r9;
  u32 st_53 = r10;
  u32 st_54 = r11;
  u32 st_55 = r12;
  u32 st_56 = r13;
  u32 st_57 = r14;
  u32 st_58 = r15;
  u32 st_59 = r16;
  u32 st_60 = r17;
  u32 st_61 = r18;
  u32 st_62 = r19;
  u32 st_63 = r20;
  u32 st_64 = r21;
  u32 st_65 = r22;
  u32 st_66 = r23;
  u32 st_67 = r24;
  u32 st_68 = r25;
  u32 st_69 = r26;
  u32 st_70 = r27;
  u32 st_71 = r28;
  u32 st_72 = r29;
  u32 st_73 = r30;
  u32 st_74 = r31;
  u32 st_75 = r32;
  u32 st_76 = r33;
  u32 st_77 = r34;
  u32 st_78 = r35;
  u32 st_79 = r36;
  u32 st_80 = r37;
  u32 st_81 = r38;
  u32 st_82 = r39;
  u32 st_83 = r40;
  u32 st_84 = r41;
  u32 st_85 = r42;
  u32 st_86 = r43;
  u32 st_87 = r44;
  u32 st_88 = r45;
  u32 st_89 = r46;
  u32 st_90 = r47;
  u32 st_91 = r48;
  u32 st_92 = r49;
  u32 st_93 = r50;
  u32 st_94 = r51;
  u32 st_95 = r52;
  u32 st_96 = r53;
  u32 st_97 = r54;
  u32 st_98 = r55;
  u32 st_99 = r56;
  u32 st_100 = r57;
  u32 st_101 = r58;
  u32 st_102 = r59;
  u32 st_103 = r60;
  u32 st_104 = r61;
  u32 st_105 = r62;
  u32 i_1 = r63;
  WL_SPIN
    Term v_128 = 0;
    u32 v_129 = 0;
    Term v_130 = 0;
    Term v_131 = 0;
    Term o_4[1];
    if (spin_81(e, o_4, i_1, 9ull) == 0) {
      return 0;
    }
    v_131 = o_4[0];
    v_130 = v_131;
    u32 v_132 = 0;
    u32 v_133 = 0;
    Term o_5[1];
    if (spin_82(e, o_5, i_1, 9ull) == 0) {
      return 0;
    }
    v_133 = o_5[0];
    v_132 = v_133;
    Term v_134 = 0;
    u32 v_135 = 0;
    Term o_6[2];
    if (spin_87(e, o_6, v_130, v_132, 3ull, r_8, 1315859240ull) == 0) {
      return 0;
    }
    v_134 = o_6[0];
    v_135 = o_6[1];
    v_128 = v_134;
    v_129 = v_135;
    Term v_136 = 0;
    Term v_137 = 0;
    Term v_138 = 0;
    Term v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    u32 v_185 = 0;
    u32 v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    u32 v_191 = 0;
    u32 v_192 = 0;
    Term o_7[57];
    if (spin_88(e, o_7, v_128, v_129, r_9, b_0, b_1, b_2, b_3, b_4, chips_1, aux_1, out_1, st_53, st_54, st_55, st_56, st_57, st_58, st_59, st_60, st_61, st_62, st_63, st_64, st_65, st_66, st_67, st_68, st_69, st_70, st_71, st_72, st_73, st_74, st_75, st_76, st_77, st_78, st_79, st_80, st_81, st_82, st_83, st_84, st_85, st_86, st_87, st_88, st_89, st_90, st_91, st_92, st_93, st_94, st_95, st_96, st_97, st_98, st_99, st_100, st_101, st_102, st_103, st_104, st_105, i_1) == 0) {
      return 0;
    }
    v_136 = o_7[0];
    v_137 = o_7[1];
    v_138 = o_7[2];
    v_139 = o_7[3];
    v_140 = o_7[4];
    v_141 = o_7[5];
    v_142 = o_7[6];
    v_143 = o_7[7];
    v_144 = o_7[8];
    v_145 = o_7[9];
    v_146 = o_7[10];
    v_147 = o_7[11];
    v_148 = o_7[12];
    v_149 = o_7[13];
    v_150 = o_7[14];
    v_151 = o_7[15];
    v_152 = o_7[16];
    v_153 = o_7[17];
    v_154 = o_7[18];
    v_155 = o_7[19];
    v_156 = o_7[20];
    v_157 = o_7[21];
    v_158 = o_7[22];
    v_159 = o_7[23];
    v_160 = o_7[24];
    v_161 = o_7[25];
    v_162 = o_7[26];
    v_163 = o_7[27];
    v_164 = o_7[28];
    v_165 = o_7[29];
    v_166 = o_7[30];
    v_167 = o_7[31];
    v_168 = o_7[32];
    v_169 = o_7[33];
    v_170 = o_7[34];
    v_171 = o_7[35];
    v_172 = o_7[36];
    v_173 = o_7[37];
    v_174 = o_7[38];
    v_175 = o_7[39];
    v_176 = o_7[40];
    v_177 = o_7[41];
    v_178 = o_7[42];
    v_179 = o_7[43];
    v_180 = o_7[44];
    v_181 = o_7[45];
    v_182 = o_7[46];
    v_183 = o_7[47];
    v_184 = o_7[48];
    v_185 = o_7[49];
    v_186 = o_7[50];
    v_187 = o_7[51];
    v_188 = o_7[52];
    v_189 = o_7[53];
    v_190 = o_7[54];
    v_191 = o_7[55];
    v_192 = o_7[56];
    v_71 = v_136;
    v_72 = v_137;
    v_73 = v_138;
    v_74 = v_139;
    v_75 = v_140;
    v_76 = v_141;
    v_77 = v_142;
    v_78 = v_143;
    v_79 = v_144;
    v_80 = v_145;
    v_81 = v_146;
    v_82 = v_147;
    v_83 = v_148;
    v_84 = v_149;
    v_85 = v_150;
    v_86 = v_151;
    v_87 = v_152;
    v_88 = v_153;
    v_89 = v_154;
    v_90 = v_155;
    v_91 = v_156;
    v_92 = v_157;
    v_93 = v_158;
    v_94 = v_159;
    v_95 = v_160;
    v_96 = v_161;
    v_97 = v_162;
    v_98 = v_163;
    v_99 = v_164;
    v_100 = v_165;
    v_101 = v_166;
    v_102 = v_167;
    v_103 = v_168;
    v_104 = v_169;
    v_105 = v_170;
    v_106 = v_171;
    v_107 = v_172;
    v_108 = v_173;
    v_109 = v_174;
    v_110 = v_175;
    v_111 = v_176;
    v_112 = v_177;
    v_113 = v_178;
    v_114 = v_179;
    v_115 = v_180;
    v_116 = v_181;
    v_117 = v_182;
    v_118 = v_183;
    v_119 = v_184;
    v_120 = v_185;
    v_121 = v_186;
    v_122 = v_187;
    v_123 = v_188;
    v_124 = v_189;
    v_125 = v_190;
    v_126 = v_191;
    v_127 = v_192;
  break;
  }
  o[0] = v_71;
  o[1] = v_72;
  o[2] = v_73;
  o[3] = v_74;
  o[4] = v_75;
  o[5] = v_76;
  o[6] = v_77;
  o[7] = v_78;
  o[8] = v_79;
  o[9] = v_80;
  o[10] = v_81;
  o[11] = v_82;
  o[12] = v_83;
  o[13] = v_84;
  o[14] = v_85;
  o[15] = v_86;
  o[16] = v_87;
  o[17] = v_88;
  o[18] = v_89;
  o[19] = v_90;
  o[20] = v_91;
  o[21] = v_92;
  o[22] = v_93;
  o[23] = v_94;
  o[24] = v_95;
  o[25] = v_96;
  o[26] = v_97;
  o[27] = v_98;
  o[28] = v_99;
  o[29] = v_100;
  o[30] = v_101;
  o[31] = v_102;
  o[32] = v_103;
  o[33] = v_104;
  o[34] = v_105;
  o[35] = v_106;
  o[36] = v_107;
  o[37] = v_108;
  o[38] = v_109;
  o[39] = v_110;
  o[40] = v_111;
  o[41] = v_112;
  o[42] = v_113;
  o[43] = v_114;
  o[44] = v_115;
  o[45] = v_116;
  o[46] = v_117;
  o[47] = v_118;
  o[48] = v_119;
  o[49] = v_120;
  o[50] = v_121;
  o[51] = v_122;
  o[52] = v_123;
  o[53] = v_124;
  o[54] = v_125;
  o[55] = v_126;
  o[56] = v_127;
  return 1;
}

INLINE Term spin_95(Env e, THR Term* o, Term r0, u32 r1, Term r2, u32 r3, u32 r4) {
  u32 wpoll = 0;
  Term v_3 = 0;
  Term v_4 = 0;
  u32 v_5 = 0;
  Term r_0 = r0;
  u32 r_1 = r1;
  Term nxt_0 = r2;
  u32 prev_0 = r3;
  u32 i_1 = r4;
  WL_SPIN
    u32 v_6 = 0;
    u32 v_7 = 0;
    Term o_0[1];
    if (spin_3(e, o_0, U32_BIN(U32_BIN(r_1, &, 2ull), ==, 2ull), i_1, prev_0) == 0) {
      return 0;
    }
    v_7 = o_0[0];
    v_6 = v_7;
    Term at_1 = blk_at(nxt_0, i_1, 0);
    u32 c_1 = blk_read(e.mem, 0, term_loc(nxt_0), at_1 + 0);
    blk_write(e.mem, 0, term_loc(nxt_0), at_1 + 0, v_6);
    v_3 = r_0;
    v_4 = nxt_0;
    v_5 = v_6;
  break;
  }
  o[0] = v_3;
  o[1] = v_4;
  o[2] = v_5;
  return 1;
}

INLINE Term spin_96(Env e, THR Term* o, Term r0, Term r1, Term r2, Term r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8) {
  u32 wpoll = 0;
  Term v_14 = 0;
  Term v_15 = 0;
  Term v_16 = 0;
  Term v_17 = 0;
  u32 v_18 = 0;
  u32 v_19 = 0;
  u32 v_20 = 0;
  Term t_7 = r0;
  Term t_8 = r1;
  Term t_9 = r2;
  Term t_10 = r3;
  u32 t_11 = r4;
  u32 t_12 = r5;
  u32 t_13 = r6;
  u32 i_1 = r7;
  u32 n_1 = r8;
  WL_SPIN
    Term v_21 = 0;
    u32 v_22 = 0;
    Term v_23 = 0;
    u32 v_24 = 0;
    Term o_0[2];
    if (spin_42(e, o_0, t_7, i_1, 0ull) == 0) {
      return 0;
    }
    v_23 = o_0[0];
    v_24 = o_0[1];
    v_21 = v_23;
    v_22 = v_24;
    Term v_25 = 0;
    Term v_26 = 0;
    Term v_27 = 0;
    Term v_28 = 0;
    u32 v_29 = 0;
    u32 v_30 = 0;
    u32 v_31 = 0;
    Term o_1[7];
    if (spin_89(e, o_1, v_21, v_22, t_8, t_9, t_10, t_11, t_12, t_13, i_1, n_1) == 0) {
      return 0;
    }
    v_25 = o_1[0];
    v_26 = o_1[1];
    v_27 = o_1[2];
    v_28 = o_1[3];
    v_29 = o_1[4];
    v_30 = o_1[5];
    v_31 = o_1[6];
    v_14 = v_25;
    v_15 = v_26;
    v_16 = v_27;
    v_17 = v_28;
    v_18 = v_29;
    v_19 = v_30;
    v_20 = v_31;
  break;
  }
  o[0] = v_14;
  o[1] = v_15;
  o[2] = v_16;
  o[3] = v_17;
  o[4] = v_18;
  o[5] = v_19;
  o[6] = v_20;
  return 1;
}

INLINE Term spin_97(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3) {
  u32 wpoll = 0;
  Term v_4 = 0;
  Term v_5 = 0;
  Term n_1 = r0;
  u32 i_0 = r1;
  Term r_0 = r2;
  Term r_1 = r3;
  WL_SPIN
    if (n_1 == 0) {
      v_4 = r_0;
      v_5 = r_1;
    } else {
      Term p_0 = (n_1 - 1);
      Term v_6 = 0;
      Term v_7 = 0;
      Term v_8 = 0;
      Term v_9 = 0;
      Term o_0[2];
      if (spin_90(e, o_0, r_0, r_1, i_0) == 0) {
        return 0;
      }
      v_8 = o_0[0];
      v_9 = o_0[1];
      v_6 = v_8;
      v_7 = v_9;
      r0 = p_0;
      r1 = U32_BIN(i_0, +, 1ull);
      r2 = v_6;
      r3 = v_7;
      n_1 = r0;
      i_0 = r1;
      r_0 = r2;
      r_1 = r3;
      WL_AGAIN(spin_97);
    }
  break;
  }
  o[0] = v_4;
  o[1] = v_5;
  return 1;
}

INLINE Term spin_98(Env e, THR Term* o, Term r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, Term r6, Term r7, Term r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58, u32 r59, u32 r60, u32 r61, u32 r62) {
  u32 wpoll = 0;
  Term v_69 = 0;
  Term v_70 = 0;
  Term v_71 = 0;
  Term v_72 = 0;
  u32 v_73 = 0;
  u32 v_74 = 0;
  u32 v_75 = 0;
  u32 v_76 = 0;
  u32 v_77 = 0;
  u32 v_78 = 0;
  u32 v_79 = 0;
  u32 v_80 = 0;
  u32 v_81 = 0;
  u32 v_82 = 0;
  u32 v_83 = 0;
  u32 v_84 = 0;
  u32 v_85 = 0;
  u32 v_86 = 0;
  u32 v_87 = 0;
  u32 v_88 = 0;
  u32 v_89 = 0;
  u32 v_90 = 0;
  u32 v_91 = 0;
  u32 v_92 = 0;
  u32 v_93 = 0;
  u32 v_94 = 0;
  u32 v_95 = 0;
  u32 v_96 = 0;
  u32 v_97 = 0;
  u32 v_98 = 0;
  u32 v_99 = 0;
  u32 v_100 = 0;
  u32 v_101 = 0;
  u32 v_102 = 0;
  u32 v_103 = 0;
  u32 v_104 = 0;
  u32 v_105 = 0;
  u32 v_106 = 0;
  u32 v_107 = 0;
  u32 v_108 = 0;
  u32 v_109 = 0;
  u32 v_110 = 0;
  u32 v_111 = 0;
  u32 v_112 = 0;
  u32 v_113 = 0;
  u32 v_114 = 0;
  u32 v_115 = 0;
  u32 v_116 = 0;
  u32 v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  Term r_0 = r0;
  u32 r_1 = r1;
  u32 r_2 = r2;
  u32 r_3 = r3;
  u32 r_4 = r4;
  u32 r_5 = r5;
  Term chips_0 = r6;
  Term aux_0 = r7;
  Term out_0 = r8;
  u32 st_0 = r9;
  u32 st_1 = r10;
  u32 st_2 = r11;
  u32 st_3 = r12;
  u32 st_4 = r13;
  u32 st_5 = r14;
  u32 st_6 = r15;
  u32 st_7 = r16;
  u32 st_8 = r17;
  u32 st_9 = r18;
  u32 st_10 = r19;
  u32 st_11 = r20;
  u32 st_12 = r21;
  u32 st_13 = r22;
  u32 st_14 = r23;
  u32 st_15 = r24;
  u32 st_16 = r25;
  u32 st_17 = r26;
  u32 st_18 = r27;
  u32 st_19 = r28;
  u32 st_20 = r29;
  u32 st_21 = r30;
  u32 st_22 = r31;
  u32 st_23 = r32;
  u32 st_24 = r33;
  u32 st_25 = r34;
  u32 st_26 = r35;
  u32 st_27 = r36;
  u32 st_28 = r37;
  u32 st_29 = r38;
  u32 st_30 = r39;
  u32 st_31 = r40;
  u32 st_32 = r41;
  u32 st_33 = r42;
  u32 st_34 = r43;
  u32 st_35 = r44;
  u32 st_36 = r45;
  u32 st_37 = r46;
  u32 st_38 = r47;
  u32 st_39 = r48;
  u32 st_40 = r49;
  u32 st_41 = r50;
  u32 st_42 = r51;
  u32 st_43 = r52;
  u32 st_44 = r53;
  u32 st_45 = r54;
  u32 st_46 = r55;
  u32 st_47 = r56;
  u32 st_48 = r57;
  u32 st_49 = r58;
  u32 st_50 = r59;
  u32 st_51 = r60;
  u32 st_52 = r61;
  u32 i_1 = r62;
  WL_SPIN
    Term v_126 = 0;
    u32 v_127 = 0;
    Term v_128 = 0;
    Term v_129 = 0;
    Term o_1[1];
    if (spin_81(e, o_1, i_1, 9ull) == 0) {
      return 0;
    }
    v_129 = o_1[0];
    v_128 = v_129;
    u32 v_130 = 0;
    u32 v_131 = 0;
    Term o_2[1];
    if (spin_82(e, o_2, i_1, 9ull) == 0) {
      return 0;
    }
    v_131 = o_2[0];
    v_130 = v_131;
    Term v_132 = 0;
    u32 v_133 = 0;
    Term o_3[2];
    if (spin_93(e, o_3, v_128, v_130, 2ull, r_0, 0ull) == 0) {
      return 0;
    }
    v_132 = o_3[0];
    v_133 = o_3[1];
    v_126 = v_132;
    v_127 = v_133;
    Term v_134 = 0;
    Term v_135 = 0;
    Term v_136 = 0;
    Term v_137 = 0;
    u32 v_138 = 0;
    u32 v_139 = 0;
    u32 v_140 = 0;
    u32 v_141 = 0;
    u32 v_142 = 0;
    u32 v_143 = 0;
    u32 v_144 = 0;
    u32 v_145 = 0;
    u32 v_146 = 0;
    u32 v_147 = 0;
    u32 v_148 = 0;
    u32 v_149 = 0;
    u32 v_150 = 0;
    u32 v_151 = 0;
    u32 v_152 = 0;
    u32 v_153 = 0;
    u32 v_154 = 0;
    u32 v_155 = 0;
    u32 v_156 = 0;
    u32 v_157 = 0;
    u32 v_158 = 0;
    u32 v_159 = 0;
    u32 v_160 = 0;
    u32 v_161 = 0;
    u32 v_162 = 0;
    u32 v_163 = 0;
    u32 v_164 = 0;
    u32 v_165 = 0;
    u32 v_166 = 0;
    u32 v_167 = 0;
    u32 v_168 = 0;
    u32 v_169 = 0;
    u32 v_170 = 0;
    u32 v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    u32 v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    u32 v_183 = 0;
    u32 v_184 = 0;
    u32 v_185 = 0;
    u32 v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    Term o_4[57];
    if (spin_94(e, o_4, v_126, v_127, r_1, r_2, r_3, r_4, r_5, chips_0, aux_0, out_0, st_0, st_1, st_2, st_3, st_4, st_5, st_6, st_7, st_8, st_9, st_10, st_11, st_12, st_13, st_14, st_15, st_16, st_17, st_18, st_19, st_20, st_21, st_22, st_23, st_24, st_25, st_26, st_27, st_28, st_29, st_30, st_31, st_32, st_33, st_34, st_35, st_36, st_37, st_38, st_39, st_40, st_41, st_42, st_43, st_44, st_45, st_46, st_47, st_48, st_49, st_50, st_51, st_52, i_1) == 0) {
      return 0;
    }
    v_134 = o_4[0];
    v_135 = o_4[1];
    v_136 = o_4[2];
    v_137 = o_4[3];
    v_138 = o_4[4];
    v_139 = o_4[5];
    v_140 = o_4[6];
    v_141 = o_4[7];
    v_142 = o_4[8];
    v_143 = o_4[9];
    v_144 = o_4[10];
    v_145 = o_4[11];
    v_146 = o_4[12];
    v_147 = o_4[13];
    v_148 = o_4[14];
    v_149 = o_4[15];
    v_150 = o_4[16];
    v_151 = o_4[17];
    v_152 = o_4[18];
    v_153 = o_4[19];
    v_154 = o_4[20];
    v_155 = o_4[21];
    v_156 = o_4[22];
    v_157 = o_4[23];
    v_158 = o_4[24];
    v_159 = o_4[25];
    v_160 = o_4[26];
    v_161 = o_4[27];
    v_162 = o_4[28];
    v_163 = o_4[29];
    v_164 = o_4[30];
    v_165 = o_4[31];
    v_166 = o_4[32];
    v_167 = o_4[33];
    v_168 = o_4[34];
    v_169 = o_4[35];
    v_170 = o_4[36];
    v_171 = o_4[37];
    v_172 = o_4[38];
    v_173 = o_4[39];
    v_174 = o_4[40];
    v_175 = o_4[41];
    v_176 = o_4[42];
    v_177 = o_4[43];
    v_178 = o_4[44];
    v_179 = o_4[45];
    v_180 = o_4[46];
    v_181 = o_4[47];
    v_182 = o_4[48];
    v_183 = o_4[49];
    v_184 = o_4[50];
    v_185 = o_4[51];
    v_186 = o_4[52];
    v_187 = o_4[53];
    v_188 = o_4[54];
    v_189 = o_4[55];
    v_190 = o_4[56];
    v_69 = v_134;
    v_70 = v_135;
    v_71 = v_136;
    v_72 = v_137;
    v_73 = v_138;
    v_74 = v_139;
    v_75 = v_140;
    v_76 = v_141;
    v_77 = v_142;
    v_78 = v_143;
    v_79 = v_144;
    v_80 = v_145;
    v_81 = v_146;
    v_82 = v_147;
    v_83 = v_148;
    v_84 = v_149;
    v_85 = v_150;
    v_86 = v_151;
    v_87 = v_152;
    v_88 = v_153;
    v_89 = v_154;
    v_90 = v_155;
    v_91 = v_156;
    v_92 = v_157;
    v_93 = v_158;
    v_94 = v_159;
    v_95 = v_160;
    v_96 = v_161;
    v_97 = v_162;
    v_98 = v_163;
    v_99 = v_164;
    v_100 = v_165;
    v_101 = v_166;
    v_102 = v_167;
    v_103 = v_168;
    v_104 = v_169;
    v_105 = v_170;
    v_106 = v_171;
    v_107 = v_172;
    v_108 = v_173;
    v_109 = v_174;
    v_110 = v_175;
    v_111 = v_176;
    v_112 = v_177;
    v_113 = v_178;
    v_114 = v_179;
    v_115 = v_180;
    v_116 = v_181;
    v_117 = v_182;
    v_118 = v_183;
    v_119 = v_184;
    v_120 = v_185;
    v_121 = v_186;
    v_122 = v_187;
    v_123 = v_188;
    v_124 = v_189;
    v_125 = v_190;
  break;
  }
  o[0] = v_69;
  o[1] = v_70;
  o[2] = v_71;
  o[3] = v_72;
  o[4] = v_73;
  o[5] = v_74;
  o[6] = v_75;
  o[7] = v_76;
  o[8] = v_77;
  o[9] = v_78;
  o[10] = v_79;
  o[11] = v_80;
  o[12] = v_81;
  o[13] = v_82;
  o[14] = v_83;
  o[15] = v_84;
  o[16] = v_85;
  o[17] = v_86;
  o[18] = v_87;
  o[19] = v_88;
  o[20] = v_89;
  o[21] = v_90;
  o[22] = v_91;
  o[23] = v_92;
  o[24] = v_93;
  o[25] = v_94;
  o[26] = v_95;
  o[27] = v_96;
  o[28] = v_97;
  o[29] = v_98;
  o[30] = v_99;
  o[31] = v_100;
  o[32] = v_101;
  o[33] = v_102;
  o[34] = v_103;
  o[35] = v_104;
  o[36] = v_105;
  o[37] = v_106;
  o[38] = v_107;
  o[39] = v_108;
  o[40] = v_109;
  o[41] = v_110;
  o[42] = v_111;
  o[43] = v_112;
  o[44] = v_113;
  o[45] = v_114;
  o[46] = v_115;
  o[47] = v_116;
  o[48] = v_117;
  o[49] = v_118;
  o[50] = v_119;
  o[51] = v_120;
  o[52] = v_121;
  o[53] = v_122;
  o[54] = v_123;
  o[55] = v_124;
  o[56] = v_125;
  return 1;
}

INLINE Term spin_99(Env e, THR Term* o, Term r0, Term r1, u32 r2, u32 r3) {
  u32 wpoll = 0;
  Term v_6 = 0;
  Term v_7 = 0;
  u32 v_8 = 0;
  Term x_3 = r0;
  Term x_4 = r1;
  u32 x_5 = r2;
  u32 i_1 = r3;
  WL_SPIN
    Term at_0 = blk_at(x_3, U32_BIN(i_1, *, 2ull), 0);
    u32 c_0 = blk_read(e.mem, 0, term_loc(x_3), at_0 + 0);
    Term v_9 = 0;
    Term v_10 = 0;
    u32 v_11 = 0;
    Term o_0[3];
    if (spin_95(e, o_0, x_3, c_0, x_4, x_5, i_1) == 0) {
      return 0;
    }
    v_9 = o_0[0];
    v_10 = o_0[1];
    v_11 = o_0[2];
    v_6 = v_9;
    v_7 = v_10;
    v_8 = v_11;
  break;
  }
  o[0] = v_6;
  o[1] = v_7;
  o[2] = v_8;
  return 1;
}

INLINE Term spin_100(Env e, THR Term* o, Term r0, u32 r1, u32 r2, Term r3, Term r4, Term r5, Term r6, u32 r7, u32 r8, u32 r9) {
  u32 wpoll = 0;
  Term v_14 = 0;
  Term v_15 = 0;
  Term v_16 = 0;
  Term v_17 = 0;
  u32 v_18 = 0;
  u32 v_19 = 0;
  u32 v_20 = 0;
  Term fuel_0 = r0;
  u32 i_0 = r1;
  u32 n_1 = r2;
  Term t_0 = r3;
  Term t_1 = r4;
  Term t_2 = r5;
  Term t_3 = r6;
  u32 t_4 = r7;
  u32 t_5 = r8;
  u32 t_6 = r9;
  WL_SPIN
    if (fuel_0 == 0) {
      v_14 = t_0;
      v_15 = t_1;
      v_16 = t_2;
      v_17 = t_3;
      v_18 = t_4;
      v_19 = t_5;
      v_20 = t_6;
    } else {
      Term p_0 = (fuel_0 - 1);
      Term v_21 = 0;
      Term v_22 = 0;
      Term v_23 = 0;
      Term v_24 = 0;
      u32 v_25 = 0;
      u32 v_26 = 0;
      u32 v_27 = 0;
      Term v_28 = 0;
      Term v_29 = 0;
      Term v_30 = 0;
      Term v_31 = 0;
      u32 v_32 = 0;
      u32 v_33 = 0;
      u32 v_34 = 0;
      Term o_0[7];
      if (spin_96(e, o_0, t_0, t_1, t_2, t_3, t_4, t_5, t_6, i_0, n_1) == 0) {
        return 0;
      }
      v_28 = o_0[0];
      v_29 = o_0[1];
      v_30 = o_0[2];
      v_31 = o_0[3];
      v_32 = o_0[4];
      v_33 = o_0[5];
      v_34 = o_0[6];
      v_21 = v_28;
      v_22 = v_29;
      v_23 = v_30;
      v_24 = v_31;
      v_25 = v_32;
      v_26 = v_33;
      v_27 = v_34;
      r0 = p_0;
      r1 = U32_BIN(i_0, +, 1ull);
      r2 = n_1;
      r3 = v_21;
      r4 = v_22;
      r5 = v_23;
      r6 = v_24;
      r7 = v_25;
      r8 = v_26;
      r9 = v_27;
      fuel_0 = r0;
      i_0 = r1;
      n_1 = r2;
      t_0 = r3;
      t_1 = r4;
      t_2 = r5;
      t_3 = r6;
      t_4 = r7;
      t_5 = r8;
      t_6 = r9;
      WL_AGAIN(spin_100);
    }
  break;
  }
  o[0] = v_14;
  o[1] = v_15;
  o[2] = v_16;
  o[3] = v_17;
  o[4] = v_18;
  o[5] = v_19;
  o[6] = v_20;
  return 1;
}

INLINE Term spin_101(Env e, THR Term* o, Term r0, u32 r1, u32 r2, Term r3, u32 r4) {
  u32 wpoll = 0;
  Term v_1 = 0;
  Term fuel_0 = r0;
  u32 small_0 = r1;
  u32 n_1 = r2;
  Term d_0 = r3;
  u32 cap_0 = r4;
  WL_SPIN
    if (fuel_0 == 0) {
      v_1 = d_0;
    } else {
      Term f_0 = (fuel_0 - 1);
      if (small_0 == 1) {
        r0 = f_0;
        r1 = U32_BIN(U32_BIN(cap_0, *, 2ull), <, n_1);
        r2 = n_1;
        r3 = nat_chk(e, d_0 + 1);
        r4 = U32_BIN(cap_0, *, 2ull);
        fuel_0 = r0;
        small_0 = r1;
        n_1 = r2;
        d_0 = r3;
        cap_0 = r4;
        WL_AGAIN(spin_101);
      } else {
        v_1 = d_0;
      }
    }
  break;
  }
  o[0] = v_1;
  return 1;
}

INLINE Term spin_102(Env e, THR Term* o, Term r0, Term r1, Term r2, Term r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57) {
  u32 wpoll = 0;
  Term v_114 = 0;
  Term v_115 = 0;
  Term v_116 = 0;
  Term v_117 = 0;
  u32 v_118 = 0;
  u32 v_119 = 0;
  u32 v_120 = 0;
  u32 v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  u32 v_128 = 0;
  u32 v_129 = 0;
  u32 v_130 = 0;
  u32 v_131 = 0;
  u32 v_132 = 0;
  u32 v_133 = 0;
  u32 v_134 = 0;
  u32 v_135 = 0;
  u32 v_136 = 0;
  u32 v_137 = 0;
  u32 v_138 = 0;
  u32 v_139 = 0;
  u32 v_140 = 0;
  u32 v_141 = 0;
  u32 v_142 = 0;
  u32 v_143 = 0;
  u32 v_144 = 0;
  u32 v_145 = 0;
  u32 v_146 = 0;
  u32 v_147 = 0;
  u32 v_148 = 0;
  u32 v_149 = 0;
  u32 v_150 = 0;
  u32 v_151 = 0;
  u32 v_152 = 0;
  u32 v_153 = 0;
  u32 v_154 = 0;
  u32 v_155 = 0;
  u32 v_156 = 0;
  u32 v_157 = 0;
  u32 v_158 = 0;
  u32 v_159 = 0;
  u32 v_160 = 0;
  u32 v_161 = 0;
  u32 v_162 = 0;
  u32 v_163 = 0;
  u32 v_164 = 0;
  u32 v_165 = 0;
  u32 v_166 = 0;
  u32 v_167 = 0;
  u32 v_168 = 0;
  u32 v_169 = 0;
  u32 v_170 = 0;
  Term e_57 = r0;
  Term e_58 = r1;
  Term e_59 = r2;
  Term e_60 = r3;
  u32 e_61 = r4;
  u32 e_62 = r5;
  u32 e_63 = r6;
  u32 e_64 = r7;
  u32 e_65 = r8;
  u32 e_66 = r9;
  u32 e_67 = r10;
  u32 e_68 = r11;
  u32 e_69 = r12;
  u32 e_70 = r13;
  u32 e_71 = r14;
  u32 e_72 = r15;
  u32 e_73 = r16;
  u32 e_74 = r17;
  u32 e_75 = r18;
  u32 e_76 = r19;
  u32 e_77 = r20;
  u32 e_78 = r21;
  u32 e_79 = r22;
  u32 e_80 = r23;
  u32 e_81 = r24;
  u32 e_82 = r25;
  u32 e_83 = r26;
  u32 e_84 = r27;
  u32 e_85 = r28;
  u32 e_86 = r29;
  u32 e_87 = r30;
  u32 e_88 = r31;
  u32 e_89 = r32;
  u32 e_90 = r33;
  u32 e_91 = r34;
  u32 e_92 = r35;
  u32 e_93 = r36;
  u32 e_94 = r37;
  u32 e_95 = r38;
  u32 e_96 = r39;
  u32 e_97 = r40;
  u32 e_98 = r41;
  u32 e_99 = r42;
  u32 e_100 = r43;
  u32 e_101 = r44;
  u32 e_102 = r45;
  u32 e_103 = r46;
  u32 e_104 = r47;
  u32 e_105 = r48;
  u32 e_106 = r49;
  u32 e_107 = r50;
  u32 e_108 = r51;
  u32 e_109 = r52;
  u32 e_110 = r53;
  u32 e_111 = r54;
  u32 e_112 = r55;
  u32 e_113 = r56;
  u32 i_1 = r57;
  WL_SPIN
    Term v_171 = 0;
    u32 v_172 = 0;
    u32 v_173 = 0;
    u32 v_174 = 0;
    u32 v_175 = 0;
    u32 v_176 = 0;
    Term v_177 = 0;
    u32 v_178 = 0;
    u32 v_179 = 0;
    u32 v_180 = 0;
    u32 v_181 = 0;
    u32 v_182 = 0;
    Term o_0[6];
    if (spin_41(e, o_0, e_57, i_1) == 0) {
      return 0;
    }
    v_177 = o_0[0];
    v_178 = o_0[1];
    v_179 = o_0[2];
    v_180 = o_0[3];
    v_181 = o_0[4];
    v_182 = o_0[5];
    v_171 = v_177;
    v_172 = v_178;
    v_173 = v_179;
    v_174 = v_180;
    v_175 = v_181;
    v_176 = v_182;
    Term v_183 = 0;
    Term v_184 = 0;
    Term v_185 = 0;
    Term v_186 = 0;
    u32 v_187 = 0;
    u32 v_188 = 0;
    u32 v_189 = 0;
    u32 v_190 = 0;
    u32 v_191 = 0;
    u32 v_192 = 0;
    u32 v_193 = 0;
    u32 v_194 = 0;
    u32 v_195 = 0;
    u32 v_196 = 0;
    u32 v_197 = 0;
    u32 v_198 = 0;
    u32 v_199 = 0;
    u32 v_200 = 0;
    u32 v_201 = 0;
    u32 v_202 = 0;
    u32 v_203 = 0;
    u32 v_204 = 0;
    u32 v_205 = 0;
    u32 v_206 = 0;
    u32 v_207 = 0;
    u32 v_208 = 0;
    u32 v_209 = 0;
    u32 v_210 = 0;
    u32 v_211 = 0;
    u32 v_212 = 0;
    u32 v_213 = 0;
    u32 v_214 = 0;
    u32 v_215 = 0;
    u32 v_216 = 0;
    u32 v_217 = 0;
    u32 v_218 = 0;
    u32 v_219 = 0;
    u32 v_220 = 0;
    u32 v_221 = 0;
    u32 v_222 = 0;
    u32 v_223 = 0;
    u32 v_224 = 0;
    u32 v_225 = 0;
    u32 v_226 = 0;
    u32 v_227 = 0;
    u32 v_228 = 0;
    u32 v_229 = 0;
    u32 v_230 = 0;
    u32 v_231 = 0;
    u32 v_232 = 0;
    u32 v_233 = 0;
    u32 v_234 = 0;
    u32 v_235 = 0;
    u32 v_236 = 0;
    u32 v_237 = 0;
    u32 v_238 = 0;
    u32 v_239 = 0;
    Term o_1[57];
    if (spin_98(e, o_1, v_171, v_172, v_173, v_174, v_175, v_176, e_58, e_59, e_60, e_61, e_62, e_63, e_64, e_65, e_66, e_67, e_68, e_69, e_70, e_71, e_72, e_73, e_74, e_75, e_76, e_77, e_78, e_79, e_80, e_81, e_82, e_83, e_84, e_85, e_86, e_87, e_88, e_89, e_90, e_91, e_92, e_93, e_94, e_95, e_96, e_97, e_98, e_99, e_100, e_101, e_102, e_103, e_104, e_105, e_106, e_107, e_108, e_109, e_110, e_111, e_112, e_113, i_1) == 0) {
      return 0;
    }
    v_183 = o_1[0];
    v_184 = o_1[1];
    v_185 = o_1[2];
    v_186 = o_1[3];
    v_187 = o_1[4];
    v_188 = o_1[5];
    v_189 = o_1[6];
    v_190 = o_1[7];
    v_191 = o_1[8];
    v_192 = o_1[9];
    v_193 = o_1[10];
    v_194 = o_1[11];
    v_195 = o_1[12];
    v_196 = o_1[13];
    v_197 = o_1[14];
    v_198 = o_1[15];
    v_199 = o_1[16];
    v_200 = o_1[17];
    v_201 = o_1[18];
    v_202 = o_1[19];
    v_203 = o_1[20];
    v_204 = o_1[21];
    v_205 = o_1[22];
    v_206 = o_1[23];
    v_207 = o_1[24];
    v_208 = o_1[25];
    v_209 = o_1[26];
    v_210 = o_1[27];
    v_211 = o_1[28];
    v_212 = o_1[29];
    v_213 = o_1[30];
    v_214 = o_1[31];
    v_215 = o_1[32];
    v_216 = o_1[33];
    v_217 = o_1[34];
    v_218 = o_1[35];
    v_219 = o_1[36];
    v_220 = o_1[37];
    v_221 = o_1[38];
    v_222 = o_1[39];
    v_223 = o_1[40];
    v_224 = o_1[41];
    v_225 = o_1[42];
    v_226 = o_1[43];
    v_227 = o_1[44];
    v_228 = o_1[45];
    v_229 = o_1[46];
    v_230 = o_1[47];
    v_231 = o_1[48];
    v_232 = o_1[49];
    v_233 = o_1[50];
    v_234 = o_1[51];
    v_235 = o_1[52];
    v_236 = o_1[53];
    v_237 = o_1[54];
    v_238 = o_1[55];
    v_239 = o_1[56];
    v_114 = v_183;
    v_115 = v_184;
    v_116 = v_185;
    v_117 = v_186;
    v_118 = v_187;
    v_119 = v_188;
    v_120 = v_189;
    v_121 = v_190;
    v_122 = v_191;
    v_123 = v_192;
    v_124 = v_193;
    v_125 = v_194;
    v_126 = v_195;
    v_127 = v_196;
    v_128 = v_197;
    v_129 = v_198;
    v_130 = v_199;
    v_131 = v_200;
    v_132 = v_201;
    v_133 = v_202;
    v_134 = v_203;
    v_135 = v_204;
    v_136 = v_205;
    v_137 = v_206;
    v_138 = v_207;
    v_139 = v_208;
    v_140 = v_209;
    v_141 = v_210;
    v_142 = v_211;
    v_143 = v_212;
    v_144 = v_213;
    v_145 = v_214;
    v_146 = v_215;
    v_147 = v_216;
    v_148 = v_217;
    v_149 = v_218;
    v_150 = v_219;
    v_151 = v_220;
    v_152 = v_221;
    v_153 = v_222;
    v_154 = v_223;
    v_155 = v_224;
    v_156 = v_225;
    v_157 = v_226;
    v_158 = v_227;
    v_159 = v_228;
    v_160 = v_229;
    v_161 = v_230;
    v_162 = v_231;
    v_163 = v_232;
    v_164 = v_233;
    v_165 = v_234;
    v_166 = v_235;
    v_167 = v_236;
    v_168 = v_237;
    v_169 = v_238;
    v_170 = v_239;
  break;
  }
  o[0] = v_114;
  o[1] = v_115;
  o[2] = v_116;
  o[3] = v_117;
  o[4] = v_118;
  o[5] = v_119;
  o[6] = v_120;
  o[7] = v_121;
  o[8] = v_122;
  o[9] = v_123;
  o[10] = v_124;
  o[11] = v_125;
  o[12] = v_126;
  o[13] = v_127;
  o[14] = v_128;
  o[15] = v_129;
  o[16] = v_130;
  o[17] = v_131;
  o[18] = v_132;
  o[19] = v_133;
  o[20] = v_134;
  o[21] = v_135;
  o[22] = v_136;
  o[23] = v_137;
  o[24] = v_138;
  o[25] = v_139;
  o[26] = v_140;
  o[27] = v_141;
  o[28] = v_142;
  o[29] = v_143;
  o[30] = v_144;
  o[31] = v_145;
  o[32] = v_146;
  o[33] = v_147;
  o[34] = v_148;
  o[35] = v_149;
  o[36] = v_150;
  o[37] = v_151;
  o[38] = v_152;
  o[39] = v_153;
  o[40] = v_154;
  o[41] = v_155;
  o[42] = v_156;
  o[43] = v_157;
  o[44] = v_158;
  o[45] = v_159;
  o[46] = v_160;
  o[47] = v_161;
  o[48] = v_162;
  o[49] = v_163;
  o[50] = v_164;
  o[51] = v_165;
  o[52] = v_166;
  o[53] = v_167;
  o[54] = v_168;
  o[55] = v_169;
  o[56] = v_170;
  return 1;
}

INLINE Term spin_103(Env e, THR Term* o, u32 r0) {
  u32 wpoll = 0;
  Term v_5 = 0;
  u32 n_1 = r0;
  WL_SPIN
    Term v_6 = 0;
    Term o_0[1];
    if (spin_101(e, o_0, 40ull, U32_BIN(1ull, <, n_1), n_1, 0, 1ull) == 0) {
      return 0;
    }
    v_6 = o_0[0];
    v_5 = v_6;
  break;
  }
  o[0] = v_5;
  return 1;
}

INLINE Term spin_104(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3, u32 r4) {
  u32 wpoll = 0;
  Term v_10 = 0;
  Term v_11 = 0;
  u32 v_12 = 0;
  Term fuel_0 = r0;
  u32 i_0 = r1;
  Term x_0 = r2;
  Term x_1 = r3;
  u32 x_2 = r4;
  WL_SPIN
    if (fuel_0 == 0) {
      v_10 = x_0;
      v_11 = x_1;
      v_12 = x_2;
    } else {
      Term p_0 = (fuel_0 - 1);
      Term v_13 = 0;
      Term v_14 = 0;
      u32 v_15 = 0;
      Term v_16 = 0;
      Term v_17 = 0;
      u32 v_18 = 0;
      Term o_2[3];
      if (spin_99(e, o_2, x_0, x_1, x_2, i_0) == 0) {
        return 0;
      }
      v_16 = o_2[0];
      v_17 = o_2[1];
      v_18 = o_2[2];
      v_13 = v_16;
      v_14 = v_17;
      v_15 = v_18;
      r0 = p_0;
      r1 = U32_BIN(i_0, -, 1ull);
      r2 = v_13;
      r3 = v_14;
      r4 = v_15;
      fuel_0 = r0;
      i_0 = r1;
      x_0 = r2;
      x_1 = r3;
      x_2 = r4;
      WL_AGAIN(spin_104);
    }
  break;
  }
  o[0] = v_10;
  o[1] = v_11;
  o[2] = v_12;
  return 1;
}

INLINE Term spin_105(Env e, THR Term* o, Term r0, u32 r1, Term r2) {
  u32 wpoll = 0;
  Term v_3 = 0;
  Term xs_0 = r0;
  u32 i_0 = r1;
  Term a_0 = r2;
  WL_SPIN
    if (term_aux(xs_0) == CID_NIL) {
      v_3 = a_0;
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, xs_0, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      Term at_0 = blk_at(a_0, i_0, 0);
      u32 c_0 = blk_read(e.mem, 0, term_loc(a_0), at_0 + 0);
      blk_write(e.mem, 0, term_loc(a_0), at_0 + 0, f_0);
      spare_free(e, cls_fit(2), sp_0);
      r0 = f_1;
      r1 = U32_BIN(i_0, +, 1ull);
      r2 = a_0;
      xs_0 = r0;
      i_0 = r1;
      a_0 = r2;
      WL_AGAIN(spin_105);
    }
  break;
  }
  o[0] = v_3;
  return 1;
}

FAR Term spin_106(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3, Term r4, Term r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 r13, u32 r14, u32 r15, u32 r16, u32 r17, u32 r18, u32 r19, u32 r20, u32 r21, u32 r22, u32 r23, u32 r24, u32 r25, u32 r26, u32 r27, u32 r28, u32 r29, u32 r30, u32 r31, u32 r32, u32 r33, u32 r34, u32 r35, u32 r36, u32 r37, u32 r38, u32 r39, u32 r40, u32 r41, u32 r42, u32 r43, u32 r44, u32 r45, u32 r46, u32 r47, u32 r48, u32 r49, u32 r50, u32 r51, u32 r52, u32 r53, u32 r54, u32 r55, u32 r56, u32 r57, u32 r58) {
  u32 wpoll = 0;
  Term v_118 = 0;
  Term v_119 = 0;
  Term v_120 = 0;
  Term v_121 = 0;
  u32 v_122 = 0;
  u32 v_123 = 0;
  u32 v_124 = 0;
  u32 v_125 = 0;
  u32 v_126 = 0;
  u32 v_127 = 0;
  u32 v_128 = 0;
  u32 v_129 = 0;
  u32 v_130 = 0;
  u32 v_131 = 0;
  u32 v_132 = 0;
  u32 v_133 = 0;
  u32 v_134 = 0;
  u32 v_135 = 0;
  u32 v_136 = 0;
  u32 v_137 = 0;
  u32 v_138 = 0;
  u32 v_139 = 0;
  u32 v_140 = 0;
  u32 v_141 = 0;
  u32 v_142 = 0;
  u32 v_143 = 0;
  u32 v_144 = 0;
  u32 v_145 = 0;
  u32 v_146 = 0;
  u32 v_147 = 0;
  u32 v_148 = 0;
  u32 v_149 = 0;
  u32 v_150 = 0;
  u32 v_151 = 0;
  u32 v_152 = 0;
  u32 v_153 = 0;
  u32 v_154 = 0;
  u32 v_155 = 0;
  u32 v_156 = 0;
  u32 v_157 = 0;
  u32 v_158 = 0;
  u32 v_159 = 0;
  u32 v_160 = 0;
  u32 v_161 = 0;
  u32 v_162 = 0;
  u32 v_163 = 0;
  u32 v_164 = 0;
  u32 v_165 = 0;
  u32 v_166 = 0;
  u32 v_167 = 0;
  u32 v_168 = 0;
  u32 v_169 = 0;
  u32 v_170 = 0;
  u32 v_171 = 0;
  u32 v_172 = 0;
  u32 v_173 = 0;
  u32 v_174 = 0;
  Term fuel_0 = r0;
  u32 i_0 = r1;
  Term e_0 = r2;
  Term e_1 = r3;
  Term e_2 = r4;
  Term e_3 = r5;
  u32 e_4 = r6;
  u32 e_5 = r7;
  u32 e_6 = r8;
  u32 e_7 = r9;
  u32 e_8 = r10;
  u32 e_9 = r11;
  u32 e_10 = r12;
  u32 e_11 = r13;
  u32 e_12 = r14;
  u32 e_13 = r15;
  u32 e_14 = r16;
  u32 e_15 = r17;
  u32 e_16 = r18;
  u32 e_17 = r19;
  u32 e_18 = r20;
  u32 e_19 = r21;
  u32 e_20 = r22;
  u32 e_21 = r23;
  u32 e_22 = r24;
  u32 e_23 = r25;
  u32 e_24 = r26;
  u32 e_25 = r27;
  u32 e_26 = r28;
  u32 e_27 = r29;
  u32 e_28 = r30;
  u32 e_29 = r31;
  u32 e_30 = r32;
  u32 e_31 = r33;
  u32 e_32 = r34;
  u32 e_33 = r35;
  u32 e_34 = r36;
  u32 e_35 = r37;
  u32 e_36 = r38;
  u32 e_37 = r39;
  u32 e_38 = r40;
  u32 e_39 = r41;
  u32 e_40 = r42;
  u32 e_41 = r43;
  u32 e_42 = r44;
  u32 e_43 = r45;
  u32 e_44 = r46;
  u32 e_45 = r47;
  u32 e_46 = r48;
  u32 e_47 = r49;
  u32 e_48 = r50;
  u32 e_49 = r51;
  u32 e_50 = r52;
  u32 e_51 = r53;
  u32 e_52 = r54;
  u32 e_53 = r55;
  u32 e_54 = r56;
  u32 e_55 = r57;
  u32 e_56 = r58;
  WL_SPIN
    if (fuel_0 == 0) {
      v_118 = e_0;
      v_119 = e_1;
      v_120 = e_2;
      v_121 = e_3;
      v_122 = e_4;
      v_123 = e_5;
      v_124 = e_6;
      v_125 = e_7;
      v_126 = e_8;
      v_127 = e_9;
      v_128 = e_10;
      v_129 = e_11;
      v_130 = e_12;
      v_131 = e_13;
      v_132 = e_14;
      v_133 = e_15;
      v_134 = e_16;
      v_135 = e_17;
      v_136 = e_18;
      v_137 = e_19;
      v_138 = e_20;
      v_139 = e_21;
      v_140 = e_22;
      v_141 = e_23;
      v_142 = e_24;
      v_143 = e_25;
      v_144 = e_26;
      v_145 = e_27;
      v_146 = e_28;
      v_147 = e_29;
      v_148 = e_30;
      v_149 = e_31;
      v_150 = e_32;
      v_151 = e_33;
      v_152 = e_34;
      v_153 = e_35;
      v_154 = e_36;
      v_155 = e_37;
      v_156 = e_38;
      v_157 = e_39;
      v_158 = e_40;
      v_159 = e_41;
      v_160 = e_42;
      v_161 = e_43;
      v_162 = e_44;
      v_163 = e_45;
      v_164 = e_46;
      v_165 = e_47;
      v_166 = e_48;
      v_167 = e_49;
      v_168 = e_50;
      v_169 = e_51;
      v_170 = e_52;
      v_171 = e_53;
      v_172 = e_54;
      v_173 = e_55;
      v_174 = e_56;
    } else {
      Term p_0 = (fuel_0 - 1);
      Term v_175 = 0;
      Term v_176 = 0;
      Term v_177 = 0;
      Term v_178 = 0;
      u32 v_179 = 0;
      u32 v_180 = 0;
      u32 v_181 = 0;
      u32 v_182 = 0;
      u32 v_183 = 0;
      u32 v_184 = 0;
      u32 v_185 = 0;
      u32 v_186 = 0;
      u32 v_187 = 0;
      u32 v_188 = 0;
      u32 v_189 = 0;
      u32 v_190 = 0;
      u32 v_191 = 0;
      u32 v_192 = 0;
      u32 v_193 = 0;
      u32 v_194 = 0;
      u32 v_195 = 0;
      u32 v_196 = 0;
      u32 v_197 = 0;
      u32 v_198 = 0;
      u32 v_199 = 0;
      u32 v_200 = 0;
      u32 v_201 = 0;
      u32 v_202 = 0;
      u32 v_203 = 0;
      u32 v_204 = 0;
      u32 v_205 = 0;
      u32 v_206 = 0;
      u32 v_207 = 0;
      u32 v_208 = 0;
      u32 v_209 = 0;
      u32 v_210 = 0;
      u32 v_211 = 0;
      u32 v_212 = 0;
      u32 v_213 = 0;
      u32 v_214 = 0;
      u32 v_215 = 0;
      u32 v_216 = 0;
      u32 v_217 = 0;
      u32 v_218 = 0;
      u32 v_219 = 0;
      u32 v_220 = 0;
      u32 v_221 = 0;
      u32 v_222 = 0;
      u32 v_223 = 0;
      u32 v_224 = 0;
      u32 v_225 = 0;
      u32 v_226 = 0;
      u32 v_227 = 0;
      u32 v_228 = 0;
      u32 v_229 = 0;
      u32 v_230 = 0;
      u32 v_231 = 0;
      Term v_232 = 0;
      Term v_233 = 0;
      Term v_234 = 0;
      Term v_235 = 0;
      u32 v_236 = 0;
      u32 v_237 = 0;
      u32 v_238 = 0;
      u32 v_239 = 0;
      u32 v_240 = 0;
      u32 v_241 = 0;
      u32 v_242 = 0;
      u32 v_243 = 0;
      u32 v_244 = 0;
      u32 v_245 = 0;
      u32 v_246 = 0;
      u32 v_247 = 0;
      u32 v_248 = 0;
      u32 v_249 = 0;
      u32 v_250 = 0;
      u32 v_251 = 0;
      u32 v_252 = 0;
      u32 v_253 = 0;
      u32 v_254 = 0;
      u32 v_255 = 0;
      u32 v_256 = 0;
      u32 v_257 = 0;
      u32 v_258 = 0;
      u32 v_259 = 0;
      u32 v_260 = 0;
      u32 v_261 = 0;
      u32 v_262 = 0;
      u32 v_263 = 0;
      u32 v_264 = 0;
      u32 v_265 = 0;
      u32 v_266 = 0;
      u32 v_267 = 0;
      u32 v_268 = 0;
      u32 v_269 = 0;
      u32 v_270 = 0;
      u32 v_271 = 0;
      u32 v_272 = 0;
      u32 v_273 = 0;
      u32 v_274 = 0;
      u32 v_275 = 0;
      u32 v_276 = 0;
      u32 v_277 = 0;
      u32 v_278 = 0;
      u32 v_279 = 0;
      u32 v_280 = 0;
      u32 v_281 = 0;
      u32 v_282 = 0;
      u32 v_283 = 0;
      u32 v_284 = 0;
      u32 v_285 = 0;
      u32 v_286 = 0;
      u32 v_287 = 0;
      u32 v_288 = 0;
      Term o_2[57];
      if (spin_102(e, o_2, e_0, e_1, e_2, e_3, e_4, e_5, e_6, e_7, e_8, e_9, e_10, e_11, e_12, e_13, e_14, e_15, e_16, e_17, e_18, e_19, e_20, e_21, e_22, e_23, e_24, e_25, e_26, e_27, e_28, e_29, e_30, e_31, e_32, e_33, e_34, e_35, e_36, e_37, e_38, e_39, e_40, e_41, e_42, e_43, e_44, e_45, e_46, e_47, e_48, e_49, e_50, e_51, e_52, e_53, e_54, e_55, e_56, i_0) == 0) {
        return 0;
      }
      v_232 = o_2[0];
      v_233 = o_2[1];
      v_234 = o_2[2];
      v_235 = o_2[3];
      v_236 = o_2[4];
      v_237 = o_2[5];
      v_238 = o_2[6];
      v_239 = o_2[7];
      v_240 = o_2[8];
      v_241 = o_2[9];
      v_242 = o_2[10];
      v_243 = o_2[11];
      v_244 = o_2[12];
      v_245 = o_2[13];
      v_246 = o_2[14];
      v_247 = o_2[15];
      v_248 = o_2[16];
      v_249 = o_2[17];
      v_250 = o_2[18];
      v_251 = o_2[19];
      v_252 = o_2[20];
      v_253 = o_2[21];
      v_254 = o_2[22];
      v_255 = o_2[23];
      v_256 = o_2[24];
      v_257 = o_2[25];
      v_258 = o_2[26];
      v_259 = o_2[27];
      v_260 = o_2[28];
      v_261 = o_2[29];
      v_262 = o_2[30];
      v_263 = o_2[31];
      v_264 = o_2[32];
      v_265 = o_2[33];
      v_266 = o_2[34];
      v_267 = o_2[35];
      v_268 = o_2[36];
      v_269 = o_2[37];
      v_270 = o_2[38];
      v_271 = o_2[39];
      v_272 = o_2[40];
      v_273 = o_2[41];
      v_274 = o_2[42];
      v_275 = o_2[43];
      v_276 = o_2[44];
      v_277 = o_2[45];
      v_278 = o_2[46];
      v_279 = o_2[47];
      v_280 = o_2[48];
      v_281 = o_2[49];
      v_282 = o_2[50];
      v_283 = o_2[51];
      v_284 = o_2[52];
      v_285 = o_2[53];
      v_286 = o_2[54];
      v_287 = o_2[55];
      v_288 = o_2[56];
      v_175 = v_232;
      v_176 = v_233;
      v_177 = v_234;
      v_178 = v_235;
      v_179 = v_236;
      v_180 = v_237;
      v_181 = v_238;
      v_182 = v_239;
      v_183 = v_240;
      v_184 = v_241;
      v_185 = v_242;
      v_186 = v_243;
      v_187 = v_244;
      v_188 = v_245;
      v_189 = v_246;
      v_190 = v_247;
      v_191 = v_248;
      v_192 = v_249;
      v_193 = v_250;
      v_194 = v_251;
      v_195 = v_252;
      v_196 = v_253;
      v_197 = v_254;
      v_198 = v_255;
      v_199 = v_256;
      v_200 = v_257;
      v_201 = v_258;
      v_202 = v_259;
      v_203 = v_260;
      v_204 = v_261;
      v_205 = v_262;
      v_206 = v_263;
      v_207 = v_264;
      v_208 = v_265;
      v_209 = v_266;
      v_210 = v_267;
      v_211 = v_268;
      v_212 = v_269;
      v_213 = v_270;
      v_214 = v_271;
      v_215 = v_272;
      v_216 = v_273;
      v_217 = v_274;
      v_218 = v_275;
      v_219 = v_276;
      v_220 = v_277;
      v_221 = v_278;
      v_222 = v_279;
      v_223 = v_280;
      v_224 = v_281;
      v_225 = v_282;
      v_226 = v_283;
      v_227 = v_284;
      v_228 = v_285;
      v_229 = v_286;
      v_230 = v_287;
      v_231 = v_288;
      r0 = p_0;
      r1 = U32_BIN(i_0, +, 1ull);
      r2 = v_175;
      r3 = v_176;
      r4 = v_177;
      r5 = v_178;
      r6 = v_179;
      r7 = v_180;
      r8 = v_181;
      r9 = v_182;
      r10 = v_183;
      r11 = v_184;
      r12 = v_185;
      r13 = v_186;
      r14 = v_187;
      r15 = v_188;
      r16 = v_189;
      r17 = v_190;
      r18 = v_191;
      r19 = v_192;
      r20 = v_193;
      r21 = v_194;
      r22 = v_195;
      r23 = v_196;
      r24 = v_197;
      r25 = v_198;
      r26 = v_199;
      r27 = v_200;
      r28 = v_201;
      r29 = v_202;
      r30 = v_203;
      r31 = v_204;
      r32 = v_205;
      r33 = v_206;
      r34 = v_207;
      r35 = v_208;
      r36 = v_209;
      r37 = v_210;
      r38 = v_211;
      r39 = v_212;
      r40 = v_213;
      r41 = v_214;
      r42 = v_215;
      r43 = v_216;
      r44 = v_217;
      r45 = v_218;
      r46 = v_219;
      r47 = v_220;
      r48 = v_221;
      r49 = v_222;
      r50 = v_223;
      r51 = v_224;
      r52 = v_225;
      r53 = v_226;
      r54 = v_227;
      r55 = v_228;
      r56 = v_229;
      r57 = v_230;
      r58 = v_231;
      fuel_0 = r0;
      i_0 = r1;
      e_0 = r2;
      e_1 = r3;
      e_2 = r4;
      e_3 = r5;
      e_4 = r6;
      e_5 = r7;
      e_6 = r8;
      e_7 = r9;
      e_8 = r10;
      e_9 = r11;
      e_10 = r12;
      e_11 = r13;
      e_12 = r14;
      e_13 = r15;
      e_14 = r16;
      e_15 = r17;
      e_16 = r18;
      e_17 = r19;
      e_18 = r20;
      e_19 = r21;
      e_20 = r22;
      e_21 = r23;
      e_22 = r24;
      e_23 = r25;
      e_24 = r26;
      e_25 = r27;
      e_26 = r28;
      e_27 = r29;
      e_28 = r30;
      e_29 = r31;
      e_30 = r32;
      e_31 = r33;
      e_32 = r34;
      e_33 = r35;
      e_34 = r36;
      e_35 = r37;
      e_36 = r38;
      e_37 = r39;
      e_38 = r40;
      e_39 = r41;
      e_40 = r42;
      e_41 = r43;
      e_42 = r44;
      e_43 = r45;
      e_44 = r46;
      e_45 = r47;
      e_46 = r48;
      e_47 = r49;
      e_48 = r50;
      e_49 = r51;
      e_50 = r52;
      e_51 = r53;
      e_52 = r54;
      e_53 = r55;
      e_54 = r56;
      e_55 = r57;
      e_56 = r58;
      WL_AGAIN(spin_106);
    }
  break;
  }
  o[0] = v_118;
  o[1] = v_119;
  o[2] = v_120;
  o[3] = v_121;
  o[4] = v_122;
  o[5] = v_123;
  o[6] = v_124;
  o[7] = v_125;
  o[8] = v_126;
  o[9] = v_127;
  o[10] = v_128;
  o[11] = v_129;
  o[12] = v_130;
  o[13] = v_131;
  o[14] = v_132;
  o[15] = v_133;
  o[16] = v_134;
  o[17] = v_135;
  o[18] = v_136;
  o[19] = v_137;
  o[20] = v_138;
  o[21] = v_139;
  o[22] = v_140;
  o[23] = v_141;
  o[24] = v_142;
  o[25] = v_143;
  o[26] = v_144;
  o[27] = v_145;
  o[28] = v_146;
  o[29] = v_147;
  o[30] = v_148;
  o[31] = v_149;
  o[32] = v_150;
  o[33] = v_151;
  o[34] = v_152;
  o[35] = v_153;
  o[36] = v_154;
  o[37] = v_155;
  o[38] = v_156;
  o[39] = v_157;
  o[40] = v_158;
  o[41] = v_159;
  o[42] = v_160;
  o[43] = v_161;
  o[44] = v_162;
  o[45] = v_163;
  o[46] = v_164;
  o[47] = v_165;
  o[48] = v_166;
  o[49] = v_167;
  o[50] = v_168;
  o[51] = v_169;
  o[52] = v_170;
  o[53] = v_171;
  o[54] = v_172;
  o[55] = v_173;
  o[56] = v_174;
  return 1;
}

INLINE Term spin_107(Env e, THR Term* o, u32 r0, Term r1, Term r2) {
  u32 wpoll = 0;
  Term v_5 = 0;
  u32 r_0 = r0;
  Term r_1 = r1;
  Term r_2 = r2;
  WL_SPIN
    if (r_0 == 1) {
      v_5 = r_1;
    } else {
      term_sink(e, r_2);
      v_5 = term_pak(CID_NIL, 0);
    }
  break;
  }
  o[0] = v_5;
  return 1;
}

INLINE Term spin_108(Env e, THR Term* o, Term r0, Term r1) {
  u32 wpoll = 0;
  Term v_7 = 0;
  Term bs_0 = r0;
  Term acc_0 = r1;
  WL_SPIN
    if (term_aux(bs_0) == CID_CON) {
      Term fb_14[2];
      u64 sp_0 = ctr_take(e, bs_0, 2, fb_14);
      Term f_0 = fb_14[0];
      Term f_1 = fb_14[1];
      if (term_aux(f_1) == CID_CON) {
        Term fb_15[2];
        u64 sp_1 = ctr_take(e, f_1, 2, fb_15);
        Term f_2 = fb_15[0];
        Term f_3 = fb_15[1];
        if (term_aux(f_3) == CID_CON) {
          Term fb_16[2];
          u64 sp_2 = ctr_take(e, f_3, 2, fb_16);
          Term f_4 = fb_16[0];
          Term f_5 = fb_16[1];
          if (term_aux(f_5) == CID_CON) {
            Term fb_17[2];
            u64 sp_3 = ctr_take(e, f_5, 2, fb_17);
            Term f_6 = fb_17[0];
            Term f_7 = fb_17[1];
            Term a_0 = 8ull;
            Term a_1 = 16ull;
            Term a_2 = 24ull;
            u32 w_0 = U32_BIN(U32_BIN(U32_BIN(f_0, |, (a_0 >= 32 ? 0 : U32_BIN(f_2, <<, a_0))), |, (a_1 >= 32 ? 0 : U32_BIN(f_4, <<, a_1))), |, (a_2 >= 32 ? 0 : U32_BIN(f_6, <<, a_2)));
            u64 nd_4 = sp_0 >= HEAP_OFF ? sp_0 : heap_alloc(e, cls_fit(2));
            e.mem[nd_4 + 0] = w_0;
            e.mem[nd_4 + 1] = acc_0;
            spare_free(e, cls_fit(2), sp_3);
            spare_free(e, cls_fit(2), sp_2);
            spare_free(e, cls_fit(2), sp_1);
            r0 = f_7;
            r1 = term_ctr(CID_CON, nd_4);
            bs_0 = r0;
            acc_0 = r1;
            WL_AGAIN(spin_108);
          } else {
            term_sink(e, f_5);
            v_7 = acc_0;
            spare_free(e, cls_fit(2), sp_2);
            spare_free(e, cls_fit(2), sp_1);
            spare_free(e, cls_fit(2), sp_0);
          }
        } else {
          term_sink(e, f_3);
          v_7 = acc_0;
          spare_free(e, cls_fit(2), sp_1);
          spare_free(e, cls_fit(2), sp_0);
        }
      } else {
        term_sink(e, f_1);
        v_7 = acc_0;
        spare_free(e, cls_fit(2), sp_0);
      }
    } else {
      term_sink(e, bs_0);
      v_7 = acc_0;
    }
  break;
  }
  o[0] = v_7;
  return 1;
}

INLINE Term spin_109(Env e, THR Term* o, Term r0, u32 r1) {
  u32 wpoll = 0;
  Term v_10 = 0;
  Term ws_0 = r0;
  u32 n_0 = r1;
  WL_SPIN
    Term v_11 = 0;
    Term v_12 = 0;
    Term o_3[1];
    if (spin_103(e, o_3, n_0) == 0) {
      return 0;
    }
    v_12 = o_3[0];
    v_11 = v_12;
    Term fv_0[1];
    fv_0[0] = 0ull;
    Term v_13 = 0;
    Term o_4[1];
    if (spin_105(e, o_4, ws_0, 0ull, blk_new(e, 0, v_11, 0, 1, fv_0)) == 0) {
      return 0;
    }
    v_13 = o_4[0];
    v_10 = v_13;
  break;
  }
  o[0] = v_10;
  return 1;
}

INLINE Term spin_111(Env e, THR Term* o, Term r0, Term r1) {
  u32 wpoll = 0;
  Term v_4 = 0;
  Term xs_2 = r0;
  Term acc_1 = r1;
  WL_SPIN
    if (term_aux(xs_2) == CID_NIL) {
      v_4 = acc_1;
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, xs_2, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      u64 nd_0 = sp_0 >= HEAP_OFF ? sp_0 : heap_alloc(e, cls_fit(2));
      e.mem[nd_0 + 0] = f_0;
      e.mem[nd_0 + 1] = acc_1;
      r0 = f_1;
      r1 = term_ctr(CID_CON, nd_0);
      xs_2 = r0;
      acc_1 = r1;
      WL_AGAIN(spin_111);
    }
  break;
  }
  o[0] = v_4;
  return 1;
}

INLINE Term spin_110(Env e, THR Term* o, Term r0) {
  u32 wpoll = 0;
  Term v_2 = 0;
  Term xs_1 = r0;
  WL_SPIN
    Term v_3 = 0;
    Term o_0[1];
    if (spin_111(e, o_0, xs_1, term_pak(CID_NIL, 0)) == 0) {
      return 0;
    }
    v_3 = o_0[0];
    v_2 = v_3;
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_112(Env e, THR Term* o, Term r0, Term r1) {
  u32 wpoll = 0;
  Term v_1 = 0;
  Term qr_0 = r0;
  Term qr_1 = r1;
  WL_SPIN
    v_1 = qr_0;
  break;
  }
  o[0] = v_1;
  return 1;
}

INLINE Term spin_113(Env e, THR Term* o, u32 r0, u32 r1, Term r2) {
  u32 wpoll = 0;
  u32 v_2 = 0;
  u32 r_0 = r0;
  u32 r_1 = r1;
  Term r_2 = r2;
  WL_SPIN
    if (r_0 == 1) {
      v_2 = r_1;
    } else {
      term_sink(e, r_2);
      v_2 = 0ull;
    }
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_114(Env e, THR Term* o, Term r0, u32 r1, Term r2, Term r3, u32 r4) {
  u32 wpoll = 0;
  Term v_6 = 0;
  Term fb_0 = r0;
  u32 fb_1 = r1;
  Term fb_2 = r2;
  Term fb_3 = r3;
  u32 sz_0 = r4;
  WL_SPIN
    u64 nd_3 = heap_alloc(e, cls_fit(5));
    e.mem[nd_3 + 0] = fb_0;
    e.mem[nd_3 + 1] = fb_1;
    e.mem[nd_3 + 2] = fb_2;
    e.mem[nd_3 + 3] = fb_3;
    e.mem[nd_3 + 4] = sz_0;
    v_6 = term_clo(FID_READ_TICKER_BYTES_C133, nd_3);
  break;
  }
  o[0] = v_6;
  return 1;
}

INLINE Term spin_115(Env e, THR Term* o, Term r0, Term r1) {
  u32 wpoll = 0;
  u32 v_4 = 0;
  Term v_5 = 0;
  Term qr_0 = r0;
  Term qr_1 = r1;
  WL_SPIN
    v_4 = ((u64)(u32)(nat_chk(e, 48ull + qr_1)));
    v_5 = qr_0;
  break;
  }
  o[0] = v_4;
  o[1] = v_5;
  return 1;
}

INLINE Term spin_116(Env e, THR Term* o, u32 r0, Term r1) {
  u32 wpoll = 0;
  Term v_1 = 0;
  u32 c_1 = r0;
  Term ps_0 = r1;
  WL_SPIN
    if (term_aux(ps_0) == CID_NIL) {
      u64 nd_0 = heap_alloc(e, cls_fit(2));
      e.mem[nd_0 + 0] = rfc_seal(e, c_1);
      e.mem[nd_0 + 1] = rfc_seal(e, term_pak(CID_SNIL, 0));
      u64 nd_1 = heap_alloc(e, cls_fit(2));
      e.mem[nd_1 + 0] = term_ctr(CID_SCON, nd_0);
      e.mem[nd_1 + 1] = term_pak(CID_NIL, 0);
      v_1 = term_ctr(CID_CON, nd_1);
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, ps_0, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      u64 nd_2 = sp_0 >= HEAP_OFF ? sp_0 : heap_alloc(e, cls_fit(2));
      e.mem[nd_2 + 0] = rfc_seal(e, c_1);
      e.mem[nd_2 + 1] = rfc_seal(e, f_0);
      u64 nd_3 = heap_alloc(e, cls_fit(2));
      e.mem[nd_3 + 0] = term_ctr(CID_SCON, nd_2);
      e.mem[nd_3 + 1] = f_1;
      v_1 = term_ctr(CID_CON, nd_3);
    }
  break;
  }
  o[0] = v_1;
  return 1;
}

INLINE Term spin_117(Env e, THR Term* o, Term r0, Term r1) {
  u32 wpoll = 0;
  Term v_2 = 0;
  Term a_3 = r0;
  Term b_2 = r1;
  WL_SPIN
    Term a_4 = (b_2 == 0 ? 0 : a_3 / b_2);
    Term a_5 = (b_2 == 0 ? a_3 : a_3 % b_2);
    Term v_3 = 0;
    Term o_0[1];
    if (spin_112(e, o_0, a_4, a_5) == 0) {
      return 0;
    }
    v_3 = o_0[0];
    v_2 = v_3;
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_118(Env e, THR Term* o, Term r0, Term r1, Term r2, Term r3, Term r4) {
  u32 wpoll = 0;
  u32 v_14 = 0;
  Term v_15 = 0;
  Term v_16 = 0;
  Term v_17 = 0;
  Term v_18 = 0;
  Term k_0 = r0;
  Term xs_0 = r1;
  Term acc_0 = r2;
  Term nl_0 = r3;
  Term nr_0 = r4;
  WL_SPIN
    if (k_0 == 0) {
      Term v_19 = 0;
      Term v_20 = 0;
      Term o_2[1];
      if (spin_110(e, o_2, acc_0) == 0) {
        return 0;
      }
      v_20 = o_2[0];
      v_19 = v_20;
      v_14 = 1;
      v_15 = nl_0;
      v_16 = v_19;
      v_17 = nr_0;
      v_18 = xs_0;
    } else {
      Term p_0 = (k_0 - 1);
      if (term_aux(xs_0) == CID_NIL) {
        Term v_21 = 0;
        Term v_22 = 0;
        Term o_3[1];
        if (spin_110(e, o_3, acc_0) == 0) {
          return 0;
        }
        v_22 = o_3[0];
        v_21 = v_22;
        v_14 = 1;
        v_15 = nl_0;
        v_16 = v_21;
        v_17 = nr_0;
        v_18 = term_pak(CID_NIL, 0);
      } else {
        Term fb_1[2];
        u64 sp_2 = ctr_take(e, xs_0, 2, fb_1);
        Term f_6 = fb_1[0];
        Term f_7 = fb_1[1];
        u64 sp_3 = term_loc(f_6);
        Term f_8 = e.mem[sp_3 + 0];
        Term f_9 = e.mem[sp_3 + 1];
        heap_free(e, cls_fit(2), sp_3);
        u64 nd_1 = sp_2 >= HEAP_OFF ? sp_2 : heap_alloc(e, cls_fit(2));
        e.mem[nd_1 + 0] = f_8;
        e.mem[nd_1 + 1] = f_9;
        u64 nd_2 = heap_alloc(e, cls_fit(2));
        e.mem[nd_2 + 0] = term_ctr(CID_TUPLE, nd_1);
        e.mem[nd_2 + 1] = acc_0;
        r0 = p_0;
        r1 = f_7;
        r2 = term_ctr(CID_CON, nd_2);
        r3 = nl_0;
        r4 = nr_0;
        k_0 = r0;
        xs_0 = r1;
        acc_0 = r2;
        nl_0 = r3;
        nr_0 = r4;
        WL_AGAIN(spin_118);
      }
    }
  break;
  }
  o[0] = v_14;
  o[1] = v_15;
  o[2] = v_16;
  o[3] = v_17;
  o[4] = v_18;
  return 1;
}

INLINE Term spin_121(Env e, THR Term* o, u32 r0, Term r1, Term r2) {
  u32 wpoll = 0;
  Term v_6 = 0;
  u32 code_0 = r0;
  Term msg_0 = r1;
  Term k_0 = r2;
  WL_SPIN
    term_sink(e, k_0);
    u64 nd_5 = heap_alloc(e, cls_fit(2));
    e.mem[nd_5 + 0] = code_0;
    e.mem[nd_5 + 1] = msg_0;
    v_6 = term_ctr(CID_HALT, nd_5);
  break;
  }
  o[0] = v_6;
  return 1;
}

INLINE Term spin_120(Env e, THR Term* o, u32 r0, Term r1, Term r2) {
  u32 wpoll = 0;
  Term v_4 = 0;
  u32 r_0 = r0;
  Term r_1 = r1;
  Term r_2 = r2;
  WL_SPIN
    if (r_0 == 1) {
      u64 nd_3 = heap_alloc(e, cls_fit(1));
      e.mem[nd_3 + 0] = r_1;
      v_4 = term_clo(FID_IO_PASS_C155, nd_3);
    } else {
      u64 nd_4 = heap_alloc(e, cls_fit(2));
      e.mem[nd_4 + 0] = r_1;
      e.mem[nd_4 + 1] = r_2;
      v_4 = term_clo(FID_IO_PASS_C156, nd_4);
    }
  break;
  }
  o[0] = v_4;
  return 1;
}

INLINE Term spin_119(Env e, THR Term* o, Term r0) {
  u32 wpoll = 0;
  Term v_2 = 0;
  Term act_0 = r0;
  WL_SPIN
    u64 nd_2 = heap_alloc(e, cls_fit(1));
    e.mem[nd_2 + 0] = act_0;
    v_2 = term_clo(FID_IO_TRY_C153, nd_2);
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_122(Env e, THR Term* o, Term r0, u32 r1, u32 r2, Term r3) {
  u32 wpoll = 0;
  Term v_8 = 0;
  Term fs_0 = r0;
  u32 fs_1 = r1;
  u32 fs_2 = r2;
  Term fs_3 = r3;
  WL_SPIN
    u32 v_9 = 0;
    u32 v_10 = 0;
    Term o_9[1];
    if (spin_113(e, o_9, fs_1, fs_2, fs_3) == 0) {
      return 0;
    }
    v_10 = o_9[0];
    v_9 = v_10;
    u64 nd_8 = heap_alloc(e, cls_fit(2));
    e.mem[nd_8 + 0] = fs_0;
    e.mem[nd_8 + 1] = v_9;
    v_8 = term_clo(FID_READ_TICKER_SIZED_C160, nd_8);
  break;
  }
  o[0] = v_8;
  return 1;
}

INLINE Term spin_123(Env e, THR Term* o, u32 r0, u32 r1) {
  u32 wpoll = 0;
  u32 v_2 = 0;
  u32 a_0 = r0;
  u32 b_0 = r1;
  WL_SPIN
    v_2 = U32_BIN(a_0, ==, b_0);
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_124(Env e, THR Term* o, u32 r0, Term r1, u32 r2) {
  u32 wpoll = 0;
  Term v_4 = 0;
  u32 c_0 = r0;
  Term r_0 = r1;
  u32 cut_0 = r2;
  WL_SPIN
    if (cut_0 == 0) {
      Term v_5 = 0;
      Term o_1[1];
      if (spin_116(e, o_1, c_0, r_0) == 0) {
        return 0;
      }
      v_5 = o_1[0];
      v_4 = v_5;
    } else {
      u64 nd_0 = heap_alloc(e, cls_fit(2));
      e.mem[nd_0 + 0] = term_pak(CID_SNIL, 0);
      e.mem[nd_0 + 1] = r_0;
      v_4 = term_ctr(CID_CON, nd_0);
    }
  break;
  }
  o[0] = v_4;
  return 1;
}

INLINE Term spin_125(Env e, THR Term* o, Term r0, Term r1) {
  u32 wpoll = 0;
  Term v_1 = 0;
  Term xs_1 = r0;
  Term acc_0 = r1;
  WL_SPIN
    if (term_aux(xs_1) == CID_NIL) {
      v_1 = acc_0;
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, xs_1, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      u64 nd_0 = sp_0 >= HEAP_OFF ? sp_0 : heap_alloc(e, cls_fit(2));
      e.mem[nd_0 + 0] = f_0;
      e.mem[nd_0 + 1] = acc_0;
      r0 = f_1;
      r1 = term_ctr(CID_CON, nd_0);
      xs_1 = r0;
      acc_0 = r1;
      WL_AGAIN(spin_125);
    }
  break;
  }
  o[0] = v_1;
  return 1;
}

INLINE Term spin_126(Env e, THR Term* o, Term r0, Term r1) {
  u32 wpoll = 0;
  Term v_4 = 0;
  Term ws_0 = r0;
  Term acc_0 = r1;
  WL_SPIN
    if (term_aux(ws_0) == CID_NIL) {
      Term v_5 = 0;
      Term o_1[1];
      if (spin_91(e, o_1, acc_0) == 0) {
        return 0;
      }
      v_5 = o_1[0];
      v_4 = v_5;
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, ws_0, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      u32 b0_0 = U32_BIN(f_0, &, 255ull);
      Term a_0 = 8ull;
      u32 b1_0 = U32_BIN((a_0 >= 32 ? 0 : U32_BIN(f_0, >>, a_0)), &, 255ull);
      Term a_1 = 16ull;
      u32 b2_0 = U32_BIN((a_1 >= 32 ? 0 : U32_BIN(f_0, >>, a_1)), &, 255ull);
      Term a_2 = 24ull;
      u32 b3_0 = U32_BIN((a_2 >= 32 ? 0 : U32_BIN(f_0, >>, a_2)), &, 255ull);
      u64 nd_9 = sp_0 >= HEAP_OFF ? sp_0 : heap_alloc(e, cls_fit(2));
      e.mem[nd_9 + 0] = b0_0;
      e.mem[nd_9 + 1] = acc_0;
      u64 nd_10 = heap_alloc(e, cls_fit(2));
      e.mem[nd_10 + 0] = b1_0;
      e.mem[nd_10 + 1] = term_ctr(CID_CON, nd_9);
      u64 nd_11 = heap_alloc(e, cls_fit(2));
      e.mem[nd_11 + 0] = b2_0;
      e.mem[nd_11 + 1] = term_ctr(CID_CON, nd_10);
      u64 nd_12 = heap_alloc(e, cls_fit(2));
      e.mem[nd_12 + 0] = b3_0;
      e.mem[nd_12 + 1] = term_ctr(CID_CON, nd_11);
      r0 = f_1;
      r1 = term_ctr(CID_CON, nd_12);
      ws_0 = r0;
      acc_0 = r1;
      WL_AGAIN(spin_126);
    }
  break;
  }
  o[0] = v_4;
  return 1;
}

INLINE Term spin_127(Env e, THR Term* o, Term r0, u32 r1, u32 r2, Term r3) {
  u32 wpoll = 0;
  Term v_7 = 0;
  Term ow_0 = r0;
  u32 ow_1 = r1;
  u32 ow_2 = r2;
  Term ow_3 = r3;
  WL_SPIN
    term_sink(e, ow_3);
    u64 nd_14 = heap_alloc(e, cls_fit(1));
    e.mem[nd_14 + 0] = ow_0;
    v_7 = term_clo(FID_MAIN_WRITTEN_C185, nd_14);
  break;
  }
  o[0] = v_7;
  return 1;
}

INLINE Term spin_128(Env e, THR Term* o, Term r0) {
  u32 wpoll = 0;
  Term v_2 = 0;
  Term path_0 = r0;
  WL_SPIN
    u64 nd_1 = heap_alloc(e, cls_fit(1));
    e.mem[nd_1 + 0] = path_0;
    v_2 = term_clo(FID_READ_TICKER_C190, nd_1);
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_129(Env e, THR Term* o, Term r0) {
  u32 wpoll = 0;
  Term v_1 = 0;
  Term xs_0 = r0;
  WL_SPIN
    Term v_2 = 0;
    Term o_0[1];
    if (spin_125(e, o_0, xs_0, term_pak(CID_NIL, 0)) == 0) {
      return 0;
    }
    v_2 = o_0[0];
    v_1 = v_2;
  break;
  }
  o[0] = v_1;
  return 1;
}

INLINE Term spin_130(Env e, THR Term* o, Term r0) {
  u32 wpoll = 0;
  Term v_5 = 0;
  Term parts_0 = r0;
  WL_SPIN
    if (term_aux(parts_0) == CID_CON) {
      Term fb_1[2];
      u64 sp_1 = ctr_take(e, parts_0, 2, fb_1);
      Term f_3 = fb_1[0];
      Term f_4 = fb_1[1];
      term_sink(e, f_4);
      v_5 = f_3;
      spare_free(e, cls_fit(2), sp_1);
    } else {
      v_5 = term_pak(CID_SNIL, 0);
    }
  break;
  }
  o[0] = v_5;
  return 1;
}

INLINE Term spin_131(Env e, THR Term* o, u32 r0, Term r1, Term r2) {
  u32 wpoll = 0;
  Term v_9 = 0;
  u32 b_0 = r0;
  Term item_0 = r1;
  Term acc_6 = r2;
  WL_SPIN
    if (b_0 == 1) {
      u64 nd_0 = heap_alloc(e, cls_fit(2));
      e.mem[nd_0 + 0] = item_0;
      e.mem[nd_0 + 1] = acc_6;
      v_9 = term_ctr(CID_CON, nd_0);
    } else {
      term_sink(e, item_0);
      v_9 = acc_6;
    }
  break;
  }
  o[0] = v_9;
  return 1;
}

INLINE Term spin_132(Env e, THR Term* o, u32 r0, Term r1, Term r2) {
  u32 wpoll = 0;
  Term v_2 = 0;
  u32 r_0 = r0;
  Term r_1 = r1;
  Term r_2 = r2;
  WL_SPIN
    if (r_0 == 1) {
      v_2 = r_1;
    } else {
      term_sink(e, r_2);
      v_2 = term_pak(CID_SNIL, 0);
    }
  break;
  }
  o[0] = v_2;
  return 1;
}

INLINE Term spin_133(Env e, THR Term* o, Term r0) {
  u32 wpoll = 0;
  Term v_2 = 0;
  Term args_0 = r0;
  WL_SPIN
    if (term_aux(args_0) == CID_CON) {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, args_0, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      term_sink(e, f_1);
      v_2 = f_0;
      spare_free(e, cls_fit(2), sp_0);
    } else {
      v_2 = term_ctr(CID_SCON, STAT_OFF + 92);
    }
  break;
  }
  o[0] = v_2;
  return 1;
}

// Work
// ====

// A host self-jump is a tail call: as a loop, MachineLICM hoisted eleven
// constants into symreg's entry (3.05 s against 2.51 s).
#if !DEVICE
#undef  WL_SPIN
#undef  WL_SPUN
#undef  WL_AGAIN
#define WL_SPIN
#define WL_SPUN
#define WL_AGAIN(F) __attribute__((musttail)) return WL_##F(WL_ALL)

typedef Reply (PRESERVE(preserve_none) *WlFn)(WL_SIG);
#define WL_X(F) WL_FN WL_##F(WL_SIG);
WL_TABLE WL_X(FID_ENTER)
#undef WL_X
#define WL_X(F) WL_##F,
static const WlFn wl_tab[] = { WL_TABLE };
#undef WL_X
#endif

static Reply work_loop(Env e, Stk sp, Term t, bool seq) {
  WL_BANK
  u32 rn = 0;
  r0 = t;
#if DEVICE
  Fid fid   = FID_ENTER;
  u32 wpoll = 0;
  for (;;) {
  if (err_spun(e.mem, &wpoll)) {
    return 0;
  }
  switch (fid) {
#else
  return WL_FID_ENTER(WL_ALL);
}
#endif

// Segments
// ========

#if !DEVICE
  WL_CASE(FID_OUT_DONE2)
  {
    Term r_0 = r0;
    Term r_1 = r1;
    u32 cnt_0 = r2;
    Term tr_acc_0 = r3;
    WL_OPEN
    term_sink(e, r_0);
    Term v_0 = 0;
    Term v_1 = 0;
    Term o_1[1];
    if (spin_91(e, o_1, r_1) == 0) {
      return 0;
    }
    v_1 = o_1[0];
    v_0 = v_1;
    Term v_5 = 0;
    Term v_6 = 0;
    Term o_2[1];
    if (spin_91(e, o_2, tr_acc_0) == 0) {
      return 0;
    }
    v_6 = o_2[0];
    v_5 = v_6;
    u64 nd_1 = heap_alloc(e, cls_fit(2));
    e.mem[nd_1 + 0] = cnt_0;
    e.mem[nd_1 + 1] = v_5;
    r0 = v_0;
    r1 = term_ctr(CID_CON, nd_1);
    WL_JMP(FID_LIST_APPEND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_RUN_ONE)
  {
    Term t_0 = r0;
    u32 t_1 = r1;
    WL_OPEN
    Term v_0 = 0;
    Term v_1 = 0;
    Term v_2 = 0;
    Term v_3 = 0;
    u32 v_4 = 0;
    u32 v_5 = 0;
    u32 v_6 = 0;
    u32 v_7 = 0;
    u32 v_8 = 0;
    u32 v_9 = 0;
    u32 v_10 = 0;
    u32 v_11 = 0;
    u32 v_12 = 0;
    u32 v_13 = 0;
    u32 v_14 = 0;
    u32 v_15 = 0;
    u32 v_16 = 0;
    u32 v_17 = 0;
    u32 v_18 = 0;
    u32 v_19 = 0;
    u32 v_20 = 0;
    u32 v_21 = 0;
    u32 v_22 = 0;
    u32 v_23 = 0;
    u32 v_24 = 0;
    u32 v_25 = 0;
    u32 v_26 = 0;
    u32 v_27 = 0;
    u32 v_28 = 0;
    u32 v_29 = 0;
    u32 v_30 = 0;
    u32 v_31 = 0;
    u32 v_32 = 0;
    u32 v_33 = 0;
    u32 v_34 = 0;
    u32 v_35 = 0;
    u32 v_36 = 0;
    u32 v_37 = 0;
    u32 v_38 = 0;
    u32 v_39 = 0;
    u32 v_40 = 0;
    u32 v_41 = 0;
    u32 v_42 = 0;
    u32 v_43 = 0;
    u32 v_44 = 0;
    u32 v_45 = 0;
    u32 v_46 = 0;
    u32 v_47 = 0;
    u32 v_48 = 0;
    u32 v_49 = 0;
    u32 v_50 = 0;
    u32 v_51 = 0;
    u32 v_52 = 0;
    u32 v_53 = 0;
    u32 v_54 = 0;
    u32 v_55 = 0;
    u32 v_56 = 0;
    Term fv_0[1];
    fv_0[0] = 0ull;
    Term v_57 = 0;
    Term v_58 = 0;
    Term o_0[1];
    if (spin_103(e, o_0, U32_BIN(t_1, *, 4ull)) == 0) {
      return 0;
    }
    v_58 = o_0[0];
    v_57 = v_58;
    Term fv_1[1];
    fv_1[0] = 0ull;
    Term v_59 = 0;
    Term v_60 = 0;
    Term o_1[1];
    if (spin_103(e, o_1, U32_BIN(t_1, *, 2ull)) == 0) {
      return 0;
    }
    v_60 = o_1[0];
    v_59 = v_60;
    Term fv_2[1];
    fv_2[0] = 0ull;
    Term v_61 = 0;
    Term v_62 = 0;
    Term v_63 = 0;
    Term v_64 = 0;
    u32 v_65 = 0;
    u32 v_66 = 0;
    u32 v_67 = 0;
    u32 v_68 = 0;
    u32 v_69 = 0;
    u32 v_70 = 0;
    u32 v_71 = 0;
    u32 v_72 = 0;
    u32 v_73 = 0;
    u32 v_74 = 0;
    u32 v_75 = 0;
    u32 v_76 = 0;
    u32 v_77 = 0;
    u32 v_78 = 0;
    u32 v_79 = 0;
    u32 v_80 = 0;
    u32 v_81 = 0;
    u32 v_82 = 0;
    u32 v_83 = 0;
    u32 v_84 = 0;
    u32 v_85 = 0;
    u32 v_86 = 0;
    u32 v_87 = 0;
    u32 v_88 = 0;
    u32 v_89 = 0;
    u32 v_90 = 0;
    u32 v_91 = 0;
    u32 v_92 = 0;
    u32 v_93 = 0;
    u32 v_94 = 0;
    u32 v_95 = 0;
    u32 v_96 = 0;
    u32 v_97 = 0;
    u32 v_98 = 0;
    u32 v_99 = 0;
    u32 v_100 = 0;
    u32 v_101 = 0;
    u32 v_102 = 0;
    u32 v_103 = 0;
    u32 v_104 = 0;
    u32 v_105 = 0;
    u32 v_106 = 0;
    u32 v_107 = 0;
    u32 v_108 = 0;
    u32 v_109 = 0;
    u32 v_110 = 0;
    u32 v_111 = 0;
    u32 v_112 = 0;
    u32 v_113 = 0;
    u32 v_114 = 0;
    u32 v_115 = 0;
    u32 v_116 = 0;
    u32 v_117 = 0;
    Term o_3[57];
    if (spin_106(e, o_3, t_1, 0ull, t_0, blk_new(e, 0, 12ull, 0, 1, fv_0), blk_new(e, 0, v_57, 0, 1, fv_1), blk_new(e, 0, v_59, 0, 1, fv_2), 1112014848ull, 1112014848ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0, 0, 0, 0, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 1112014848ull, 1112014848ull, 1112014848ull, 4294967295ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0) == 0) {
      return 0;
    }
    v_61 = o_3[0];
    v_62 = o_3[1];
    v_63 = o_3[2];
    v_64 = o_3[3];
    v_65 = o_3[4];
    v_66 = o_3[5];
    v_67 = o_3[6];
    v_68 = o_3[7];
    v_69 = o_3[8];
    v_70 = o_3[9];
    v_71 = o_3[10];
    v_72 = o_3[11];
    v_73 = o_3[12];
    v_74 = o_3[13];
    v_75 = o_3[14];
    v_76 = o_3[15];
    v_77 = o_3[16];
    v_78 = o_3[17];
    v_79 = o_3[18];
    v_80 = o_3[19];
    v_81 = o_3[20];
    v_82 = o_3[21];
    v_83 = o_3[22];
    v_84 = o_3[23];
    v_85 = o_3[24];
    v_86 = o_3[25];
    v_87 = o_3[26];
    v_88 = o_3[27];
    v_89 = o_3[28];
    v_90 = o_3[29];
    v_91 = o_3[30];
    v_92 = o_3[31];
    v_93 = o_3[32];
    v_94 = o_3[33];
    v_95 = o_3[34];
    v_96 = o_3[35];
    v_97 = o_3[36];
    v_98 = o_3[37];
    v_99 = o_3[38];
    v_100 = o_3[39];
    v_101 = o_3[40];
    v_102 = o_3[41];
    v_103 = o_3[42];
    v_104 = o_3[43];
    v_105 = o_3[44];
    v_106 = o_3[45];
    v_107 = o_3[46];
    v_108 = o_3[47];
    v_109 = o_3[48];
    v_110 = o_3[49];
    v_111 = o_3[50];
    v_112 = o_3[51];
    v_113 = o_3[52];
    v_114 = o_3[53];
    v_115 = o_3[54];
    v_116 = o_3[55];
    v_117 = o_3[56];
    v_0 = v_61;
    v_1 = v_62;
    v_2 = v_63;
    v_3 = v_64;
    v_4 = v_65;
    v_5 = v_66;
    v_6 = v_67;
    v_7 = v_68;
    v_8 = v_69;
    v_9 = v_70;
    v_10 = v_71;
    v_11 = v_72;
    v_12 = v_73;
    v_13 = v_74;
    v_14 = v_75;
    v_15 = v_76;
    v_16 = v_77;
    v_17 = v_78;
    v_18 = v_79;
    v_19 = v_80;
    v_20 = v_81;
    v_21 = v_82;
    v_22 = v_83;
    v_23 = v_84;
    v_24 = v_85;
    v_25 = v_86;
    v_26 = v_87;
    v_27 = v_88;
    v_28 = v_89;
    v_29 = v_90;
    v_30 = v_91;
    v_31 = v_92;
    v_32 = v_93;
    v_33 = v_94;
    v_34 = v_95;
    v_35 = v_96;
    v_36 = v_97;
    v_37 = v_98;
    v_38 = v_99;
    v_39 = v_100;
    v_40 = v_101;
    v_41 = v_102;
    v_42 = v_103;
    v_43 = v_104;
    v_44 = v_105;
    v_45 = v_106;
    v_46 = v_107;
    v_47 = v_108;
    v_48 = v_109;
    v_49 = v_110;
    v_50 = v_111;
    v_51 = v_112;
    v_52 = v_113;
    v_53 = v_114;
    v_54 = v_115;
    v_55 = v_116;
    v_56 = v_117;
    term_sink(e, v_1);
    term_sink(e, v_2);
    Term v_289 = 0;
    Term v_290 = 0;
    u32 v_291 = 0;
    Term v_292 = 0;
    Term v_293 = 0;
    Term o_4[1];
    if (spin_103(e, o_4, t_1) == 0) {
      return 0;
    }
    v_293 = o_4[0];
    v_292 = v_293;
    Term fv_3[1];
    fv_3[0] = 0ull;
    Term v_294 = 0;
    Term v_295 = 0;
    u32 v_296 = 0;
    Term o_5[3];
    if (spin_104(e, o_5, t_1, U32_BIN(t_1, -, 1ull), v_3, blk_new(e, 0, v_292, 0, 1, fv_3), U32_BIN(t_1, -, 1ull)) == 0) {
      return 0;
    }
    v_294 = o_5[0];
    v_295 = o_5[1];
    v_296 = o_5[2];
    v_289 = v_294;
    v_290 = v_295;
    v_291 = v_296;
    Term v_297 = 0;
    Term v_298 = 0;
    Term v_299 = 0;
    Term v_300 = 0;
    u32 v_301 = 0;
    u32 v_302 = 0;
    u32 v_303 = 0;
    Term v_304 = 0;
    Term v_305 = 0;
    Term v_306 = 0;
    Term v_307 = 0;
    u32 v_308 = 0;
    u32 v_309 = 0;
    u32 v_310 = 0;
    Term o_6[7];
    if (spin_100(e, o_6, t_1, 0ull, t_1, v_0, v_289, v_290, term_pak(CID_NIL, 0), 0, 0, 0ull) == 0) {
      return 0;
    }
    v_304 = o_6[0];
    v_305 = o_6[1];
    v_306 = o_6[2];
    v_307 = o_6[3];
    v_308 = o_6[4];
    v_309 = o_6[5];
    v_310 = o_6[6];
    v_297 = v_304;
    v_298 = v_305;
    v_299 = v_306;
    v_300 = v_307;
    v_301 = v_308;
    v_302 = v_309;
    v_303 = v_310;
    term_sink(e, v_297);
    term_sink(e, v_299);
    Term v_311 = 0;
    Term v_312 = 0;
    Term v_313 = 0;
    Term v_314 = 0;
    Term o_7[2];
    if (spin_97(e, o_7, U32_BIN(t_1, *, 2ull), 0ull, v_298, term_pak(CID_NIL, 0)) == 0) {
      return 0;
    }
    v_313 = o_7[0];
    v_314 = o_7[1];
    v_311 = v_313;
    v_312 = v_314;
    if (seq) {
      WL_ROOM(2);
      STK(0) = t_1;
      STK(1) = FID_RUN_ONE_K118;
      WL_PUSHN(2);
    } else {
      u64 t_2 = task_node(e, FID_RUN_ONE_K118, WL_CONT, WL_IDX, 1);
      e.mem[t_2 + 0] = t_1;
      WL_CONT = term_tsk(FID_RUN_ONE_K118, t_2);
      WL_IDX = 1;
    }
    r0 = v_311;
    r1 = v_312;
    r2 = v_303;
    r3 = v_300;
    WL_JMP(FID_OUT_DONE2);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_RUN_ONE_K118)
  {
    WL_POPN(1);
    u32 t_3 = STK(0);
    Term h_0 = r0;
    WL_OPEN
    u64 nd_0 = heap_alloc(e, cls_fit(2));
    e.mem[nd_0 + 0] = t_3;
    e.mem[nd_0 + 1] = h_0;
    r0 = term_ctr(CID_CON, nd_0);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_LIST_APPEND)
  {
    Term xs_0 = r0;
    Term ys_0 = r1;
    WL_OPEN
    WL_SPIN
    if (term_aux(xs_0) == CID_NIL) {
      r0 = ys_0;
      WL_RETN(1);
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, xs_0, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      spare_free(e, cls_fit(2), sp_0);
      if (seq) {
        WL_ROOM(2);
        STK(0) = f_0;
        STK(1) = FID_LIST_APPEND_K129;
        WL_PUSHN(2);
      } else {
        u64 t_0 = task_node(e, FID_LIST_APPEND_K129, WL_CONT, WL_IDX, 1);
        e.mem[t_0 + 0] = f_0;
        WL_CONT = term_tsk(FID_LIST_APPEND_K129, t_0);
        WL_IDX = 1;
      }
      r0 = f_1;
      r1 = ys_0;
      xs_0 = r0;
      ys_0 = r1;
      WL_AGAIN(FID_LIST_APPEND);
    }
    WL_SPUN
  }}
#endif

#if !DEVICE
  WL_CASE(FID_LIST_APPEND_K129)
  {
    WL_POPN(1);
    Term f_2 = STK(0);
    Term h_0 = r0;
    WL_OPEN
    u64 nd_0 = heap_alloc(e, cls_fit(2));
    e.mem[nd_0 + 0] = f_2;
    e.mem[nd_0 + 1] = h_0;
    r0 = term_ctr(CID_CON, nd_0);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_TICKER_BYTES_C133)
  {
    Term fb_4 = r0;
    u32 fb_5 = r1;
    Term fb_6 = r2;
    Term fb_7 = r3;
    u32 sz_1 = r4;
    Term x_2 = r5;
    WL_OPEN
    u64 nd_4 = heap_alloc(e, cls_fit(1));
    e.mem[nd_4 + 0] = fb_4;
    u64 nd_5 = heap_alloc(e, cls_fit(4));
    e.mem[nd_5 + 0] = fb_5;
    e.mem[nd_5 + 1] = fb_6;
    e.mem[nd_5 + 2] = fb_7;
    e.mem[nd_5 + 3] = sz_1;
    r0 = term_clo(FID_FILE_CLOSE, nd_4);
    r1 = term_clo(FID_READ_TICKER_BYTES_C134, nd_5);
    r2 = x_2;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_TICKER_BYTES_C134)
  {
    u32 fb_8 = r0;
    Term fb_9 = r1;
    Term fb_10 = r2;
    u32 sz_2 = r3;
    Term x_3 = r4;
    WL_OPEN
    u64 nd_6 = heap_alloc(e, cls_fit(4));
    e.mem[nd_6 + 0] = fb_8;
    e.mem[nd_6 + 1] = fb_9;
    e.mem[nd_6 + 2] = fb_10;
    e.mem[nd_6 + 3] = sz_2;
    r0 = term_clo(FID_READ_TICKER_BYTES_C135, nd_6);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_TICKER_BYTES_C135)
  {
    u32 fb_11 = r0;
    Term fb_12 = r1;
    Term fb_13 = r2;
    u32 sz_3 = r3;
    Term x_4 = r4;
    WL_OPEN
    Term v_7 = 0;
    Term v_8 = 0;
    Term v_9 = 0;
    Term v_10 = 0;
    Term v_11 = 0;
    Term o_4[1];
    if (spin_107(e, o_4, fb_11, fb_12, fb_13) == 0) {
      return 0;
    }
    v_11 = o_4[0];
    v_10 = v_11;
    Term v_12 = 0;
    Term o_5[1];
    if (spin_108(e, o_5, v_10, term_pak(CID_NIL, 0)) == 0) {
      return 0;
    }
    v_12 = o_5[0];
    v_9 = v_12;
    Term v_13 = 0;
    Term o_6[1];
    if (spin_91(e, o_6, v_9) == 0) {
      return 0;
    }
    v_13 = o_6[0];
    v_8 = v_13;
    Term a_0 = 4ull;
    Term v_14 = 0;
    Term o_7[1];
    if (spin_109(e, o_7, v_8, ((u32)(a_0) == 0 ? 0 : (u64)U32_QUO((u32)(sz_3), (u32)(a_0)))) == 0) {
      return 0;
    }
    v_14 = o_7[0];
    v_7 = v_14;
    Term a_1 = 24ull;
    u64 nd_7 = heap_alloc(e, cls_fit(2));
    e.mem[nd_7 + 0] = v_7;
    e.mem[nd_7 + 1] = ((u32)(a_1) == 0 ? 0 : (u64)U32_QUO((u32)(sz_3), (u32)(a_1)));
    r0 = term_ctr(CID_TUPLE, nd_7);
    r1 = x_4;
    WL_JMP(FID_IO_PURE);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_RUN_ALL)
  {
    Term fuel_0 = r0;
    u32 job_0 = r1;
    Term job_1 = r2;
    Term job_2 = r3;
    Term job_3 = r4;
    Term job_4 = r5;
    WL_OPEN
    WL_SPIN
    if (fuel_0 == 0) {
      term_sink(e, job_2);
      term_sink(e, job_4);
      r0 = term_pak(CID_NIL, 0);
      WL_RETN(1);
    } else {
      Term f_0 = (fuel_0 - 1);
      if (job_0 == 1) {
        if (!seq) {
          u64 t_0 = task_node(e, FID_RUN_ALL_J145, WL_CONT, WL_IDX, 2);
          u64 t_1 = task_node(e, FID_RUN_ALL, term_tsk(FID_RUN_ALL_J145, t_0), 0, 0);
          e.mem[t_1 + 0] = f_0;
          e.mem[t_1 + 1] = 0;
          e.mem[t_1 + 2] = job_1;
          e.mem[t_1 + 3] = job_2;
          e.mem[t_1 + 4] = 0;
          e.mem[t_1 + 5] = 0;
          e.mem[t_0 + 0] = term_tsk(FID_RUN_ALL, t_1);
          u64 t_2 = task_node(e, FID_RUN_ALL, term_tsk(FID_RUN_ALL_J145, t_0), 1, 0);
          e.mem[t_2 + 0] = f_0;
          e.mem[t_2 + 1] = 0;
          e.mem[t_2 + 2] = job_3;
          e.mem[t_2 + 3] = job_4;
          e.mem[t_2 + 4] = 0;
          e.mem[t_2 + 5] = 0;
          e.mem[t_0 + 1] = term_tsk(FID_RUN_ALL, t_2);
          return term_tsk(FID_RUN_ALL_J145, t_0);
        }
        WL_ROOM(4);
        STK(0) = f_0;
        STK(1) = job_3;
        STK(2) = job_4;
        STK(3) = FID_RUN_ALL_K145;
        WL_PUSHN(4);
        r0 = f_0;
        r1 = 0;
        r2 = job_1;
        r3 = job_2;
        r4 = 0;
        r5 = 0;
        fuel_0 = r0;
        job_0 = r1;
        job_1 = r2;
        job_2 = r3;
        job_3 = r4;
        job_4 = r5;
        WL_AGAIN(FID_RUN_ALL);
      } else {
        if (job_1 == 0) {
          term_sink(e, job_2);
          r0 = term_pak(CID_NIL, 0);
          WL_RETN(1);
        } else if (job_1 == 1) {
          if (term_aux(job_2) == CID_NIL) {
            r0 = term_pak(CID_NIL, 0);
            WL_RETN(1);
          } else {
            Term fb_0[2];
            u64 sp_0 = ctr_take(e, job_2, 2, fb_0);
            Term f_2 = fb_0[0];
            Term f_3 = fb_0[1];
            u64 sp_1 = term_loc(f_2);
            Term f_4 = e.mem[sp_1 + 0];
            Term f_5 = e.mem[sp_1 + 1];
            heap_free(e, cls_fit(2), sp_1);
            term_sink(e, f_3);
            spare_free(e, cls_fit(2), sp_0);
            if (seq) {
              WL_ROOM(1);
              STK(0) = FID_RUN_ALL_K148;
              WL_PUSHN(1);
            } else {
              u64 t_3 = task_node(e, FID_RUN_ALL_K148, WL_CONT, WL_IDX, 1);
              WL_CONT = term_tsk(FID_RUN_ALL_K148, t_3);
              WL_IDX = 0;
            }
            r0 = f_4;
            r1 = f_5;
            WL_JMP(FID_RUN_ONE);
          }
        } else {
          Term __0 = (job_1 - 2);
          Term v_0 = 0;
          Term v_1 = 0;
          Term o_1[1];
          if (spin_117(e, o_1, nat_chk(e, nat_chk(e, __0 + 1) + 1), 2ull) == 0) {
            return 0;
          }
          v_1 = o_1[0];
          v_0 = v_1;
          u32 v_4 = 0;
          Term v_5 = 0;
          Term v_6 = 0;
          Term v_7 = 0;
          Term v_8 = 0;
          Term a_6 = nat_chk(e, nat_chk(e, __0 + 1) + 1);
          u32 v_9 = 0;
          Term v_10 = 0;
          Term v_11 = 0;
          Term v_12 = 0;
          Term v_13 = 0;
          Term o_4[5];
          if (spin_118(e, o_4, v_0, job_2, term_pak(CID_NIL, 0), v_0, (a_6 < v_0 ? 0 : a_6 - v_0)) == 0) {
            return 0;
          }
          v_9 = o_4[0];
          v_10 = o_4[1];
          v_11 = o_4[2];
          v_12 = o_4[3];
          v_13 = o_4[4];
          v_4 = v_9;
          v_5 = v_10;
          v_6 = v_11;
          v_7 = v_12;
          v_8 = v_13;
          r0 = f_0;
          r1 = v_4;
          r2 = v_5;
          r3 = v_6;
          r4 = v_7;
          r5 = v_8;
          fuel_0 = r0;
          job_0 = r1;
          job_1 = r2;
          job_2 = r3;
          job_3 = r4;
          job_4 = r5;
          WL_AGAIN(FID_RUN_ALL);
        }
      }
    }
    WL_SPUN
  }}
#endif

#if !DEVICE
  WL_CASE(FID_RUN_ALL_K145)
  {
    Term f_1 = STK(-3);
    Term job_5 = STK(-2);
    Term job_6 = STK(-1);
    Term a_0 = r0;
    WL_OPEN
    WL_ROOM(2);
    STK(0) = a_0;
    STK(1) = FID_RUN_ALL_K146;
    WL_PUSHN(2);
    r0 = f_1;
    r1 = 0;
    r2 = job_5;
    r3 = job_6;
    r4 = 0;
    r5 = 0;
    WL_JMP(FID_RUN_ALL);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_RUN_ALL_K146)
  {
    WL_POPN(4);
    Term a_1 = STK(3);
    Term b_0 = r0;
    WL_OPEN
    r0 = a_1;
    r1 = b_0;
    WL_JMP(FID_RUN_ALL_J145);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_RUN_ALL_J145)
  {
    Term a_2 = r0;
    Term b_1 = r1;
    WL_OPEN
    r0 = a_2;
    r1 = b_1;
    WL_JMP(FID_LIST_APPEND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_RUN_ALL_K148)
  {
    Term h_0 = r0;
    WL_OPEN
    u64 nd_0 = heap_alloc(e, cls_fit(2));
    e.mem[nd_0 + 0] = h_0;
    e.mem[nd_0 + 1] = term_pak(CID_NIL, 0);
    r0 = term_ctr(CID_CON, nd_0);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_LIST_CONCAT)
  {
    Term xss_0 = r0;
    WL_OPEN
    WL_SPIN
    if (term_aux(xss_0) == CID_NIL) {
      r0 = term_pak(CID_NIL, 0);
      WL_RETN(1);
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, xss_0, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      spare_free(e, cls_fit(2), sp_0);
      if (seq) {
        WL_ROOM(2);
        STK(0) = f_0;
        STK(1) = FID_LIST_CONCAT_K150;
        WL_PUSHN(2);
      } else {
        u64 t_0 = task_node(e, FID_LIST_CONCAT_K150, WL_CONT, WL_IDX, 1);
        e.mem[t_0 + 0] = f_0;
        WL_CONT = term_tsk(FID_LIST_CONCAT_K150, t_0);
        WL_IDX = 1;
      }
      r0 = f_1;
      xss_0 = r0;
      WL_AGAIN(FID_LIST_CONCAT);
    }
    WL_SPUN
  }}
#endif

#if !DEVICE
  WL_CASE(FID_LIST_CONCAT_K150)
  {
    WL_POPN(1);
    Term f_2 = STK(0);
    Term h_0 = r0;
    WL_OPEN
    r0 = f_2;
    r1 = h_0;
    WL_JMP(FID_LIST_APPEND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_TRY_C153)
  {
    Term act_1 = r0;
    Term x_1 = r1;
    WL_OPEN
    r0 = act_1;
    r1 = term_clo(FID_IO_TRY_C154, 0);
    r2 = x_1;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_TRY_C154)
  {
    Term x_2 = r0;
    WL_OPEN
    u32 o_0 = 0;
    Term o_1 = 0;
    Term o_2 = 0;
    if (term_aux(x_2) == CID_FAIL) {
      o_0 = 0;
      u64 sp_0 = term_loc(x_2);
      Term f_0 = e.mem[sp_0 + 0];
      heap_free(e, cls_fit(1), sp_0);
      u64 sp_1 = term_loc(f_0);
      Term f_1 = e.mem[sp_1 + 0];
      Term f_2 = e.mem[sp_1 + 1];
      heap_free(e, cls_fit(2), sp_1);
      o_1 = f_1;
      o_2 = f_2;
    } else {
      o_0 = 1;
      u64 sp_2 = term_loc(x_2);
      Term f_3 = e.mem[sp_2 + 0];
      heap_free(e, cls_fit(1), sp_2);
      o_1 = f_3;
    }
    Term v_3 = 0;
    Term o_4[1];
    if (spin_120(e, o_4, o_0, o_1, o_2) == 0) {
      return 0;
    }
    v_3 = o_4[0];
    r0 = v_3;
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_PASS_C155)
  {
    Term r_3 = r0;
    Term x_3 = r1;
    WL_OPEN
    r0 = r_3;
    r1 = x_3;
    WL_JMP(FID_IO_PURE);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_PASS_C156)
  {
    u32 r_4 = r0;
    Term r_5 = r1;
    Term x_4 = r2;
    WL_OPEN
    Term v_5 = 0;
    Term o_3[1];
    if (spin_121(e, o_3, r_4, r_5, x_4) == 0) {
      return 0;
    }
    v_5 = o_3[0];
    r0 = v_5;
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_TICKER_SIZED_C160)
  {
    Term fs_4 = r0;
    u32 v_11 = r1;
    Term x_9 = r2;
    WL_OPEN
    u64 nd_9 = heap_alloc(e, cls_fit(2));
    e.mem[nd_9 + 0] = fs_4;
    e.mem[nd_9 + 1] = v_11;
    u64 nd_10 = heap_alloc(e, cls_fit(1));
    e.mem[nd_10 + 0] = v_11;
    r0 = term_clo(FID_FILE_READ_BYTES, nd_9);
    r1 = term_clo(FID_READ_TICKER_SIZED_C161, nd_10);
    r2 = x_9;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_TICKER_SIZED_C161)
  {
    u32 v_12 = r0;
    Term x_10 = r1;
    WL_OPEN
    u64 sp_7 = term_loc(x_10);
    Term f_10 = e.mem[sp_7 + 0];
    Term f_11 = e.mem[sp_7 + 1];
    heap_free(e, cls_fit(2), sp_7);
    u32 o_10 = 0;
    Term o_11 = 0;
    Term o_12 = 0;
    if (term_aux(f_11) == CID_FAIL) {
      o_10 = 0;
      u64 sp_8 = term_loc(f_11);
      Term f_12 = e.mem[sp_8 + 0];
      heap_free(e, cls_fit(1), sp_8);
      u64 sp_9 = term_loc(f_12);
      Term f_13 = e.mem[sp_9 + 0];
      Term f_14 = e.mem[sp_9 + 1];
      heap_free(e, cls_fit(2), sp_9);
      o_11 = f_13;
      o_12 = f_14;
    } else {
      o_10 = 1;
      u64 sp_10 = term_loc(f_11);
      Term f_15 = e.mem[sp_10 + 0];
      heap_free(e, cls_fit(1), sp_10);
      o_11 = f_15;
    }
    Term v_13 = 0;
    Term o_13[1];
    if (spin_114(e, o_13, f_10, o_10, o_11, o_12, v_12) == 0) {
      return 0;
    }
    v_13 = o_13[0];
    r0 = v_13;
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_NAT_SHOW_FIN)
  {
    Term g_0 = r0;
    Term acc_0 = r1;
    u32 dq_0 = r2;
    Term dq_1 = r3;
    WL_OPEN
    WL_SPIN
    if (dq_1 == 0) {
      u64 nd_0 = heap_alloc(e, cls_fit(2));
      e.mem[nd_0 + 0] = rfc_seal(e, dq_0);
      e.mem[nd_0 + 1] = rfc_seal(e, acc_0);
      r0 = term_ctr(CID_SCON, nd_0);
      WL_RETN(1);
    } else {
      Term p_0 = (dq_1 - 1);
      u64 nd_1 = heap_alloc(e, cls_fit(2));
      e.mem[nd_1 + 0] = rfc_seal(e, dq_0);
      e.mem[nd_1 + 1] = rfc_seal(e, acc_0);
      if (g_0 == 0) {
        Term n_0 = nat_chk(e, p_0 + 1);
        Term acc_1 = term_ctr(CID_SCON, nd_1);
        r0 = acc_1;
        WL_RETN(1);
      } else {
        Term g_1 = (g_0 - 1);
        Term n_1 = nat_chk(e, p_0 + 1);
        Term acc_2 = term_ctr(CID_SCON, nd_1);
        u32 v_0 = 0;
        Term v_1 = 0;
        Term a_0 = 10ull;
        Term a_1 = (a_0 == 0 ? 0 : n_1 / a_0);
        Term a_2 = (a_0 == 0 ? n_1 : n_1 % a_0);
        u32 v_2 = 0;
        Term v_3 = 0;
        Term o_0[2];
        if (spin_115(e, o_0, a_1, a_2) == 0) {
          return 0;
        }
        v_2 = o_0[0];
        v_3 = o_0[1];
        v_0 = v_2;
        v_1 = v_3;
        r0 = g_1;
        r1 = acc_2;
        r2 = v_0;
        r3 = v_1;
        g_0 = r0;
        acc_0 = r1;
        dq_0 = r2;
        dq_1 = r3;
        WL_AGAIN(FID_NAT_SHOW_FIN);
      }
    }
    WL_SPUN
  }}
#endif

#if !DEVICE
  WL_CASE(FID_STRING_LENGTH)
  {
    Term s_0 = r0;
    WL_OPEN
    WL_SPIN
    if (term_aux(s_0) == CID_SNIL) {
      r0 = 0;
      WL_RETN(1);
    } else {
      u64 sp_0 = term_peek(e, s_0);
      u32 f_0 = e.mem[sp_0 + 0];
      Term f_1 = e.mem[sp_0 + 1];
      if (seq) {
        WL_ROOM(1);
        STK(0) = FID_STRING_LENGTH_K165;
        WL_PUSHN(1);
      } else {
        u64 t_0 = task_node(e, FID_STRING_LENGTH_K165, WL_CONT, WL_IDX, 1);
        WL_CONT = term_tsk(FID_STRING_LENGTH_K165, t_0);
        WL_IDX = 0;
      }
      r0 = f_1;
      s_0 = r0;
      WL_AGAIN(FID_STRING_LENGTH);
    }
    WL_SPUN
  }}
#endif

#if !DEVICE
  WL_CASE(FID_STRING_LENGTH_K165)
  {
    Term h_0 = r0;
    WL_OPEN
    r0 = nat_chk(e, h_0 + 1);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_STRING_SPLIT)
  {
    Term s_0 = r0;
    u32 sep_0 = r1;
    WL_OPEN
    WL_SPIN
    if (term_aux(s_0) == CID_SNIL) {
      r0 = term_ctr(CID_CON, STAT_OFF + 34);
      WL_RETN(1);
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, s_0, 2, fb_0);
      u32 f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      spare_free(e, cls_fit(2), sp_0);
      if (seq) {
        WL_ROOM(3);
        STK(0) = f_0;
        STK(1) = sep_0;
        STK(2) = FID_STRING_SPLIT_K168;
        WL_PUSHN(3);
      } else {
        u64 t_0 = task_node(e, FID_STRING_SPLIT_K168, WL_CONT, WL_IDX, 1);
        e.mem[t_0 + 0] = f_0;
        e.mem[t_0 + 1] = sep_0;
        WL_CONT = term_tsk(FID_STRING_SPLIT_K168, t_0);
        WL_IDX = 2;
      }
      r0 = f_1;
      r1 = sep_0;
      s_0 = r0;
      sep_0 = r1;
      WL_AGAIN(FID_STRING_SPLIT);
    }
    WL_SPUN
  }}
#endif

#if !DEVICE
  WL_CASE(FID_STRING_SPLIT_K168)
  {
    WL_POPN(2);
    u32 f_2 = STK(0);
    u32 sep_1 = STK(1);
    Term h_0 = r0;
    WL_OPEN
    u32 v_0 = 0;
    u32 v_1 = 0;
    Term o_0[1];
    if (spin_123(e, o_0, f_2, sep_1) == 0) {
      return 0;
    }
    v_1 = o_0[0];
    v_0 = v_1;
    Term v_3 = 0;
    Term o_2[1];
    if (spin_124(e, o_2, f_2, h_0, v_0) == 0) {
      return 0;
    }
    v_3 = o_2[0];
    r0 = v_3;
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_WRITTEN_C185)
  {
    Term ow_4 = r0;
    Term x_9 = r1;
    WL_OPEN
    u64 nd_15 = heap_alloc(e, cls_fit(1));
    e.mem[nd_15 + 0] = ow_4;
    r0 = term_clo(FID_FILE_CLOSE, nd_15);
    r1 = term_clo(FID_MAIN_WRITTEN_C186, 0);
    r2 = x_9;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_WRITTEN_C186)
  {
    Term x_10 = r0;
    WL_OPEN
    u64 nd_16 = heap_alloc(e, cls_fit(1));
    e.mem[nd_16 + 0] = term_ctr(CID_SCON, STAT_OFF + 30);
    r0 = term_clo(FID_IO_PRINT, nd_16);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_ALL)
  {
    Term paths_0 = r0;
    WL_OPEN
    if (term_aux(paths_0) == CID_NIL) {
      r0 = term_clo(FID_READ_ALL_C188, 0);
      WL_RETN(1);
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, paths_0, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      u64 nd_0 = heap_alloc(e, cls_fit(2));
      e.mem[nd_0 + 0] = f_0;
      e.mem[nd_0 + 1] = f_1;
      spare_free(e, cls_fit(2), sp_0);
      r0 = term_clo(FID_READ_ALL_C189, nd_0);
      WL_RETN(1);
    }
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_ALL_C188)
  {
    Term x_0 = r0;
    WL_OPEN
    r0 = term_pak(CID_NIL, 0);
    r1 = x_0;
    WL_JMP(FID_IO_PURE);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_ALL_C189)
  {
    Term f_2 = r0;
    Term f_3 = r1;
    Term x_1 = r2;
    WL_OPEN
    Term v_0 = 0;
    Term v_1 = 0;
    Term o_5[1];
    if (spin_128(e, o_5, f_2) == 0) {
      return 0;
    }
    v_1 = o_5[0];
    v_0 = v_1;
    u64 nd_5 = heap_alloc(e, cls_fit(1));
    e.mem[nd_5 + 0] = f_3;
    r0 = v_0;
    r1 = term_clo(FID_READ_ALL_C194, nd_5);
    r2 = x_1;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_TICKER_C190)
  {
    Term path_1 = r0;
    Term x_2 = r1;
    WL_OPEN
    Term v_3 = 0;
    u64 nd_2 = heap_alloc(e, cls_fit(2));
    e.mem[nd_2 + 0] = path_1;
    e.mem[nd_2 + 1] = term_ctr(CID_SCON, STAT_OFF + 32);
    Term v_4 = 0;
    Term o_0[1];
    if (spin_119(e, o_0, term_clo(FID_FILE_OPEN, nd_2)) == 0) {
      return 0;
    }
    v_4 = o_0[0];
    v_3 = v_4;
    r0 = v_3;
    r1 = term_clo(FID_READ_TICKER_C191, 0);
    r2 = x_2;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_TICKER_C191)
  {
    Term x_3 = r0;
    WL_OPEN
    u64 nd_3 = heap_alloc(e, cls_fit(1));
    e.mem[nd_3 + 0] = x_3;
    r0 = term_clo(FID_READ_TICKER_C192, nd_3);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_TICKER_C192)
  {
    Term x_4 = r0;
    Term x_5 = r1;
    WL_OPEN
    u64 nd_4 = heap_alloc(e, cls_fit(1));
    e.mem[nd_4 + 0] = x_4;
    r0 = term_clo(FID_FILE_SIZE, nd_4);
    r1 = term_clo(FID_READ_TICKER_C193, 0);
    r2 = x_5;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_TICKER_C193)
  {
    Term x_6 = r0;
    WL_OPEN
    u64 sp_1 = term_loc(x_6);
    Term f_4 = e.mem[sp_1 + 0];
    Term f_5 = e.mem[sp_1 + 1];
    heap_free(e, cls_fit(2), sp_1);
    u32 o_1 = 0;
    u32 o_2 = 0;
    Term o_3 = 0;
    if (term_aux(f_5) == CID_FAIL) {
      o_1 = 0;
      u64 sp_2 = term_loc(f_5);
      Term f_6 = e.mem[sp_2 + 0];
      heap_free(e, cls_fit(1), sp_2);
      u64 sp_3 = term_loc(f_6);
      Term f_7 = e.mem[sp_3 + 0];
      Term f_8 = e.mem[sp_3 + 1];
      heap_free(e, cls_fit(2), sp_3);
      o_2 = f_7;
      o_3 = f_8;
    } else {
      o_1 = 1;
      u64 sp_4 = term_loc(f_5);
      Term f_9 = e.mem[sp_4 + 0];
      heap_free(e, cls_fit(1), sp_4);
      o_2 = f_9;
    }
    Term v_5 = 0;
    Term o_4[1];
    if (spin_122(e, o_4, f_4, o_1, o_2, o_3) == 0) {
      return 0;
    }
    v_5 = o_4[0];
    r0 = v_5;
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_ALL_C194)
  {
    Term f_10 = r0;
    Term x_7 = r1;
    WL_OPEN
    u64 sp_5 = term_loc(x_7);
    Term f_11 = e.mem[sp_5 + 0];
    Term f_12 = e.mem[sp_5 + 1];
    heap_free(e, cls_fit(2), sp_5);
    if (seq) {
      WL_ROOM(3);
      STK(0) = f_11;
      STK(1) = f_12;
      STK(2) = FID_READ_ALL_K195;
      WL_PUSHN(3);
    } else {
      u64 t_0 = task_node(e, FID_READ_ALL_K195, WL_CONT, WL_IDX, 1);
      e.mem[t_0 + 0] = f_11;
      e.mem[t_0 + 1] = f_12;
      WL_CONT = term_tsk(FID_READ_ALL_K195, t_0);
      WL_IDX = 2;
    }
    r0 = f_10;
    WL_JMP(FID_READ_ALL);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_ALL_K195)
  {
    WL_POPN(2);
    Term f_13 = STK(0);
    u32 f_14 = STK(1);
    Term h_0 = r0;
    WL_OPEN
    u64 nd_6 = heap_alloc(e, cls_fit(3));
    e.mem[nd_6 + 0] = f_13;
    e.mem[nd_6 + 1] = f_14;
    e.mem[nd_6 + 2] = h_0;
    r0 = term_clo(FID_READ_ALL_C196, nd_6);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_ALL_C196)
  {
    Term f_15 = r0;
    u32 f_16 = r1;
    Term h_1 = r2;
    Term x_8 = r3;
    WL_OPEN
    u64 nd_7 = heap_alloc(e, cls_fit(2));
    e.mem[nd_7 + 0] = f_15;
    e.mem[nd_7 + 1] = f_16;
    r0 = h_1;
    r1 = term_clo(FID_READ_ALL_C197, nd_7);
    r2 = x_8;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_ALL_C197)
  {
    Term f_17 = r0;
    u32 f_18 = r1;
    Term x_9 = r2;
    WL_OPEN
    u64 nd_8 = heap_alloc(e, cls_fit(3));
    e.mem[nd_8 + 0] = f_17;
    e.mem[nd_8 + 1] = f_18;
    e.mem[nd_8 + 2] = x_9;
    r0 = term_clo(FID_READ_ALL_C198, nd_8);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_READ_ALL_C198)
  {
    Term f_19 = r0;
    u32 f_20 = r1;
    Term x_10 = r2;
    Term x_11 = r3;
    WL_OPEN
    u64 nd_9 = heap_alloc(e, cls_fit(2));
    e.mem[nd_9 + 0] = f_19;
    e.mem[nd_9 + 1] = f_20;
    u64 nd_10 = heap_alloc(e, cls_fit(2));
    e.mem[nd_10 + 0] = term_ctr(CID_TUPLE, nd_9);
    e.mem[nd_10 + 1] = x_10;
    r0 = term_ctr(CID_CON, nd_10);
    r1 = x_11;
    WL_JMP(FID_IO_PURE);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_NAT_SHOW)
  {
    Term n_0 = r0;
    WL_OPEN
    u32 v_0 = 0;
    Term v_1 = 0;
    Term a_0 = 10ull;
    Term a_1 = (a_0 == 0 ? 0 : n_0 / a_0);
    Term a_2 = (a_0 == 0 ? n_0 : n_0 % a_0);
    u32 v_2 = 0;
    Term v_3 = 0;
    Term o_0[2];
    if (spin_115(e, o_0, a_1, a_2) == 0) {
      return 0;
    }
    v_2 = o_0[0];
    v_3 = o_0[1];
    v_0 = v_2;
    v_1 = v_3;
    r0 = n_0;
    r1 = term_pak(CID_SNIL, 0);
    r2 = v_0;
    r3 = v_1;
    WL_JMP(FID_NAT_SHOW_FIN);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_LIST_LENGTH)
  {
    Term xs_0 = r0;
    WL_OPEN
    WL_SPIN
    if (term_aux(xs_0) == CID_NIL) {
      r0 = 0;
      WL_RETN(1);
    } else {
      u64 sp_0 = term_peek(e, xs_0);
      Term f_0 = e.mem[sp_0 + 0];
      Term f_1 = e.mem[sp_0 + 1];
      if (seq) {
        WL_ROOM(1);
        STK(0) = FID_LIST_LENGTH_K201;
        WL_PUSHN(1);
      } else {
        u64 t_0 = task_node(e, FID_LIST_LENGTH_K201, WL_CONT, WL_IDX, 1);
        WL_CONT = term_tsk(FID_LIST_LENGTH_K201, t_0);
        WL_IDX = 0;
      }
      r0 = f_1;
      xs_0 = r0;
      WL_AGAIN(FID_LIST_LENGTH);
    }
    WL_SPUN
  }}
#endif

#if !DEVICE
  WL_CASE(FID_LIST_LENGTH_K201)
  {
    Term h_0 = r0;
    WL_OPEN
    r0 = nat_chk(e, h_0 + 1);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_STRING_LINES)
  {
    Term s_0 = r0;
    WL_OPEN
    r0 = s_0;
    r1 = 10ull;
    WL_JMP(FID_STRING_SPLIT);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_PATHS_OF)
  {
    Term lines_0 = r0;
    Term dir_0 = r1;
    Term acc_0 = r2;
    WL_OPEN
    if (term_aux(lines_0) == CID_NIL) {
      term_sink(e, dir_0);
      Term v_0 = 0;
      Term o_1[1];
      if (spin_129(e, o_1, acc_0) == 0) {
        return 0;
      }
      v_0 = o_1[0];
      r0 = v_0;
      WL_RETN(1);
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, lines_0, 2, fb_0);
      Term f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      spare_free(e, cls_fit(2), sp_0);
      if (seq) {
        WL_ROOM(4);
        STK(0) = f_1;
        STK(1) = dir_0;
        STK(2) = acc_0;
        STK(3) = FID_PATHS_OF_K205;
        WL_PUSHN(4);
      } else {
        u64 t_0 = task_node(e, FID_PATHS_OF_K205, WL_CONT, WL_IDX, 1);
        e.mem[t_0 + 0] = f_1;
        e.mem[t_0 + 1] = dir_0;
        e.mem[t_0 + 2] = acc_0;
        WL_CONT = term_tsk(FID_PATHS_OF_K205, t_0);
        WL_IDX = 3;
      }
      r0 = f_0;
      r1 = 32ull;
      WL_JMP(FID_STRING_SPLIT);
    }
  }}
#endif

#if !DEVICE
  WL_CASE(FID_PATHS_OF_K205)
  {
    WL_POPN(3);
    Term f_2 = STK(0);
    Term dir_1 = STK(1);
    Term acc_1 = STK(2);
    Term h_0 = r0;
    WL_OPEN
    Term v_3 = 0;
    Term v_4 = 0;
    Term o_2[1];
    if (spin_130(e, o_2, h_0) == 0) {
      return 0;
    }
    v_4 = o_2[0];
    v_3 = v_4;
    if (seq) {
      WL_ROOM(5);
      STK(0) = f_2;
      STK(1) = dir_1;
      STK(2) = acc_1;
      STK(3) = v_3;
      STK(4) = FID_PATHS_OF_K206;
      WL_PUSHN(5);
    } else {
      u64 t_1 = task_node(e, FID_PATHS_OF_K206, WL_CONT, WL_IDX, 1);
      e.mem[t_1 + 0] = f_2;
      e.mem[t_1 + 1] = dir_1;
      e.mem[t_1 + 2] = acc_1;
      e.mem[t_1 + 3] = v_3;
      WL_CONT = term_tsk(FID_PATHS_OF_K206, t_1);
      WL_IDX = 4;
    }
    r0 = v_3;
    WL_JMP(FID_STRING_LENGTH);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_PATHS_OF_K206)
  {
    WL_POPN(4);
    Term f_5 = STK(0);
    Term dir_2 = STK(1);
    Term acc_2 = STK(2);
    Term v_6 = STK(3);
    Term h_1 = r0;
    WL_OPEN
    if (seq) {
      WL_ROOM(5);
      STK(0) = f_5;
      STK(1) = dir_2;
      STK(2) = acc_2;
      STK(3) = h_1;
      STK(4) = FID_PATHS_OF_K207;
      WL_PUSHN(5);
    } else {
      u64 t_2 = task_node(e, FID_PATHS_OF_K207, WL_CONT, WL_IDX, 1);
      e.mem[t_2 + 0] = f_5;
      e.mem[t_2 + 1] = dir_2;
      e.mem[t_2 + 2] = acc_2;
      e.mem[t_2 + 3] = h_1;
      WL_CONT = term_tsk(FID_PATHS_OF_K207, t_2);
      WL_IDX = 4;
    }
    r0 = v_6;
    r1 = term_ctr(CID_SCON, STAT_OFF + 6);
    WL_JMP(FID_STRING_APPEND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_PATHS_OF_K207)
  {
    WL_POPN(4);
    Term f_6 = STK(0);
    Term dir_3 = STK(1);
    Term acc_3 = STK(2);
    Term h_2 = STK(3);
    Term h_3 = r0;
    WL_OPEN
    if (seq) {
      WL_ROOM(5);
      STK(0) = f_6;
      STK(1) = dir_3;
      STK(2) = acc_3;
      STK(3) = h_2;
      STK(4) = FID_PATHS_OF_K208;
      WL_PUSHN(5);
    } else {
      u64 t_3 = task_node(e, FID_PATHS_OF_K208, WL_CONT, WL_IDX, 1);
      e.mem[t_3 + 0] = f_6;
      e.mem[t_3 + 1] = dir_3;
      e.mem[t_3 + 2] = acc_3;
      e.mem[t_3 + 3] = h_2;
      WL_CONT = term_tsk(FID_PATHS_OF_K208, t_3);
      WL_IDX = 4;
    }
    r0 = term_ctr(CID_SCON, STAT_OFF + 58);
    r1 = h_3;
    WL_JMP(FID_STRING_APPEND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_PATHS_OF_K208)
  {
    WL_POPN(4);
    Term f_7 = STK(0);
    Term dir_4 = STK(1);
    Term acc_4 = STK(2);
    Term h_4 = STK(3);
    Term h_5 = r0;
    WL_OPEN
    dir_4 = term_keep(e, dir_4);
    if (seq) {
      WL_ROOM(5);
      STK(0) = f_7;
      STK(1) = dir_4;
      STK(2) = acc_4;
      STK(3) = h_4;
      STK(4) = FID_PATHS_OF_K209;
      WL_PUSHN(5);
    } else {
      u64 t_4 = task_node(e, FID_PATHS_OF_K209, WL_CONT, WL_IDX, 1);
      e.mem[t_4 + 0] = f_7;
      e.mem[t_4 + 1] = dir_4;
      e.mem[t_4 + 2] = acc_4;
      e.mem[t_4 + 3] = h_4;
      WL_CONT = term_tsk(FID_PATHS_OF_K209, t_4);
      WL_IDX = 4;
    }
    r0 = dir_4;
    r1 = h_5;
    WL_JMP(FID_STRING_APPEND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_PATHS_OF_K209)
  {
    WL_POPN(4);
    Term f_8 = STK(0);
    Term dir_5 = STK(1);
    Term acc_5 = STK(2);
    Term h_6 = STK(3);
    Term h_7 = r0;
    WL_OPEN
    Term v_7 = 0;
    Term v_8 = 0;
    Term o_3[1];
    if (spin_131(e, o_3, (0 < h_6), h_7, acc_5) == 0) {
      return 0;
    }
    v_8 = o_3[0];
    v_7 = v_8;
    r0 = f_8;
    r1 = dir_5;
    r2 = v_7;
    WL_JMP(FID_PATHS_OF);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_PURE)
  {
    Term x_0 = r0;
    Term k_0 = r1;
    WL_OPEN
    r0 = k_0;
    r1 = x_0;
    WL_JMP(FID_CLO_APPLY);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_STRING_APPEND)
  {
    Term a_0 = r0;
    Term b_0 = r1;
    WL_OPEN
    WL_SPIN
    if (term_aux(a_0) == CID_SNIL) {
      r0 = b_0;
      WL_RETN(1);
    } else {
      Term fb_0[2];
      u64 sp_0 = ctr_take(e, a_0, 2, fb_0);
      u32 f_0 = fb_0[0];
      Term f_1 = fb_0[1];
      spare_free(e, cls_fit(2), sp_0);
      if (seq) {
        WL_ROOM(2);
        STK(0) = f_0;
        STK(1) = FID_STRING_APPEND_K275;
        WL_PUSHN(2);
      } else {
        u64 t_0 = task_node(e, FID_STRING_APPEND_K275, WL_CONT, WL_IDX, 1);
        e.mem[t_0 + 0] = f_0;
        WL_CONT = term_tsk(FID_STRING_APPEND_K275, t_0);
        WL_IDX = 1;
      }
      r0 = f_1;
      r1 = b_0;
      a_0 = r0;
      b_0 = r1;
      WL_AGAIN(FID_STRING_APPEND);
    }
    WL_SPUN
  }}
#endif

#if !DEVICE
  WL_CASE(FID_STRING_APPEND_K275)
  {
    WL_POPN(1);
    u32 f_2 = STK(0);
    Term h_0 = r0;
    WL_OPEN
    u64 nd_0 = heap_alloc(e, cls_fit(2));
    e.mem[nd_0 + 0] = rfc_seal(e, f_2);
    e.mem[nd_0 + 1] = rfc_seal(e, h_0);
    r0 = term_ctr(CID_SCON, nd_0);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_BIND)
  {
    Term m_0 = r0;
    Term f_0 = r1;
    Term k_0 = r2;
    WL_OPEN
    u64 nd_0 = heap_alloc(e, cls_fit(2));
    e.mem[nd_0 + 0] = f_0;
    e.mem[nd_0 + 1] = k_0;
    r0 = m_0;
    r1 = term_clo(FID_IO_BIND_C316, nd_0);
    WL_JMP(FID_CLO_APPLY);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_BIND_C316)
  {
    Term f_1 = r0;
    Term k_1 = r1;
    Term x_0 = r2;
    WL_OPEN
    if (seq) {
      WL_ROOM(2);
      STK(0) = k_1;
      STK(1) = FID_IO_BIND_K317;
      WL_PUSHN(2);
    } else {
      u64 t_0 = task_node(e, FID_IO_BIND_K317, WL_CONT, WL_IDX, 1);
      e.mem[t_0 + 0] = k_1;
      WL_CONT = term_tsk(FID_IO_BIND_K317, t_0);
      WL_IDX = 1;
    }
    r0 = f_1;
    r1 = x_0;
    WL_JMP(FID_CLO_APPLY);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_BIND_K317)
  {
    WL_POPN(1);
    Term k_2 = STK(0);
    Term h_0 = r0;
    WL_OPEN
    r0 = h_0;
    r1 = k_2;
    WL_JMP(FID_CLO_APPLY);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN)
  {
    WL_OPEN
    r0 = term_clo(FID_MAIN_C319, 0);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C319)
  {
    Term x_0 = r0;
    WL_OPEN
    r0 = term_clo(FID_IO_ARGS, 0);
    r1 = term_clo(FID_MAIN_C320, 0);
    r2 = x_0;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C320)
  {
    Term x_1 = r0;
    WL_OPEN
    Term v_0 = 0;
    Term v_1 = 0;
    Term o_0[1];
    if (spin_133(e, o_0, x_1) == 0) {
      return 0;
    }
    v_1 = o_0[0];
    v_0 = v_1;
    v_0 = term_keep(e, v_0);
    if (seq) {
      WL_ROOM(2);
      STK(0) = v_0;
      STK(1) = FID_MAIN_K321;
      WL_PUSHN(2);
    } else {
      u64 t_0 = task_node(e, FID_MAIN_K321, WL_CONT, WL_IDX, 1);
      e.mem[t_0 + 0] = v_0;
      WL_CONT = term_tsk(FID_MAIN_K321, t_0);
      WL_IDX = 1;
    }
    r0 = v_0;
    r1 = term_ctr(CID_SCON, STAT_OFF + 110);
    WL_JMP(FID_STRING_APPEND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K321)
  {
    WL_POPN(1);
    Term v_3 = STK(0);
    Term h_0 = r0;
    WL_OPEN
    u64 nd_0 = heap_alloc(e, cls_fit(2));
    e.mem[nd_0 + 0] = v_3;
    e.mem[nd_0 + 1] = h_0;
    r0 = term_clo(FID_MAIN_C322, nd_0);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C322)
  {
    Term v_4 = r0;
    Term h_1 = r1;
    Term x_2 = r2;
    WL_OPEN
    Term v_5 = 0;
    u64 nd_1 = heap_alloc(e, cls_fit(2));
    e.mem[nd_1 + 0] = h_1;
    e.mem[nd_1 + 1] = term_ctr(CID_SCON, STAT_OFF + 32);
    Term v_6 = 0;
    Term o_1[1];
    if (spin_119(e, o_1, term_clo(FID_FILE_OPEN, nd_1)) == 0) {
      return 0;
    }
    v_6 = o_1[0];
    v_5 = v_6;
    u64 nd_2 = heap_alloc(e, cls_fit(1));
    e.mem[nd_2 + 0] = v_4;
    r0 = v_5;
    r1 = term_clo(FID_MAIN_C323, nd_2);
    r2 = x_2;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C323)
  {
    Term v_7 = r0;
    Term x_3 = r1;
    WL_OPEN
    u64 nd_3 = heap_alloc(e, cls_fit(2));
    e.mem[nd_3 + 0] = v_7;
    e.mem[nd_3 + 1] = x_3;
    r0 = term_clo(FID_MAIN_C324, nd_3);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C324)
  {
    Term v_8 = r0;
    Term x_4 = r1;
    Term x_5 = r2;
    WL_OPEN
    u64 nd_4 = heap_alloc(e, cls_fit(1));
    e.mem[nd_4 + 0] = x_4;
    u64 nd_5 = heap_alloc(e, cls_fit(1));
    e.mem[nd_5 + 0] = v_8;
    r0 = term_clo(FID_FILE_SIZE, nd_4);
    r1 = term_clo(FID_MAIN_C325, nd_5);
    r2 = x_5;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C325)
  {
    Term v_9 = r0;
    Term x_6 = r1;
    WL_OPEN
    u64 sp_1 = term_loc(x_6);
    Term f_2 = e.mem[sp_1 + 0];
    Term f_3 = e.mem[sp_1 + 1];
    heap_free(e, cls_fit(2), sp_1);
    u32 o_2 = 0;
    u32 o_3 = 0;
    Term o_4 = 0;
    if (term_aux(f_3) == CID_FAIL) {
      o_2 = 0;
      u64 sp_2 = term_loc(f_3);
      Term f_4 = e.mem[sp_2 + 0];
      heap_free(e, cls_fit(1), sp_2);
      u64 sp_3 = term_loc(f_4);
      Term f_5 = e.mem[sp_3 + 0];
      Term f_6 = e.mem[sp_3 + 1];
      heap_free(e, cls_fit(2), sp_3);
      o_3 = f_5;
      o_4 = f_6;
    } else {
      o_2 = 1;
      u64 sp_4 = term_loc(f_3);
      Term f_7 = e.mem[sp_4 + 0];
      heap_free(e, cls_fit(1), sp_4);
      o_3 = f_7;
    }
    u64 nd_6 = heap_alloc(e, cls_fit(5));
    e.mem[nd_6 + 0] = f_2;
    e.mem[nd_6 + 1] = o_2;
    e.mem[nd_6 + 2] = o_3;
    e.mem[nd_6 + 3] = o_4;
    e.mem[nd_6 + 4] = v_9;
    r0 = term_clo(FID_MAIN_C326, nd_6);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C326)
  {
    Term f_8 = r0;
    u32 o_5 = r1;
    u32 o_6 = r2;
    Term o_7 = r3;
    Term v_10 = r4;
    Term x_7 = r5;
    WL_OPEN
    u32 v_11 = 0;
    u32 v_12 = 0;
    Term o_8[1];
    if (spin_113(e, o_8, o_5, o_6, o_7) == 0) {
      return 0;
    }
    v_12 = o_8[0];
    v_11 = v_12;
    u64 nd_7 = heap_alloc(e, cls_fit(2));
    e.mem[nd_7 + 0] = f_8;
    e.mem[nd_7 + 1] = v_11;
    u64 nd_8 = heap_alloc(e, cls_fit(1));
    e.mem[nd_8 + 0] = v_10;
    r0 = term_clo(FID_FILE_READ, nd_7);
    r1 = term_clo(FID_MAIN_C327, nd_8);
    r2 = x_7;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C327)
  {
    Term v_13 = r0;
    Term x_8 = r1;
    WL_OPEN
    u64 sp_5 = term_loc(x_8);
    Term f_9 = e.mem[sp_5 + 0];
    Term f_10 = e.mem[sp_5 + 1];
    heap_free(e, cls_fit(2), sp_5);
    u32 o_9 = 0;
    Term o_10 = 0;
    Term o_11 = 0;
    if (term_aux(f_10) == CID_FAIL) {
      o_9 = 0;
      u64 sp_6 = term_loc(f_10);
      Term f_11 = e.mem[sp_6 + 0];
      heap_free(e, cls_fit(1), sp_6);
      u64 sp_7 = term_loc(f_11);
      Term f_12 = e.mem[sp_7 + 0];
      Term f_13 = e.mem[sp_7 + 1];
      heap_free(e, cls_fit(2), sp_7);
      o_10 = f_12;
      o_11 = f_13;
    } else {
      o_9 = 1;
      u64 sp_8 = term_loc(f_10);
      Term f_14 = e.mem[sp_8 + 0];
      heap_free(e, cls_fit(1), sp_8);
      o_10 = f_14;
    }
    Term v_14 = 0;
    Term v_15 = 0;
    Term o_12[1];
    if (spin_132(e, o_12, o_9, o_10, o_11) == 0) {
      return 0;
    }
    v_15 = o_12[0];
    v_14 = v_15;
    if (seq) {
      WL_ROOM(3);
      STK(0) = f_9;
      STK(1) = v_13;
      STK(2) = FID_MAIN_K328;
      WL_PUSHN(3);
    } else {
      u64 t_1 = task_node(e, FID_MAIN_K328, WL_CONT, WL_IDX, 1);
      e.mem[t_1 + 0] = f_9;
      e.mem[t_1 + 1] = v_13;
      WL_CONT = term_tsk(FID_MAIN_K328, t_1);
      WL_IDX = 2;
    }
    r0 = v_14;
    WL_JMP(FID_STRING_LINES);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K328)
  {
    WL_POPN(2);
    Term f_15 = STK(0);
    Term v_16 = STK(1);
    Term h_2 = r0;
    WL_OPEN
    v_16 = term_keep(e, v_16);
    if (seq) {
      WL_ROOM(3);
      STK(0) = f_15;
      STK(1) = v_16;
      STK(2) = FID_MAIN_K329;
      WL_PUSHN(3);
    } else {
      u64 t_2 = task_node(e, FID_MAIN_K329, WL_CONT, WL_IDX, 1);
      e.mem[t_2 + 0] = f_15;
      e.mem[t_2 + 1] = v_16;
      WL_CONT = term_tsk(FID_MAIN_K329, t_2);
      WL_IDX = 2;
    }
    r0 = h_2;
    r1 = v_16;
    r2 = term_pak(CID_NIL, 0);
    WL_JMP(FID_PATHS_OF);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K329)
  {
    WL_POPN(2);
    Term f_16 = STK(0);
    Term v_17 = STK(1);
    Term paths_0 = r0;
    WL_OPEN
    if (seq) {
      WL_ROOM(4);
      STK(0) = f_16;
      STK(1) = v_17;
      STK(2) = paths_0;
      STK(3) = FID_MAIN_K330;
      WL_PUSHN(4);
    } else {
      u64 t_3 = task_node(e, FID_MAIN_K330, WL_CONT, WL_IDX, 1);
      e.mem[t_3 + 0] = f_16;
      e.mem[t_3 + 1] = v_17;
      e.mem[t_3 + 2] = paths_0;
      WL_CONT = term_tsk(FID_MAIN_K330, t_3);
      WL_IDX = 3;
    }
    r0 = paths_0;
    WL_JMP(FID_LIST_LENGTH);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K330)
  {
    WL_POPN(3);
    Term f_17 = STK(0);
    Term v_18 = STK(1);
    Term paths_1 = STK(2);
    Term n_0 = r0;
    WL_OPEN
    u64 nd_9 = heap_alloc(e, cls_fit(4));
    e.mem[nd_9 + 0] = f_17;
    e.mem[nd_9 + 1] = v_18;
    e.mem[nd_9 + 2] = paths_1;
    e.mem[nd_9 + 3] = n_0;
    r0 = term_clo(FID_MAIN_C331, nd_9);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C331)
  {
    Term f_18 = r0;
    Term v_19 = r1;
    Term paths_2 = r2;
    Term n_1 = r3;
    Term x_9 = r4;
    WL_OPEN
    u64 nd_10 = heap_alloc(e, cls_fit(1));
    e.mem[nd_10 + 0] = f_18;
    u64 nd_11 = heap_alloc(e, cls_fit(3));
    e.mem[nd_11 + 0] = v_19;
    e.mem[nd_11 + 1] = paths_2;
    e.mem[nd_11 + 2] = n_1;
    r0 = term_clo(FID_FILE_CLOSE, nd_10);
    r1 = term_clo(FID_MAIN_C332, nd_11);
    r2 = x_9;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C332)
  {
    Term v_20 = r0;
    Term paths_3 = r1;
    Term n_2 = r2;
    Term x_10 = r3;
    WL_OPEN
    if (seq) {
      WL_ROOM(4);
      STK(0) = v_20;
      STK(1) = paths_3;
      STK(2) = n_2;
      STK(3) = FID_MAIN_K333;
      WL_PUSHN(4);
    } else {
      u64 t_4 = task_node(e, FID_MAIN_K333, WL_CONT, WL_IDX, 1);
      e.mem[t_4 + 0] = v_20;
      e.mem[t_4 + 1] = paths_3;
      e.mem[t_4 + 2] = n_2;
      WL_CONT = term_tsk(FID_MAIN_K333, t_4);
      WL_IDX = 3;
    }
    r0 = n_2;
    WL_JMP(FID_NAT_SHOW);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K333)
  {
    WL_POPN(3);
    Term v_21 = STK(0);
    Term paths_4 = STK(1);
    Term n_3 = STK(2);
    Term h_3 = r0;
    WL_OPEN
    if (seq) {
      WL_ROOM(4);
      STK(0) = v_21;
      STK(1) = paths_4;
      STK(2) = n_3;
      STK(3) = FID_MAIN_K334;
      WL_PUSHN(4);
    } else {
      u64 t_5 = task_node(e, FID_MAIN_K334, WL_CONT, WL_IDX, 1);
      e.mem[t_5 + 0] = v_21;
      e.mem[t_5 + 1] = paths_4;
      e.mem[t_5 + 2] = n_3;
      WL_CONT = term_tsk(FID_MAIN_K334, t_5);
      WL_IDX = 3;
    }
    r0 = term_ctr(CID_SCON, STAT_OFF + 70);
    r1 = h_3;
    WL_JMP(FID_STRING_APPEND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K334)
  {
    WL_POPN(3);
    Term v_22 = STK(0);
    Term paths_5 = STK(1);
    Term n_4 = STK(2);
    Term h_4 = r0;
    WL_OPEN
    u64 nd_12 = heap_alloc(e, cls_fit(4));
    e.mem[nd_12 + 0] = v_22;
    e.mem[nd_12 + 1] = paths_5;
    e.mem[nd_12 + 2] = n_4;
    e.mem[nd_12 + 3] = h_4;
    r0 = term_clo(FID_MAIN_C335, nd_12);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C335)
  {
    Term v_23 = r0;
    Term paths_6 = r1;
    Term n_5 = r2;
    Term h_5 = r3;
    Term x_11 = r4;
    WL_OPEN
    u64 nd_13 = heap_alloc(e, cls_fit(1));
    e.mem[nd_13 + 0] = h_5;
    u64 nd_14 = heap_alloc(e, cls_fit(3));
    e.mem[nd_14 + 0] = v_23;
    e.mem[nd_14 + 1] = paths_6;
    e.mem[nd_14 + 2] = n_5;
    r0 = term_clo(FID_IO_PRINT, nd_13);
    r1 = term_clo(FID_MAIN_C336, nd_14);
    r2 = x_11;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C336)
  {
    Term v_24 = r0;
    Term paths_7 = r1;
    Term n_6 = r2;
    Term x_12 = r3;
    WL_OPEN
    if (seq) {
      WL_ROOM(3);
      STK(0) = v_24;
      STK(1) = n_6;
      STK(2) = FID_MAIN_K337;
      WL_PUSHN(3);
    } else {
      u64 t_6 = task_node(e, FID_MAIN_K337, WL_CONT, WL_IDX, 1);
      e.mem[t_6 + 0] = v_24;
      e.mem[t_6 + 1] = n_6;
      WL_CONT = term_tsk(FID_MAIN_K337, t_6);
      WL_IDX = 2;
    }
    r0 = paths_7;
    WL_JMP(FID_READ_ALL);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K337)
  {
    WL_POPN(2);
    Term v_25 = STK(0);
    Term n_7 = STK(1);
    Term h_6 = r0;
    WL_OPEN
    u64 nd_15 = heap_alloc(e, cls_fit(3));
    e.mem[nd_15 + 0] = v_25;
    e.mem[nd_15 + 1] = n_7;
    e.mem[nd_15 + 2] = h_6;
    r0 = term_clo(FID_MAIN_C338, nd_15);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C338)
  {
    Term v_26 = r0;
    Term n_8 = r1;
    Term h_7 = r2;
    Term x_13 = r3;
    WL_OPEN
    u64 nd_16 = heap_alloc(e, cls_fit(2));
    e.mem[nd_16 + 0] = v_26;
    e.mem[nd_16 + 1] = n_8;
    r0 = h_7;
    r1 = term_clo(FID_MAIN_C339, nd_16);
    r2 = x_13;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C339)
  {
    Term v_27 = r0;
    Term n_9 = r1;
    Term x_14 = r2;
    WL_OPEN
    u64 nd_17 = heap_alloc(e, cls_fit(3));
    e.mem[nd_17 + 0] = v_27;
    e.mem[nd_17 + 1] = n_9;
    e.mem[nd_17 + 2] = x_14;
    r0 = term_clo(FID_MAIN_C340, nd_17);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C340)
  {
    Term v_28 = r0;
    Term n_10 = r1;
    Term x_15 = r2;
    Term x_16 = r3;
    WL_OPEN
    u64 nd_18 = heap_alloc(e, cls_fit(3));
    e.mem[nd_18 + 0] = v_28;
    e.mem[nd_18 + 1] = n_10;
    e.mem[nd_18 + 2] = x_15;
    r0 = term_clo(FID_IO_NOW, 0);
    r1 = term_clo(FID_MAIN_C341, nd_18);
    r2 = x_16;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C341)
  {
    Term v_29 = r0;
    Term n_11 = r1;
    Term x_17 = r2;
    Term x_18 = r3;
    WL_OPEN
    if (seq) {
      WL_ROOM(3);
      STK(0) = x_18;
      STK(1) = v_29;
      STK(2) = FID_MAIN_K342;
      WL_PUSHN(3);
    } else {
      u64 t_7 = task_node(e, FID_MAIN_K342, WL_CONT, WL_IDX, 1);
      e.mem[t_7 + 0] = x_18;
      e.mem[t_7 + 1] = v_29;
      WL_CONT = term_tsk(FID_MAIN_K342, t_7);
      WL_IDX = 2;
    }
    r0 = nat_chk(e, n_11 + 64ull);
    r1 = 0;
    r2 = n_11;
    r3 = x_17;
    r4 = 0;
    r5 = 0;
    WL_JMP(FID_RUN_ALL);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K342)
  {
    WL_POPN(2);
    Term x_19 = STK(0);
    Term v_30 = STK(1);
    Term h_8 = r0;
    WL_OPEN
    if (seq) {
      WL_ROOM(3);
      STK(0) = x_19;
      STK(1) = v_30;
      STK(2) = FID_MAIN_K343;
      WL_PUSHN(3);
    } else {
      u64 t_8 = task_node(e, FID_MAIN_K343, WL_CONT, WL_IDX, 1);
      e.mem[t_8 + 0] = x_19;
      e.mem[t_8 + 1] = v_30;
      WL_CONT = term_tsk(FID_MAIN_K343, t_8);
      WL_IDX = 2;
    }
    r0 = h_8;
    WL_JMP(FID_LIST_CONCAT);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K343)
  {
    WL_POPN(2);
    Term x_20 = STK(0);
    Term v_31 = STK(1);
    Term flat_0 = r0;
    WL_OPEN
    u64 nd_19 = heap_alloc(e, cls_fit(3));
    e.mem[nd_19 + 0] = x_20;
    e.mem[nd_19 + 1] = v_31;
    e.mem[nd_19 + 2] = flat_0;
    r0 = term_clo(FID_MAIN_C344, nd_19);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C344)
  {
    Term x_21 = r0;
    Term v_32 = r1;
    Term flat_1 = r2;
    Term x_22 = r3;
    WL_OPEN
    u64 nd_20 = heap_alloc(e, cls_fit(3));
    e.mem[nd_20 + 0] = x_21;
    e.mem[nd_20 + 1] = v_32;
    e.mem[nd_20 + 2] = flat_1;
    r0 = term_clo(FID_IO_NOW, 0);
    r1 = term_clo(FID_MAIN_C345, nd_20);
    r2 = x_22;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C345)
  {
    Term x_23 = r0;
    Term v_33 = r1;
    Term flat_2 = r2;
    Term x_24 = r3;
    WL_OPEN
    if (seq) {
      WL_ROOM(3);
      STK(0) = v_33;
      STK(1) = flat_2;
      STK(2) = FID_MAIN_K346;
      WL_PUSHN(3);
    } else {
      u64 t_9 = task_node(e, FID_MAIN_K346, WL_CONT, WL_IDX, 1);
      e.mem[t_9 + 0] = v_33;
      e.mem[t_9 + 1] = flat_2;
      WL_CONT = term_tsk(FID_MAIN_K346, t_9);
      WL_IDX = 2;
    }
    r0 = (x_24 < x_23 ? 0 : x_24 - x_23);
    WL_JMP(FID_NAT_SHOW);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K346)
  {
    WL_POPN(2);
    Term v_34 = STK(0);
    Term flat_3 = STK(1);
    Term h_9 = r0;
    WL_OPEN
    if (seq) {
      WL_ROOM(3);
      STK(0) = v_34;
      STK(1) = flat_3;
      STK(2) = FID_MAIN_K347;
      WL_PUSHN(3);
    } else {
      u64 t_10 = task_node(e, FID_MAIN_K347, WL_CONT, WL_IDX, 1);
      e.mem[t_10 + 0] = v_34;
      e.mem[t_10 + 1] = flat_3;
      WL_CONT = term_tsk(FID_MAIN_K347, t_10);
      WL_IDX = 2;
    }
    r0 = term_ctr(CID_SCON, STAT_OFF + 52);
    r1 = h_9;
    WL_JMP(FID_STRING_APPEND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K347)
  {
    WL_POPN(2);
    Term v_35 = STK(0);
    Term flat_4 = STK(1);
    Term h_10 = r0;
    WL_OPEN
    u64 nd_21 = heap_alloc(e, cls_fit(3));
    e.mem[nd_21 + 0] = v_35;
    e.mem[nd_21 + 1] = flat_4;
    e.mem[nd_21 + 2] = h_10;
    r0 = term_clo(FID_MAIN_C348, nd_21);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C348)
  {
    Term v_36 = r0;
    Term flat_5 = r1;
    Term h_11 = r2;
    Term x_25 = r3;
    WL_OPEN
    u64 nd_22 = heap_alloc(e, cls_fit(1));
    e.mem[nd_22 + 0] = h_11;
    u64 nd_23 = heap_alloc(e, cls_fit(2));
    e.mem[nd_23 + 0] = v_36;
    e.mem[nd_23 + 1] = flat_5;
    r0 = term_clo(FID_IO_PRINT, nd_22);
    r1 = term_clo(FID_MAIN_C349, nd_23);
    r2 = x_25;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C349)
  {
    Term v_37 = r0;
    Term flat_6 = r1;
    Term x_26 = r2;
    WL_OPEN
    if (seq) {
      WL_ROOM(2);
      STK(0) = flat_6;
      STK(1) = FID_MAIN_K350;
      WL_PUSHN(2);
    } else {
      u64 t_11 = task_node(e, FID_MAIN_K350, WL_CONT, WL_IDX, 1);
      e.mem[t_11 + 0] = flat_6;
      WL_CONT = term_tsk(FID_MAIN_K350, t_11);
      WL_IDX = 1;
    }
    r0 = v_37;
    r1 = term_ctr(CID_SCON, STAT_OFF + 54);
    WL_JMP(FID_STRING_APPEND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_K350)
  {
    WL_POPN(1);
    Term flat_7 = STK(0);
    Term h_12 = r0;
    WL_OPEN
    u64 nd_24 = heap_alloc(e, cls_fit(2));
    e.mem[nd_24 + 0] = flat_7;
    e.mem[nd_24 + 1] = h_12;
    r0 = term_clo(FID_MAIN_C351, nd_24);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C351)
  {
    Term flat_8 = r0;
    Term h_13 = r1;
    Term x_27 = r2;
    WL_OPEN
    Term v_38 = 0;
    u64 nd_25 = heap_alloc(e, cls_fit(2));
    e.mem[nd_25 + 0] = h_13;
    e.mem[nd_25 + 1] = term_ctr(CID_SCON, STAT_OFF + 56);
    Term v_39 = 0;
    Term o_13[1];
    if (spin_119(e, o_13, term_clo(FID_FILE_OPEN, nd_25)) == 0) {
      return 0;
    }
    v_39 = o_13[0];
    v_38 = v_39;
    u64 nd_26 = heap_alloc(e, cls_fit(1));
    e.mem[nd_26 + 0] = flat_8;
    r0 = v_38;
    r1 = term_clo(FID_MAIN_C352, nd_26);
    r2 = x_27;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C352)
  {
    Term flat_9 = r0;
    Term x_28 = r1;
    WL_OPEN
    u64 nd_27 = heap_alloc(e, cls_fit(2));
    e.mem[nd_27 + 0] = flat_9;
    e.mem[nd_27 + 1] = x_28;
    r0 = term_clo(FID_MAIN_C353, nd_27);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C353)
  {
    Term flat_10 = r0;
    Term x_29 = r1;
    Term x_30 = r2;
    WL_OPEN
    Term v_40 = 0;
    Term v_41 = 0;
    Term o_14[1];
    if (spin_126(e, o_14, flat_10, term_pak(CID_NIL, 0)) == 0) {
      return 0;
    }
    v_41 = o_14[0];
    v_40 = v_41;
    u64 nd_28 = heap_alloc(e, cls_fit(2));
    e.mem[nd_28 + 0] = x_29;
    e.mem[nd_28 + 1] = v_40;
    r0 = term_clo(FID_FILE_WRITE_BYTES, nd_28);
    r1 = term_clo(FID_MAIN_C354, 0);
    r2 = x_30;
    WL_JMP(FID_IO_BIND);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_MAIN_C354)
  {
    Term x_31 = r0;
    WL_OPEN
    u64 sp_9 = term_loc(x_31);
    Term f_19 = e.mem[sp_9 + 0];
    Term f_20 = e.mem[sp_9 + 1];
    heap_free(e, cls_fit(2), sp_9);
    u32 o_15 = 0;
    u32 o_16 = 0;
    Term o_17 = 0;
    if (term_aux(f_20) == CID_FAIL) {
      o_15 = 0;
      u64 sp_10 = term_loc(f_20);
      Term f_21 = e.mem[sp_10 + 0];
      heap_free(e, cls_fit(1), sp_10);
      u64 sp_11 = term_loc(f_21);
      Term f_22 = e.mem[sp_11 + 0];
      Term f_23 = e.mem[sp_11 + 1];
      heap_free(e, cls_fit(2), sp_11);
      o_16 = f_22;
      o_17 = f_23;
    } else {
      o_15 = 1;
      u64 sp_12 = term_loc(f_20);
      Term f_24 = e.mem[sp_12 + 0];
      heap_free(e, cls_fit(1), sp_12);
    }
    Term v_42 = 0;
    Term o_18[1];
    if (spin_127(e, o_18, f_19, o_15, o_16, o_17) == 0) {
      return 0;
    }
    v_42 = o_18[0];
    r0 = v_42;
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_ARGS)
  {
    Term k_0 = r0;
    WL_OPEN
    u64 nd_29 = heap_alloc(e, cls_fit(1));
    e.mem[nd_29 + 0] = k_0;
    r0 = term_ctr(CID_IO_ARGS, nd_29);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_FILE_OPEN)
  {
    Term path_0 = r0;
    Term mode_0 = r1;
    Term k_1 = r2;
    WL_OPEN
    u64 nd_30 = heap_alloc(e, cls_fit(3));
    e.mem[nd_30 + 0] = path_0;
    e.mem[nd_30 + 1] = mode_0;
    e.mem[nd_30 + 2] = k_1;
    r0 = term_ctr(CID_FILE_OPEN, nd_30);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_FILE_SIZE)
  {
    Term file_0 = r0;
    Term k_2 = r1;
    WL_OPEN
    u64 nd_31 = heap_alloc(e, cls_fit(2));
    e.mem[nd_31 + 0] = file_0;
    e.mem[nd_31 + 1] = k_2;
    r0 = term_ctr(CID_FILE_SIZE, nd_31);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_FILE_READ)
  {
    Term file_1 = r0;
    Term max_0 = r1;
    Term k_3 = r2;
    WL_OPEN
    u64 nd_32 = heap_alloc(e, cls_fit(3));
    e.mem[nd_32 + 0] = file_1;
    e.mem[nd_32 + 1] = max_0;
    e.mem[nd_32 + 2] = k_3;
    r0 = term_ctr(CID_FILE_READ, nd_32);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_FILE_CLOSE)
  {
    Term file_2 = r0;
    Term k_4 = r1;
    WL_OPEN
    u64 nd_33 = heap_alloc(e, cls_fit(2));
    e.mem[nd_33 + 0] = file_2;
    e.mem[nd_33 + 1] = k_4;
    r0 = term_ctr(CID_FILE_CLOSE, nd_33);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_PRINT)
  {
    Term text_0 = r0;
    Term k_5 = r1;
    WL_OPEN
    u64 nd_34 = heap_alloc(e, cls_fit(2));
    e.mem[nd_34 + 0] = text_0;
    e.mem[nd_34 + 1] = k_5;
    r0 = term_ctr(CID_IO_PRINT, nd_34);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_IO_NOW)
  {
    Term k_6 = r0;
    WL_OPEN
    u64 nd_35 = heap_alloc(e, cls_fit(1));
    e.mem[nd_35 + 0] = k_6;
    r0 = term_ctr(CID_IO_NOW, nd_35);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_FILE_WRITE_BYTES)
  {
    Term file_3 = r0;
    Term data_0 = r1;
    Term k_7 = r2;
    WL_OPEN
    u64 nd_36 = heap_alloc(e, cls_fit(3));
    e.mem[nd_36 + 0] = file_3;
    e.mem[nd_36 + 1] = data_0;
    e.mem[nd_36 + 2] = k_7;
    r0 = term_ctr(CID_FILE_WRITE_BYTES, nd_36);
    WL_RETN(1);
  }}
#endif

#if !DEVICE
  WL_CASE(FID_FILE_READ_BYTES)
  {
    Term file_4 = r0;
    Term max_1 = r1;
    Term k_8 = r2;
    WL_OPEN
    u64 nd_37 = heap_alloc(e, cls_fit(3));
    e.mem[nd_37 + 0] = file_4;
    e.mem[nd_37 + 1] = max_1;
    e.mem[nd_37 + 2] = k_8;
    r0 = term_ctr(CID_FILE_READ_BYTES, nd_37);
    WL_RETN(1);
  }}
#endif

// A task enters through its words: a continuation's results ride r0.. and
// its parameters the stack; any other segment's parameters ride r0...
  WL_CASE(FID_ENTER)
  {
    Term t = r0;
    WL_OPEN
    Fid f   = (u32)term_aux(t);
    Loc a   = term_loc(t);
    u32 war = fid_arity(f);
    WL_FRAME(t)
    if (fid_seqk(f)) {
      u32 rw = fid_resw(f);
      WL_LOAD(a + war - rw, rw)
      WL_ARGS(a, war - rw + 1)
    } else {
      WL_LOAD(a, war)
    }
    heap_free(e, cls_fit(war + 2), a);
    WL_DYN(f);
  }}

  WL_CASE(FID_IO_EMIT)
  {
    Term x = r0;
    WL_OPEN
    Loc l = heap_alloc(e, 0);
    e.mem[l] = x;
    r0 = term_ctr(CID_EMIT, l);
    WL_RETN(1);
  }}

  WL_CASE(FID_CLO_APPLY)
  {
    Term fun = r0;
    Term arg = r1;
    WL_OPEN
    Fid f    = (Fid)term_aux(fun);
    u32 war  = fid_arity(f) - 1;
    Loc a    = term_loc(fun);
    WL_LOAD(a, war)
    spare_free(e, cls_fit(war), a);
    WL_LAST(arg)
    WL_DYN(f);
  }}

  WL_CASE(FID_EXIT)
  {
    u32  n = rn;
    Term rv[WL_RESW];
    WL_SAVE(rv)
    WL_OPEN
    if (err_seen(e.mem)) {
      return 0;
    }
    sp -= 2 * LANE_STEP;
    Term cont = STK(0);
    u32  idx  = (u32)STK(1);
    if (cont != TERM_HOLE && fid_seqk((u32)term_aux(cont))) {
      Fid wf = (u32)term_aux(cont);
      Loc wa = term_loc(cont);
      u32 wn = fid_arity(wf);
      WL_FRAME(cont)
      WL_ARGS(wa, wn - n + 1)
      heap_free(e, cls_fit(wn + 2), wa);
      WL_TAKE(rv)
      WL_DYN(wf);
    }
    return task_deliver(e.mem, cont, idx, rv, n);
  }}

#if DEVICE
  default: {
    err_post(e.mem, ERR_FIDS);
    return 0;
  }
  }
  }
}
#endif

// Monk
// ====

// One turn on a ring: its head task below put0 runs (a growing lane skips
// a fork-free one). The host grows a row ring by ring and works a ring
// until it drains; a device lane does both.
INLINE u32 monk_step(Env e, Stk stk, Ring rg, u32 put0, bool seq, u32 base,
  u32 stride, Cur cur) {
  Corpus   H   = e.mem;
  DEV u32* get = ring_get(H, rg);
  if (*get == put0) {
    return 0;
  }
  DEV u32* lo = (DEV u32*)ring_slot(H, rg, *get);
  u32      hi = a32_load_acq(lo + 1);
  Term     t  = (((u64)hi << 32) | a32_load(lo)) & ~RFC_BIT;
  if ((hi >> 31) != ring_lap(*get) || (!seq && fid_nofk((u32)term_aux(t)))) {
    return 0;
  }
  a32_store(get, *get + 1);
  u32 spin = 0;
  for (;;) {
    Reply r = work_loop(e, stk, t, seq);
    if (r == 0) {
      return 2;
    }
    if ((u32)H[task_tail(r) + 1] == 0) {
      if (err_spun(H, &spin)) {
        return 2;
      }
      if (stride != 0 && fid_nofk((u32)term_aux(r))) {
        ring_push(H, ring_pick(base, stride, cur), r);
        return 2;
      }
      t      = r;
      seq    = false;
      stride = 0;
      continue;
    }
    task_deal(H, r, base, stride, cur);
    return 1;
  }
}

// Dev
// ===

// TG_HOLD words of threadgroup memory (lane 0's write keeps them) hold
// one group per Apple core: without them bitonic runs 1.35x, kmeans
// 1.19x, matmul 1.13x. A grow pass runs at most CUBE_T rounds, so a group
// that never fills still cuts at a kernel end.

#if DEVICE

INLINE void dev_cut(Env e) {
  if (err_seen(e.mem)) {
    return;
  }
  for (Cls c = 0; c < NCLS_ALL; c += 1) {
    u64 gen = (u64)KEEP(c) << c;
    while (ALC_LEN(e, c) >= gen) {
      Loc head = ALC_AT(e, c);
      Loc tail = head;
      for (u32 i = KEEP(c); --i;) {
        tail = e.mem[tail];
      }
      ALC_AT(e, c)    = e.mem[tail];
      ALC_LEN(e, c)  -= gen;
      e.mem[tail]     = 0;
      bank_push(e.mem, c, head);
    }
  }
}

// Pass 2, one group: each bank's [top, wr) slides onto rd, CUBE_T entries
// a step (loads, barrier, stores: rd <= top), off the host's pages.
INLINE void bank_pack(Corpus H, u32 lane) {
  for (Cls c = 0; c < NCLS_ALL; c += 1) {
    DEV Bank* b  = bank_at(H, c);
    u32       rd = b->rd;
    u32       n  = b->wr - b->top;
    for (u32 i = 0; i < n; i += CUBE_T) {
      Term v = i + lane < n ? H[b->off + b->top + i + lane] : 0;
      BAR();
      if (i + lane < n) {
        H[b->off + rd + i + lane] = v;
      }
    }
    BAR();
    if (lane == 0) {
      b->rd = b->wr = b->top = rd + n;
    }
  }
}

// One kernel, one pipeline: pass 0 grows the frontier (a task a lane a
// turn, votes between barriers), pass 1 works it (a lane drains its
// ring), pass 2 packs the banks; one call of monk_step, so the program
// compiles once.
#ifdef __METAL_VERSION__
kernel void bend_dev(Corpus H [[buffer(0)]], constant u32& pass [[buffer(1)]],
  threadgroup volatile u64* hold [[threadgroup(0)]],
  u32 grids [[threadgroups_per_grid]],
  u32 row [[threadgroup_position_in_grid]],
  u32 lane [[thread_position_in_threadgroup]]) {
#else
extern "C" __global__ void bend_dev(Corpus H, u32 pass) {
  extern __shared__ volatile u64 hold[];
  u32 grids = gridDim.x;
  u32 row   = blockIdx.x;
  u32 lane  = threadIdx.x;
#endif
  if (pass == 2) {
    bank_pack(H, lane);
    return;
  }
  u32  stride = grids == 1 ? CUBE_G : 1;
  u32  me     = row * CUBE_T + stride * lane;
  Ring rg     = pass ? ring_flip(me) : me;
  Env  e      = { H, H + ALC_OFF + me };
  Stk  stk    = (Stk)(H + STAK_OFF + me);
  if (lane == 0) {
    hold[0] = 0;
  }
  GA32 tg_cur, tg_grew, tg_has;
  g32_ini(&tg_cur);
  g32_ini(&tg_grew);
  g32_ini(&tg_has);
  BAR();
  u32 put0      = a32_load(ring_put(H, rg));
  u32 seen_has  = 0;
  u32 seen_grew = 0;
  for (u32 turn = 0; pass || turn < CUBE_T; turn += 1) {
    if (pass) {
      if (*ring_get(H, rg) == put0 || err_seen(H)) {
        break;
      }
    } else {
      put0 = a32_load(ring_put(H, rg));
      u32 vote = put0 != a32_load(ring_get(H, rg));
      if (lane == 0 && (err_seen(H) || root_done(H))) {
        vote = CUBE_T;
      }
      g32_add(&tg_has, vote);
      BAR();
      u32 has = g32_get(&tg_has);
      if (has - seen_has >= CUBE_T) {
        break;
      }
      seen_has = has;
    }
    u32 ran = monk_step(e, stk, rg, put0, pass, pass ? rg : row * CUBE_T,
      pass ? 0 : stride, &tg_cur);
    if (!pass) {
      if (ran == 1) {
        g32_add(&tg_grew, 1);
      }
      BARD();
      u32 grew = g32_get(&tg_grew);
      if (grew == seen_grew) {
        break;
      }
      seen_grew = grew;
    }
  }
  dev_cut(e);
}

#endif

// Window
// ======

// The Linux kit's fill, the Mac's window_msl in the runtime's dialect:
// a ! build carries window_dev in its cubin, a host build walks the
// pixels itself. An Image is a quadtree over 2^k x 2^k: a Qua at level
// i splits its square in four (tl, tr, bl, br), a Qua under the pixels
// follows tl, a Pix is 0xRRGGBB.
#if defined(__linux__) || defined(__CUDACC_RTC__)

INLINE u32 window_pix(Corpus H, Term t, u32 k, u32 x, u32 y) {
  for (u32 i = k; term_tag(t) == TAG_CTR;) {
    u32 j = 0;
    if (i > 0) {
      i -= 1;
      j = ((y >> i) & 1) * 2 + ((x >> i) & 1);
    }
    Loc l = term_rfc(t) ? H[term_loc(t)] >> 24 : term_loc(t);
    t = H[l + j];
  }
  return (u32)term_loc(t) & 0xFFFFFF;
}

#ifdef __CUDACC_RTC__
extern "C" __global__ void window_dev(Corpus H, Term root, u32 w, u32 h,
  u32 k, u32* out) {
  u32 x = blockIdx.x * blockDim.x + threadIdx.x;
  u32 y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x < w && y < h) {
    out[y * w + x] = window_pix(H, root, k, x, y);
  }
}
#endif

#endif

#if !DEVICE

// Row
// ===

static void row_grow(Env e, Stk stk, u32 base, u32 stride, u32 want) {
  Corpus H = e.mem;
  u32 cur = 0;
  for (;;) {
    u32 put0[CUBE_T];
    u32 has = 0;
    for (u32 i = 0; i < CUBE_T; i += 1) {
      put0[i] = *ring_put(H, base + i * stride);
      has += put0[i] != *ring_get(H, base + i * stride);
    }
    if (root_done(H) || has >= want) {
      return;
    }
    u32 grew = 0;
    u32 ran  = 0;
    for (u32 i = 0; i < CUBE_T && ran != 2; i += 1) {
      ran   = monk_step(e, stk, base + i * stride, put0[i], false, base,
        stride, &cur);
      grew += ran == 1;
    }
    if (grew == 0) {
      return;
    }
  }
}

// Pool
// ====

static void* pool_try(u64 bytes) {
  return mmap(NULL, bytes, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
}

static void* pool_mmap(u64 bytes) {
  void* p = pool_try(bytes);
  if (p == MAP_FAILED) {
    err_fail("reservation failed");
  }
  return p;
}

static Term* pool_stack(void) {
  u64   len = 1ull << 31;
  char* p   = pool_mmap(len + 16384 + SIGSTKSZ);
  if (mprotect(p + len, 16384, PROT_NONE) != 0) {
    err_fail("stack guard failed");
  }
  stack_t ss = { .ss_sp = p + len + 16384, .ss_size = SIGSTKSZ };
  sigaltstack(&ss, NULL);
  struct sigaction sa = { .sa_handler = err_trap, .sa_flags = SA_ONSTACK };
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
  return (Term*)p;
}

static void* pool_work(void* arg) {
  Term* stk  = pool_stack();
  u64   seen = 0;
  for (;;) {
    pthread_mutex_lock(&pool_lock);
    while (atomic_load_explicit(&pool_tick, memory_order_acquire) == seen) {
      pthread_cond_wait(&pool_wake, &pool_lock);
    }
    pthread_mutex_unlock(&pool_lock);
    seen = atomic_load_explicit(&pool_tick, memory_order_acquire);
    Env e = { CORPUS, ALC[1 + (u32)(uintptr_t)arg] };
    for (;;) {
      u32 r = atomic_fetch_add_explicit(&pool_row, 1, memory_order_relaxed);
      if (r >= (pool_grow ? CUBE_G : LANES / LINE)) {
        break;
      }
      if (pool_grow) {
        row_grow(e, stk, r * CUBE_T, 1, CUBE_T);
      } else {
        for (u32 i = 0; i < LINE; i += 1) {
          Ring rg   = r * LINE + i;
          u32  put0 = a32_load(ring_put(e.mem, rg));
          while (*ring_get(e.mem, rg) != put0 && !err_seen(e.mem)) {
            monk_step(e, stk, rg, put0, true, rg, 0, NULL);
          }
        }
      }
    }
    u32 done = atomic_fetch_add_explicit(&pool_done, 1, memory_order_release);
    if (done + 1 == pool_size) {
      pthread_mutex_lock(&pool_lock);
      pthread_cond_broadcast(&pool_wake);
      pthread_mutex_unlock(&pool_lock);
    }
  }
}

OUTLINE void pool_open(void) {
  static bool up;
  if (up) {
    return;
  }
  up = true;
  for (u32 w = 0; w < pool_size; w += 1) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, pool_work, (void*)(uintptr_t)w)) {
      err_fail("pthread_create");
    }
  }
}

// The CPUs this process may use: affinity mask under the cgroup quota
static int cpu_read(const char* path, long* a, long* b) {
  FILE* f = fopen(path, "r");
  int   n = f == NULL ? 0 : fscanf(f, "%ld %ld", a, b);
  if (f != NULL) {
    fclose(f);
  }
  return n;
}

static long cpu_count(void) {
  long n = sysconf(_SC_NPROCESSORS_ONLN);
#ifdef __linux__
  cpu_set_t set;
  if (sched_getaffinity(0, sizeof set, &set) == 0) {
    n = CPU_COUNT(&set);
  }
  long q = 0;
  long p = 0;
  if (cpu_read("/sys/fs/cgroup/cpu.max", &q, &p) != 2) {
    cpu_read("/sys/fs/cgroup/cpu/cpu.cfs_quota_us", &q, &p);
    cpu_read("/sys/fs/cgroup/cpu/cpu.cfs_period_us", &p, &p);
  }
  if (q > 0 && p > 0 && (q + p - 1) / p < n) {
    n = (q + p - 1) / p;
  }
#endif
  return n;
}

OUTLINE void pool_turn(bool grow) {
  pool_grow = grow;
  atomic_store_explicit(&pool_row, 0, memory_order_relaxed);
  atomic_store_explicit(&pool_done, 0, memory_order_relaxed);
  pthread_mutex_lock(&pool_lock);
  atomic_fetch_add_explicit(&pool_tick, 1, memory_order_release);
  pthread_cond_broadcast(&pool_wake);
  while (atomic_load_explicit(&pool_done, memory_order_acquire) < pool_size) {
    pthread_cond_wait(&pool_wake, &pool_lock);
  }
  pthread_mutex_unlock(&pool_lock);
}

// Gpu
// ===

// gpu_make compiles the device program and, given a path, writes it as
// <binary>.gpu (--gpu-build, run by bend -o): Metal's binary archive
// of the pipeline (keyed by the compiled function, so a wrong file
// misses), CUDA's cubin behind a hash of the text. A launch loads it,
// else notes and compiles (Metal's OS cache keeps that pipeline; CUDA
// writes the file).

static const char* gpu_path(void) {
  static char path[4096];
  u32 n = sizeof path - 8;
#ifdef __APPLE__
  _NSGetExecutablePath(path, &n);
#else
  path[readlink("/proc/self/exe", path, n)] = 0;
#endif
  return strcat(path, ".gpu");
}

static void gpu_note(const char* path) {
  fprintf(stderr, "bend: compiling the GPU program (%s is missing or"
    " stale)\n", path);
}

#if !BEND_CUDA
#define gpu_map pool_mmap
#endif

#if BEND_METAL || BEND_CUDA

static void gpu_kernel(u32 pass, u32 groups);

static void gpu_run(u32 f) {
  if (f < CUBE_T) {
    gpu_kernel(0, 1);
  }
  if (f < LANES) {
    gpu_kernel(0, CUBE_G);
  }
  gpu_kernel(1, CUBE_G);
  gpu_kernel(2, 1);
}

#endif

#if BEND_METAL

static bool gpu_probe(void) {
  return (gpu_dev = MTLCreateSystemDefaultDevice()) != nil;
}

static MTLComputePipelineDescriptor* gpu_desc(void) {
  NSError* err = nil;
  MTLCompileOptions* opts = [MTLCompileOptions new];
  opts.mathMode = MTLMathModeSafe;
  opts.preprocessorMacros = @{ @"CUBE_LOG": @(CUBE_LOG) };
  id<MTLLibrary> lib = [gpu_dev newLibraryWithSource:@(BEND_SRC) options:opts
    error:&err];
  if (!lib) {
    err_fail([[err localizedDescription] UTF8String]);
  }
  MTLComputePipelineDescriptor* d = [MTLComputePipelineDescriptor new];
  d.computeFunction = [lib newFunctionWithName:@"bend_dev"];
  return d;
}

static bool gpu_make(const char* path) {
  NSError* err = nil;
  id<MTLBinaryArchive> ar = [gpu_dev
    newBinaryArchiveWithDescriptor:[MTLBinaryArchiveDescriptor new] error:&err];
  if (![ar addComputePipelineFunctionsWithDescriptor:gpu_desc() error:&err]) {
    err_fail([[err localizedDescription] UTF8String]);
  }
  return [ar serializeToURL:[NSURL fileURLWithPath:@(path)] error:&err];
}

static id<MTLComputePipelineState> gpu_pipe(MTLComputePipelineDescriptor* d,
  id<MTLBinaryArchive> ar) {
  NSError* err = nil;
  d.binaryArchives = ar ? @[ar] : @[];
  id<MTLComputePipelineState> pso = [gpu_dev
    newComputePipelineStateWithDescriptor:d
    options:ar ? MTLPipelineOptionFailOnBinaryArchiveMiss : 0 reflection:nil
    error:&err];
  if (!pso && !ar) {
    err_fail([[err localizedDescription] UTF8String]);
  }
  return pso;
}

static u64 gpu_span(void) {
  u64 span = [gpu_dev recommendedMaxWorkingSetSize];
  u64 most = [gpu_dev maxBufferLength];
  span = span < most ? span : most;
  return span < (2ull << 30) ? span : 2ull << 30;
}

static void gpu_load(u64 bytes) {
  gpu_buf = [gpu_dev newBufferWithBytesNoCopy:CORPUS length:bytes
    options:MTLResourceStorageModeShared
      | MTLResourceHazardTrackingModeUntracked deallocator:nil];
  if (!gpu_buf) {
    err_fail("the GPU span is more than the device has");
  }
  @autoreleasepool {
    gpu_que = [gpu_dev newCommandQueue];
    const char* path = gpu_path();
    MTLBinaryArchiveDescriptor* ad = [MTLBinaryArchiveDescriptor new];
    ad.url = [NSURL fileURLWithPath:@(path)];
    MTLComputePipelineDescriptor* d = gpu_desc();
    id<MTLBinaryArchive> ar = [gpu_dev newBinaryArchiveWithDescriptor:ad
      error:nil];
    gpu_pso = ar ? gpu_pipe(d, ar) : nil;
    if (!gpu_pso) {
      gpu_note(path);
      gpu_pso = gpu_pipe(d, nil);
    }
  }
}

static void gpu_kernel(u32 pass, u32 groups) {
  [gpu_enc setComputePipelineState:gpu_pso];
  [gpu_enc setBuffer:gpu_buf offset:0 atIndex:0];
  [gpu_enc setBytes:&pass length:sizeof pass atIndex:1];
  [gpu_enc setThreadgroupMemoryLength:TG_HOLD * 8 atIndex:0];
  [gpu_enc dispatchThreadgroups:MTLSizeMake(groups, 1, 1)
    threadsPerThreadgroup:MTLSizeMake(CUBE_T, 1, 1)];
  [gpu_enc memoryBarrierWithScope:MTLBarrierScopeBuffers];
}

static void gpu_pass(u32 f) {
  @autoreleasepool {
    id<MTLCommandBuffer> cb = [gpu_que commandBuffer];
    gpu_enc = [cb computeCommandEncoder];
    gpu_run(f);
    [gpu_enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    if ([cb error]) {
      err_fail([[[cb error] localizedDescription] UTF8String]);
    }
  }
}

#elif BEND_CUDA

// the bag from the device: a group of 128 lanes per 64 KB of L2, a power of
// two from 16 to 128 groups. Apple keeps the 128 the bag was tuned on: on an
// M4 (10 cores) 32 groups ran bitonic 1.85 -> 1.29 s, but the light one-pass
// benches 1.25x, their lanes four times fewer.
static void gpu_shape(int units) {
  CUBE_LOG = 31 - CLZ(units < 16 ? 16 : units > 128 ? 128 : units);
}

static bool gpu_probe(void) {
  int       managed = 0;
  CUcontext ctx;
  // one stream, so one hardware queue: the default 8 each cost a channel
  // at context creation and teardown, about half of the startup
  setenv("CUDA_DEVICE_MAX_CONNECTIONS", "1", 0);
  if (cuInit(0) == CUDA_SUCCESS && cuDeviceGet(&gpu_dev, 0) == CUDA_SUCCESS) {
    cuDeviceGetAttribute(&managed,
      CU_DEVICE_ATTRIBUTE_CONCURRENT_MANAGED_ACCESS, gpu_dev);
  }
  int l2 = 1 << 23;
  cuDeviceGetAttribute(&l2, CU_DEVICE_ATTRIBUTE_L2_CACHE_SIZE, gpu_dev);
  gpu_shape(l2 >> 16);
  return managed != 0
    && cuDevicePrimaryCtxRetain(&ctx, gpu_dev) == CUDA_SUCCESS
    && cuCtxSetCurrent(ctx) == CUDA_SUCCESS;
}

static Corpus gpu_map(u64 bytes) {
  CUdeviceptr p = 0;
  if (cuMemAllocManaged(&p, bytes, CU_MEM_ATTACH_GLOBAL) != CUDA_SUCCESS) {
    err_fail("corpus reservation failed");
  }
#if CUDA_VERSION >= 13000
  cuMemAdvise(p, bytes, CU_MEM_ADVISE_SET_PREFERRED_LOCATION,
    (CUmemLocation){ CU_MEM_LOCATION_TYPE_DEVICE, gpu_dev });
#else
  cuMemAdvise(p, bytes, CU_MEM_ADVISE_SET_PREFERRED_LOCATION, gpu_dev);
#endif
  return (Corpus)(uintptr_t)p;
}

static u64 gpu_hash(void) {
  u64 key = 14695981039346656037ull ^ CUBE_LOG;
  for (const char* p = BEND_SRC; *p != 0; p += 1) {
    key = (key ^ (u8)*p) * 1099511628211ull;
  }
  return key;
}

static bool gpu_make(const char* path) {
  int cc[2] = {0, 0};
  cuDeviceGetAttribute(cc,
    CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, gpu_dev);
  cuDeviceGetAttribute(cc + 1,
    CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, gpu_dev);
  char arch[40];
  char bag[24];
  snprintf(arch, sizeof arch, "--gpu-architecture=sm_%d%d", cc[0], cc[1]);
  snprintf(bag, sizeof bag, "-DCUBE_LOG=%u", CUBE_LOG);
  const char* opts[] = { arch, bag, "--fmad=false", "-default-device" };
  nvrtcProgram prog;
  if (nvrtcCreateProgram(&prog, BEND_SRC, "bend.cu", 0, NULL, NULL)
    != NVRTC_SUCCESS) {
    err_fail("cannot compile the CUDA library");
  }
  if (nvrtcCompileProgram(prog, 4, opts) != NVRTC_SUCCESS) {
    size_t n = 0;
    nvrtcGetProgramLogSize(prog, &n);
    char* log = calloc(n + 1, 1);
    if (log != NULL && nvrtcGetProgramLog(prog, log) == NVRTC_SUCCESS) {
      fprintf(stderr, "%s\n", log);
    }
    err_fail("cannot compile the CUDA library");
  }
  size_t len = 0;
  nvrtcGetCUBINSize(prog, &len);
  char* bin = malloc(len);
  if (bin == NULL || nvrtcGetCUBIN(prog, bin) != NVRTC_SUCCESS) {
    err_fail("cannot load the CUDA library");
  }
  nvrtcDestroyProgram(&prog);
  u64   key = gpu_hash();
  FILE* out = path == NULL ? NULL : fopen(path, "wb");
  bool  ok  = out != NULL && fwrite(&key, 8, 1, out) == 1
    && fwrite(bin, 1, len, out) == len && fclose(out) == 0;
  if (cuModuleLoadData(&gpu_lib, bin) != CUDA_SUCCESS) {
    err_fail("cannot load the CUDA library");
  }
  free(bin);
  return path == NULL || ok;
}

static u64 gpu_span(void) {
  size_t span = 0;
  cuDeviceTotalMem(&span, gpu_dev);
  return span;
}

static void gpu_load(u64 bytes) {
  const char* path = gpu_path();
  int         fd   = open(path, O_RDONLY);
  struct stat st   = { 0 };
  u64         key  = 0;
  char*       bin  = fd < 0 || fstat(fd, &st) != 0 || st.st_size <= 8 ? NULL
    : mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (bin != NULL && bin != MAP_FAILED) {
    memcpy(&key, bin, 8);
  }
  if (key != gpu_hash()
    || cuModuleLoadData(&gpu_lib, bin + 8) != CUDA_SUCCESS) {
    gpu_note(path);
    gpu_make(path);
  }
  if (cuModuleGetFunction(&gpu_pso, gpu_lib, "bend_dev") != CUDA_SUCCESS) {
    err_fail("cannot load the GPU program");
  }
}

static void gpu_kernel(u32 pass, u32 groups) {
  void* args[] = { &CORPUS, &pass };
  if (cuLaunchKernel(gpu_pso, groups, 1, 1, CUBE_T, 1, 1, TG_HOLD * 8, NULL,
    args, NULL) != CUDA_SUCCESS) {
    err_fail("device launch failed");
  }
}

static void gpu_pass(u32 f) {
  gpu_run(f);
  if (cuCtxSynchronize() != CUDA_SUCCESS) {
    err_fail("device fault");
  }
}

#else

#define gpu_probe() false
#define gpu_make(p) true
#define gpu_span()  0
#define gpu_load(b)
#define gpu_pass(f)

#endif

// Cube
// ====

static void cube_run(Corpus H, bool gpu) {
  for (;;) {
    u32 f = a32_load(a32_at(H, H_CURSOR));
    a32_store(a32_at(H, H_CURSOR), 0);
    if (root_done(H)) {
      return;
    }
    if (f == 0) {
      err_fail("frontier drained without a result");
    }
    if (gpu) {
      gpu_pass(f);
    } else {
      // Under a unit (CUBE_T / LINE a row) per thread, the column grows to
      // the rows that give one; no more: each touches a page of every plane.
      if (f * (CUBE_T / LINE) < pool_size) {
        row_grow((Env){ H, ALC[0] }, io_stk, 0, CUBE_G,
          (pool_size + CUBE_T / LINE - 1) / (CUBE_T / LINE));
      }
      if (f < CUBE) {
        pool_turn(true);
      }
      pool_turn(false);
    }
    u32 ec = a32_load(a32_at(H, H_ERROR_CODE));
    if (ec != 0) {
      err_post(H, ec);
    }
  }
}

// Corpus
// ======

static Corpus corpus_setup(bool gpu, long threads, u64 bytes) {
  io_gpu     = gpu;
  KEEP_WORDS = gpu ? CHUNK : CAP_WORDS;
  u64 dflt   = gpu ? gpu_span() : 1ull << 43;
  u64 size   = (gpu && bytes != 0 ? bytes : dflt) & ~16383ull;
  // The cores reserve the whole Loc space (8 TiB, MAP_NORESERVE). A kernel
  // with fewer address bits (39-bit arm64, Sv39) or a ulimit -v gets the
  // largest power of two that fits, down to 8 GiB.
  CORPUS = gpu ? gpu_map(size) : pool_try(size);
  while (CORPUS == MAP_FAILED && size > 1ull << 33) {
    CORPUS = pool_try(size /= 2);
  }
  if (CORPUS == MAP_FAILED) {
    err_fail("reservation failed");
  }
  u64 span = size / 8;
  u64 cap  = span > HEAP_OFF ? (span - HEAP_OFF) / (PAGE_LEN + 10) : 0;
  if (cap <= CUBE) {
    err_fail("the GPU span is under the rings, stacks and a page per lane");
  }
  cap = cap < ~0u ? cap : ~0u - 1;
  Corpus H  = CORPUS;
#if BEND_CUDA
  if (gpu) {
    cuMemsetD8((CUdeviceptr)(uintptr_t)H, 0, STAK_OFF * 8);
    cuCtxSynchronize();
  }
#endif
  memcpy(H + STAT_OFF, STAT_IMG, STAT_LEN * sizeof(u64));
  u64    at = HEAP_OFF + (cap << PAGE_BITS);
  for (u32 c = 0; c < NCLS_ALL; c += 1) {
    bank_at(H, c)->off = at;
    at += 2 * (cap >> ((c < NCLS ? NCLS : c) - PAGE_BITS));
  }
  a32_store(a32_at(H, H_BUMP), 1);
  a32_store(a32_at(H, H_CAP), (u32)cap);
  if (gpu) {
    gpu_load(size);
  }
  pool_size = threads < 1 ? 1 : threads < CUBE_T ? threads : CUBE_T;
  return H;
}

OUTLINE Term corpus_eval(Corpus H, Term t) {
  Env  e = { H, ALC[0] };
  Term rv[WL_RESW];
  for (;;) {
    Reply r = work_loop(e, io_stk, t, !BANGS
      && (pool_size == 1 || fid_nofk((u32)term_aux(t))));
    if (r == 0) {
      if (root_done(H)) {
        break;
      }
      err_fail("solo delivery lost");
    }
    if ((u32)H[task_tail(r) + 1] == 0) {
      t = r;
      if (io_gpu && fid_bangs((u32)term_aux(t))) {
        Loc  tl   = task_tail(t);
        Term cont = H[tl];
        u32  idx  = (u32)(H[tl + 1] >> 32) & 0xFFFF;
        H[tl]     = TERM_HOLE;
        a32_store(a32_at(H, H_CURSOR), 1);
        ring_push(H, 0, t);
        cube_run(H, true);
        Term p = task_deliver(H, cont, idx, rv, root_take(H, rv));
        if (root_done(H)) {
          break;
        }
        if (p == 0) {
          err_fail("seam delivery lost");
        }
        t = p;
      }
      continue;
    }
    task_deal(H, r, 0, 0, (Cur)0);
    pool_open();
    cube_run(H, false);
    break;
  }
  root_take(H, rv);
  return rv[0];
}

// Io
// ==

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define IO_READ 1
#define IO_TIME 2
#define IO_PARK TERM_HOLE

// A handle is its host value, a descriptor or a pointer, packed in one
// word (a pointer split over the aux and loc bits). Its type is a law of
// base, opaque and linear: a program cannot forge, copy or reuse one, so
// nothing stands between the value and the host.
#define io_hand(v)   term_make(TAG_PAK, (u64)(v) >> 40, (u64)(v) & LOC_MASK)
#define io_hand_v(t) (((u64)term_aux(t) << 40) | term_loc(t))

struct IoWork;
typedef void (*IoCall)(struct IoWork* w);
typedef Term (*IoPack)(Env e, struct IoWork* w);

// IoWork ::=
//   | IoWork(hand, made, word, size, data, text, code, call, pack)
typedef struct IoWork {
  intptr_t hand;
  intptr_t made;
  u32      word;
  u64      size;
  char*    data;
  char*    text;
  u32      code;
  IoCall   call;
  IoPack   pack;
} IoWork;

typedef Term (*Effect)(Env e, Term* f, IoWork* w);

// IoEff ::=
//   | IoEff(run, ask)
typedef struct {
  Effect run;
  u32    ask;
} IoEff;

static IoEff io_eff_rows[1 << 16];
static u32   io_live;

static u64 io_tick(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}

OUTLINE void* io_mem(void* mem) {
  if (mem == NULL) {
    err_fail("host allocation failed");
  }
  return mem;
}

static int io_sys_addr(const char* host, u32 port, struct sockaddr_in* at) {
  memset(at, 0, sizeof(*at));
  at->sin_family = AF_INET;
  at->sin_port   = htons((uint16_t)port);
  for (const char* p = host; *p != 0; p += 1) {
    bool zero = *p == '0' && p[1] >= '0' && p[1] <= '9';
    if ((p == host || p[-1] == '.') && zero) {
      return -1;
    }
  }
  return port > 65535 || inet_pton(AF_INET, host, &at->sin_addr) != 1
    ? -1 : 0;
}

// The program's arguments (IO.args).
static int    io_argc = 0;
static char** io_argv = NULL;

static void io_eff(u32 cid, Effect run, u32 need) {
  IoEff row = { run, need };
  io_eff_rows[cid] = row;
}

static u64 io_sys_end(IoWork* w, ssize_t n) {
  w->code = n < 0 ? (u32)errno : 0;
  return n < 0 ? 0 : (u64)n;
}

// A computation's activation for its whole life: cont over item is its
// next request; parked, work.word and time are its fd or deadline, evts
// what the fd must be ready for, and work.pack resumes it (io_exec runs
// cont, the request); work leads, so an effect's IoWork* is its activation.
// IoAct ::=
//   | IoAct(work, cont, item, time, evts, next)
typedef struct IoAct {
  IoWork        work;
  Term          cont;
  Term          item;
  u64           time;
  short         evts;
  struct IoAct* next;
} IoAct;

// IoQue ::=
//   | IoQue(head, last)
typedef struct {
  IoAct* head;
  IoAct* last;
} IoQue;

static IoQue io_runs;
static IoQue io_park;
static IoQue io_jobs;

static void io_push(IoQue* q, IoAct* a) {
  a->next = NULL;
  *(q->head == NULL ? &q->head : &q->last->next) = a;
  q->last = a;
}

static IoAct* io_pop(IoQue* q) {
  IoAct* a = q->head;
  q->head  = a->next;
  return a;
}

static void io_spawn(Term m) {
  IoAct* a = io_mem(calloc(1, sizeof(IoAct)));
  a->cont  = m;
  a->item  = term_clo(FID_IO_EMIT, 0);
  io_push(&io_runs, a);
  io_live += 1;
}

// Parks the effect's activation until fd is ready for evts (POLLIN or
// POLLOUT); the loop then calls more on its thread, whose value readies
// the activation, or IO_PARK, a re-park.
static Term io_wait_on(IoWork* w, int fd, short evts, IoPack more) {
  IoAct* a     = (IoAct*)w;
  a->work.word = (u32)fd;
  a->work.pack = more;
  a->time      = 0;
  a->evts      = evts;
  io_push(&io_park, a);
  return IO_PARK;
}

OUTLINE void io_out(FILE* h, const char* data, u64 len) {
  if (fwrite(data, 1, len, h) != len) {
    err_fail("a short write on a standard stream");
  }
}

OUTLINE void io_sync(void) {
  if (fflush(stdout) != 0) {
    err_fail("a short write on a standard stream");
  }
}

// the edge is UTF-8
static u64 io_utf8(char* buf, u64 c) {
  u64 k = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
  for (u64 i = k; i > 1; i -= 1) {
    buf[i - 1] = (char)(0x80 | (c & 0x3F));
    c >>= 6;
  }
  buf[0] = (char)(k == 1 ? c : (0xF00 >> k) | c);
  return k;
}

OUTLINE char* io_cstr(Env e, Term s, u64* len) {
  u64   cap = 64;
  u64   n   = 0;
  char* buf = io_mem(malloc(cap));
  while (term_aux(s) == CID_SCON) {
    Term fb[2];
    spare_free(e, cls_fit(2), ctr_take(e, s, 2, fb));
    if (n + 5 > cap) {
      cap *= 2;
      buf = io_mem(realloc(buf, cap));
    }
    n += io_utf8(buf + n, fb[0]);
    s = fb[1];
  }
  buf[n] = 0;
  *len = n;
  return buf;
}

OUTLINE void io_errs(Env e, Term s) {
  u64   n    = 0;
  char* text = io_cstr(e, s, &n);
  io_sync();
  io_out(stderr, text, n);
  io_out(stderr, "\n", 1);
  free(text);
}

#define io_nul(s, n) (strlen(s) != (n))

#define io_seal(e, t, hot) ((hot) != 0 ? rfc_seal(e, t) : (t))

static Term io_node(Env e, u64 cid, Term a, Term b, int hot) {
  Loc l = heap_alloc(e, 1);
  e.mem[l]     = io_seal(e, a, hot);
  e.mem[l + 1] = io_seal(e, b, hot);
  return term_ctr(cid, l);
}

// io_str decodes UTF-8 as WHATWG does: the lead byte sets the count of
// continuation bytes and the range of the second; a byte that breaks the
// sequence (or the end) yields one U+FFFD and is read again as a lead.
static Term io_str(Env e, const char* p, u64 n) {
  Term s    = term_pak(CID_SNIL, 0);
  Loc  hole = 0;
  u64  c = 0, need = 0, lo = 0x80, hi = 0xBF;
  for (u64 i = 0; i < n || need > 0; i += 1) {
    u64 b = i < n ? (uint8_t)p[i] : 0x100;
    if (need > 0 && (b < lo || b > hi)) {
      need = 0;
      c    = 0xFFFD;
      i   -= 1;
    } else if (need > 0) {
      lo = 0x80;
      hi = 0xBF;
      c  = (c << 6) | (b & 0x3F);
      if (--need > 0) {
        continue;
      }
    } else if (b < 0x80) {
      c = b;
    } else if (b < 0xC2 || b > 0xF4) {
      c = 0xFFFD;
    } else {
      need = b < 0xE0 ? 1 : b < 0xF0 ? 2 : 3;
      lo   = b == 0xE0 ? 0xA0 : b == 0xF0 ? 0x90 : 0x80;
      hi   = b == 0xED ? 0x9F : b == 0xF4 ? 0x8F : 0xBF;
      c    = b & (0x3F >> need);
      continue;
    }
    Loc  l = heap_alloc(e, 1);
    Term t = term_ctr(CID_SCON, l);
    e.mem[l] = c;
    if (hole == 0) {
      s = t;
    } else {
      e.mem[hole] = io_seal(e, t, IO_HOTS & 1);
    }
    hole = l + 1;
  }
  if (hole != 0) {
    e.mem[hole] = io_seal(e, term_pak(CID_SNIL, 0), IO_HOTS & 1);
  }
  return s;
}

#define io_tup(e, a, b) io_node(e, CID_TUPLE, a, b, IO_HOTS & 2)
#define io_done(e, v)   io_box(e, CID_DONE, v, IO_HOTS & 4)

static Term io_box(Env e, u64 cid, Term v, int hot) {
  Loc l = heap_alloc(e, 0);
  e.mem[l] = io_seal(e, v, hot);
  return term_ctr(cid, l);
}

static Term io_fail(Env e, u32 code, const char* text) {
  const char* s = text != NULL ? text : strerror((int)code);
  Term t = io_tup(e, code, io_str(e, s, strlen(s)));
  return io_box(e, CID_FAIL, t, IO_HOTS & 8);
}

static lock           io_gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t io_bell = PTHREAD_COND_INITIALIZER;
static u32            io_busy;
static u32            io_size;
static int            io_wake_fd[2];

static void io_take(Env e) {
  IoAct*  acts[64];
  ssize_t n;
  while ((n = read(io_wake_fd[0], acts, sizeof acts)) > 0) {
    for (u32 i = 0; i < (u32)n / sizeof(IoAct*); i += 1) {
      IoAct* a = acts[i];
      a->item  = a->work.pack(e, &a->work);
      io_push(&io_runs, a);
      io_busy -= 1;
    }
  }
}

static void* io_help(void* arg) {
  for (;;) {
    pthread_mutex_lock(&io_gate);
    while (io_jobs.head == NULL) {
      pthread_cond_wait(&io_bell, &io_gate);
    }
    IoAct* a = io_pop(&io_jobs);
    pthread_mutex_unlock(&io_gate);
    a->work.call(&a->work);
    while (write(io_wake_fd[1], &a, sizeof a) != sizeof a) {
    }
  }
}

// A helper takes the effect's activation: call on its thread, then pack
// on the loop's, whose value readies the activation.
static Term io_work(IoWork* w, IoCall call, IoPack pack) {
  w->call  = call;
  w->pack  = pack;
  io_busy += 1;
  if (io_busy > io_size && io_size < IO_HELP) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, io_help, NULL)) {
      err_fail("pthread_create");
    }
    pthread_detach(tid);
    io_size += 1;
  }
  pthread_mutex_lock(&io_gate);
  io_push(&io_jobs, (IoAct*)w);
  pthread_cond_signal(&io_bell);
  pthread_mutex_unlock(&io_gate);
  return IO_PARK;
}

// Runs the request in cont: the effect takes its fields (the node goes)
// and answers a value, which readies the activation, or IO_PARK, a moved.
static Term io_exec(Env e, IoWork* w) {
  IoAct* a = (IoAct*)w;
  Term   fs[256];
  u32    c = (u32)term_aux(a->cont);
  u32    n = cid_arity(c);
  spare_free(e, cls_fit(n), ctr_take(e, a->cont, n, fs));
  a->cont = fs[n - 1];
  return io_eff_rows[c].run(e, fs, w);
}

static void io_wait(Env e) {
  struct pollfd* fds = io_mem(malloc((io_live + 1) * sizeof *fds));
  u32 n    = 1;
  u64 soon = 0;
  int ms   = -1;
  fds[0].fd     = io_wake_fd[0];
  fds[0].events = POLLIN;
  for (IoAct* a = io_park.head; a != NULL; a = a->next) {
    if (a->time != 0) {
      soon = soon == 0 || a->time < soon ? a->time : soon;
    } else {
      fds[n].fd     = (int)a->work.word;
      fds[n].events = a->evts;
      n += 1;
    }
  }
  if (soon != 0) {
    u64 now = io_tick();
    u64 gap = soon > now ? (soon - now) / 1000000 + 1 : 0;
    ms = gap > 0x7fffffff ? 0x7fffffff : (int)gap;
  }
  io_sync();
  while (poll(fds, n, ms) < 0) {
    if (errno != EINTR) {
      err_fail("the poller failed");
    }
  }
  if (fds[0].revents != 0) {
    io_take(e);
  }
  u64   now  = io_tick();
  u32   i    = 1;
  IoQue todo = io_park;
  io_park.head = NULL;
  io_park.last = NULL;
  while (todo.head != NULL) {
    IoAct* a   = io_pop(&todo);
    bool   due = a->time == 0 ? fds[i].revents != 0 : a->time <= now;
    i += a->time == 0;
    if (!due) {
      io_push(&io_park, a);
      continue;
    }
    Term x = a->work.pack(e, &a->work);
    if (x != IO_PARK) {
      a->item = x;
      io_push(&io_runs, a);
    }
  }
  free(fds);
}

static int f32_text(char* buf, f32 v) {
  int n = 0;
  int p = 0;
  if (v != v) {
    return sprintf(buf, "nan");
  }
  for (; p < 9; p += 1) {
    n = snprintf(buf, 40, "%.*e", p, (double)v);
    if (strtof(buf, NULL) == v) {
      break;
    }
  }
  char* ep = strchr(buf, 'e');
  if (ep == NULL) {
    return n;
  }
  int ex = atoi(ep + 1);
  if (ex >= 21 || ex <= -7) {
    n = (int)(ep - buf) + sprintf(ep, "e%c%d", ex < 0 ? '-' : '+', abs(ex));
  } else if (ex <= p) {
    n = snprintf(buf, 40, "%.*f", p - ex, (double)v);
  } else {
    int s = *buf == '-';
    memmove(buf + s + 1, buf + s + 2, p);
    memset(buf + s + 1 + p, '0', ex - p);
    n = s + 1 + ex;
  }
  return n;
}

static Term f32_show(Env e, Term x) {
  char buf[40];
  return io_str(e, buf, f32_text(buf, f32_unbox(x)));
}

static Term f32_read(Env e, Term s) {
  u64 n = 0;
  char* text = io_cstr(e, s, &n);
  char* end;
  f32 v = strtof(text, &end);
  Term out = n > 0 && (u64)(end - text) == n && strpbrk(text, "xX(") == NULL
    ? io_box(e, CID_SOME, f32_rewrap(v), 0) : term_pak(CID_NONE, 0);
  free(text);
  return out;
}

// Show
// ====

#if MAIN_PURE

// A pure main's value, spelled as term_show spells it: d is a node of
// SHOW_DESC (see show_main), w the value's words. A boxed Data reads its
// arm by cid off a Term (packed, or a node), an inline one by tag off
// its words.
static void show_val(Env e, u32 d, const Term* w, char chain);

// char_show: an escape, a \u{hex}, else the code point in UTF-8
static void show_chr(u64 c, char q) {
  char b[4];
  int  k = c == 10 ? 'n' : c == 9 ? 't' : c == 13 ? 'r' : c == 0 ? '0'
    : c == 92 || c == (u64)q ? (int)c : 0;
  if (k != 0) {
    printf("\\%c", k);
  } else if (c < 32 || c == 127 || (c >= 0xD800 && c <= 0xDFFF)
    || c > 0x10FFFF) {
    printf("\\u{%llx}", (unsigned long long)c);
  } else {
    fwrite(b, 1, io_utf8(b, c), stdout);
  }
}

// The shortest text that reads back, as a literal: a point before an e
static void show_f32(u32 x) {
  char  buf[40];
  int   n  = f32_text(buf, f32_unbox(x));
  char* ep = memchr(buf, 'e', n);
  int   m  = ep == NULL ? n : (int)(ep - buf);
  buf[n] = 0;
  if (strpbrk(buf, ".ni") == NULL) {
    printf("%.*s.0%s", m, buf, buf + m);
  } else {
    fputs(buf, stdout);
  }
}

static void show_arr(Env e, u32 d, Term t, u32 lo, u32 c) {
  if (c > SHOW_DESC[d + 2]) {
    c -= 1;
    show_arr(e, d, t, lo, c);
    fputs(", ", stdout);
    show_arr(e, d, t, lo + (1u << c), c);
  } else {
    Term v[1u << c];
    for (u32 j = 0; j < 1u << c; j += 1) {
      v[j] = blk_read(e.mem, term_tag(t) == TAG_ARR, term_peek(e, t), lo + j);
    }
    show_val(e, SHOW_DESC[d + 1], v, 0);
  }
}

// chain is the bracket of the [a, b] or (a, b) this value continues, or
// 0: a Con or Nil spells a list, a Tuple a tuple, their tails continue
static void show_val(Env e, u32 d, const Term* w, char chain) {
  const u32* D = SHOW_DESC;
  Term one;
  char zs[4];
  u32  zn = 0;
  for (bool tail = true; tail;) switch (tail = false, D[d]) {
    case 0: printf("%u", (u32)w[0]); break;
    case 1: show_f32((u32)w[0]); break;
    case 2: printf("%llun", (unsigned long long)w[0]); break;
    case 3:
      putchar('\'');
      show_chr(D[d + 1] != 0 ? term_loc(w[0]) : w[0], '\'');
      putchar('\'');
      break;
    case 4:
      putchar('"');
      for (Term s = w[0]; term_aux(s) == CID_SCON;) {
        Loc l = term_peek(e, s);
        show_chr(e.mem[l], '"');
        s = e.mem[l + 1];
      }
      putchar('"');
      break;
    case 5: fputs("{==}", stdout); break;
    case 6:
      putchar('[');
      show_arr(e, d, w[0], 0, blk_cls(w[0]));
      putchar(']');
      break;
    default: {
      Term t   = w[0];
      bool box = D[d + 1] != 0;
      u32  key = box ? (u32)term_aux(t) : D[d + 2] > 1 ? (u32)t : 0;
      u32  a   = d + 3;
      for (u32 i = 0; box ? D[a + 1] != key : i != key; i += 1) {
        a += 3 + 2 * D[a + 2];
      }
      if (box) {
        one = term_loc(t);
        w   = term_tag(t) == TAG_PAK ? &one : e.mem + term_peek(e, t);
      }
      const char* k = SHOW_NAMES[D[a]];
      char o = '{';
      char z = '}';
      if (strcmp(k, "Con") == 0 || strcmp(k, "Nil") == 0) {
        o = '[';
        z = ']';
      } else if (strcmp(k, "Tuple") == 0) {
        o = '(';
        z = ')';
      }
      if (o == '{') {
        printf("%s{", k);
      } else if (chain != o) {
        putchar(o);
      }
      if (o == '{' || chain != o) {
        zs[zn++] = z;
      }
      for (u32 j = 0; j < D[a + 2]; j += 1) {
        if (o == '[' ? j == 0 && chain == o : j > 0) {
          fputs(", ", stdout);
        }
        if (j == 1 && o != '{') {
          tail  = true;
          chain = o;
          d     = D[a + 4 + 2 * j];
          w     = w + D[a + 3 + 2 * j];
        } else {
          show_val(e, D[a + 4 + 2 * j], w + D[a + 3 + 2 * j], 0);
        }
      }
    }
  }
  while (zn > 0) {
    putchar(zs[--zn]);
  }
}

#endif

// The continuation applied to the item is the next request.
static int io_step(Env e, IoAct* a) {
  for (;;) {
    Loc  ap  = task_node(e, FID_CLO_APPLY, TERM_HOLE, 0, 0);
    e.mem[ap]     = a->cont;
    e.mem[ap + 1] = a->item;
    Term req = corpus_eval(e.mem, term_tsk(FID_CLO_APPLY, ap));
    u32  c   = (u32)term_aux(req);
    Loc  at  = term_peek(e, req);
    if (c == CID_EMIT) {
      term_drop(e, req);
      free(a);
      io_live -= 1;
      return -1;
    }
    if (c == CID_HALT) {
      io_errs(e, e.mem[at + 1]);
      return (int)(u32)e.mem[at];
    }
    if (io_eff_rows[c].run == NULL) {
      err_fail("an alien request");
    }
    u32 need = io_eff_rows[c].ask;
    u32 word = (u32)(need & IO_READ ? io_hand_v(e.mem[at]) : e.mem[at]);
    a->cont  = req;
    if (need != 0) {
      io_wait_on(&a->work, (int)word, POLLIN, io_exec);
      a->time = need & IO_TIME ? io_tick() + (u64)word * 1000000ull : 0;
      return -1;
    }
    Term x = io_exec(e, &a->work);
    if (x == IO_PARK) {
      return -1;
    }
    a->item = x;
  }
}

OUTLINE int io_loop(Corpus H) {
  Env e = { H, ALC[0] };
  io_stk = pool_stack();
  signal(SIGPIPE, SIG_IGN);
  if (pipe(io_wake_fd) | fcntl(io_wake_fd[0], F_SETFL, O_NONBLOCK)) {
    err_fail("the event loop failed to open");
  }
  Term m = corpus_eval(H, term_tsk(MAIN_FID, task_node(e, MAIN_FID,
    TERM_HOLE, 0, 0)));
#if MAIN_PURE
  show_val(e, 0, H + H_ROOT_WORD, 0);
  putchar('\n');
  return 0;
#endif
  io_spawn(m);
  for (u32 n = 0;; n += 1) {
    if (io_runs.head == NULL) {
      if (io_live == 0) {
        return 0;
      }
      if (io_park.head == NULL && io_busy == 0) {
        io_sync();
        fprintf(stderr, "bend: deadlock: every computation waits on a"
          " channel\n");
        return 1;
      }
      io_wait(e);
      continue;
    }
    if ((n & 63) == 0 && io_busy != 0) {
      io_take(e);
    }
    int code = io_step(e, io_pop(&io_runs));
    if (code >= 0) {
      return code;
    }
  }
}

// Chan
// ====

// ChanRow ::=
//   | ChanRow(gen, next, room, size, head, live, shut, ring, wait)
typedef struct {
  u32   gen;
  u32   next;
  u32   room;
  u32   size;
  u32   head;
  u32   live;
  u32   shut;
  Term* ring;
  IoQue wait;
} ChanRow;

// A channel is Data: its handle is copied and may outlive the row, so it
// names the row by index and generation, a freed row waits on a list and
// comes back one generation up, and a stale copy finds no row (closed).
static ChanRow* chan_rows;
static u32      chan_len;
static u32      chan_idle = ~0u;

#define chan_some(e, v) io_box(e, CID_SOME, v, IO_HOTS & 32)
#define chan_bool(b)    term_pak((b) ? CID_TRUE : CID_FALSE, 0)

static Term chan_open(u32 room) {
  u32 i = chan_idle;
  if (i != ~0u) {
    chan_idle = chan_rows[i].next;
  } else {
    if (chan_len == 1u << 24) {
      err_fail("more than 16777216 channels at once");
    }
    if ((chan_len & (chan_len - 1)) == 0) {
      chan_rows = io_mem(realloc(chan_rows,
        (chan_len == 0 ? 1 : 2 * chan_len) * sizeof(ChanRow)));
    }
    i = chan_len;
    chan_len += 1;
    chan_rows[i].gen = 0;
  }
  ChanRow* row = &chan_rows[i];
  row->gen  += 1;
  row->room  = room;
  row->size  = 0;
  row->head  = 0;
  row->live  = 1;
  row->shut  = 0;
  row->ring  = room == 0 ? NULL : io_mem(malloc(room * sizeof(Term)));
  row->wait.head = NULL;
  row->wait.last = NULL;
  return io_hand(((u64)row->gen << 24) | i);
}

static ChanRow* chan_at(Term t) {
  u64      v   = io_hand_v(t);
  u32      i   = (u32)v & 0xFFFFFF;
  ChanRow* row = i < chan_len ? &chan_rows[i] : NULL;
  return row != NULL && row->live && row->gen == (u32)(v >> 24) ? row : NULL;
}

// Parks the effect's activation on row with item: a sent value, or
// TERM_HOLE for a receiver.
static Term chan_park(ChanRow* row, IoWork* w, Term item) {
  IoAct* a = (IoAct*)w;
  a->item  = item;
  io_push(&row->wait, a);
  return IO_PARK;
}

static Term chan_wake(ChanRow* row, Term x) {
  IoAct* a  = io_pop(&row->wait);
  Term item = a->item;
  a->item   = x;
  io_push(&io_runs, a);
  return item;
}

static Term chan_take(ChanRow* row) {
  Term v = row->ring[row->head];
  row->head = (row->head + 1) % row->room;
  row->size -= 1;
  if (row->wait.head != NULL) {
    Term item = chan_wake(row, chan_bool(true));
    row->ring[(row->head + row->size) % row->room] = item;
    row->size += 1;
  }
  return v;
}

static void chan_free(ChanRow* row) {
  free(row->ring);
  row->live = 0;
  row->next = chan_idle;
  chan_idle = (u32)(row - chan_rows);
}

static void chan_shut(Env e, ChanRow* row) {
  row->shut = 1;
  while (row->wait.head != NULL) {
    bool rcv = row->wait.head->item == TERM_HOLE;
    Term x = rcv ? term_pak(CID_NONE, 0) : chan_bool(false);
    term_sink(e, chan_wake(row, x));
  }
  if (row->size == 0) {
    chan_free(row);
  }
}

// Requests
// ========

// IO
// ==

Term io_args_run(Env e, Term* f, IoWork* w) {
  Term xs = term_pak(CID_NIL, 0);
  for (int i = io_argc; i > 0; i -= 1) {
    const char* a = io_argv[i - 1];
    xs = io_node(e, CID_CON, io_str(e, a, strlen(a)), xs, IO_HOTS & 16);
  }
  return xs;
}

static void __attribute__((constructor)) io_args_use(void) {
  io_eff(CID_IO_ARGS, io_args_run, 0);
}
// File
// ====

static int file_open_mode(const char* mode) {
  if (strcmp(mode, "r") == 0) {
    return O_RDONLY;
  }
  if (strcmp(mode, "w") == 0) {
    return O_WRONLY | O_CREAT | O_TRUNC;
  }
  if (strcmp(mode, "a") == 0) {
    return O_WRONLY | O_CREAT | O_APPEND;
  }
  return -1;
}

static void file_open_call(IoWork* w) {
  w->made = (intptr_t)io_sys_end(w, open(w->data, (int)w->word, 0644));
}

static Term file_open_pack(Env e, IoWork* w) {
  free(w->data);
  return w->code != 0 ? io_fail(e, w->code, NULL)
    : io_done(e, io_hand(w->made));
}

Term file_open_run(Env e, Term* f, IoWork* w) {
  uint64_t mn = 0;
  w->data = io_cstr(e, f[0], &w->size);
  char* mode = io_cstr(e, f[1], &mn);
  int flags = io_nul(mode, mn) ? -1 : file_open_mode(mode);
  free(mode);
  w->word = (uint32_t)flags;
  if (io_nul(w->data, w->size) || flags < 0) {
    w->code = io_nul(w->data, w->size) ? EILSEQ : EINVAL;
    return file_open_pack(e, w);
  }
  return io_work(w, file_open_call, file_open_pack);
}

static void __attribute__((constructor)) file_open_use(void) {
  io_eff(CID_FILE_OPEN, file_open_run, 0);
}
// File
// ====

#include <sys/stat.h>

// The size in bytes, as the host reports it; a file past 4 GiB fails
// with EOVERFLOW.
static void file_size_call(IoWork* w) {
  struct stat st;
  int n = fstat((int)w->hand, &st);
  io_sys_end(w, n);
  if (n == 0) {
    w->code = st.st_size > (off_t)UINT32_MAX ? EOVERFLOW : 0;
    w->word = (u32)st.st_size;
  }
}

static Term file_size_pack(Env e, IoWork* w) {
  Term r = w->code ? io_fail(e, w->code, NULL) : io_done(e, w->word);
  return io_tup(e, io_hand(w->hand), r);
}

Term file_size_run(Env e, Term* f, IoWork* w) {
  w->hand = (intptr_t)io_hand_v(f[0]);
  return io_work(w, file_size_call, file_size_pack);
}

static void __attribute__((constructor)) file_size_use(void) {
  io_eff(CID_FILE_SIZE, file_size_run, 0);
}
// File
// ====

static void file_read_call(IoWork* w) {
  int fd = (int)w->hand;
  w->size = io_sys_end(w, read(fd, w->data, w->word));
}

static Term file_read_pack(Env e, IoWork* w) {
  Term r = w->code ? io_fail(e, w->code, NULL)
    : io_done(e, io_str(e, w->data, w->size));
  free(w->data);
  return io_tup(e, io_hand(w->hand), r);
}

Term file_read_run(Env e, Term* f, IoWork* w) {
  w->hand = (intptr_t)io_hand_v(f[0]);
  w->word = f[1] < INT32_MAX ? f[1] : INT32_MAX;
  w->data = io_mem(malloc(w->word + 1));
  return io_work(w, file_read_call, file_read_pack);
}

static void __attribute__((constructor)) file_read_use(void) {
  io_eff(CID_FILE_READ, file_read_run, 0);
}
// File
// ====

Term file_close_run(Env e, Term* f, IoWork* w) {
  close((int)io_hand_v(f[0]));
  return term_pak(CID_UNIT, 0);
}

static void __attribute__((constructor)) file_close_use(void) {
  io_eff(CID_FILE_CLOSE, file_close_run, 0);
}
// IO
// ==

void io_print(const char* data, uint64_t len) {
  io_out(stdout, data, len);
  io_out(stdout, "\n", 1);
}

Term io_print_run(Env e, Term* f, IoWork* w) {
  uint64_t n = 0;
  char* text = io_cstr(e, f[0], &n);
  io_print(text, n);
  free(text);
  return term_pak(CID_UNIT, 0);
}

static void __attribute__((constructor)) io_print_use(void) {
  io_eff(CID_IO_PRINT, io_print_run, 0);
}
// IO
// ==

Term io_now_run(Env e, Term* f, IoWork* w) {
  return (Term)(io_tick() / 1000000);
}

static void __attribute__((constructor)) io_now_use(void) {
  io_eff(CID_IO_NOW, io_now_run, 0);
}
// File
// ====

// The bytes as they are (0..255), one List cell each; a value past 255
// fails with EINVAL before any byte is written.
static void file_write_bytes_call(IoWork* w) {
  int fd = (int)w->hand;
  ssize_t n = 0;
  for (uint64_t at = 0; n >= 0 && at < w->size; at += (uint64_t)n) {
    n = write(fd, w->data + at, w->size - at);
  }
  io_sys_end(w, n);
}

static Term file_write_bytes_pack(Env e, IoWork* w) {
  Term r = w->code != 0 ? io_fail(e, w->code, NULL)
    : io_done(e, term_pak(CID_UNIT, 0));
  free(w->data);
  return io_tup(e, io_hand(w->hand), r);
}

Term file_write_bytes_run(Env e, Term* f, IoWork* w) {
  u64  cap = 64;
  Term xs  = f[1];
  w->hand = (intptr_t)io_hand_v(f[0]);
  w->code = 0;
  w->size = 0;
  w->data = io_mem(malloc(cap));
  while (term_aux(xs) == CID_CON) {
    Term fb[2];
    spare_free(e, cls_fit(2), ctr_take(e, xs, 2, fb));
    if (w->size == cap) {
      cap *= 2;
      w->data = io_mem(realloc(w->data, cap));
    }
    w->code = fb[0] > 255 ? EINVAL : w->code;
    w->data[w->size++] = (char)fb[0];
    xs = fb[1];
  }
  return w->code ? file_write_bytes_pack(e, w)
    : io_work(w, file_write_bytes_call, file_write_bytes_pack);
}

static void __attribute__((constructor)) file_write_bytes_use(void) {
  io_eff(CID_FILE_WRITE_BYTES, file_write_bytes_run, 0);
}
// File
// ====

// The bytes as they are (0..255), one List cell each; a text reader
// would decode them as UTF-8.
static void file_read_bytes_call(IoWork* w) {
  int fd = (int)w->hand;
  w->size = io_sys_end(w, read(fd, w->data, w->word));
}

static Term file_read_bytes_pack(Env e, IoWork* w) {
  Term r;
  if (w->code) {
    r = io_fail(e, w->code, NULL);
  } else {
    Term xs = term_pak(CID_NIL, 0);
    for (u64 i = w->size; i > 0; i -= 1) {
      xs = io_node(e, CID_CON, ((uint8_t*)w->data)[i - 1], xs, IO_HOTS & 16);
    }
    r = io_done(e, xs);
  }
  free(w->data);
  return io_tup(e, io_hand(w->hand), r);
}

Term file_read_bytes_run(Env e, Term* f, IoWork* w) {
  w->hand = (intptr_t)io_hand_v(f[0]);
  w->word = f[1] < INT32_MAX ? f[1] : INT32_MAX;
  w->data = io_mem(malloc(w->word + 1));
  return io_work(w, file_read_bytes_call, file_read_bytes_pack);
}

static void __attribute__((constructor)) file_read_bytes_use(void) {
  io_eff(CID_FILE_READ_BYTES, file_read_bytes_run, 0);
}


// Cli
// ===

static void cli_fail(const char* msg, const char* arg) {
  fprintf(stderr, "bend: %s%s\n", msg, arg != NULL ? arg : "");
  exit(1);
}

// Main
// ====

int main(int argc, char** argv) {
  long thr = 0;
  int  gpu = -1;
  u64  mem = 0;
  io_argv = argv + 1;
  for (int i = 1; i < argc; i += 1) {
    const char* a = argv[i];
    const char* v = i + 1 < argc ? argv[i + 1] : NULL;
    if (strcmp(a, "--") == 0) {
      while (i + 1 < argc) {
        io_argv[io_argc++] = argv[++i];
      }
    } else if (strcmp(a, "--help") == 0) {
      printf(CLI_HELP, argv[0]);
      return 0;
    } else if (strcmp(a, "--gpu-build") == 0) {
      if (gpu_probe() && !gpu_make(gpu_path())) {
        cli_fail("cannot write ", gpu_path());
      }
      return 0;
    } else if (strcmp(a, "--threads") == 0) {
      char* end = NULL;
      thr = v != NULL ? strtol(v, &end, 10) : 0;
      if (thr < 1 || end == NULL || *end != '\0') {
        cli_fail("expected a thread count of 1 or more after --threads", NULL);
      }
      i += 1;
    } else if (strcmp(a, "--gpu") == 0) {
      char*  end = NULL;
      double n   = v != NULL ? strtod(v, &end) : 0;
      u64    mul = end == NULL ? 0 : strcmp(end, "GB") == 0 ? 1ull << 30
        : strcmp(end, "MB") == 0 ? 1ull << 20 : 0;
      if (v != NULL && strcmp(v, "off") == 0) {
        gpu = 0;
      } else if (v != NULL && (strcmp(v, "on") == 0 || (mul != 0 && n > 0))) {
        gpu = 1;
        mem = (u64)(n * (double)mul);
      } else {
        cli_fail("expected on, off or a size like 4GB after --gpu", NULL);
      }
      i += 1;
    } else {
      io_argv[io_argc++] = argv[i];
    }
  }
  bool dev = gpu != 0 && BANGS != 0 && gpu_probe();
  if (gpu == 1 && BANGS != 0 && !dev) {
    cli_fail("--gpu on, but this binary found no GPU device", NULL);
  }
  Corpus H  = corpus_setup(dev, thr > 0 ? thr : cpu_count(), mem);
  int code  = io_loop(H);
  io_sync();
  return code;
}

#endif
