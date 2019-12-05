# Overview

Nanos implements a tracing mechanism inspired by [Linux'
ftrace](https://www.kernel.org/doc/Documentation/trace/ftrace.txt). Here we
discuss how to enable tracing in nanos and present some utilities for parsing
the resulting trace data.

# Enabling tracing

To enable tracing, build nanos be specifying `TRACE=ftrace` on the make command; e.g.,

```make clean && make TRACE=ftrace TARGET=<target> run```

The resulting kernel will now collect timing data on the invocation of all
kernel function calls.

# Collecting trace data

Once your application is running, access trace data via http:

```wget localhost:9090/ftrace/trace```

This will create a file called `trace` in your working directory that looks like:

```
# tracer: function_graph
#
# CPU  DURATION                  FUNCTION CALLS
# |     |   |                     |   |   |   |
 0)               |  mcache_alloc() {
 0) ! 207.666 us  |    runtime_memcpy();
 0)   0.071 us    |    runtime_memcpy();
 0)   0.077 us    |    objcache_allocate();
 0) ! 208.173 us  |  }  
 0)   0.093 us    |  install_fallback_fault_handler();
 ...
```

# Trace options

While we do not provide nearly the full set of configuration options as Linux,
we do provide a few:

1. Tracer selection

We implement two different types of tracers:

  * #### function_graph
    function_graph tracing ...
