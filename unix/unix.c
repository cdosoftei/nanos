#include <unix_internal.h>

// conditionalize
// fix config/build, remove this include to take off network
#include <net.h>
#include <gdb.h>

static heap processes;


file allocate_fd(process p, bytes size, int *fd)
{
    file f = allocate(p->h, size);
    // check err
    *fd = allocate_u64(p->fdallocator, 1);
    f->offset = 0;
    f->check = 0;
    f->read = f->write = 0;
    p->files[*fd] = f;    
    return f;
}

static boolean node_contents(tuple t, buffer d)
{
    return false;
}    

void default_fault_handler(thread t, context frame)
{
    print_frame(t->frame);
    print_stack(t->frame);    

    if (table_find (t->p->root, sym(fault))) {
        console("starting gdb\n");
        init_tcp_gdb(t->p->h, t->p, 1234);
        thread_sleep(current);
    }
    halt("");
}


static CLOSURE_0_4(stdout, int, void*, u64, u64, status_handler);
static int stdout(void *d, u64 length, u64 offset, status_handler s)
{
    u8 *z = d;
    for (int i = 0; i< length; i++) {
        serial_out(z[i]);
    }
    return length;
}

static u64 futex_key_function(void *x)
{
    return u64_from_pointer(x);
}

static boolean futex_key_equal(void *a, void *b)
{
    return a == b;
}

static void *linux_syscalls[SYS_MAX];

process create_process(heap h, heap pages, heap physical, tuple root)
{
    process p = allocate(h, sizeof(struct process));
    p->h = h;
    p->brk = 0;
    p->pid = allocate_u64(processes, 1);
    // xxx - take from virtual allocator
    p->virtual = create_id_heap(h, 0x7000000000ull, 0x10000000000ull, 0x100000000);
    p->virtual32 = create_id_heap(h, 0x10000000, 0xe0000000, PAGESIZE);
    p->pages = pages;
    p->cwd = root;
    p->root = root;
    p->fdallocator = create_id_heap(h, 3, FDMAX - 3, 1);
    p->physical = physical;
    zero(p->files, sizeof(p->files));
    p->files[1] = allocate(p->h, sizeof(struct file));
    p->files[1]->write = closure(p->h, stdout);
    p->files[2] = p->files[1];
    p->futices = allocate_table(h, futex_key_function, futex_key_equal);
    p->threads = allocate_vector(h, 5);
    p->syscall_handlers = linux_syscalls;
    return p;
}

void *syscall;

#define offsetof(__t, __e) u64_from_pointer(&((__t)0)->__e)


// return value is fucked up and need ENOENT - enoent could be initialized
buffer install_syscall(heap h)
{
    buffer b = allocate_buffer(h, 100);
    int working = REGISTER_A;
    rprintf ("current location: %p\n", current);
    mov_64_imm(b, working, u64_from_pointer(current));
    indirect_displacement(b, REGISTER_A, REGISTER_A, offsetof(thread, p));
    indirect_displacement(b, REGISTER_A, REGISTER_A, offsetof(process, syscall_handlers));
    indirect_scale(b, REGISTER_A, 3, REGISTER_B, REGISTER_A);
    jump_indirect(b, REGISTER_A);
    return b;
}

extern char *syscall_name(int);
static u64 syscall_debug()
{
    u64 *f = current->frame;
    int call = f[FRAME_VECTOR];
    //    if (table_find(current->p->root, sym(debugsyscalls)))
    //        thread_log(current, syscall_name(call));

    u64 (*h)(u64, u64, u64, u64, u64, u64) = current->p->syscall_handlers[call];
    u64 res = -ENOENT;
    if (h) {
        res = h(f[FRAME_RDI], f[FRAME_RSI], f[FRAME_RDX], f[FRAME_R10], f[FRAME_R8], f[FRAME_R9]);
    } else {
        rprintf("nosyscall %s\n", syscall_name(call));
    }
    return res;
}

void init_unix(heap h, heap pages, heap physical, tuple root)
{
    set_syscall_handler(syscall_enter);
    processes = create_id_heap(h, 1, 65535, 1);
    process kernel = create_process(h, pages, physical, root);
    current = create_thread(kernel);
    frame = current->frame;
    init_vdso(physical, pages);
    register_file_syscalls(linux_syscalls);
#ifdef NET    
    register_net_syscalls(linux_syscalls);
#endif
    register_signal_syscalls(linux_syscalls);
    register_mmap_syscalls(linux_syscalls);
    register_thread_syscalls(linux_syscalls);
    register_poll_syscalls(linux_syscalls);
    //buffer b = install_syscall(h);
    //syscall = b->contents;
    // debug the synthesized version later, at least we have the table dispatch
    syscall = syscall_debug;
}


