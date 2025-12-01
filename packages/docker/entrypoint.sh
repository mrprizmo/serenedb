#!/bin/sh
set -e

if [ -z "$SERENE_INIT_PORT" ] ; then
    SERENE_INIT_PORT=8999
fi

AUTHENTICATION="true"

# if command starts with an option, prepend serened
case "$1" in
    -*) set -- serened "$@" ;;
    *) ;;
esac

# check for numa
NUMACTL=""

if [ -d /sys/devices/system/node/node1 -a -f /proc/self/numa_maps ]; then
    if [ "$NUMA" = "" ]; then
        NUMACTL="numactl --interleave=all"
    elif [ "$NUMA" != "disable" ]; then
        NUMACTL="numactl --interleave=$NUMA"
    fi

    if [ "$NUMACTL" != "" ]; then
        if $NUMACTL echo > /dev/null 2>&1; then
            echo "using NUMA $NUMACTL"
        else
            echo "cannot start with NUMA $NUMACTL: please ensure that docker is running with --cap-add SYS_NICE"
            NUMACTL=""
        fi
    fi
fi

if [ "$1" = "serened" ]; then
    # Make a copy of the configuration file to patch it,
    # note that this must work regardless under which user we run:
    cp /etc/serenedb/serened.conf /tmp/serened.conf

    SERENE_STORAGE_ENGINE=rocksdb
    if [ ! -z "$SERENE_ENCRYPTION_KEYFILE" ]; then
        echo "Using encrypted database"
        sed -i /tmp/serened.conf -e "s;^.*encryption-keyfile.*;encryption-keyfile=$SERENE_ENCRYPTION_KEYFILE;"
    fi

    if [ ! -z "$SERENE_NO_AUTH" ]; then
        AUTHENTICATION="false"
    elif [ ! -f /var/lib/serenedb/SERVER ]; then
        if [ ! -z "$SERENE_ROOT_PASSWORD_FILE" ]; then
            if [ -f "$SERENE_ROOT_PASSWORD_FILE" ]; then
                SERENE_ROOT_PASSWORD="$(cat $SERENE_ROOT_PASSWORD_FILE)"
            else
                echo "WARNING: password file '$SERENE_ROOT_PASSWORD_FILE' does not exist"
            fi
        fi

        if [ ! -z "$SERENE_RANDOM_ROOT_PASSWORD" ]; then
            SERENE_ROOT_PASSWORD=$(pwgen -s -1 16)
            echo "==========================================="
            echo "GENERATED ROOT PASSWORD: $SERENE_ROOT_PASSWORD"
            echo "==========================================="
        fi

        # Please note that the +x in the following line is for the case that SERENE_ROOT_PASSWORD is set but to an empty value,
        # please do not remove!
        if [ -z "${SERENE_ROOT_PASSWORD+x}" ]; then
            echo >&2 "error: database is uninitialized and password option is not specified "
            echo >&2 "  You need to specify one of SERENE_NO_AUTH, SERENE_ROOT_PASSWORD, SERENE_ROOT_PASSWORD_FILE and SERENE_RANDOM_ROOT_PASSWORD"
            exit 1
        fi


        # Root user by default has empty password, so we don't need to init root password if it's set but empty.
        # It's also common case for coordinators in cluster, because it's impossible to specify password for cluster nodes with serene-init-database
        # So empty SERENE_ROOT_PASSWORD should be used for coordinator if password auth required for them, but better to use jwt/ssl.
        if [ ! -z "${SERENE_ROOT_PASSWORD}" ]; then
            echo "Initializing root user"
            SERENEDB_DEFAULT_ROOT_PASSWORD="$SERENE_ROOT_PASSWORD" /usr/sbin/serene-init-database -c /tmp/serened.conf --server.rest-server false --log.level error --database.init-database true || true
        fi
    fi

    set -- "$@" --server.authentication="$AUTHENTICATION" --config /tmp/serened.conf
else
    NUMACTL=""
fi

exec $NUMACTL "$@"
