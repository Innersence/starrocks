#!/usr/bin/env bash
# This file is licensed under the Elastic License 2.0. Copyright 2021 StarRocks Limited.

# get jdk version, return version as an Integer.
# 1.8 => 8, 13.0 => 13
jdk_version() {
    local result
    local java_cmd=$JAVA_HOME/bin/java
    local IFS=$'\n'
    # remove \r for Cygwin
    local lines=$("$java_cmd" -Xms32M -Xmx32M -version 2>&1 | tr '\r' '\n')
    if [[ -z $java_cmd ]]
    then
        result=no_java
    else
        for line in $lines; do
            if [[ (-z $result) && ($line = *"version \""*) ]]
            then
                local ver=$(echo $line | sed -e 's/.*version "\(.*\)"\(.*\)/\1/; 1q')
                # on macOS, sed doesn't support '?'
                if [[ $ver = "1."* ]]
                then
                    result=$(echo $ver | sed -e 's/1\.\([0-9]*\)\(.*\)/\1/; 1q')
                else
                    result=$(echo $ver | sed -e 's/\([0-9]*\)\(.*\)/\1/; 1q')
                fi
            fi
        done
    fi
    echo "$result"
}

export_env_from_conf() {
    while read line; do
        envline=`echo $line | sed 's/[[:blank:]]*=[[:blank:]]*/=/g' | sed 's/^[[:blank:]]*//g' | egrep "^[[:upper:]]([[:upper:]]|_|[[:digit:]])*="`
        envline=`eval "echo $envline"`
        if [[ $envline == *"="* ]]; then
            eval 'export "$envline"'
        fi
    done < $1
}

# Read the config [mem_limit] from configuration file of BE and set the upper limit of TCMalloc memory allocation
# if mem_limit is not set, use 90% of machine memory
export_mem_limit_from_conf() {
    mem_limit_is_set=false;
    while read line; do
        envline=`echo $line | sed 's/[[:blank:]]*=[[:blank:]]*/=/g' | sed 's/^[[:blank:]]*//g' | egrep "^mem_limit=*"`
        if [[ $envline == *"="* ]]; then
            value=`echo $envline | sed 's/mem_limit=//g'`
            mem_limit_is_set=true
        fi
    done < $1

    # read /proc/meminfo to fetch total memory of machine
    mem_total=$(cat /proc/meminfo |grep 'MemTotal' |awk -F : '{print $2}' |sed 's/^[ \t]*//g' | awk '{printf $1}')
    if [ "$mem_total" == "" ]; then
        echo "can't get mem info from /proc/meminfo"
        return 1
    fi

    if [ "$mem_limit_is_set" == "false" ]; then
        # if not set, the mem limit if 90% of total memory
        mem=`expr $mem_total \* 90 / 100 / 1024`
    else
        final=${value: -1}
        case $final in
            t|T)
                value=`echo ${value%?}`
                mem=`expr $value \* 1024 \* 1024`
                ;;
            g|G)
                value=`echo ${value%?}`
                mem=`expr $value \* 1024`
                ;;
            m|M)
                value=`echo ${value%?}`
                mem=`expr $value`
                ;;
            k|K)
                value=`echo ${value%?}`
                mem=`expr $value / 1024`
                ;;
            b|B)
                value=`echo ${value%?}`
                mem=`expr $value / 1024 / 1024`
                ;;
            %)
                value=`echo ${value%?}`
                mem=`expr $mem_total \* $value / 100 / 1024`
                ;;
            *)
                mem=`expr $value / 1024 / 1024`
                ;;
            esac
    fi

    if [ $mem -le 0 ]; then
        echo "invalid mem limit: mem_limit<=0M"
        return 1
    elif [ $mem -gt `expr $mem_total / 1024` ]; then
        echo "mem_limit is larger then machine memory"
        return 1
    fi

    export TCMALLOC_HEAP_LIMIT_MB=${mem}
    return 0
}