#!/bin/sh -e
# ./musl-install.sh $musl-package $install-dir $jobs
# Some values are hardcoded

if [ $# != 3 ]; then
	echo Invalid argument number
	exit 1
fi

if ! echo "$1" | grep -i '^/' >/dev/null ||
	! echo "$2" | grep -i '^/' >/dev/null; then
	echo Only absolute paths are accepted
	exit 1
fi

if ! echo $3 | grep '^[0-9]*$' >/dev/null; then
	echo Invalid job number
	exit 1
fi

if [ -f local/bin/musl-gcc ]; then
	exit 0
fi

cd "$(dirname "$1")"

package_name=$(basename "$1")
package_dir_name=$(basename "$package_name" .tar.gz)

tar -xf "$package_name"
cd "$package_dir_name"

echo Now: $(pwd)

./configure --prefix="$2" --disable-shared
echo Starting make
make -j$3
make install
