#/bin/sh

if [ $# -eq 0 ];then
   echo "usage: ./genChart xxx.dem"
   exit
fi

CHART_NAME=${1%.*}

gnuplot -e "file_name='$CHART_NAME'" $1

