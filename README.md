# MiniDB: Visual Database Storage Engine in C++

MiniDB is an educational, disk-backed storage engine built in C++ to make database internals visible through a live browser-based visualizer.

The goal of this project is not to build a production database, but to understand and demonstrate how a database stores records, manages pages, uses memory, handles buffer hits/misses, and evicts pages when the buffer pool is full.

This project was inspired by the database systems concepts taught in CMU's Database Systems course materials, especially storage management, buffer pools, page layouts, and replacement policies.

---

## Demo

The demo shows MiniDB executing real commands in the terminal while the browser visualizer updates from actual runtime events.

The visualizer shows:

- how a logical row becomes serialized bytes
- how tuples are stored inside slotted pages
- how the Free Space Map selects or creates pages
- how disk pages are loaded into the buffer pool
- how buffer hits and buffer misses behave
- how LRU-K selects eviction victims
- how dirty pages are flushed back to disk

---

## Why I Built This

I started learning database internals and wanted a hands-on way to understand what happens below the SQL layer.

Instead of only reading about pages, buffer pools, disk I/O, and eviction policies, I implemented a small storage engine and added a visualizer so each command shows the internal execution flow.

MiniDB helped me connect DBMS concepts with actual implementation details in C++.

---

## Features

### Storage Engine

- Disk-backed page storage
- Fixed-size pages
- Tuple serialization
- Slotted page layout
- Slot directory with offsets and tuple sizes
- Free Space Map for page reuse
- Record identifiers using page ID and slot ID
- Tombstone delete behavior

### Buffer Pool

- 3-frame buffer pool
- Page pinning and unpinning
- Dirty page tracking
- Buffer hit and buffer miss detection
- Disk reads and disk writes
- Dirty page flushing

### Replacement Policy

- LRU-K replacement policy
- Tracks access history for each frame
- Evicts colder pages when memory is full
- Protects frequently accessed pages from eviction

### Visualizer

- Browser-based runtime visualizer
- Real MiniDB execution events
- Flow arrows for internal operations
- Slotted page view
- Buffer pool frame view
- All pages overview
- Event history panel

---

## Architecture

```text
MiniDB CLI
   |
   v
MiniDB Engine
   |
   +--> Tuple Serializer
   |
   +--> Free Space Map
   |
   +--> Slotted Page
   |
   +--> Buffer Pool Manager
          |
          +--> LRU-K Replacer
          |
          +--> Disk Manager
   |
   v
DemoEventLogger
   |
   v
data/events.jsonl
   |
   v
Browser Visualizer
```

---

## Internal Flow

```text
Logical Row
   |
   v
Tuple Serialization
   |
   v
Free Space Map
   |
   v
Slotted Page
   |
   v
Buffer Pool
   |
   v
Disk Manager
```

For reads:

```text
Query / timedget
   |
   v
Check Buffer Pool
   |
   +--> Buffer Hit  -> return from memory
   |
   +--> Buffer Miss -> read page from disk -> load into buffer
```

When the buffer pool is full:

```text
New page needed
   |
   v
LRU-K selects victim frame
   |
   v
Flush dirty page if needed
   |
   v
Reuse frame for new page
```

---

## Project Structure

```text
minidb/
  include/
    buffer_frame.h
    buffer_pool_manager.h
    buffer_pool_metrics.h
    constants.h
    demo_event_logger.h
    disk_manager.h
    free_space_map.h
    lru_k_replace.h
    metrics_exporter.h
    minidb_engine.h
    page.h
    rid.h
    slotted_page.h
    tuple.h

  src/
    buffer_pool_manager.cpp
    demo_event_logger.cpp
    disk_manager.cpp
    free_space_map.cpp
    lru_k_replace.cpp
    main.cpp
    metrics_exporter.cpp
    minidb_engine.cpp
    slotted_page.cpp

  monitoring/
    visualizer.html
    visualizer_server.py

  data/
    runtime database files and event logs

  CMakeLists.txt
  Dockerfile
  docker-compose.yml
  .gitignore
```

---

## How to Run

### 1. Build the Docker environment

```bash
docker compose build
```

### 2. Start MiniDB

```bash
docker compose run --rm minidb-dev
```

Inside the container:

```bash
cd /app
rm -rf build
mkdir build
cd build
cmake ..
make
cd /app
mkdir -p data
rm -f data/minidb.db data/events.jsonl data/trace.log
./build/minidb | tee data/trace.log
```

### 3. Start the visualizer

Open a second terminal and run:

```bash
docker compose --profile monitoring up visualizer-server
```

Then open:

```text
http://localhost:9100/visualizer
```

---

## Demo Commands

Run these commands in MiniDB to see the full internal flow:

```text
show state

explain tuple 101 Sindhu CS 70000

insert 101 Sindhu CS 70000
show page 0

insert 102 alex rr 8000
insertbig 201
insertbig 202
insertbig 203

timedget 101
timedget 102

show buffer
show lruk

insertbig 205
insertbig 206

timedget 205
timedget 201
timedget 203

show lruk
insertbig 207
show metrics
```

---

## What Each Command Shows

### `show state`

Shows the current state of:

- buffer pool
- Free Space Map
- metrics
- operation counts

### `explain tuple`

Shows how a logical row is converted into bytes before storage.

Example:

```text
explain tuple 101 Sindhu CS 70000
```

### `insert`

Serializes the tuple, checks the Free Space Map, selects or creates a page, and stores the tuple in a slotted page.

Example:

```text
insert 101 Sindhu CS 70000
```

### `insertbig`

Inserts a large tuple to force page-level behavior, page allocation, buffer pool pressure, and LRU-K eviction.

Example:

```text
insertbig 201
```

### `timedget`

Fetches a tuple and shows whether it was served from memory or disk.

Example:

```text
timedget 101
```

### `show buffer`

Displays all buffer frames, including page IDs, dirty state, pin count, and last-used information.

### `show lruk`

Shows LRU-K access history and which pages are hot or cold.

### `show metrics`

Shows final runtime metrics such as:

- buffer hits
- buffer misses
- disk reads
- disk writes
- evictions
- dirty flushes
- pages allocated

---

## Concepts Demonstrated

### Tuple Serialization

MiniDB converts logical records into compact byte arrays before writing them into pages.

### Slotted Page Layout

Each page stores tuple bytes and slot metadata. Slots point to tuple offsets and tuple sizes.

### Free Space Map

The Free Space Map tracks how much space is available in each page and helps MiniDB reuse pages before allocating new ones.

### Buffer Pool

The buffer pool caches disk pages in memory. Pages are pinned while in use and unpinned after operations finish.

### Buffer Hit

A buffer hit happens when the requested page is already in memory.

### Buffer Miss

A buffer miss happens when the requested page is not in memory, so MiniDB reads it from disk.

### LRU-K Replacement

LRU-K tracks the last K accesses of each page. When the buffer pool is full, it evicts the page whose K-th most recent access is oldest.

### Dirty Page Flush

Modified pages are marked dirty. Before eviction or during flush, dirty pages are written back to disk.

### Tombstone Delete

MiniDB marks a deleted slot as a tombstone. The tuple is treated as deleted, but the physical space is not compacted immediately.

---

## Metrics Tracked

MiniDB tracks:

```text
buffer_hits
buffer_misses
disk_reads
disk_writes
evictions
dirty_flushes
pages_allocated
insert_count
read_count
scan_count
delete_count
```

These metrics are displayed in the terminal and reflected in the visualizer.

---

## Visualizer

The visualizer is driven by real runtime events written by the C++ engine.

```text
MiniDB command
   |
   v
DemoEventLogger writes data/events.jsonl
   |
   v
visualizer_server.py serves /events
   |
   v
visualizer.html updates the browser UI
```

The visualizer is not hardcoded animation. It updates from actual MiniDB execution events.

---

## Tech Stack

- C++
- CMake
- Docker
- Python
- HTML
- CSS
- JavaScript
- JSONL event logging

---

## Learning Outcome

Through this project, I learned how storage engines manage records and pages internally, how buffer pools reduce disk reads, how replacement policies choose eviction victims, and why dirty pages must be flushed before being removed from memory.

This project strengthened my understanding of:

- database storage internals
- memory management in DBMS
- disk I/O
- page layouts
- buffer pool design
- replacement algorithms
- systems programming in C++

---

## Future Improvements

Planned improvements:

- page compaction after tombstone deletes
- B+ tree indexing
- write-ahead logging
- crash recovery
- SQL parser
- multiple tables
- more replacement policies
- web-based command input for public demos

---

## Disclaimer

MiniDB is an educational project created to learn and visualize database internals. It is not intended to be used as a production database system.
