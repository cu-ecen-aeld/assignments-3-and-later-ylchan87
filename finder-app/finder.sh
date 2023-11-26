#! /bin/bash
filesdir=$1
searchstr=$2
if [[ $filesdir == "" || $searchstr == "" ]]; then
    echo "Usage: finder.sh filesdir searchstr"
    exit 1
fi

# args for find
# -L follow symlink
# -type f files only i.e.exclude folders

# readarray
# ref: https://www.baeldung.com/linux/reading-output-into-array
# -t option will remove the trailing newlines from each line. 
# We used the < <(COMMAND) trick to redirect the COMMAND output to the standard input. 
# The <(COMMAND) is called process substitution. It makes the output of the COMMAND appear like a file. 
# Then, we redirect the file to standard input using the < FILE. 
readarray -t files < <(find -L "${filesdir}" -name "*" -type f)
files_count=${#files[@]}

total_occurence_count=0
for file in "${files[@]}";do
    occurence_count=$(grep -c "${searchstr}" "${file}")
    total_occurence_count=$((total_occurence_count + occurence_count))
done

echo "The number of files are ${files_count} and the number of matching lines are ${total_occurence_count}"