# Call parameters:
#   $0 - datafilename
#   $1 - outputfilename

set terminal push
set terminal png size 1290,770

set datafile separator ","
set grid
set autoscale
set key top left box
set title  "Latency per Message Size"
set xlabel "Message Size (bytes)"
set ylabel "Latency (seconds)"
set format x "%.1s%c"
set format y "%.1s%cS"

set output '$1.png'
plot '$0'\
     index 2 using 2:($$3/2) t "TCP"                   with linespoints,\
  '' index 3 using 2:($$3/2) t "UDP"                   with linespoints,\
  '' index 0 using 2:($$3/2) t "multicast/best effort" with linespoints,\
  '' index 1 using 2:($$3/2) t "multicast/reliable"    with linespoints

set xrange [0:1100]
set output '$1-zoom.png'
replot

set output
set terminal pop

