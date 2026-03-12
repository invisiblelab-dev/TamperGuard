# Filebench scripts

## `filebench_repeats.sh`

Install filebench:
```bash
git clone git@github.com:filebench/filebench.git
```
Follow the instructions in filebench/README.md (https://github.com/filebench/filebench#) to build and install filebench.

If having problems on running filebench, try using the branch containing the fix in the following PR: https://github.com/filebench/filebench/pull/169 (the fix is not yet merged, so you need to use the branch manually).

Set the $dir to the mount point for each workload (scripts/filebench/workloads/webserver.f, scripts/filebench/workloads/fileserver.f, scripts/filebench/workloads/varmail.f).

Run the filebench workload:
```bash
./scripts/filebench/filebench_repeats.sh --workload scripts/filebench/workloads/webserver.f --workload scripts/filebench/workloads/fileserver.f --workload scripts/filebench/workloads/varmail.f --repeats 5 --interval 10
```

Run a workload multiple times with a fixed interval (default 10 seconds) and write per-run logs.

Logs are written to `/tmp/filebench_runs_YYYYmmdd_HHMMSS/` by default.

## TamperGuard results

In the /tamper_guard_results directory, there are results from the TamperGuard experiments, as explained in the submitted paper.
