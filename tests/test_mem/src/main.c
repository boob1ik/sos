#include <stdint.h>
#include <stddef.h>
#include <os.h>

#define PAGE_SIZE (4096/4)

#define TEST_SEG_MAX  10

struct seg {
    uint32_t *adr;
    uint32_t size;
} segs[TEST_SEG_MAX];

static int seg_cnt = 0;

static const mem_attributes_t test_attr = {
    .shared = MEM_SHARED_OFF,
    .exec = MEM_EXEC_NEVER,
    .type = MEM_TYPE_NORMAL,
    .inner_cached = MEM_CACHED_WRITE_BACK,
    .outer_cached = MEM_CACHED_WRITE_BACK,
    .process_access = MEM_ACCESS_RW,
    .os_access = MEM_ACCESS_RW,
    .security = MEM_SECURITY_OFF
};

static void panic ()
{
    asm volatile ("bkpt");
}

static bool is_ok_memory (uint32_t *start, uint32_t *end)
{
    if (!start || !end)
        return false;

    while (start < end) {
        *start = (uint32_t)start;
        if (*start != (uint32_t)start)
            panic();
        ++start;
    }
    return true;
}

static bool alloc_seg (int i, size_t size)
{
    if (i < 0 || i > TEST_SEG_MAX)
        return false;
    uint32_t *adr = os_malloc(size, test_attr, 0);
    if (!adr)
        return false;
    segs[i].adr = adr;
    segs[i].size = size;
    if (!is_ok_memory(adr, adr + PAGE_SIZE * size))
        return false;
    seg_cnt++;
    return true;
}

static bool free_seg (int i)
{
    if (!segs[i].adr)
        return false;
    if (os_mfree(segs[i].adr) != OK)
        return false;
    segs[i].adr = NULL;
    seg_cnt--;
    return true;
}

static void test_1_2_3_4_to_1_2_3_4 ()
{
    for (int i = 0, size = 1; i < TEST_SEG_MAX; i++, size++) {
        if (!alloc_seg(i, size))
            panic();
    }
    for (int i = 0; i < TEST_SEG_MAX; i++) {
        if (!free_seg(i))
            panic();
    }

}

static void test_1_2_3_4_to_4_3_2_1 ()
{
    for (int i = 0, size = 1; i < TEST_SEG_MAX; i++, size++) {
        if (!alloc_seg(i, size))
            panic();
    }
    for (int i = seg_cnt - 1; i >= 0; i--) {
        if (!free_seg(i))
            panic();
    }
}

static void test_4_3_2_1_to_4_3_2_1 ()
{
    for (int i = 0, size = 20; i < TEST_SEG_MAX; i++, size--) {
        if (!alloc_seg(i, size))
            panic();
    }
    for (int i = 0; i < TEST_SEG_MAX; i++) {
        if (!free_seg(i))
            panic();
    }
}

static void test_1_2_3_4_to_1_3_2_4 ()
{
    for (int i = 0, size = 1; i < TEST_SEG_MAX; i++, size++) {
        if (!alloc_seg(i, size))
            panic();
    }

    for (int i = 0; i < TEST_SEG_MAX; i += 2) {
        if (!free_seg(i))
            panic();
    }
    for (int i = 1; i < TEST_SEG_MAX; i += 2) {
        if (!free_seg(i))
            panic();
    }
}

static int rand ()
{
    //kernel_time_t time = 0;
    //return os_time(OS_CLOCK_MONOTONIC, time, NULL) % 10;
    return 0;
}

static void test_rand ()
{
    for (int i = 0, size = rand(); i < TEST_SEG_MAX; i++, size = rand()) {
        uint32_t *adr = os_malloc(size, test_attr, 0);
        segs[i].adr = adr;
        segs[i].size = size;
        if (!adr)
            break;
        if (!is_ok_memory(adr, adr + PAGE_SIZE * size))
            panic();
    }

    for (int i = seg_cnt; i >= 0; i--) {
        if (!segs[i].adr)
            break;
        if (os_mfree(segs[i].adr) != OK)
            panic();
        segs[i].adr = NULL;
    }
    seg_cnt = 0;
}

mem_attributes_t devattr = { //
    .shared = MEM_SHARED_OFF, //
    .exec = MEM_EXEC_NEVER, //
    .type = MEM_TYPE_DEVICE, //
    .inner_cached = MEM_CACHED_OFF, //
    .outer_cached = MEM_CACHED_OFF, //
    .process_access = MEM_ACCESS_RW, //
    .os_access = MEM_ACCESS_RO, //
};//

int main()
{
    volatile int cnt = 3;
    while (cnt--) {
        os_mmap(0x20D4000u, 4, devattr);
        cnt++;
        test_1_2_3_4_to_1_2_3_4();
        test_1_2_3_4_to_4_3_2_1();
        test_4_3_2_1_to_4_3_2_1();
        test_1_2_3_4_to_1_3_2_4();
        //test_rand();
    }
    return 0;
}
