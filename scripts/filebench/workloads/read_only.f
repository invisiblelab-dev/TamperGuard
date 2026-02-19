set $dir=/tmp/filebench
set $nfiles=10000
# IMPORTANT: must match the dataset layout already on disk under $dir/bigfileset.
# If your existing dataset was created with dirwidth=1, keep this at 1; otherwise
# Filebench will try to reuse paths that don't exist and will fail early.
set $meandirwidth=1
set $filesize=128k
set $nthreads=4
set $iosize=1m
set $meanappendsize=16k
set $runtime=1200

# "Read-only" here means: the *run phase* does only reads.
# We intentionally DO NOT use trusttree: with reuse+trusttree, Filebench will try to
# open every expected file and abort if any single file is missing.
# With reuse (no trusttree), missing files get created during `create files`, and
# existing files are re-used with minimal extra writes.
define fileset name=bigfileset,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,reuse,readonly

define process name=reader,instances=1
{
  thread name=readerthread,memsize=10m,instances=$nthreads
  {
    flowop openfile     name=open1,filesetname=bigfileset,fd=1
    flowop readwholefile name=read1,fd=1,iosize=$iosize
    flowop closefile    name=close1,fd=1
  }
}

# Populate/index the fileset before starting the run.
create files

system "sync ."
system "echo 3 > /proc/sys/vm/drop_caches"

system "echo Running READ-ONLY workload for $runtime seconds..."
run $runtime
