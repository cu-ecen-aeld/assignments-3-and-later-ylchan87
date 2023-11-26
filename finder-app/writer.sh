#! /bin/bash
writefile=$1
writestr=$2
if [[ $writefile == "" || $writestr == "" ]]; then
    echo "Usage: writer.sh writefile writestr"
    exit 1
fi

writedir=$(dirname "${writefile}")
mkdir -p "${writedir}"

echo "${writestr}" > "${writefile}"
