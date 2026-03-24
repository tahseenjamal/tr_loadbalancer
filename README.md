# FE + Redis + TR Architecture (Detailed README)

## Overview

This document describes the **specialized M3UA frontend architecture** designed for a **TCAP router cluster**.

This architecture is **not a generic SS7 STP replacement**.

Instead, it is a **purpose-built, dialog-aware M3UA frontend layer** that can sit between:

- **Telecom / MNO network peers** (M3UA / SCTP side)
- **TR (TCAP Router) cluster**
- **Backend applications** behind the TR layer

The purpose of this design is to provide:

- **M3UA/SCTP termination at the frontend**
- **Dialog ownership tracking** (OTID / DTID)
- **Return path routing to the correct TR**
- **Horizontal scale using multiple FE nodes**
- **Shared ownership state using Redis**
- **High-throughput, narrow-purpose packet forwarding**

---

# 1) High-Level Architecture Diagram

```text
                         ┌──────────────────────────────┐
                         │         Telecom / MNO        │
                         │   (M3UA / SCTP Peer Side)    │
                         └──────────────┬───────────────┘
                                        │
                         ┌──────────────┴───────────────┐
                         │   FE Cluster (M3UA Frontend) │
                         │  Specialized SCTP/M3UA LB    │
                         └──────────────┬───────────────┘
                                        │
          ┌─────────────────────────────┼─────────────────────────────┐
          │                             │                             │
          │                             │                             │
┌─────────▼─────────┐         ┌─────────▼─────────┐         ┌─────────▼─────────┐
│       FE-1        │         │       FE-2        │         │       FE-N        │
│  C Frontend Node  │         │  C Frontend Node  │         │  C Frontend Node  │
│  M3UA/SCTP Peer   │         │  M3UA/SCTP Peer   │         │  M3UA/SCTP Peer   │
│  Local cache      │         │  Local cache      │         │  Local cache      │
└─────────┬─────────┘         └─────────┬─────────┘         └─────────┬─────────┘
          │                             │                             │
          └───────────────┬─────────────┴─────────────┬───────────────┘
                          │                           │
                    ┌─────▼───────────────────────────▼─────┐
                    │          Redis Shared State           │
                    │   OTID/DTID -> TR ownership mapping   │
                    │   Cross-FE dialog stickiness store    │
                    └─────┬───────────────────────────┬─────┘
                          │                           │
          ┌───────────────┴─────────────┬─────────────┴───────────────┐
          │                             │                             │
          │                             │                             │
┌─────────▼─────────┐         ┌─────────▼─────────┐         ┌─────────▼─────────┐
│       TR-1        │         │       TR-2        │         │       TR-N        │
│   C TCAP Router   │         │   C TCAP Router   │         │   C TCAP Router   │
│ Dialog-aware LB   │         │ Dialog-aware LB   │         │ Dialog-aware LB   │
│ Backend stickiness│         │ Backend stickiness│         │ Backend stickiness│
└─────────┬─────────┘         └─────────┬─────────┘         └─────────┬─────────┘
          │                             │                             │
          └───────────────┬─────────────┴─────────────┬───────────────┘
                          │                           │
                 ┌────────▼────────┐         ┌────────▼────────┐
                 │  Backend App-1  │         │  Backend App-N  │
                 │  Sends M3UA     │         │  Sends M3UA     │
                 │  Receives replies│        │  Receives replies│
                 └───────────────────┘         └───────────────────┘
````

---

# 2) Simplified Clean Diagram

```text
+----------------------+
|   Telecom / M3UA     |
+----------+-----------+
           |
           |
   +-------+--------+------------------+
   |                |                  |
   |                |                  |
+--v---+         +--v---+           +--v---+
| FE-1 |         | FE-2 |    ...    | FE-N |
+--+---+         +--+---+           +--+---+
   |                |                  |
   +-------+--------+------------------+
           |
     +-----v------+
     |   Redis    |
     | Shared Map |
     | OTID/DTID  |
     +-----+------+
           |
   +-------+--------+------------------+
   |                |                  |
   |                |                  |
+--v---+         +--v---+           +--v---+
| TR-1 |         | TR-2 |    ...    | TR-N |
+--+---+         +--+---+           +--+---+
   |                |                  |
   +-------+--------+------------------+
           |
     +-----v------+
     | Backend(s) |
     +------------+
```

---

# 3) Multi-FE / Multi-TR Realistic Topology (Recommended)

```text
                         +----------------------+
                         |   Telecom / M3UA     |
                         +----------+-----------+
                                    |
                    +---------------+---------------+
                    |                               |
               +----v----+                     +----v----+
               |  FE-1   |<------Redis-------> |  FE-2   |
               +----+----+                     +----+----+
                    |\                             /|
                    | \                           / |
                    |  \                         /  |
                    |   \                       /   |
               +----v----+---------------------+----v----+
               |               TR-1 / TR-2 / TR-N        |
               |  each TR connected to multiple FE nodes |
               +--------------------+--------------------+
                                    |
                             +------v------+
                             | Backend App |
                             +-------------+
```

---

# Architecture Summary

## Core idea

The FE layer is a **specialized M3UA frontend** that:

* Accepts M3UA/SCTP from TR nodes
* Maintains minimal ASP state (ASPUP / ASPAC)
* Forwards M3UA DATA to the telecom side
* Tracks dialog ownership (OTID / DTID)
* Uses **Redis** as a shared ownership store
* Routes inbound traffic back to the correct TR

This gives you:

* **Horizontal scaling**
* **Cross-FE dialog awareness**
* **Failover-friendly ownership state**
* **Much simpler hot path than a generic STP**

---

# What Each Component Does

## Telecom / MNO

This is the external SS7 / SIGTRAN side.

Typical characteristics:

* M3UA over SCTP
* Sends and receives TCAP-bearing traffic
* May be an STP, SMSC-facing network element, HLR/MAP side, USSD side, etc.

The FE cluster behaves as the **M3UA peer** toward this side.

---

## FE (Frontend) Nodes

The FE nodes are **specialized M3UA frontend relays**.

### FE responsibilities

* Listen for TR connections on M3UA/SCTP
* Accept ASPUP / ASPAC from TR
* Maintain minimal TR-side association state
* Maintain upstream telecom-side association(s)
* Parse enough of M3UA/SCCP/TCAP to extract OTID / DTID
* Claim and refresh dialog ownership
* Forward outbound traffic to telecom
* Route inbound traffic to the correct TR

### FE should remain narrow-purpose

FE is **not** intended to be:

* Full SS7 STP
* Full SCCP router
* Full M3UA network management implementation
* Generic telecom core signaling switch

Instead, FE is:

> A **dialog-aware M3UA pass-through / relay layer**

This narrow scope is what makes it lightweight and scalable.

---

## Redis

Redis acts as the **shared dialog ownership store**.

Typical keys:

* `otid:<id> -> tr:<n>`
* later optionally `dtid:<id> -> tr:<n>`

### Why Redis is used

When multiple FE nodes exist:

* FE-1 may forward outbound BEGIN and claim ownership
* Response may return to FE-2
* FE-2 must still know which TR owns that dialog

Redis provides:

* Shared ownership state across FE nodes
* Cross-node dialog stickiness
* Recovery after FE restarts (within TTL window)
* Operational simplicity

### Important production note

Redis should **not** be the only hot-path lookup for every packet.

Best practice:

* **Local cache first**
* **Redis second**

More on that below.

---

## TR (TCAP Router) Nodes

TR nodes are your **dialog-aware TCAP routers**.

### TR responsibilities

* Accept M3UA from backend apps
* Parse enough M3UA / SCCP / TCAP to identify dialogs
* Maintain backend-side dialog stickiness
* Route all packets of a transaction to the correct backend
* Send outbound traffic to FE
* Receive return traffic from FE and forward to correct backend

TR is where your application-specific intelligence lives.

---

## Backend Applications

Backend apps are the service logic layer.

Examples:

* USSD application
* MAP service logic
* HLR/HSS integration logic
* OTP / service control platform
* Internal business logic engines

In your current design:

* Backend app speaks M3UA toward TR
* TR maintains backend dialog affinity
* FE maintains TR dialog affinity toward telecom

This creates a **two-stage stickiness model**:

1. **Telecom -> FE -> correct TR**
2. **TR -> correct backend app**

---

# Packet Flow

## Outbound Path

```text
Backend App -> TR -> FE -> Telecom
```

### Detailed outbound flow

1. Backend app sends M3UA packet to TR
2. TR parses M3UA / SCCP / TCAP
3. TR identifies OTID (for BEGIN) or DTID (for subsequent dialogs)
4. TR applies backend dialog routing internally
5. TR forwards M3UA packet to FE
6. FE parses enough to extract dialog ID
7. FE claims or refreshes ownership:

   * local cache update
   * Redis SET / SETNX / refresh TTL
8. FE forwards the packet upstream to telecom

---

## Inbound Path

```text
Telecom -> FE -> Redis lookup -> correct TR -> Backend App
```

### Detailed inbound flow

1. Telecom sends M3UA packet to FE
2. FE extracts dialog identifier (OTID / DTID depending on phase)
3. FE checks **local ownership cache**
4. On local miss, FE checks **Redis**
5. FE resolves owning TR
6. FE forwards the packet to that TR
7. TR processes the packet
8. TR routes to the correct backend app

---

# Why TR Should Connect to Multiple FE Nodes

This is one of the most important design points.

## Problem if TR connects to only one FE

Example:

* TR-1 is connected only to FE-1
* FE-1 claims ownership for `otid:10203`
* Telecom response later lands on FE-2
* FE-2 looks up Redis and correctly finds: `otid:10203 -> TR-1`
* But FE-2 has no active connection to TR-1

Now FE-2 cannot deliver the packet.

That means:

* Redis ownership alone is **not sufficient**
* Connectivity topology must also support delivery

## Correct solution

Each TR should maintain active associations to **multiple FE nodes**.

Example:

* TR-1 connected to FE-1 and FE-2
* TR-2 connected to FE-1 and FE-2
* TR-N connected to FE-1 and FE-2

Then any FE that receives a packet can:

* Resolve ownership via local cache or Redis
* Deliver directly to the owning TR

### This is the recommended production topology

> **Every TR connected to multiple FE nodes**

This is much cleaner than:

* FE-to-FE forwarding mesh
* Forced upstream path affinity hacks
* Single FE ownership assumptions

---

# Local Cache + Redis (Best Practice)

## Do NOT design the final system as:

> Every packet = Redis round-trip

That will work in a lab, but it is not the best production design.

## Best design

Use:

* **Local in-memory cache** for the hot path
* **Redis** as shared backing store

### FE local cache role

Each FE should maintain a local table such as:

* `otid -> tr_id`
* `dtid -> tr_id`

This should be:

* lock-sharded
* low-latency
* TTL aware
* refreshed on activity

### Recommended lookup sequence

#### On inbound packet

1. Parse dialog ID
2. Check local cache
3. If found: route immediately
4. If not found: check Redis
5. If Redis hit:

   * populate local cache
   * route to TR
6. If no Redis hit:

   * drop / log / quarantine depending on policy

#### On outbound packet

1. Parse dialog ID
2. Claim ownership locally
3. Write-through or refresh Redis
4. Forward upstream

This gives:

* Fast steady-state routing
* Cross-FE correctness
* Reduced Redis load
* Better scalability

---

# Why This Can Work for High Load

## Why this can be fast

Your FE is intentionally narrow in scope.

It does not need to do everything a generic STP does.

That means:

* Short hot path
* Minimal parsing
* Minimal branching
* Minimal protocol surface
* Predictable ownership logic
* Efficient local cache hits

This can make FE extremely fast for the **specific path you care about**.

## Why this can be lighter than osmo-stp for your case

`osmo-stp` is a real STP implementation with broader responsibilities.

Your FE is:

* more specialized
* more constrained
* more optimized for dialog ownership routing

So for your specific use case, FE can potentially be:

* simpler
* easier to tune
* faster on the narrow path

---

# Important Limitation

## This is NOT a generic STP replacement

This architecture should be described as:

* **Specialized M3UA frontend**
* **Dialog-aware relay**
* **TCAP ownership router frontend**
* **TR-facing M3UA stickiness layer**

It should **not** be described as:

* Full SS7 STP
* Full M3UA switch
* Generic SCCP/SS7 routing platform

That distinction matters technically and commercially.

---

# Production Readiness Checklist

A lab version is not enough.

To make this production-grade, FE should include the following.

## Required capabilities

* Minimal but correct M3UA parsing
* Correct ASPUP / ASPAC state handling
* Robust M3UA DATA extraction
* Safe SCCP / TCAP extraction for OTID / DTID
* BEGIN / CONTINUE / END / ABORT awareness
* Ownership claim on BEGIN
* Ownership refresh on subsequent traffic
* Ownership cleanup on END / ABORT
* TTL-based stale mapping cleanup
* Local cache + Redis shared state
* Multi-FE TR connectivity
* Multi-upstream support
* Backpressure handling
* Connection loss handling
* Reconnect handling
* Metrics and observability
* Rate limiting / drop policies
* Memory safety under malformed traffic

## Strongly recommended

* Epoll-based socket handling
* Worker thread sharding
* Lock-sharded hash maps
* Redis connection pooling
* Structured logs
* Per-peer counters
* Per-dialog lifecycle metrics
* Health endpoints / stats export

---

# Suggested Naming

For documentation and product positioning, good names include:

* **FE (Frontend)**
* **M3UA Frontend**
* **M3UA Edge**
* **Dialog-Aware M3UA Frontend**
* **TCAP Frontend Relay**
* **M3UA Session Frontend**

Recommended practical internal naming:

> **FE = Frontend M3UA Layer**

And keep TR as:

> **TR = TCAP Router**

---

# Recommended Positioning in Your Stack

## Current best path

### Phase 1

* Use `osmo-stp` where needed for stability
* Productionize TR first
* Validate real telecom flows

### Phase 2

* Continue FE as specialized frontend R&D
* Harden local cache + Redis logic
* Add multi-FE topology
* Add DTID lifecycle correctness

### Phase 3

* Load test FE heavily
* Failure test FE + Redis + TR
* Only replace `osmo-stp` when FE is proven stable in real traffic patterns

This is the safest and smartest rollout path.

---

# Final Summary

This architecture is best understood as:

> A **specialized, high-performance, dialog-aware M3UA frontend cluster**
> that uses **Redis shared state** to ensure **cross-FE dialog stickiness**
> and routes return traffic to the correct **TCAP Router (TR)**,
> which then maintains backend-side stickiness for the application layer.

In short:

* **Telecom <-> FE** = frontend ownership and M3UA relay
* **FE <-> Redis** = shared dialog ownership state
* **FE <-> TR** = dialog-aware TR selection / return routing
* **TR <-> Backend** = backend application affinity and service logic

This is a **very strong architecture for your specific use case** when built as:

* narrow-purpose
* high-performance
* local-cache-first
* Redis-backed
* multi-FE connected
* not pretending to be a full generic STP

---

# Quick Reference Flows

## Outbound

```text
Backend App -> TR -> FE -> Telecom
```

## Inbound

```text
Telecom -> FE -> Local Cache / Redis -> TR -> Backend App
```

## Best topology rule

```text
Each TR should connect to multiple FE nodes.
```

## Best performance rule

```text
Use local cache first, Redis second.
```

## Best product rule

```text
Treat FE as a specialized M3UA frontend, not a generic STP.
```

