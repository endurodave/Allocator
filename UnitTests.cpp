// Unit tests for Allocator. Checks run in every build type (not assert()), and
// failures are counted so every test runs. main() exits non-zero on failure.

#include "Allocator.h"
#include <iostream>
#include <cstddef>
#include <cstdint>
#include <new>

static int s_failures = 0;

#define CHECK(cond) \
    do { if (!(cond)) { std::cout << "  FAILED: " #cond " (" __FILE__ ":" << __LINE__ << ")" << std::endl; s_failures++; } } while (0)

static bool IsMaxAligned(const void* p)
{
    return reinterpret_cast<uintptr_t>(p) % alignof(std::max_align_t) == 0;
}

// A type that needs the strictest fundamental alignment
struct MaxAligned
{
    std::max_align_t value;
    char c;
};

// A DECLARE_ALLOCATOR class with a pool of 2
class PoolClass
{
    DECLARE_ALLOCATOR
public:
    int value = 0;
};
IMPLEMENT_ALLOCATOR(PoolClass, 2, NULL)

// AllocatorPool memory is aligned for any type, so pooled objects are aligned
static void Pool_BlocksAreAligned()
{
    AllocatorPool<MaxAligned, 4> pool;
    void* blocks[4];
    for (int i = 0; i < 4; i++)
    {
        blocks[i] = pool.Allocate(sizeof(MaxAligned));
        CHECK(blocks[i] != NULL);
        CHECK(IsMaxAligned(blocks[i]));
    }
    for (int i = 0; i < 4; i++)
        pool.Deallocate(blocks[i]);
}

// An exhausted pool with no new-handler returns NULL and counts nothing
static void Pool_ExhaustedReturnsNull()
{
    std::new_handler saved = std::set_new_handler(nullptr);

    Allocator allocator(32, 2);
    void* p1 = allocator.Allocate(32);
    void* p2 = allocator.Allocate(32);
    CHECK(p1 != NULL && p2 != NULL);

    void* p3 = allocator.Allocate(32);
    CHECK(p3 == NULL);
    CHECK(allocator.GetBlocksInUse() == 2);
    CHECK(allocator.GetAllocations() == 2);

    allocator.Deallocate(p1);
    allocator.Deallocate(p2);
    CHECK(allocator.GetBlocksInUse() == 0);

    std::set_new_handler(saved);
}

// When the pool is exhausted, the new-handler is called and the allocation
// retried, so a handler that frees a block makes the allocation succeed
static Allocator* s_handlerAllocator = NULL;
static void* s_handlerBlock = NULL;
static int s_handlerCalls = 0;
static void FreeOneBlock()
{
    s_handlerCalls++;
    s_handlerAllocator->Deallocate(s_handlerBlock);
    std::set_new_handler(nullptr);
}

static void Pool_NewHandlerFreesBlockAndRetries()
{
    std::new_handler saved = std::set_new_handler(FreeOneBlock);

    Allocator allocator(32, 1);
    void* p1 = allocator.Allocate(32);
    CHECK(p1 != NULL);
    s_handlerAllocator = &allocator;
    s_handlerBlock = p1;
    s_handlerCalls = 0;

    void* p2 = allocator.Allocate(32);
    CHECK(s_handlerCalls == 1);
    CHECK(p2 == p1);    // the block the handler freed
    CHECK(allocator.GetBlocksInUse() == 1);

    allocator.Deallocate(p2);
    std::set_new_handler(saved);
}

// Deallocate(NULL) is ignored: delete of a null pointer may call it
static void Deallocate_NullIgnored()
{
    Allocator allocator(32, 2);
    allocator.Deallocate(NULL);
    CHECK(allocator.GetBlocksInUse() == 0);
    CHECK(allocator.GetDeallocations() == 0);

    // Still works afterward
    void* p = allocator.Allocate(32);
    CHECK(p != NULL);
    allocator.Deallocate(p);

    PoolClass::operator delete(nullptr);
}

// DECLARE_ALLOCATOR's operator new throws std::bad_alloc when the pool is
// exhausted, instead of returning NULL
static void DeclareAllocator_ExhaustedThrows()
{
    std::new_handler saved = std::set_new_handler(nullptr);

    PoolClass* a = new PoolClass();
    PoolClass* b = new PoolClass();
    bool threw = false;
    try
    {
        PoolClass* c = new PoolClass();
        delete c;
    }
    catch (const std::bad_alloc&)
    {
        threw = true;
    }
    CHECK(threw);
    delete a;
    delete b;

    // Space again after delete
    PoolClass* d = new PoolClass();
    CHECK(d != NULL);
    delete d;

    std::set_new_handler(saved);
}

// Heap blocks mode: blocks come from the heap and are reused after Deallocate
static void HeapBlocks_Reuse()
{
    Allocator allocator(64);
    void* p1 = allocator.Allocate(64);
    CHECK(p1 != NULL);
    CHECK(IsMaxAligned(p1));
    allocator.Deallocate(p1);
    void* p2 = allocator.Allocate(64);
    CHECK(p2 == p1);
    CHECK(allocator.GetBlockCount() == 1);
    CHECK(allocator.GetAllocations() == 2);
    allocator.Deallocate(p2);
}

// Static pool mode uses the caller's memory
static void StaticPool_UsesCallerMemory()
{
    alignas(std::max_align_t) static char memory[32 * 4];
    Allocator allocator(32, 4, memory);
    for (int i = 0; i < 4; i++)
    {
        char* p = static_cast<char*>(allocator.Allocate(32));
        CHECK(p >= memory && p < memory + sizeof(memory));
    }
}

// GetBlockCount() counts pool blocks as they are first handed out, as it does
// heap blocks; a recycled block is not counted again
static void Pool_BlockCount()
{
    Allocator allocator(32, 4);
    CHECK(allocator.GetBlockCount() == 0);
    void* p1 = allocator.Allocate(32);
    void* p2 = allocator.Allocate(32);
    CHECK(allocator.GetBlockCount() == 2);
    allocator.Deallocate(p1);
    void* p3 = allocator.Allocate(32);
    CHECK(p3 == p1);
    CHECK(allocator.GetBlockCount() == 2);
    allocator.Deallocate(p2);
    allocator.Deallocate(p3);
}

int RunUnitTests()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "Pool_BlocksAreAligned",                  Pool_BlocksAreAligned },
        { "Pool_ExhaustedReturnsNull",              Pool_ExhaustedReturnsNull },
        { "Pool_NewHandlerFreesBlockAndRetries",    Pool_NewHandlerFreesBlockAndRetries },
        { "Deallocate_NullIgnored",                 Deallocate_NullIgnored },
        { "DeclareAllocator_ExhaustedThrows",       DeclareAllocator_ExhaustedThrows },
        { "HeapBlocks_Reuse",                       HeapBlocks_Reuse },
        { "StaticPool_UsesCallerMemory",            StaticPool_UsesCallerMemory },
        { "Pool_BlockCount",                        Pool_BlockCount },
    };

    std::cout << "-- Unit tests --" << std::endl;
    s_failures = 0;
    for (const auto& test : tests)
    {
        int before = s_failures;
        test.fn();
        std::cout << (s_failures == before ? "PASS " : "FAIL ") << test.name << std::endl;
    }
    std::cout << (s_failures == 0 ? "All unit tests passed." : "Unit tests FAILED.") << std::endl;
    return s_failures;
}
