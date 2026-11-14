// Allocator PlatformIO example: give a class its own fixed block allocator,
// with blocks from the heap or from a static pool. Runs on Arduino boards and
// the desktop (native).
//
// @see https://github.com/endurodave/Allocator

#include "Allocator.h"
#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif

static void Log(const char* msg)
{
#ifdef ARDUINO
    Serial.println(msg);
#else
    printf("%s\n", msg);
#endif
}

// Heap blocks mode: new blocks come from the heap one at a time as needed;
// deleted blocks are kept on a free-list and reused.
class Message
{
    DECLARE_ALLOCATOR
public:
    static Allocator& GetAllocator() { return _allocator; }
    Message(int id) : id(id) { memset(payload, 0, sizeof(payload)); }
    int id;
    char payload[32];
};
IMPLEMENT_ALLOCATOR(Message, 0, 0)

// Static pool mode: up to 8 Samples in a pool reserved at compile time, so
// creating one never touches the heap and takes a fixed time.
class Sample
{
public:
    Sample(float value) : value(value) {}
    float value;

    void* operator new(size_t size)
    {
        void* p = _pool.Allocate(size);
        if (!p)
            AllocatorBadAlloc();
        return p;
    }
    void operator delete(void* p) { _pool.Deallocate(p); }

    // nothrow form: returns NULL when the pool is full
    void* operator new(size_t size, const std::nothrow_t&) noexcept { return _pool.Allocate(size); }
    void operator delete(void* p, const std::nothrow_t&) noexcept { _pool.Deallocate(p); }

    static Allocator& GetAllocator() { return _pool; }

private:
    static AllocatorPool<Sample, 8> _pool;
};
AllocatorPool<Sample, 8> Sample::_pool;

static void LogStats(const char* label, Allocator& alloc)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "%s: block %u bytes, %lu blocks, %lu in use, %lu allocations",
        label, (unsigned)alloc.GetBlockSize(), (unsigned long)alloc.GetBlockCount(),
        (unsigned long)alloc.GetBlocksInUse(), (unsigned long)alloc.GetAllocations());
    Log(buf);
}

static bool Demo()
{
    bool ok = true;

    // Heap blocks: create and delete Messages; freed blocks are reused
    Message* first = new Message(1);
    delete first;
    Message* second = new Message(2);
    Log(second == first ? "Message block reused: yes" : "Message block reused: no");
    ok = ok && second == first;
    delete second;

    Message* msgs[10];
    for (int i = 0; i < 10; i++)
        msgs[i] = new Message(i);
    for (int i = 0; i < 10; i++)
        ok = ok && msgs[i]->id == i;
    for (int i = 0; i < 10; i++)
        delete msgs[i];

    // Static pool: 8 Samples fit, a 9th does not
    Sample* samples[8];
    for (int i = 0; i < 8; i++)
        samples[i] = new Sample(i * 0.5f);
    Sample* extra = new (std::nothrow) Sample(99.0f);
    Log(extra == NULL ? "Static pool full after 8 Samples: yes" : "Static pool full after 8 Samples: no");
    ok = ok && extra == NULL;
    for (int i = 0; i < 8; i++)
        ok = ok && samples[i]->value == i * 0.5f;
    for (int i = 0; i < 8; i++)
        delete samples[i];

    // A freed slot can be used again
    Sample* again = new (std::nothrow) Sample(1.5f);
    ok = ok && again != NULL;
    delete again;

    // A standalone Allocator for raw blocks
    Allocator buffers(64, 4, NULL, "buffers");
    void* a = buffers.Allocate(64);
    void* b = buffers.Allocate(64);
    ok = ok && a != NULL && b != NULL && a != b;
    buffers.Deallocate(a);
    buffers.Deallocate(b);

    LogStats("Message", Message::GetAllocator());
    LogStats("Sample", Sample::GetAllocator());
    LogStats("buffers", buffers);
    return ok;
}

#ifdef ARDUINO
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Log("Allocator demo");
    Log(Demo() ? "Allocator demo PASSED" : "Allocator demo FAILED");
}

void loop()
{
}
#else
int main()
{
    Log("Allocator demo");
    bool ok = Demo();
    Log(ok ? "Allocator demo PASSED" : "Allocator demo FAILED");
    return ok ? 0 : 1;
}
#endif
