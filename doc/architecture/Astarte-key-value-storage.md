# Astarte key-value storage architecture

<!--
Copyright 2026 SECO Mind Srl

SPDX-License-Identifier: Apache-2.0
-->

This library provides a namespaced, persistent key-value storage solution specifically designed for Zephyr RTOS. It leverages the Zephyr Memory Storage (ZMS) subsystem as its underlying non-volatile backend.

## ZMS ID mapping and memory layout

The library abstracts standard key-value pairs by mapping them to discrete 32-bit ZMS IDs. The ID space is segmented to reserve specific addresses for metadata, system states, and actual user data.

| ZMS reserved IDs                           | Value          | Purpose                                                                         |
| ------------------------------------------ | -------------- | ------------------------------------------------------------------------------- |
| `ASTARTE_KEY_VALUE_ENTRY_MIN_USABLE_ID`    | 0x1FFFF + 1    | The minimum allowable ID for standard key-value payloads.                       |
| `ASTARTE_KEY_VALUE_ENTRY_MAX_USABLE_ID`    | UINT32_MAX - 4 | The maximum allowable ID for standard key-value payloads.                       |
| `ASTARTE_KEY_VALUE_ENTRY_INTENT_ID`        | UINT32_MAX - 3 | Stores the Write-Ahead Log (WAL) intent block.                                  |
| `ASTARTE_KEY_VALUE_ENTRY_HEAD_AND_TAIL_ID` | UINT32_MAX - 2 | Stores the global linked list's head and tail pointers.                         |
| `ASTARTE_KEY_VALUE_ENTRY_VERSION_ID`       | UINT32_MAX - 1 | Stores the formatting version to detect breaking changes during initialization. |
| `ASTARTE_KEY_VALUE_ENTRY_NULL_ID`          | UINT32_MAX	  | Represents a null/invalid pointer in the linked list.                           |

## Hashing and collision resolution

To locate or allocate ZMS IDs for a given namespace and key, the library utilizes a custom hashing implementation.
- The namespace and key strings are individually hashed using Zephyr's optimized `sys_hash32` function.
- These two hashes are mathematically combined using a fixed permutation constant (`0x9e3779b9U`).
- The resulting combined hash is mapped into the usable ZMS ID range using Lemire's Multiply-and-Shift Method for fast and unbiased range reduction.
- Hash collisions are handled natively via linear probing.
- If a collision occurs, the driver linearly increments the ZMS ID until it finds a matching key or an empty slot, wrapping around to the minimum usable ID if the maximum is exceeded.

## ZMS entry structure

When a key-value pair is committed to ZMS, it is serialized into a single contiguous block. The entry is divided into a fixed-size header and a dynamically sized data section.

| Field name       | Size     | Description                                                    |
| -----------------| -------- | -------------------------------------------------------------- |
| `namespace_len`  | 2	      | Length of the namespace string.                                |
| `key_len`        | 2        | Length of the key string.                                      |
| `next_id`        | 4        | ZMS ID of the next entry in the linked list.                   |
| `prev_id`        | 4        | ZMS ID of the previous entry in the linked list.               |
| Namespace string | Variable | The literal namespace characters, excluding a null terminator. |
| Key string       | Variable | The literal key characters, excluding a null terminator.       |
| Value data       | Variable | The binary payload for the key-value pair.                     |

## Global linked list and iteration

Because ZMS does not natively provide a way to iterate through keys sorted by a namespace, the library maintains a global doubly-linked list of all stored entries.
- The absolute head_id and tail_id are continuously updated and stored in the specialized `ASTARTE_KEY_VALUE_ENTRY_HEAD_AND_TAIL_ID` record.
- Every insertion dynamically patches the previous tail's `next_id` and points the new node back via `prev_id`.
- Iterators are initialized at the head of the list and traverse the structure sequentially.
- During iteration, the driver aggressively checks the namespace of the next node, automatically skipping entries that belong to distinct namespaces without exposing them to the user.

## Data integrity and power-loss resilience

A critical architectural feature of this library is its resilience to sudden power losses during multi-step ZMS operations. This is achieved using an Intent Block, acting as a Write-Ahead Log (WAL).

### The intent block

Before the library makes physical mutations to the linked list or hash map, it registers its intention at `ASTARTE_KEY_VALUE_ENTRY_INTENT_ID`. The intent states include:
- `ASTARTE_KEY_VALUE_ENTRY_INTENT_NONE`: The system is stable; no pending operations.
- `ASTARTE_KEY_VALUE_ENTRY_INTENT_INSERTING`: Tracks dangling pointers during a 3-step node insertion.
- `ASTARTE_KEY_VALUE_ENTRY_INTENT_UPDATING`: Tracks a localized payload swap.
- `ASTARTE_KEY_VALUE_ENTRY_INTENT_DELETING`: Tracks unlinking a node from the global list.
- `ASTARTE_KEY_VALUE_ENTRY_INTENT_SHIFTING`: Tracks the hash map repair process following a deletion.

### Initialization and recovery

When `astarte_key_value_open` mounts the ZMS partition, it immediately triggers `astarte_key_value_entry_intent_resolve`.
- If an INSERTING intent is found, the library identifies orphaned payloads and reverts the dangling `next_id` of the previous tail node.
- If a DELETING intent is found, the library patches the disconnected `prev_id` and `next_id` neighbors before physically deleting the target payload.
- If a SHIFTING intent is found, the library resumes the gap-filling process required to maintain the integrity of linear probing.

### Deletion and memory compaction

To ensure the linear probing sequence remains unbroken after an entry is removed, the library executes a "shift back" routine.
- Once an entry is unlinked and deleted, the driver transitions to the SHIFTING intent state.
- It calculates the cyclic absolute distance of subsequent elements from their natural hashes.
- If a subsequent entry is found to be closer to the "hole" than its current physical location, the entry is physically relocated to fill the hole.
- The routine updates the shifted entry's linked-list neighbors to reflect its new ZMS ID and continues moving the hole downward until the end of the probing cluster is reached.
