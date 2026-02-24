# Performance Benchmarks

## Branching Comparison

The `perf_branching_comparison` benchmark compares Lumberjack's branchless design against a traditional branching logger.

### Building

```bash
cd build
cmake -Dlumberjack_BUILD_TESTS=ON ..
cmake --build .
```

### Running

```bash
./tests/perf_branching_comparison
```

### What It Tests

1. **Disabled Log Levels** - The most common case in production where debug logs are suppressed
2. **Enabled Log Levels** - Active logging with I/O overhead
3. **Mixed Workload** - Realistic scenario with some enabled and some disabled logs
4. **Mostly Disabled** - Production-like scenario with only errors enabled
5. **Tight Loop** - Tests branch prediction effectiveness

### Expected Results

Lumberjack should show performance advantages primarily in:
- Disabled log calls (1-3ns faster per call)
- Mixed workloads with unpredictable patterns
- Scenarios where branch prediction is less effective

The performance difference comes from eliminating branch mispredictions in the CPU pipeline. While the absolute difference per call is small (nanoseconds), it compounds significantly in hot paths with thousands of log calls.

### Interpreting Output

The benchmark reports:
- **Mean**: Average time per operation
- **Median**: Middle value (less affected by outliers)
- **Min/Max**: Range of measurements
- **StdDev**: Consistency of measurements (lower is better)

Lower values are better. The comparison shows the speedup factor between implementations.
