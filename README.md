![License MIT](https://img.shields.io/github/license/BehaviorTree/BehaviorTree.CPP?color=blue)
[![conan Ubuntu](https://github.com/endurodave/Allocator/actions/workflows/cmake_ubuntu.yml/badge.svg)](https://github.com/endurodave/Allocator/actions/workflows/cmake_ubuntu.yml)
[![conan Ubuntu](https://github.com/endurodave/Allocator/actions/workflows/cmake_clang.yml/badge.svg)](https://github.com/endurodave/Allocator/actions/workflows/cmake_clang.yml)
[![conan Windows](https://github.com/endurodave/Allocator/actions/workflows/cmake_windows.yml/badge.svg)](https://github.com/endurodave/Allocator/actions/workflows/cmake_windows.yml)

# An Efficient C++ Fixed Block Memory Allocator

A C++ fixed block memory allocator that increases system performance and offers heap fragmentation fault protection.

Originally published on CodeProject at: <a href="https://www.codeproject.com/Articles/1083210/An-Efficient-Cplusplus-Fixed-Block-Memory-Allocato"><strong>An Efficient C++ Fixed Block Memory Allocator</strong></a>

See <a href="https://github.com/endurodave/C_Allocator">A Fixed Block Memory Allocator in C</a> for a C language version of this allocator. 

# Table of Contents

- [An Efficient C++ Fixed Block Memory Allocator](#an-efficient-c-fixed-block-memory-allocator)
- [Table of Contents](#table-of-contents)
- [Build](#build)
- [Introduction](#introduction)
- [Storage Recycling](#storage-recycling)
- [Heap vs. Pool](#heap-vs-pool)
- [Class Design](#class-design)
- [Using the Code](#using-the-code)
- [Run Time](#run-time)
- [Benchmarking](#benchmarking)
- [Allocator Decisions](#allocator-decisions)
- [Debugging Memory Leaks](#debugging-memory-leaks)
- [Error Handling](#error-handling)
- [Limitations](#limitations)
- [Porting Issues](#porting-issues)
- [Reference Articles](#reference-articles)


# Build

<p><a href="https://www.cmake.org/">CMake</a>&nbsp;is used to create the build files. CMake is free and open-source software. Windows, Linux and other toolchains are supported. See the <strong>CMakeLists.txt </strong>file for more information.</p>

# Introduction

<p>Custom fixed block memory allocators are used to solve at least two types of memory related problems. First, global heap allocations/deallocations can be slow and nondeterministic. You never know how long the memory manager is going to take. Secondly, to eliminate the possibility of a memory allocation fault caused by a fragmented heap &ndash; a valid concern, especially on mission-critical type systems.</p>

<p>Even if the system isn&#39;t considered mission-critical, some embedded systems are designed to run for weeks or years without a reboot. Depending on allocation patterns and heap implementation, long-term heap use can lead to heap faults.</p>

<p>The typical solution is to statically declare all objects up front and get rid of dynamic allocation altogether. However, static allocation can waste storage, because the objects, even if they&#39;re not actively being used, exist and take up space. In addition, implementing a system around dynamic allocation can offer a more natural design architecture, as opposed to statically declaring all objects.</p>

<p>Fixed block memory allocators are not a new idea. People have been designing various sorts of custom memory allocators for a long time. What I am presenting here is a simple C++ allocator implementation that I&#39;ve used successfully on a variety of projects.</p>

<p>The solution presented here will:</p>

<ul>
	<li>Be faster than the global heap</li>
	<li>Eliminate heap fragmentation memory faults</li>
	<li>Require no additional storage overhead (except for a few bytes of static memory)</li>
	<li>Be easy to use</li>
	<li>Use minimal code space</li>
</ul>

<p>A simple class that dispenses and reclaims memory will provide all of the aforementioned benefits, as I&#39;ll show.</p>

<p>After reading this article, be sure to read the follow-on article &quot;<b><a href="http://www.codeproject.com/Articles/1084801/Replace-malloc-free-with-a-fast-fixed-block-memory">Replace malloc/free with a Fast Fixed Block Memory Allocator</a></b>&quot; to see how <code>Allocator</code> is used to create a really fast <code>malloc()</code> and <code>free()</code> CRT replacement.</p>

# Storage Recycling

<p>The basic philosophy of the memory management scheme is to recycle memory obtained during object allocations. Once storage for an object has been created, it&#39;s never returned to the heap. Instead, the memory is recycled, allowing another object of the same type to reuse the space. I&#39;ve implemented a class called <code>Allocator</code> that expresses the technique.</p>

<p>When the application deletes using <code>Allocator</code>, the memory block for a single object is freed for use again but is not actually released back to the memory manager. Freed blocks are retained in a linked list, called the free-list, to be doled out again for another object of the same type. On every allocation request, <code>Allocator</code> first checks the free-list for an existing memory block. Only if none are available is a new one created. Depending on the desired behavior of <code>Allocator</code>, storage comes from either the global heap or a <code>static</code> memory pool with one of three operating modes:</p>

<ol>
	<li>Heap blocks</li>
	<li>Heap pool</li>
	<li>Static pool</li>
</ol>

# Heap vs. Pool

<p>The <code>Allocator</code> class is capable of creating new blocks from the heap or a memory pool whenever the free-list cannot provide an existing one. If the pool is used, you must specify the number of objects up front. Using the total objects, a pool large enough to handle the maximum number of instances is created. Obtaining block memory from the heap, on the other hand, has no such quantity limitations &ndash; construct as many new objects as storage permits.</p>

<p>The<em> heap blocks</em> mode allocates from the global heap a new memory block for a single object as necessary to fulfill memory requests. A deallocation puts the block into a free list for later reuse. Creating fresh new blocks off the heap when the free-list is empty frees you from having to set an object limit. This approach offers dynamic-like operation since the number of blocks can expand at run-time. The disadvantage is a loss of deterministic execution during block creation.</p>

<p>The <em>heap pool</em> mode creates a single pool from the global heap to hold all blocks. The pool is created using <code>operator new</code> when the <code>Allocator</code> object is constructed. <code>Allocator</code> then provides blocks of memory from the pool during allocations.</p>

<p>The <em>static pool</em> mode uses a single memory pool, typically located in static memory, to hold all blocks. The static memory pool is not created by <code>Allocator</code> but instead is provided by the user of the class.</p>

<p>The heap pool and static pool modes offers consistent allocation execution times because the memory manager is never involved with obtaining individual blocks. This makes a new operation very fast and deterministic.</p>

# Class Design

<p>The class interface is really straightforward. <code>Allocate()</code> returns a pointer to a memory block and <code>Deallocate()</code> frees the memory block for use again. The constructor is responsible for setting the object size and, if necessary, allocating a memory pool.</p>

<p>The arguments passed into the class constructor determine where the new blocks will be obtained. The <code>size</code> argument controls the fixed memory block size. The <code>objects</code> argument sets how many blocks are allowed. <code>0 </code>means get new blocks from the heap as necessary, whereas any other non-zero value indicates using a pool, heap or static, to handle the specified number of instances. The <code>memory</code> argument is a pointer to a optional <code>static</code> memory. If <code>memory</code> is <code>0</code> and <code>objects</code> is not <code>0</code>, <code>Allocator</code> will create a pool from the heap. The static memory pool must be <code>size</code> x <code>objects</code> bytes in size. The <code>name</code> argument optionally gives the allocator a name, which is useful for gather allocator usage metrics.</p>

<pre lang="C++">
class Allocator
{
public:
    Allocator(size_t size, UINT objects=0, CHAR* memory=NULL, const CHAR* name=NULL);
...</pre>

<p>The examples below show how each of the three operational mode is controlled via the constructor arguments.</p>

<pre lang="C++">
// Heap blocks mode with unlimited 100 byte blocks
Allocator allocatorHeapBlocks(100);

// Heap pool mode with 20, 100 byte blocks
Allocator allocatorHeapPool(100, 20);

// Static pool mode with 20, 100 byte blocks
char staticMemoryPool[100 * 20];
Allocator allocatorStaticPool(100, 20, staticMemoryPool);</pre>

<p>To simplify the <code>static</code> pool method somewhat, a simple <code>AllocatorPool&lt;&gt;</code> template class is used. The first template argument is block type and the second argument is the block quantity.</p>

<pre lang="C++">
// Static pool mode with 20 MyClass sized blocks 
AllocatorPool&lt;MyClass, 20&gt; allocatorStaticPool2;</pre>

<p>By calling <code>Allocate()</code>, a pointer to a memory block the size of one instance is returned. When obtaining the block, the free-list is checked to determine if a fee block already exists. If so, just unlink the existing block from the free-list and return a pointer to it; otherwise create a new one from the pool/heap.</p>

<pre lang="C++">
void* memory1 = allocatorHeapBlocks.Allocate(100);</pre>

<p><code>Deallocate()</code> just pushes the block address onto a stack. The stack is actually implemented as a singly linked list (the free-list), but the class only adds/removes from the head so the behavior is that of a stack. Using a stack makes the allocation/deallocations really fast. No searching through a list &ndash; just push or pop a block and go.</p>

<pre lang="C++">
allocatorHeapBlocks.Deallocate(memory1);</pre>

<p>Now comes a handy for linking blocks together in the free-list without consuming any extra storage for the pointers. If, for example, we use the global <code>operator new</code>, storage is allocated first then the constructor is called. The destruction process is just the reverse; destructor is called, then memory freed. After the destructor is executed, but before the storage is released back to the heap, the memory is no longer being utilized by the object and is freed to be used for other things, like a next pointer. Since the <code>Allocator</code> class needs to keep the deleted blocks around, during <code>operator delete</code> we put the list&#39;s next pointer in that currently unused object space. When the block is reused by the application, the pointer is no longer needed and will be overwritten by the newly formed object. This way, there is no per-instance storage overhead incurred.</p>

<p>Using freed object space as the memory to link blocks together means the object must be large enough to hold a pointer. The code in the constructor initializer list ensures the minimum block size is never below the size of a pointer.</p>

<p>The class destructor frees the storage allocated during execution by deleting the memory pool or, if blocks were obtained off the heap, by traversing the free-list and deleting each block. Since the <code>Allocator</code> class is typically used as a class scope <code>static</code>, it will only be called upon termination of the program. For most embedded devices, the application is terminated when someone yanks the power from the system. Obviously, in this case, the destructor is not required.</p>

<p>If you&#39;re using the heap blocks method, the allocated blocks cannot be freed when the application terminates unless all the instances are checked into the free-list. Therefore, all outstanding objects must be &quot;deleted&quot; before the program ends. Otherwise, you&#39;ve got yourself a nice memory leak. Which brings up an interesting point. Doesn&#39;t <code>Allocator</code> have to track both the free and used blocks? The short answer is no. The long answer is that once a block is given to the application via a pointer, it then becomes the application&#39;s responsibility to return that pointer to <code>Allocator</code> by means of a call to <code>Deallocate()</code> before the program ends. This way, we only need to keep track of the freed blocks.</p>

# Using the Code

<p>I wanted <code>Allocator</code> to be extremely easy to use, so I created macros to automate the interface within a client class. The macros provide a <code>static</code> instance of <code>Allocator</code> and two member functions: <code>operator new</code> and <code>operator delete</code>. By overloading the <code>new</code> and <code>delete</code> operators, <code>Allocator</code> intercepts and handles all memory allocation duties for the client class.</p>

<p>The <code>DECLARE_ALLOCATOR</code> macro provides the header file interface and should be included within the class declaration like this:</p>

<pre lang="C++">
#include &quot;Allocator.h&quot;
class MyClass
{
    DECLARE_ALLOCATOR
    // remaining class definition
};</pre>

<p>The <code>operator new</code> function calls <code>Allocator</code> to create memory for a single instance of the class. After the memory is allocated, by definition, the <code>operator new</code> calls the appropriate constructor for the class. When overloading <code>new</code> only the memory allocation duties can be taken over. The constructor call is guaranteed by the language. Similarly, when deleting an object, the system first calls the destructor for us, and then <code>operator delete</code> is executed. The <code>operator delete</code> uses the <code>Deallocate()</code> function to store the memory block in the free-list.</p>

<p>Among C++ programmers, it&#39;s relatively common knowledge that when deleting a class using a base pointer, the destructor should be declared virtual. This ensures that when deleting a class, the correct derived destructor is called. What is less apparent, however, is how the virtual destructor changes which class&#39;s overloaded <code>operator delete</code> is called.</p>

<p>Although not explicitly declared, the <code>operator delete</code> is a <code>static</code> function. As such, it cannot be declared virtual. So at first glance, one would assume that deleting an object with a base pointer couldn&#39;t be routed to the correct class. After all, calling an ordinary <code>static</code> function with a base pointer will invoke the base member&#39;s version. However, as we know, calling an <code>operator delete</code> first calls the destructor. With a virtual destructor, the call is routed to the derived class. After the class&#39;s destructor executes, the <code>operator delete</code> for that derived class is called. So in essence, the overloaded <code>operator delete</code> was routed to the derived class by way of the virtual destructor. Therefore, if deletes using a base pointer are performed, the base class destructor must be declared virtual. Otherwise, the wrong destructor and overloaded <code>operator delete</code> will be called.</p>

<p>The <code>IMPLEMENT_ALLOCATOR</code> macro is the source file portion of the interface and should be placed in file scope.</p>

<pre lang="C++">
IMPLEMENT_ALLOCATOR(MyClass, 0, 0)</pre>

<p>Once the macros are in place, the caller can create and destroy instances of this class and deleted object stored will be recycled:</p>

<pre lang="C++">
MyClass* myClass = new MyClass();
delete myClass;</pre>

<p>Both single and multiple inheritance situations work with the <code>Allocator</code> class. For example, assuming the class <code>Derived</code> inherits from class <code>Base</code> the following code fragment is legal.</p>

<pre lang="C++">
Base* base = new Derived;
delete base;</pre>

# Run Time

<p>At run time, <code>Allocator</code> will initially have no blocks within the free-list, so the first call to <code>Allocate()</code> will get a block from the pool or heap. As execution continues, the system demand for objects of any given allocator instance will fluctuate and new storage will only be allocated when the free-list cannot offer up an existing block. Eventually, the system will settle into some peak number of instances so that each allocation will be obtained from existing blocks instead of the pool/heap.</p>

<p>Compared to obtaining all blocks using the memory manager, the class saves a lot of processing power. During allocations, a pointer is just popped off the free-list, making it extremely quick. Deallocations just push a block pointer back onto the list, which is equally as fast.</p>

# Benchmarking

<p>Benchmarking the <code>Allocator</code> performance vs. the global heap on a Windows PC shows just how fast the class is. An basic test of allocating and deallocating 20000 4096 and 2048 sized blocks in a somewhat interleaved fashion&nbsp;tests the speed improvement. See the attached source code for the exact algorithm.&nbsp;</p>

<h4>Windows Allocation&nbsp;Times in Milliseconds</h4>

<table class="ArticleTable">
	<thead>
		<tr>
			<td>Allocator</td>
			<td>Mode</td>
			<td>Run</td>
			<td>Benchmark Time (mS)</td>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td>Global Heap</td>
			<td>Debug Heap</td>
			<td>1</td>
			<td>1640</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Debug Heap</td>
			<td>2</td>
			<td>1864</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Debug Heap</td>
			<td>3</td>
			<td>1855</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Release Heap</td>
			<td>1</td>
			<td>55</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Release Heap</td>
			<td>2</td>
			<td>47</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Release Heap</td>
			<td>3</td>
			<td>47</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Static Pool</td>
			<td>1</td>
			<td>19</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Static Pool</td>
			<td>2</td>
			<td>7</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Static Pool</td>
			<td>3</td>
			<td>7</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Heap Blocks</td>
			<td>1</td>
			<td>30</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Heap Blocks</td>
			<td>2</td>
			<td>7</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Heap Blocks</td>
			<td>3</td>
			<td>7</td>
		</tr>
	</tbody>
</table>

<p>Windows uses a debug heap when executing within the debugger. The debug heap adds extra safety checks slowing its performance. The release heap is much faster as the checks are disabled. The debug heap can be disabled within&nbsp;Visual Studio by setting&nbsp;<strong>_NO_DEBUG_HEAP=1</strong> in the <strong>Debugging &gt; Environment&nbsp;</strong>project option.&nbsp;</p>

<p>The debug global heap is predictably the slowest at about 1.8 seconds. The release heap is much faster at ~50mS. This benchmark&nbsp;test is very simplistic and a more realistic scenario with varying blocks sizes and random new/delete intervals might produce different results. However, the basic point is illustrated nicely; the memory manager is slower than allocator and highly dependent on the platform&#39;s implementation.</p>

<p>The <code>Allocator</code> running in static pool mode doesn&#39;t rely upon the heap. This has a fast&nbsp;execution time of around&nbsp;7mS once the free-list is populated with blocks. The 19mS on Run 1&nbsp;accounts for dicing up the fixed memory pool into individual blocks on the first run.&nbsp;</p>

<p>The <code>Allocator</code> running heap blocks mode is just as fast once the free-list is populated with blocks obtained from the heap. Recall that the heap blocks mode relies upon the global heap to get new blocks, but then recycles them into the free-list for later use. Run 1 shows the allocation hit creating the memory blocks at 30mS. Subsequent benchmarks clock in a very fast 7mS since the free-list is fully populated.&nbsp;</p>

<p>As the benchmarking shows, the <code>Allocator</code> is highly efficient and about seven&nbsp;times faster than the Windows&nbsp;global release heap.</p>

<p>For comparison on an embedded system, I ran the same tests using Keil running on&nbsp;an ARM STM32F4 CPU at&nbsp;168MHz. I had to lower the maximum blocks to 500 and the blocks sizes to 32 and&nbsp;16 bytes due to the constrained resources. Here are the results.</p>

<h4>ARM STM32F4 Allocation&nbsp;Times in Milliseconds</h4>

<table class="ArticleTable">
	<thead>
		<tr>
			<td>Allocator</td>
			<td>Mode</td>
			<td>Run</td>
			<td>Benchmark Time (mS)</td>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td>Global Heap</td>
			<td>Release</td>
			<td>1</td>
			<td>11.6</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Release&nbsp;</td>
			<td>2</td>
			<td>11.6</td>
		</tr>
		<tr>
			<td>Global Heap</td>
			<td>Release&nbsp;</td>
			<td>3</td>
			<td>11.6</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Static Pool</td>
			<td>1</td>
			<td>0.85</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Static Pool</td>
			<td>2</td>
			<td>0.79</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Static Pool</td>
			<td>3</td>
			<td>0.79</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Heap Blocks</td>
			<td>1</td>
			<td>1.19</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Heap Blocks</td>
			<td>2</td>
			<td>0.79</td>
		</tr>
		<tr>
			<td>Allocator</td>
			<td>Heap Blocks</td>
			<td>3</td>
			<td>0.79</td>
		</tr>
	</tbody>
</table>

<p>As the ARM benchmark results show, the <code>Allocator</code> class is about 15 times faster which is quite significant. It should be noted that the benchmark test really aggravated&nbsp;the Keil heap. The benchmark test allocates, in the ARM case, 500 16-byte blocks. Then every other 16-byte block is deleted followed by allocating 500 32-byte blocks. That last group of 500 allocations added 9.2mS to the overall 11.6mS time. What this says is that when the heap gets fragmented, you can expect the memory manager to take longer with non-deterministic times.</p>

# Allocator Decisions

<p>The first decision to make is do you need an allocator at all. If you don&#39;t have an execution speed or fault-tolerance requirement for your project, you probably don&#39;t need a custom allocator and the global heap will work just fine.</p>

<p>On the other hand, if you do require speed and/or fault-tolerance, the allocator can help and the mode of operation depends on your project requirements. An architect for a mission critical design may forbid all use of the global heap. Yet dynamic allocation may lead to a more efficient or elegant design. In this case, you could use the heap blocks mode during debug development to gain memory usage metrics, then for release switch to the <code>static</code> pool method to create statically allocated pools thus eliminating all global heap access. A few compile-time macros switch between the modes.</p>

<p>Alternatively, the heap blocks mode may be fine for the application. It does utilize the heap to obtain new blocks, but does prevent heap-fragmentation faults and speeds allocations once the free-list is populated with enough blocks.</p>

<p>While not implemented in the source code due to multi-threaded issues outside the scope of this article, it is easy have the <code>Allocator</code> constructor keep a <code>static</code> list of all constructed instances. Run the system for a while, then at some pointer iterate through all allocators and output metrics like block count and name via the <code>GetBlockCount()</code> and <code>GetName()</code> functions. The usage metrics provide information on sizing the fixed memory pool for each allocator when switching over to a memory pool. Always add a few more blocks than maximum measured block count to give the system some added resiliency against the pool running out of memory.</p>

# Debugging Memory Leaks

<p>Debugging memory leaks can be very difficult, mostly because the heap is a black box with no visibility into the types and sizes of objects allocated. With <code>Allocator</code>, memory leaks are a bit easier to find since the <code>Allocator</code> tracks the total block count. Repeatedly output (to the console for example) the <code>GetBlockCount()</code> and <code>GetName()</code> for each allocator instance and comparing the differences should expose the allocator with an ever increasing block count.</p>

# Error Handling

<p>Allocation errors in C++ are typically caught using the new-handler function. If the memory manager faults while attempting to allocate memory off the heap, the user&#39;s error-handling function is called via the new-handler function pointer. By assigning the user&#39;s function address to the new-handler, the memory manager is able to call a custom error-handling routine. To make the <code>Allocator</code> class&#39;s error handling consistent, allocations that exceed the pool&#39;s storage capacity also call the function pointed to by new-handler, centralizing all memory allocation faults in one place.</p>

<pre lang="C++">
static void out_of_memory()
{
    // new-handler function called by Allocator when pool is out of memory
    assert(0);
}

int _tmain(int argc, _TCHAR* argv[])
{
    std::set_new_handler(out_of_memory);
...</pre>

# Limitations

<p>The class does not support arrays of objects. An overloaded <code>operator new[]</code> poses a problem for the object recycling method. When this function is called, the <code>size_t</code> argument contains the total memory required for all of the array elements. Creating separate storage for each element is not an option because multiple calls to new don&#39;t guarantee that the blocks are within a contiguous space, which an array needs. Since <code>Allocator</code> only handles same size blocks, arrays are not allowed.</p>

# Porting Issues

<p><code>Allocator</code> calls the <code>new_handler()</code> directly when the <code>static</code> pool runs out of storage, which may not be appropriate for some systems. This implementation assumes the new-handler function will not return, such as an infinite loop trap or assertion, so it makes no sense to call the handler if the function resolves allocation failures by compacting the heap. A fixed pool is being used and no amount of compaction will remedy that.</p>

# Reference Articles

<p><b><a href="https://github.com/endurodave/xallocator">Replace malloc/free with a Fast Fixed Block Memory Allocator</a></b>&nbsp; - use <code>Allocator</code> to create a really fast <code>malloc()</code> and <code>free()</code> CRT replacement.</p>



