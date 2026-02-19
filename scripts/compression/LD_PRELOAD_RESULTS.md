# Benchmark LD_PRELOAD

> **Note**: These are preliminary test results as of **December 4, 2025**. Results may change as the implementation evolves and optimizations are applied.

## Compression Benchmark

We have developed a benchmark where we test write time, read time and compression ratio against the *silesia* files: `dickens`, `mozilla`, `nci`, `mr` and `xml`.

The test consists on measuring write and read time by using a small C program which uses a `256kb` buffer (or a different size) to read chunks of the *silesia* file and write them to a new file.  We collect the write time. Then, we read the new file and collect the read time. The program also has an option to execute only the write, or only the final read (this is important for the next part).

The Benchmark is ran **5 times** for each combination of config-file.

## Modular Lib Configs

The test is done against various configs of Modular Lib:

- Local Layer only
- LZ4 with levels -3, 0 and 9 (ultra fast, default and high)
- ZSTD with levels -5, 3 and 15 (ultra fast, default and high)

## Test Scenarios

We want to test 4 different scenarios for comparison:

- No lib in the middle, only the standard `FS`
- Modular Lib with `FUSE`
- Modular Lib with `LD_PRELOAD` where the C program is executed in a “single” preload
- Modular Lib with `LD_PRELOAD` for write and then again for read. Why? We want to test the “real case scenario” where the read operations are not done in the same call and how slow is our reconstruction relying on the file’s metadata

## Results

### Write

This is the comparison table of `FS vs the other scenarios` where negative is good, as it means that it is faster than Normal FS. The main observations we can take from it are:

- For `lz4_default` and `lz4_ultra_fast` we can see that is **faster** to have that compression than to have only the normal fs. Same can’t be said for other algorithms and levels. As they become heavier, the write time overhead increases.
- LDPreload Single and Split are the same when taking in consideration the standard deviation, so the reconstruct doesn’t add real overhead
- LDPreload is faster than FUSE. On average it is 11.3% faster taking in consideration only the ones with compression.

| **File** | **Config** | **NormalFS (s)** | **FUSE (s)** | **FUSE Δ%** | **LDPSplit (s)** | **LDPSplit Δ%** | **LDPSingle (s)** | **LDPSingle Δ%** |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| dickens | lz4_default | 0.056491 | 0.045692 ± 0.001665 | -19.1% ± 2.9% | 0.036145 ± 0.000714 | -36.0% ± 1.3% | 0.036767 ± 0.000331 | -34.9% ± 0.6% |
| dickens | lz4_high | 0.056128 | 0.422232 ± 0.006839 | +652.3% ± 12.2% | 0.412577 ± 0.000537 | +635.1% ± 1.0% | 0.411160 ± 0.001190 | +632.5% ± 2.1% |
| dickens | lz4_ultra_fast | 0.056237 | 0.043712 ± 0.001590 | -22.3% ± 2.8% | 0.033598 ± 0.000590 | -40.3% ± 1.0% | 0.032779 ± 0.001015 | -41.7% ± 1.8% |
| dickens | no_compression | 0.013150 | 0.022196 ± 0.002111 | +68.8% ± 16.1% | 0.013874 ± 0.000914 | +5.5% ± 7.0% | 0.014558 ± 0.000410 | +10.7% ± 3.1% |
| dickens | zstd_default | 0.055833 | 0.222493 ± 0.003320 | +298.5% ± 5.9% | 0.206598 ± 0.001083 | +270.0% ± 1.9% | 0.208487 ± 0.003834 | +273.4% ± 6.9% |
| dickens | zstd_high | 0.056045 | 3.011520 ± 0.009332 | +5273.4% ± 16.7% | 2.947020 ± 0.010800 | +5158.3% ± 19.3% | 2.940140 ± 0.008789 | +5146.1% ± 15.7% |
| dickens | zstd_ultra_fast | 0.055429 | 0.143992 ± 0.002720 | +159.8% ± 4.9% | 0.128445 ± 0.001219 | +131.7% ± 2.2% | 0.131354 ± 0.003108 | +137.0% ± 5.6% |
| mozilla | lz4_default | 0.362137 | 0.168995 ± 0.015598 | -53.3% ± 4.3% | 0.127594 ± 0.002141 | -64.8% ± 0.6% | 0.153555 ± 0.036222 | -57.6% ± 10.0% |
| mozilla | lz4_high | 0.364979 | 1.251430 ± 0.016539 | +242.9% ± 4.5% | 1.214140 ± 0.008215 | +232.7% ± 2.3% | 1.215090 ± 0.011244 | +232.9% ± 3.1% |
| mozilla | lz4_ultra_fast | 0.363826 | 0.179145 ± 0.011251 | -50.8% ± 3.1% | 0.234671 ± 0.019587 | -35.5% ± 5.4% | 0.281229 ± 0.005796 | -22.7% ± 1.6% |
| mozilla | no_compression | 0.217209 | 0.187921 ± 0.097722 | -13.5% ± 45.0% | 0.208793 ± 0.130786 | -3.9% ± 60.2% | 0.213078 ± 0.133360 | -1.9% ± 61.4% |
| mozilla | zstd_default | 0.363265 | 0.862681 ± 0.008098 | +137.5% ± 2.2% | 0.775332 ± 0.004058 | +113.4% ± 1.1% | 0.776675 ± 0.001735 | +113.8% ± 0.5% |
| mozilla | zstd_high | 0.362016 | 13.246100 ± 0.028137 | +3559.0% ± 7.8% | 12.793400 ± 0.026785 | +3433.9% ± 7.4% | 12.746400 ± 0.039267 | +3420.9% ± 10.8% |
| mozilla | zstd_ultra_fast | 0.363954 | 0.468908 ± 0.006732 | +28.8% ± 1.8% | 0.388791 ± 0.002035 | +6.8% ± 0.6% | 0.406868 ± 0.019159 | +11.8% ± 5.3% |
| mr | lz4_default | 0.056855 | 0.037692 ± 0.003022 | -33.7% ± 5.3% | 0.025586 ± 0.000495 | -55.0% ± 0.9% | 0.026893 ± 0.001252 | -52.7% ± 2.2% |
| mr | lz4_high | 0.055565 | 0.454752 ± 0.002535 | +718.4% ± 4.6% | 0.443395 ± 0.001987 | +698.0% ± 3.6% | 0.444002 ± 0.002981 | +699.1% ± 5.4% |
| mr | lz4_ultra_fast | 0.055330 | 0.028635 ± 0.002728 | -48.2% ± 4.9% | 0.021589 ± 0.001168 | -61.0% ± 2.1% | 0.021381 ± 0.000816 | -61.4% ± 1.5% |
| mr | no_compression | 0.056529 | 0.052244 ± 0.003484 | -7.6% ± 6.2% | 0.054532 ± 0.006411 | -3.5% ± 11.3% | 0.055004 ± 0.004448 | -2.7% ± 7.9% |
| mr | zstd_default | 0.056369 | 0.202785 ± 0.002295 | +259.7% ± 4.1% | 0.187421 ± 0.001674 | +232.5% ± 3.0% | 0.186074 ± 0.000337 | +230.1% ± 0.6% |
| mr | zstd_high | 0.055918 | 2.189850 ± 0.059466 | +3816.2% ± 106.3% | 2.080420 ± 0.011479 | +3620.5% ± 20.5% | 2.087810 ± 0.012942 | +3633.7% ± 23.1% |
| mr | zstd_ultra_fast | 0.056313 | 0.107150 ± 0.001824 | +90.3% ± 3.2% | 0.094666 ± 0.000524 | +68.1% ± 0.9% | 0.094067 ± 0.001682 | +67.0% ± 3.0% |
| nci | lz4_default | 0.230657 | 0.083332 ± 0.007726 | -63.9% ± 3.3% | 0.049006 ± 0.001228 | -78.8% ± 0.5% | 0.049790 ± 0.002108 | -78.4% ± 0.9% |
| nci | lz4_high | 0.231092 | 0.760565 ± 0.006378 | +229.1% ± 2.8% | 0.725287 ± 0.003428 | +213.9% ± 1.5% | 0.723484 ± 0.004048 | +213.1% ± 1.8% |
| nci | lz4_ultra_fast | 0.230343 | 0.084210 ± 0.011844 | -63.4% ± 5.1% | 0.047054 ± 0.001492 | -79.6% ± 0.6% | 0.046507 ± 0.000423 | -79.8% ± 0.2% |
| nci | no_compression | 0.231777 | 0.221697 ± 0.001365 | -4.3% ± 0.6% | 0.226385 ± 0.003179 | -2.3% ± 1.4% | 0.230536 ± 0.004227 | -0.5% ± 1.8% |
| nci | zstd_default | 0.230861 | 0.241453 ± 0.008936 | +4.6% ± 3.9% | 0.196792 ± 0.001959 | -14.8% ± 0.8% | 0.199617 ± 0.001941 | -13.5% ± 0.8% |
| nci | zstd_high | 0.231397 | 11.254400 ± 0.006192 | +4763.7% ± 2.7% | 11.126800 ± 0.021765 | +4708.5% ± 9.4% | 11.160900 ± 0.031834 | +4723.3% ± 13.8% |
| nci | zstd_ultra_fast | 0.230570 | 0.173021 ± 0.007934 | -25.0% ± 3.4% | 0.135982 ± 0.002005 | -41.0% ± 0.9% | 0.139095 ± 0.002547 | -39.7% ± 1.1% |
| xml | lz4_default | 0.019916 | 0.014823 ± 0.000907 | -25.6% ± 4.6% | 0.011069 ± 0.000469 | -44.4% ± 2.4% | 0.011692 ± 0.000375 | -41.3% ± 1.9% |
| xml | lz4_high | 0.019695 | 0.090771 ± 0.001197 | +360.9% ± 6.1% | 0.084183 ± 0.000426 | +327.4% ± 2.2% | 0.084616 ± 0.000307 | +329.6% ± 1.6% |
| xml | lz4_ultra_fast | 0.019031 | 0.014049 ± 0.001176 | -26.2% ± 6.2% | 0.010654 ± 0.000176 | -44.0% ± 0.9% | 0.010508 ± 0.000569 | -44.8% ± 3.0% |
| xml | no_compression | 0.020413 | 0.013533 ± 0.000556 | -33.7% ± 2.7% | 0.016050 ± 0.006731 | -21.4% ± 33.0% | 0.018641 ± 0.006693 | -8.7% ± 32.8% |
| xml | zstd_default | 0.019925 | 0.049443 ± 0.001124 | +148.1% ± 5.6% | 0.042112 ± 0.000276 | +111.4% ± 1.4% | 0.041897 ± 0.000483 | +110.3% ± 2.4% |
| xml | zstd_high | 0.019945 | 1.592580 ± 0.007455 | +7884.8% ± 37.4% | 1.574700 ± 0.008672 | +7795.2% ± 43.5% | 1.578410 ± 0.007961 | +7813.8% ± 39.9% |
| xml | zstd_ultra_fast | 0.019775 | 0.037916 ± 0.001347 | +91.7% ± 6.8% | 0.032497 ± 0.000393 | +64.3% ± 2.0% | 0.032995 ± 0.001098 | +66.9% ± 5.6% |

### Read

This is the comparison table of `FS vs the other scenarios` where negative is good, as it means that it is faster than Normal FS. The main observations we can take from it are:

- On the `nci` file with LZ4, the LDPreload is 30 to 50% slower than normal FS. On the other hand, FUSE is 330 to 380%, which is a considerable difference.
- Overall, FUSE is much slower than LDPreload and we have massive gains with it. It is 52% faster on average.
- The reads are significantly slower than the writes when compared with normal FS. For the LZ4 default and ultra fast we have a 180-190% overhead in `dickens` , ~140% in `mozilla` and ~40% in `nci`

| File | Config | NormalFS (s) | FUSE (s) | FUSE Δ% | LDPSplit (s) | LDPSplit Δ% | LDPSingle (s) | LDPSingle Δ% |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| dickens | lz4_default | 0.001399 | 0.009287 ± 0.000499 | +563.8% ± 35.6% | 0.003925 ± 0.000043 | +180.6% ± 3.1% | 0.003906 ± 0.000064 | +179.2% ± 4.6% |
| dickens | lz4_high | 0.001408 | 0.009320 ± 0.000174 | +562.1% ± 12.4% | 0.004030 ± 0.000028 | +186.3% ± 2.0% | 0.004070 ± 0.000058 | +189.2% ± 4.1% |
| dickens | lz4_ultra_fast | 0.001453 | 0.009353 ± 0.000526 | +543.9% ± 36.2% | 0.004197 ± 0.000039 | +188.9% ± 2.7% | 0.004219 ± 0.000048 | +190.4% ± 3.3% |
| dickens | no_compression | 0.001368 | 0.004554 ± 0.000237 | +232.9% ± 17.4% | 0.001404 ± 0.000070 | +2.6% ± 5.1% | 0.001369 ± 0.000102 | +0.1% ± 7.4% |
| dickens | zstd_default | 0.001410 | 0.080434 ± 0.004114 | +5605.8% ± 291.8% | 0.059061 ± 0.000266 | +4089.6% ± 18.9% | 0.058206 ± 0.000160 | +4029.0% ± 11.4% |
| dickens | zstd_high | 0.001437 | 0.073613 ± 0.004006 | +5022.0% ± 278.7% | 0.052218 ± 0.000290 | +3533.3% ± 20.2% | 0.051872 ± 0.000303 | +3509.3% ± 21.1% |
| dickens | zstd_ultra_fast | 0.001425 | 0.053552 ± 0.003741 | +3658.3% ± 262.5% | 0.038107 ± 0.000289 | +2574.4% ± 20.3% | 0.037908 ± 0.000194 | +2560.5% ± 13.6% |
| mozilla | lz4_default | 0.007375 | 0.050935 ± 0.005029 | +590.6% ± 68.2% | 0.018049 ± 0.000105 | +144.7% ± 1.4% | 0.018400 ± 0.000599 | +149.5% ± 8.1% |
| mozilla | lz4_high | 0.007457 | 0.048369 ± 0.001726 | +548.6% ± 23.1% | 0.016892 ± 0.000167 | +126.5% ± 2.2% | 0.016809 ± 0.000315 | +125.4% ± 4.2% |
| mozilla | lz4_ultra_fast | 0.007448 | 0.051022 ± 0.006879 | +585.1% ± 92.4% | 0.018679 ± 0.000076 | +150.8% ± 1.0% | 0.018559 ± 0.000182 | +149.2% ± 2.4% |
| mozilla | no_compression | 0.007405 | 0.022965 ± 0.000738 | +210.1% ± 10.0% | 0.008131 ± 0.000513 | +9.8% ± 6.9% | 0.007230 ± 0.000314 | -2.4% ± 4.2% |
| mozilla | zstd_default | 0.007396 | 0.256574 ± 0.019156 | +3369.2% ± 259.0% | 0.182878 ± 0.010710 | +2372.7% ± 144.8% | 0.181427 ± 0.004684 | +2353.1% ± 63.3% |
| mozilla | zstd_high | 0.007440 | 0.309336 ± 0.003998 | +4057.8% ± 53.7% | 0.214203 ± 0.000872 | +2779.1% ± 11.7% | 0.213198 ± 0.000783 | +2765.6% ± 10.5% |
| mozilla | zstd_ultra_fast | 0.007292 | 0.186300 ± 0.019085 | +2454.9% ± 261.7% | 0.116677 ± 0.001611 | +1500.1% ± 22.1% | 0.115414 ± 0.000679 | +1482.8% ± 9.3% |
| mr | lz4_default | 0.001393 | 0.012464 ± 0.001287 | +795.0% ± 92.4% | 0.003324 ± 0.000099 | +138.7% ± 7.1% | 0.003338 ± 0.000037 | +139.7% ± 2.6% |
| mr | lz4_high | 0.001432 | 0.010317 ± 0.000744 | +620.4% ± 52.0% | 0.003294 ± 0.000046 | +130.0% ± 3.2% | 0.003368 ± 0.000075 | +135.2% ± 5.2% |
| mr | lz4_ultra_fast | 0.001435 | 0.010396 ± 0.001485 | +624.7% ± 103.5% | 0.002856 ± 0.000055 | +99.1% ± 3.8% | 0.002917 ± 0.000042 | +103.3% ± 3.0% |
| mr | no_compression | 0.001491 | 0.004545 ± 0.000303 | +204.8% ± 20.3% | 0.001555 ± 0.000110 | +4.3% ± 7.4% | 0.001407 ± 0.000026 | -5.7% ± 1.7% |
| mr | zstd_default | 0.001389 | 0.070695 ± 0.005852 | +4989.5% ± 421.3% | 0.046774 ± 0.000599 | +3267.4% ± 43.1% | 0.046105 ± 0.000207 | +3219.2% ± 14.9% |
| mr | zstd_high | 0.001386 | 0.066475 ± 0.002404 | +4697.6% ± 173.5% | 0.047561 ± 0.000619 | +3332.5% ± 44.6% | 0.047435 ± 0.000251 | +3323.5% ± 18.1% |
| mr | zstd_ultra_fast | 0.001382 | 0.045641 ± 0.002767 | +3202.3% ± 200.2% | 0.029197 ± 0.000210 | +2012.5% ± 15.2% | 0.028735 ± 0.000172 | +1979.1% ± 12.4% |
| nci | lz4_default | 0.004898 | 0.023339 ± 0.002228 | +376.5% ± 45.5% | 0.007417 ± 0.000080 | +51.4% ± 1.6% | 0.007530 ± 0.000079 | +53.7% ± 1.6% |
| nci | lz4_high | 0.004934 | 0.021486 ± 0.002909 | +335.5% ± 59.0% | 0.006447 ± 0.000059 | +30.7% ± 1.2% | 0.006400 ± 0.000082 | +29.7% ± 1.7% |
| nci | lz4_ultra_fast | 0.005029 | 0.024210 ± 0.002063 | +381.4% ± 41.0% | 0.007505 ± 0.000075 | +49.2% ± 1.5% | 0.007557 ± 0.000020 | +50.3% ± 0.4% |
| nci | no_compression | 0.005004 | 0.015158 ± 0.001698 | +202.9% ± 33.9% | 0.005664 ± 0.000142 | +13.2% ± 2.8% | 0.005019 ± 0.000107 | +0.3% ± 2.1% |
| nci | zstd_default | 0.004800 | 0.105942 ± 0.003827 | +2107.0% ± 79.7% | 0.063740 ± 0.000098 | +1227.8% ± 2.0% | 0.063249 ± 0.000197 | +1217.6% ± 4.1% |
| nci | zstd_high | 0.004942 | 0.082292 ± 0.002263 | +1565.1% ± 45.8% | 0.047513 ± 0.000258 | +861.4% ± 5.2% | 0.047529 ± 0.000448 | +861.7% ± 9.1% |
| nci | zstd_ultra_fast | 0.004933 | 0.095776 ± 0.012230 | +1841.6% ± 247.9% | 0.053860 ± 0.000725 | +991.8% ± 14.7% | 0.053297 ± 0.000164 | +980.4% ± 3.3% |
| xml | lz4_default | 0.000735 | 0.003992 ± 0.000119 | +443.3% ± 16.2% | 0.001612 ± 0.000021 | +119.5% ± 2.9% | 0.001623 ± 0.000036 | +120.9% ± 4.9% |
| xml | lz4_high | 0.000761 | 0.003284 ± 0.000200 | +331.7% ± 26.3% | 0.001256 ± 0.000039 | +65.1% ± 5.2% | 0.001303 ± 0.000030 | +71.3% ± 4.0% |
| xml | lz4_ultra_fast | 0.000715 | 0.004475 ± 0.000373 | +526.2% ± 52.3% | 0.001631 ± 0.000009 | +128.1% ± 1.3% | 0.001688 ± 0.000063 | +136.2% ± 8.8% |
| xml | no_compression | 0.000777 | 0.002729 ± 0.000633 | +251.3% ± 81.5% | 0.000812 ± 0.000035 | +4.6% ± 4.5% | 0.000760 ± 0.000049 | -2.1% ± 6.3% |
| xml | zstd_default | 0.000708 | 0.019624 ± 0.002555 | +2671.9% ± 360.8% | 0.012665 ± 0.000478 | +1688.9% ± 67.5% | 0.012258 ± 0.000087 | +1631.4% ± 12.3% |
| xml | zstd_high | 0.000749 | 0.015097 ± 0.000643 | +1914.3% ± 85.8% | 0.010102 ± 0.000102 | +1247.8% ± 13.6% | 0.009833 ± 0.000115 | +1212.0% ± 15.4% |
| xml | zstd_ultra_fast | 0.000703 | 0.017139 ± 0.001401 | +2337.9% ± 199.2% | 0.011199 ± 0.000079 | +1492.9% ± 11.3% | 0.011034 ± 0.000057 | +1469.5% ± 8.1% |

## Compression Ratios

| File | Config | NormalFS | FUSE | LDPSplit | LDPSingle |
| --- | --- | --- | --- | --- | --- |
| dickens | lz4_default | 0.9997 | 1.3421 | 1.3421 | 1.3421 |
| dickens | lz4_high | 0.9997 | 2.0067 | 2.0067 | 2.0067 |
| dickens | lz4_ultra_fast | 0.9997 | 1.2386 | 1.2386 | 1.2386 |
| dickens | no_compression | 0.9997 | 0.9997 | 0.9997 | 0.9997 |
| dickens | zstd_default | 0.9997 | 2.0181 | 2.0181 | 2.0181 |
| dickens | zstd_high | 0.9997 | 2.0280 | 2.0280 | 2.0280 |
| dickens | zstd_ultra_fast | 0.9997 | 1.3443 | 1.3443 | 1.3443 |
| mozilla | lz4_default | 1.0000 | 1.5181 | 1.5181 | 1.5181 |
| mozilla | lz4_high | 1.0000 | 1.8070 | 1.8070 | 1.8070 |
| mozilla | lz4_ultra_fast | 1.0000 | 1.1649 | 1.1649 | 1.1649 |
| mozilla | no_compression | 1.0000 | 1.0000 | 1.0000 | 1.0000 |
| mozilla | zstd_default | 1.0000 | 1.9868 | 1.9868 | 1.9868 |
| mozilla | zstd_high | 1.0000 | 2.1398 | 2.1398 | 2.1398 |
| mozilla | zstd_ultra_fast | 1.0000 | 1.1845 | 1.1845 | 1.1845 |
| mr | lz4_default | 0.9996 | 1.4086 | 1.4086 | 1.4086 |
| mr | lz4_high | 0.9996 | 2.0117 | 2.0117 | 2.0117 |
| mr | lz4_ultra_fast | 0.9996 | 1.3698 | 1.3698 | 1.3698 |
| mr | no_compression | 0.9996 | 0.9996 | 0.9996 | 0.9996 |
| mr | zstd_default | 0.9996 | 2.0184 | 2.0184 | 2.0184 |
| mr | zstd_high | 0.9996 | 2.0217 | 2.0217 | 2.0217 |
| mr | zstd_ultra_fast | 0.9996 | 1.6249 | 1.6249 | 1.6249 |
| nci | lz4_default | 0.9999 | 4.0214 | 4.0214 | 4.0214 |
| nci | lz4_high | 0.9999 | 4.0433 | 4.0433 | 4.0433 |
| nci | lz4_ultra_fast | 0.9999 | 4.0195 | 4.0195 | 4.0195 |
| nci | no_compression | 0.9999 | 0.9999 | 0.9999 | 0.9999 |
| nci | zstd_default | 0.9999 | 4.0533 | 4.0533 | 4.0533 |
| nci | zstd_high | 0.9999 | 4.0613 | 4.0613 | 4.0613 |
| nci | zstd_ultra_fast | 0.9999 | 4.0155 | 4.0155 | 4.0155 |
| xml | lz4_default | 1.0000 | 3.0490 | 3.0490 | 3.0490 |
| xml | lz4_high | 1.0000 | 3.2625 | 3.2625 | 3.2625 |
| xml | lz4_ultra_fast | 1.0000 | 2.9260 | 2.9260 | 2.9260 |
| xml | no_compression | 1.0000 | 1.0000 | 1.0000 | 1.0000 |
| xml | zstd_default | 1.0000 | 3.7073 | 3.7073 | 3.7073 |
| xml | zstd_high | 1.0000 | 4.3355 | 4.3355 | 4.3355 |
| xml | zstd_ultra_fast | 1.0000 | 3.0705 | 3.0705 | 3.0705 |

## First Conclusions

- Only fast and low compression algorithms seem to be usable in terms of overhead
- Reads add time savings of around 40% faster with compression
- Writes add significant overhead (almost 200%)
