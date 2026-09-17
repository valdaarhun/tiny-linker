elf=$1
retval=$2

./$elf

rc=$?

if [[ $rc -eq $retval ]]
then
    echo "Executable created and ran successfully!"
else
    echo "Executable didn't run correctly! Expected return value $retval, but received $rc"
fi
