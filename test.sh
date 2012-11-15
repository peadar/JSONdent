# set -e
while read filename
do
    if [ -f $filename ]
    then
        if python -m json.tool < $filename > tmp.py
        then
            ./jdent < tmp.py > tmp.py.jd
            echo $i >> failLog
            diff tmp.py tmp.py.jd >> failLog || echo "fail $filename"
        fi
    fi
done
