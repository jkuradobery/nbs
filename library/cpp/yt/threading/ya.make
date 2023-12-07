LIBRARY()

SRCS(
    at_fork.cpp
    event_count.cpp
    fork_aware_spin_lock.cpp
    fork_aware_rw_spin_lock.cpp
    futex.cpp
    public.cpp
    recursive_spin_lock.cpp
    rw_spin_lock.cpp
    spin_lock_base.cpp
    spin_lock.cpp
    spin_wait.cpp
    spin_wait_hook.cpp
)

PEERDIR(
    library/cpp/yt/assert
    library/cpp/yt/cpu_clock
    library/cpp/yt/system
    library/cpp/yt/memory
)

END()

RECURSE_FOR_TESTS(unittests)
