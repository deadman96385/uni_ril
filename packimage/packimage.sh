#!/bin/sh

SECURE_BOOT=$(get_build_var PRODUCT_SECURE_BOOT)
HOST_OUT=$(get_build_var HOST_OUT_EXECUTABLES)
PRODUCT_OUT=$(get_build_var PRODUCT_OUT)
CURPATH=$(pwd)
SPL=$PRODUCT_OUT/u-boot-spl-16k.bin
SML=$PRODUCT_OUT/sml.bin
TOS=$PRODUCT_OUT/tos.bin
UBOOT=$PRODUCT_OUT/u-boot.bin

getModuleName()
{
    local name="allmodules"
    if [ $# -gt 0 ]; then
        for loop in $@
        do
            case $loop in
            "chipram")
            name="chipram"
            break
            ;;
            "bootloader")
            name="bootloader"
            break
            ;;
            "bootimage")
            name="bootimage"
            break
            ;;
            "systemimage")
            name="systemimage"
            break
            ;;
            "userdataimage")
            name="userdataimage"
            break
            ;;
            "recoverimage")
            name="recoverimage"
            break
            ;;
            "clean")
            name="clean"
            break
            ;;
            *)
            ;;
            esac
        done
    fi
    echo $name
}

doImgHeaderInsert()
{
    local NO_SECURE_BOOT

    if [ "NONE" = $SECURE_BOOT ]; then
        NO_SECURE_BOOT=1
    else
        NO_SECURE_BOOT=0
    fi

    for loop in $@
    do
        if [ -f $loop ] ; then
            $HOST_OUT/imgheaderinsert $loop $NO_SECURE_BOOT
        else
            echo "#### no $loop,please check ####"
        fi
    done
}

case $(getModuleName "$@") in
    "chipram")
        doImgHeaderInsert $SPL $SML $TOS
        ;;
    "bootloader")
        doImgHeaderInsert $UBOOT
        ;;
    "allmodules")
        doImgHeaderInsert $SPL $SML $TOS $UBOOT
        ;;
    "clean")
        #do nothing
        ;;
     *)
        ;;
esac






