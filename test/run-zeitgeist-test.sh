#! /bin/sh -e

#echo "Building..."
#gcc test-nemo-zeitgeist.c -o test-nemo-zeitgeist \
#    ../libnemo-private/nemo-file-operations.c \
#    `pkg-config --cflags --libs glib-2.0 gtk+-3.0 zeitgeist-1.0` -I.. \
[ "`basename $(pwd)`" = "test" ] || echo "Please cd into test/"
[ "`basename $(pwd)`" = "test" ] || exit
make

echo "Configuring environment..."

TMP_PATH="/tmp/nemo-zg-test"

rm -rf "${TMP_PATH}"
mkdir -p "${TMP_PATH}/cache"

# Launching Zeitgeist
export ZEITGEIST_DATA_PATH="${TMP_PATH}"
export ZEITGEIST_DATABASE_PATH=":memory:"
export ZEITGEIST_DISABLED_EXTENSIONS="SearchEngine"
export XDG_CACHE_HOME="${TMP_PATH}/cache"
zeitgeist-daemon --replace --no-datahub >/dev/null 2>&1 &

echo "Creating files to be used by the tests..."

# test_copy_move
mkdir "${TMP_PATH}/move_dest"
touch "${TMP_PATH}/moveme.txt"

# test_copy and test_new_file_from_template
echo "#! /usr/bin/env python\nprint 'hi!'" > "${TMP_PATH}/a.py"

# test_delete
touch "${TMP_PATH}/del1.txt" "${TMP_PATH}/del2.txt"

echo "Testing..."
export G_MESSAGES_DEBUG=all
#dbus-test-runner \
#  --task zeitgeist-daemon \
#    --parameter --no-datahub \
#    --parameter --log-level=DEBUG \
#  --task gtester \
#    --parameter --verbose \
#    --parameter ./test-nemo-zeitgeist
./test-nemo-zeitgeist

echo "Cleaning up..."
zeitgeist-daemon --quit
rm -r "${TMP_PATH}"
