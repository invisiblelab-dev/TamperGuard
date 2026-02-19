set $dir=/tmp/filebench
set $nfiles=10000
set $meandirwidth=1
set $filesize=128k
set $nthreads=4
set $iosize=1m
set $meanappendsize=16k
set $runtime=1200

# Write-only workload: write to existing files only (no createfile).
# This avoids FILEBENCH_NORSC when the "no-exist" pool is empty.
define fileset name=bigfileset,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=100,writeonly

define process name=writer,instances=1
{
  thread name=writerthread,memsize=10m,instances=$nthreads
  {
    flowop openfile       name=open1,filesetname=bigfileset,fd=1
    flowop appendfilerand name=append1,fd=1,iosize=$meanappendsize
    flowop closefile      name=close1,fd=1
  }
}

system "echo Running WRITE-ONLY workload for $runtime seconds..."
run $runtime
