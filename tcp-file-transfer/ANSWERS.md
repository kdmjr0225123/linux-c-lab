# TCP File Transfer Lab: Results and Answers

## Final Protocol

All integers are in network byte order.

| Direction | Fields |
|---|---|
| Client to server (v1) | `[name_len u32][filename]` |
| Client to server (v2) | `[cmd u8 'G'/'P'][name_len u32][filename]`, uploads add `[file_size u64][sha256 32B]` |
| Server to client | `[status u8][file_size u64][sha256 32B]` followed by file data on a download |

The starter used `file_size == 0` to mean "not found," which breaks for empty files. A status byte (OK, NOT_FOUND, TOO_LARGE, BAD_NAME, BUSY, CHECKSUM_FAIL, IO_ERROR) fixes that. The server sends the SHA-256 in the header, and the client hashes the bytes as they arrive and compares at the end.

## Version 1 Required Tasks

1. **Error checking.** Every `socket`, `setsockopt`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, and `close` call is checked. Setup failures exit with `strerror(errno)`. Per-client failures are logged and the server keeps running. `SIGPIPE` is ignored so a client disconnecting mid-transfer can't kill the server.
2. **Logging.** Each request logs the client IP and port, the filename, the size, and the first 16 hex characters of the hash. Completion logs bytes sent, time, and throughput.
3. **SHA-256.** Implemented in `common/sha256.h`, so no OpenSSL dependency. Verified against `sha256sum`.
4. **Transfer timing.** The client reports *wait* (connect until the header arrives), *transfer* (header until the last byte), and *total*. The server logs service time per request.
5. **Blocking behavior.** With 10 simultaneous clients, every `connect()` succeeds right away because the kernel completes the TCP handshake and parks the connection in the listen backlog. The server only `accept()`s the next one after finishing the current transfer. The client logs show wait times climbing in a staircase: client 1 waits about 0 ms, client 10 waits about 9 times the service time. If more clients arrive than the backlog holds (5 in v1), the extra SYNs are dropped and those clients stall in connect retries.

## Performance Comparison

10 concurrent clients, 50 MB file, localhost. Times are in seconds or milliseconds as labeled.

| Server | Total time (s) | Avg wait (ms) | Avg response (ms) | Max response (ms) | CPU usage | Peak memory (MB) | Threads |
|---|---|---|---|---|---|---|---|
| v1 sequential | 4.87 | 2427 | 2703 | 4850 | 50.3% | 3.9 | 1 |
| v2 pool, 4 threads | 2.65 | 1246 | 1792 | 2635 | 89.7% | 4.1 | 5 |

The pool finished about 1.8x faster and cut average wait roughly in half. Total CPU seconds were nearly identical (2.45 vs 2.38). The pool didn't do less work; it did the same work in parallel, which is why CPU usage went from about one core (50% of 2) to nearly both. The speedup is capped near 2x because this machine has 2 cores and each request is CPU-bound (hashing plus copying through loopback). Memory barely changed because each worker only adds a stack and a 4 KB buffer.

## Challenge Answers

### Challenge 1: Large File Support

`file_size` is now `uint64_t` (sent with `htobe64`/`be64toh`). A `uint32_t` tops out at 4,294,967,295 bytes, about 4 GiB. Disk images, videos, database dumps, and backups regularly exceed that. With 32 bits, the value silently wraps around, so the client would read the wrong number of bytes and either hang or truncate the file. A 64-bit size supports up to 16 EiB, which is effectively unlimited.

### Challenge 2: Partial send()/recv()

`send_all()` and `recv_all()` in `common/net.h` loop until the full length has moved, retry on `EINTR`, and report errors or early EOF.

TCP is a byte stream with no message boundaries. `recv()` returns whatever has arrived so far, which may be one segment of a larger write or several writes merged together. `send()` only copies into the kernel's socket send buffer. If that buffer is nearly full (the receiver is slow or the window is small), it accepts only part of the data. A signal can also interrupt either call partway. The starter code assumed a single `recv()` would return all 4 bytes of the length, which usually works on localhost and fails over a real network.

### Challenge 3: Thread Pool Metrics

The pool tracks an `std::atomic<int>` of active workers, logs the queue length on every enqueue and dequeue, and keeps atomic totals of service time and time spent waiting in the queue. A `METRICS` line prints after every request, and a `FINAL` line prints on Ctrl+C.

A client's latency is its queue wait plus its service time. When requests arrive faster than the workers can finish them, the queue grows, and each new request waits behind everything ahead of it. Wait time is roughly (queue length / workers) × average service time. Service time stays flat, but wait time grows linearly with the queue, and if the arrival rate stays above capacity, the queue and the latency keep growing without bound. The benchmark logs show this: the last clients dequeued had waits several times longer than their actual service time.

### Challenge 4: File Upload Support

`./client -u <path>` uploads a file. The client sends the name, size, and SHA-256 first, and the server replies "ready" or refuses (bad name, too large). Then the data streams, and the server sends a final status.

To store files safely:
- The name is checked by `safe_filename()`, which rejects `/`, `\`, leading dots, and names over 255 characters, so `../../etc/passwd` can't escape the folder.
- Data goes into a unique temp file in `uploads/` (`O_CREAT | O_EXCL`, named with a per-upload counter), then gets `fsync`'d and hash-checked.
- Only if the hash matches is it `rename()`d into place. A corrupt or aborted upload is deleted.

For concurrent uploads, each upload writes to its own temp file, so workers never share a file descriptor or need a lock. `rename()` is atomic, so two clients uploading the same name at once both succeed and the last one to finish wins. The final file is always one complete upload, never bytes mixed from both. This was tested with two different files uploaded under the same name at the same time.

### Challenge 5: Resource Protection

| Limit | Implementation |
|---|---|
| Max file size | `MAX_FILE_SIZE` (1 GiB). Downloads and uploads over the limit get `TOO_LARGE` before any data moves. |
| Max concurrent connections | `MAX_CONNECTIONS` (16, counting queued plus active). The accept thread checks `pool.pending()` and answers extra clients with `SERVER_BUSY` instead of queuing them. |
| Idle timeout | `SO_RCVTIMEO` and `SO_SNDTIMEO` set to 10 s on each client socket. A client that sends nothing, or stops reading, gets dropped and the worker is freed. |

Every connection holds a file descriptor, kernel buffer memory, and here a worker thread. An attacker doesn't need bandwidth to exhaust these. A **slowloris**-style client opens a connection and sends one byte every few seconds. With 4 workers, 4 such clients freeze the whole server, and without a timeout they hold it forever. Other attacks include opening thousands of connections to exhaust file descriptors or fill the queue (unbounded queues also eat memory), **SYN floods** that fill the half-open backlog, and requesting or uploading huge files to burn disk, CPU, and bandwidth. The defenses are the ones above: timeouts, caps on connections and sizes, and refusing work early rather than queuing it.

## Reflection: When to Use a Thread Pool

**Use a thread pool when:**
- Requests are independent and short to medium length, like a file server, web server, or RPC handler. Work spreads across cores and one slow client doesn't block everyone.
- Load is bursty. A fixed pool absorbs bursts in the queue instead of spawning hundreds of threads, which would cost memory and context switches.
- You want predictable resource use. The thread count is a hard cap you choose, and it's easy to pair with a queue limit for backpressure.
- The work mixes CPU and blocking I/O, so some workers can run while others wait on disk or network.

**Don't use a thread pool when:**
- There's only one client at a time, or load is tiny. The sequential server is simpler and just as fast.
- You have thousands of mostly-idle long-lived connections (chat, websockets). A blocking thread per connection can't scale there. Event-driven I/O (`epoll`, `io_uring`, async runtimes) handles many sockets with few threads.
- Tasks are very long or unbounded, like streaming a 50 GB file. A few of them occupy every worker and starve short requests, unless you use separate pools or timeouts.
- The work is serialized anyway, for example every request needs the same global lock or writes the same file. Threads just add contention.
- The machine has one core and the work is pure CPU. The pool adds overhead without adding parallelism.

The benchmark backs this up: on 2 cores the pool gave about 1.8x, not 4x, because 4 CPU-bound workers can't outrun 2 cores. Pool size should match the bottleneck (cores for CPU-bound work, more threads for I/O-bound work), not the expected number of clients.
