# set -e
while read filename
do
    if [ -f "$filename" ]
    then
        if python -m json.tool < "$filename" > tmp.py 2> /dev/null
        then
            ./jdent < tmp.py > tmp.py.jd
            echo $i >> failLog
            diff tmp.py tmp.py.jd >> failLog || echo "fail $filename"
        else
            echo python $filename
        fi
    fi
done
