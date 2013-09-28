#!/bin/sh

[ -d srv/ ] && rm -r srv/
[ -d cli/ ] && rm -r cli/
( ps -e | grep tftpd >/dev/null 2>&1 ) && killall tftpd
[ -f srv.log ] && rm srv.log
[ -f cli.log ] && rm cli.log
[ -p left ] && rm left
[ -p right ] && rm right

set -e
make
mkdir srv/ cli/
fortune -l >srv/getable
fortune -l >cli/putable

cd srv/
../tftpd >../srv.log 2>&1 &
cd ../cli/
../tftp >../cli.log 2>&1 <<EOM
c localhost
p nogo
g nogo
p putable
g putable
g getable
p getable
q
EOM
cd ../
kill %1

mkfifo left right
diff left right &
md5sum srv/* | sed -e s:srv/:: >left
md5sum cli/* | sed -e s:cli/:: >right

echo >>cli.log
diff left cli.log &
cat >left <<EOM
local: Unable to read specified file
remote: File not found
local: Unable to create the new file
remote: File already exists
tftp> tftp> tftp> tftp> tftp> tftp> tftp> tftp> 
EOM
diff /dev/null srv.log

rm -r srv/ cli/
rm srv.log cli.log left right
echo "All tests passed!"
