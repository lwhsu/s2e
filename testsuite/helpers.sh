get_bitness() {
    local TARGET_NAME="$(basename $1)"
    if echo $TARGET_NAME | grep -q 32; then
        echo 32
    elif echo $TARGET_NAME | grep -q 64; then
        echo 64
    else
        echo "Invalid bitness encoded in $TARGET_NAME"
        exit 1
    fi
}

get_platform() {
    local TARGET_NAME="$(basename $1)"
    if echo $TARGET_NAME | grep -q windows; then
        echo windows
    elif echo $TARGET_NAME | grep -q linux; then
        echo linux
    elif echo $TARGET_NAME | grep -q freebsd; then
        echo freebsd
    else
        echo "Invalid platform encoded in $TARGET_NAME"
        exit 1
    fi
}

# This function takes the path to an executable file (ELF, PE), a function name in that
# executable, and returns the corresponding address.
# GNU objdump synthesizes the <name@plt> labels that the scripts below rely on;
# llvm-objdump (the system objdump on FreeBSD) does not. Prefer the GNU one
# (devel/binutils installs it in /usr/local/bin).
if [ -z "$OBJDUMP" ]; then
    if [ -x /usr/local/bin/objdump ] && /usr/local/bin/objdump --version 2>/dev/null | grep -q GNU; then
        OBJDUMP=/usr/local/bin/objdump
    elif command -v gobjdump > /dev/null 2>&1; then
        OBJDUMP=gobjdump
    else
        OBJDUMP=objdump
    fi
fi

get_func_addr() {
    local BINARY="$1"
    local FUNCTION_NAME="$2"
    local ADDR=""
    if echo $BINARY | grep -q ".exe"; then
        ADDR="$($OBJDUMP -S $BINARY  | grep "<_$FUNCTION_NAME>:" | head -n 1 | cut -d ' ' -f 1)"
        if [ "x$ADDR" = "x" ]; then
            ADDR="$($OBJDUMP -S $BINARY  | grep "<$FUNCTION_NAME>:" | head -n 1 | cut -d ' ' -f 1)"
        fi
    else
        ADDR="$($OBJDUMP -t $BINARY  | grep $FUNCTION_NAME | cut -d ' ' -f 1 | head -n 1)"
    fi

    if [ "x$ADDR" = "x" ]; then
        return
    fi
    echo 0x$ADDR
}
