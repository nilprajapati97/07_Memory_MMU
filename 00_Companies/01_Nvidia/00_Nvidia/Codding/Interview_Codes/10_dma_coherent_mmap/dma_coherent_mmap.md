# DMA-Coherent Memory mmap

## Problem
Write a kernel module that allocates DMA-coherent memory and exposes it to user space via mmap.

## Solution Overview
- Allocates DMA-coherent memory in kernel.
- Exposes memory to user space using mmap.

## Key Points
- Useful for device drivers needing DMA buffers.
