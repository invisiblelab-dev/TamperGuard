#!/bin/bash

# Check if fio is installed
if ! command -v fio &>/dev/null; then
    echo "fio not found"
    echo "You can install it from the official repository (https://github.com/axboe/fio) or using your package manager (e.g., sudo apt install fio)"
    echo ""
else
    echo "fio detected. Proceeding with the benchmark..."
    echo ""
fi

# Defaults
compress_alg="lz4"
compress_level=0
size="500M"
duration=300
block_size="262144"
num_blocks="100"
show_help=no

OPTIONS=$(getopt -o h --long compress_alg:,compress_level:,size:,duration:,help,block_size:,num_blocks: -- "$@")
eval set -- "$OPTIONS"

while true; do
    case "$1" in
    --compress_alg)
        compress_alg=$2
        shift 2
        ;;
    --compress_level)
        compress_level=$2
        shift 2
        ;;
    --size)
        size=$2
        shift 2
        ;;
    --duration)
        duration=$2
        shift 2
        ;;
    --block_size)
        block_size=$2
        shift 2
        ;;
    --num_blocks)
        num_blocks=$2
        shift 2
        ;;
    -h | --help)
        showHelp=yes
        shift
        ;;
    --)
        shift
        break
        ;;
    *)
        echo "Invalid option $1"
        exit 1
        ;;
    esac
done

if [ "$showHelp" = "yes" ]; then
    cat <<EOF
Use: $0 [opções]

Options:
  --compress_alg ALG     Compression algorithm (e.g., zstd, lz4)
  --compress_level N     Compression level (e.g., -5 to 9)
  --size SIZE            File size (e.g., 1G, 500M, 100K)
  --duration SEC		 Execution time in seconds for each test
  --block_size SIZE      Block size (e.g., 4k, 1M)
  -h, --help             Show this help message
Example:
  $0 --compress_alg=zstd --compress_level=9 --size=1G --duration=60 --block_size=4096
EOF
    exit 0
fi

cd ../..
make build >/dev/null
echo ""
echo "Build finished"

echo "root = \"layer_1\"
log_mode = \"debug\"

[layer_1]
type = \"block_align\"
block_size = $block_size
next = \"layer_2\"

[layer_2]
type = \"read_cache\"
block_size = $block_size
num_blocks = $num_blocks
next = \"layer_3\"

[layer_3]
type = \"compression\"
algorithm = \"$compress_alg\"        
level = $compress_level
next = \"local\"

[local]
type = \"local\"" >"config.toml"

echo ""
echo "Running the benchmark with cache + compression"
echo ""
make examples/fuse/run >/dev/null 2>&1 &

if [ ! -e "examples/fuse/backend_data/fio_testfile" ]; then
    echo "Test file not detected. Preparing a new one"
    fio --name=prepare --filename=examples/fuse/mount_point/fio_testfile --size=$size --rw=write \
        --bs=1M --ioengine=psync --fdatasync=1 \
        --refill_buffers=1 --randrepeat=0 \
        --buffer_compress_percentage=50 --buffer_pattern=0xdeadbeef
    echo "Test file created"
    echo ""
else
    echo "Test file detected"
    echo ""
fi

fio --name=randread --ioengine=psync --direct=1 --rw=randread \
    --bsrange=4k:64k --numjobs=1 --iodepth=16 --size=$size --runtime=$duration --time_based \
    --filename=examples/fuse/mount_point/fio_testfile --random_distribution=zoned:1/48:98/5:1/47

make examples/fuse/stop >/dev/null

echo ""
echo "Unmounting FUSE"

echo "root = \"layer_1\"
log_mode = \"debug\"

[layer_1]
type = \"compression\"
algorithm = \"$compress_alg\"        
level = $compress_level
next = \"local\"

[local]
type = \"local\"" >"config.toml"

echo ""
echo "Running the benchmark with only compression and no cache"
echo ""

make examples/fuse/run >/dev/null 2>&1 &

fio --name=randread --ioengine=psync --direct=1 --rw=randread \
    --bsrange=4k:64k --numjobs=1 --iodepth=16 --size=$size --runtime=$duration --time_based \
    --filename=examples/fuse/mount_point/fio_testfile --random_distribution=zoned:1/48:98/5:1/47

make examples/fuse/stop >/dev/null

echo ""
echo "Unmounting FUSE"
