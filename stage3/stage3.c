#include <runtime.h>
#include <kvm_platform.h>
#include <tfs.h>
#include <unix.h>
#include <gdb.h>

void add_elf_syms(heap h, buffer b);

static CLOSURE_7_1(read_program_complete, void, tuple, heap, heap, heap, heap, heap, filesystem, buffer);
static void read_program_complete(tuple root, heap pages, heap general, heap physical, heap virtual, heap backed,
                                  filesystem fs, buffer b)
{
    add_elf_syms(general, b);
    rprintf ("read program complete: %p %v\n", root, root);
    exec_elf(b, root, root, general, physical, pages, virtual, backed, fs);
}

static CLOSURE_0_1(read_program_fail, void, status);
static void read_program_fail(status s)
{
    console("fail\n");
    halt("read program failed %v\n", s);
}

void startup(heap pages,
             heap general,
             heap physical,
             heap virtual,
             tuple root)
{
    init_unix(general, pages, physical, root);
    value p = table_find(root, sym(program));
    // error on not program 
    // copied from service.c - how much should we pass?
    heap virtual_pagesized = allocate_fragmentor(general, virtual, PAGESIZE);
    heap backed = physically_backed(general, virtual_pagesized, physical, pages);
    value_handler pg = closure(general, read_program_complete, root, pages, general, physical, virtual, backed);

    vector path = split(general, p, '/');
    tuple pro = resolve_path(root, path);
    get(pro, sym(contents), pg);
}

