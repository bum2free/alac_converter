# Requirement Description
This is to convert flac/ape files to apple ALAC format to adopt my ipod
classic player.

1: It should support both whole track(with one cue file) and split tracks

2: It should add tag info to the converted output .m4a files, from the
original source files

# Usage:
./alac_converter -s <src_dir> -d <dst_dir>>

# example:
$ls
A/A1/a1.cue
A/A1/a1.ape
B/b.cue
B/b.ape

$./alac_converter -s . -d /test_output
//it will createl related directory tree rooted in dst_dir, like the
following:
/test_output/A/A1/a1/track1.m4a
/test_output/A/A1/a1/track2.m4a
...
/test_output/B/b/track1.m4a
...
