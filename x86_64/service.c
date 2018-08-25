#include <runtime.h>
#include <kvm_platform.h>
#include <pci.h>
#include <virtio.h>
#include <tfs.h>

extern void startup();
extern void start_interrupts();
// to avoid passing through tagged
// use 2m physical and fragment
static heap physical_memory;
static heap pages;

// doesnt belong here
void startup(heap pages,
             heap general,
             heap physical,
             heap virtual,
             tuple root);

// xxx -this is handing out a page per object
heap allocate_tagged_region(heap h, u64 tag)
{
    return physically_backed(h,
                             create_id_heap(h, tag << va_tag_offset, 1ull<<va_tag_offset, physical_memory->pagesize),
                             physical_memory, pages);    
}


static u8 bootstrap_region[4096];
static u64 bootstrap_base = (unsigned long long)bootstrap_region;
static u64 bootstrap_alloc(heap h, bytes length)
{
    u64 result = bootstrap_base;
    if ((result + length) >=  (u64_from_pointer(bootstrap_region) + sizeof(bootstrap_region)))
        return INVALID_PHYSICAL;
    bootstrap_base += length;
    return result;
}

queue runqueue;

static CLOSURE_1_0(read_complete, void, thunk);
static void read_complete(thunk target)
{
    enqueue(runqueue, target);
}

static context miscframe;

void runloop()
{
    thunk t;
    while(1) {
        // hopefully overall loop is being driven by the lapic periodic interrupt,
        // which should limit the skew
        timer_check();
        
        while((t = dequeue(runqueue))) {
            apply(t);
        }
        frame = miscframe;
        enable_interrupts();
        __asm__("hlt");
        disable_interrupts();
    }
}

static CLOSURE_2_3(offset_block_write, void, block_write, u64, buffer, u64, status_handler);
static void offset_block_write(block_write w, u64 start, buffer b, u64 offset, status_handler h)
{
    apply(w, b, start + offset, h);
}

static CLOSURE_2_4(offset_block_read, void, block_read, u64, void *, u64, u64, status_handler);
static void offset_block_read(block_read r, u64 start, void *dest, u64 length, u64 offset, status_handler h)
{
    apply(r, dest, length, start + offset, h);
}

void init_extra_prints(); 

CLOSURE_5_0(startup, void, heap, heap, heap, heap, tuple);

static CLOSURE_2_1(fsstarted, void, heap, heap, value);
static void fsstarted(heap h, heap virtual, value root)
{
    enqueue(runqueue, closure(h, startup, pages, h, physical_memory, virtual, root));
}

CLOSURE_3_3(attach_storage, void, heap, heap, tuple, block_read, block_write, u64);
void attach_storage(heap h, heap virtual, tuple root, block_read r, block_write w, u64 length)
{
    u64 fs_offset;
    
    // bios couldn't probe length(?) .. so from the virtio driver...remember
    // the location of that block
    for_regions(e)
        if (region_type(e) == REGION_FILESYSTEM) {
            fs_offset = region_base(e);
        }

    // with filesystem...should be hidden as functional handlers on the tuplespace    
    create_filesystem(h,
                      512, // from the device please
                      length,
                      closure(h, offset_block_read, r, fs_offset),
                      closure(h, offset_block_write, w, fs_offset),
                      closure(h, fsstarted, h, virtual));

    runloop();
}

static void read_kernel_syms(heap h, heap virtual, heap pages)
{
    u64 kern_base = INVALID_PHYSICAL;
    u64 kern_length;

    /* add kernel symbols */
    for_regions(e)
	if (region_type(e) == REGION_KERNIMAGE) {
	    kern_base = region_base(e);
	    kern_length = region_length(e);

	    u64 v = allocate_u64(virtual, kern_length);
	    map(v, kern_base, kern_length, pages);
#ifdef ELF_SYMTAB_DEBUG
	    rprintf("kernel ELF image at %P, length %P, mapped at %P\n",
		    kern_base, kern_length, v);
#endif
	    add_elf_syms(h, alloca_wrap_buffer(v, kern_length));
	}
    
    if (kern_base == INVALID_PHYSICAL) {
	console("kernel elf image region not found; no debugging symbols\n");
    }
}

void init_service_new_stack(heap pages, heap physical, heap backed, heap virtual)
{
    // just to find maintain the convention of faulting on zero references
    map(0, INVALID_PHYSICAL, PAGESIZE, pages);

    heap misc = allocate_rolling_heap(backed, 8);
    //    misc = debug_heap(misc, misc); 
    runqueue = allocate_queue(misc, 64);
    start_interrupts(pages, misc, physical);
    init_extra_prints();
    init_runtime(misc);
    init_symtab(misc);
    read_kernel_syms(misc, virtual, pages);
    
    tuple root = allocate_tuple();
    initialize_timers(misc);
    init_pci(misc);
    init_virtio_storage(misc, backed, pages, virtual, closure(misc, attach_storage, misc, virtual, root));
    init_virtio_network(misc, backed, pages);
    init_clock(backed);
    miscframe = allocate(misc, FRAME_MAX * sizeof(u64));
    pci_discover(pages, virtual);
    // just to get the hlt loop to wake up and service timers. 
    // should change this to post the delta to the front of the queue each time
    configure_timer(milliseconds(50), ignore);
}

// init linker set
void init_service()
{
    struct heap bootstrap;

    bootstrap.alloc = bootstrap_alloc;
    bootstrap.dealloc = leak;
    pages = region_allocator(&bootstrap, PAGESIZE, REGION_IDENTITY);
    physical_memory = region_allocator(&bootstrap, PAGESIZE, REGION_PHYSICAL);    

    heap virtual = create_id_heap(&bootstrap, HUGE_PAGESIZE, (1ull<<VIRTUAL_ADDRESS_BITS)- HUGE_PAGESIZE, HUGE_PAGESIZE);
    heap virtual_pagesized = allocate_fragmentor(&bootstrap, virtual, PAGESIZE);

    heap backed = physically_backed(&bootstrap, virtual_pagesized, physical_memory, pages);

    u64 stack_size = 32*PAGESIZE;
    u64 stack_location = allocate_u64(backed, stack_size);
    
    stack_location += stack_size - 16;
    asm ("mov %0, %%rsp": :"m"(stack_location));
    init_service_new_stack(pages, physical_memory, backed, virtual); 
}
