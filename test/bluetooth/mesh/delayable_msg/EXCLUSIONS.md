# Scheduler-specific cases

The following Zephyr cases are explicitly unavailable on the POSIX OSI baseline:

- `test_concurrent_manage`: requires cooperative priorities above the system worker.
- `test_stop_during_in_flight_send`: requires the same non-preemptible enqueue/stop interleaving.
- `test_stale_handler_no_early_send`: depends on busy-waiting without letting the worker run.

The cmocka entries report skip. No priority API or IRQ lock is emulated.
The original implementations remain available in Zephyr commit
`f8da8ea370508f274685a25ff19dbe5a9ff54790`,
`tests/bluetooth/mesh/delayable_msg/src/main.c`.
The seven other cases execute the actual delayable-message implementation.
