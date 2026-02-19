# Filebench scripts

## `filebench_repeats.sh`

Run a workload multiple times with a fixed interval (default 10 seconds) and write per-run logs.

Example:

```bash
./scripts/filebench/filebench_repeats.sh --workload /path/to/workload.f --repeats 5
./scripts/filebench/filebench_repeats.sh --workload workloads/webserver.f --repeats 3 --interval 30
./scripts/filebench/filebench_repeats.sh --workload my.f --repeats 10 --filebench-bin /usr/local/bin/filebench
```

Logs are written to `/tmp/filebench_runs_YYYYmmdd_HHMMSS/` by default.

## TamperGuard results

In the /tamper_guard_results directory, there are results from the TamperGuard experiments, as explained in the submitted paper.
