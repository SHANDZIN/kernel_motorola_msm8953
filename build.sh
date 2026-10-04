#!/bin/bash
#
# Compile script for DeenRev kernel

# Colors
NC='\033[0m'
RED='\033[0;31m'
LRD='\033[1;31m'
LGR='\033[1;32m'
YLW='\033[0;33m'

# Trap for Ctrl+C
handle_interrupt() {
    echo -e "\n${YLW}Build interrupted by user. Exiting...${NC}"
    DIFF=$SECONDS
    TIME_INT="$((DIFF / 60)) minute(s) and $((DIFF % 60)) second(s)"
    echo "BUILD_DURATION=$TIME_INT"
    exit 130
}
trap handle_interrupt SIGINT

# Device
DEVICE="${1:-}"
if [ "$#" -gt 0 ]; then
    shift
fi

# Flags handling
ZIP_FLAG=false
DTBS_FLAG=false

for arg in "$@"; do
    case "$arg" in
        -z)    ZIP_FLAG=true ;;
        -dtbs) DTBS_FLAG=true ;;
        *)
            echo -e "${RED}Error: Unknown option: $arg${NC}"
            exit 1
            ;;
    esac
done

# Output usage help
if [ -z "$DEVICE" ]; then
  echo -e "${RED}Error: No device specified!${NC}"
  echo -e "Usage: ./build.sh <device_name> [-dtbs] [-z]"
  echo -e "Example: ./build.sh device (just compiles)"
  echo -e "Example: ./build.sh device -dtbs (compiles DTBs only)"
  echo -e "Example: ./build.sh device -z (compiles and generates zip)"
  exit 1
fi

# Dependency Check
check_deps() {
    echo -e "${YLW}########### Checking Dependencies ############${NC}"
    local deps=("zip" "curl" "git" "make" "python3" "sha256sum" "jq" "perl" "bc" "flex" "bison" "openssl")
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            echo -e "${RED}Error: $dep is not installed. Please install it to continue.${NC}"
            exit 1
        fi
    done

    echo -e "${LGR}Dependencies: OK!${NC}"
}

# Date
TM=$(date '+%Y%m%d-%H%M')

kernel_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$kernel_dir" || exit 1
objdir="${kernel_dir}/out"
anykernel=$HOME/anykernel
# Set TOOLCHAIN_DIR to reuse an existing Clang 18 installation.
toolchain_dir="${TOOLCHAIN_DIR:-${kernel_dir}/clang}"
kernel_name="DeenRev"

zip_name="${kernel_name}-${DEVICE}-${TM}.zip"
KERNEL_IMAGE="${objdir}/arch/arm64/boot/Image.gz-dtb"

LOG_FILE="${kernel_dir}/build_log.txt"

# Defconfigs may live under configs/vendor or directly under configs.
if [ -f "${kernel_dir}/arch/arm64/configs/vendor/${DEVICE}_defconfig" ]; then
    export CONFIG_FILE="vendor/${DEVICE}_defconfig"
elif [ -f "${kernel_dir}/arch/arm64/configs/${DEVICE}_defconfig" ]; then
    export CONFIG_FILE="${DEVICE}_defconfig"
else
    echo -e "${RED}Error: ${DEVICE}_defconfig not found!${NC}"
    exit 1
fi

# Compiler setup (Clang 18 and LLVM tools).
setup_toolchain() {
    if [ ! -d "$toolchain_dir" ]; then
        echo "Clang 18 not found! Cloning to $toolchain_dir..."
        if ! git clone --depth=1 --single-branch -b 18 \
            https://gitlab.com/ThankYouMario/android_prebuilts_clang-standalone \
            "$toolchain_dir"; then
            echo "Cloning failed! Aborting..."
            exit 1
        fi
    fi

    local tool
    for tool in clang clang++ ld.lld llvm-ar llvm-nm llvm-objcopy \
                llvm-objdump llvm-readelf llvm-size llvm-strip; do
        if [ ! -x "$toolchain_dir/bin/$tool" ]; then
            echo -e "${RED}Error: Missing tool: $toolchain_dir/bin/$tool${NC}"
            exit 1
        fi
    done

    export PATH="$toolchain_dir/bin:$PATH"
    local clang_version
    clang_version=$("$toolchain_dir/bin/clang" --version) || exit 1
    if [[ "$clang_version" != *"clang version 18."* ]]; then
        echo -e "${RED}Error: Expected Clang 18 in $toolchain_dir.${NC}"
        exit 1
    fi
    echo -e "${LGR}######### Compiler Version #########${NC}"
    echo -e "${YLW}Using: ${clang_version%%$'\n'*}${NC}"
}

export ARCH=arm64
export SUBARCH=arm64
export KBUILD_BUILD_HOST=viktor
export KBUILD_BUILD_USER=vhmit
export LLVM=1
export LLVM_IAS=1
export CC=clang
export CROSS_COMPILE=aarch64-linux-gnu-
export CROSS_COMPILE_ARM32=arm-linux-gnueabi-
export CROSS_COMPILE_COMPAT=arm-linux-gnueabi-

# Keep the same toolchain for config, kernel, DTBs and exported headers.
MAKE_ARGS=(
    O="$objdir"
    ARCH="$ARCH"
    LLVM=1
    LLVM_IAS=1
    CROSS_COMPILE="$CROSS_COMPILE"
    CROSS_COMPILE_ARM32="$CROSS_COMPILE_ARM32"
    CROSS_COMPILE_COMPAT="$CROSS_COMPILE_COMPAT"
    HOSTCFLAGS=-fcommon
)

clean_all() {
    echo -e "${YLW}########### Cleaning Output Directory ############${NC}"
    rm -rf "${objdir}" "$LOG_FILE" *.zip
}

make_defconfig() {
    SECONDS=0
    echo -e "${LGR}########### Generating Defconfig ############${NC}"

    # Generates the base defconfig using the same LLVM tools as the build.
    make -s "${MAKE_ARGS[@]}" "$CONFIG_FILE" || exit "$?"


}

compile_headers() {
    echo -e "${YLW}########### Compiling Headers ############${NC}"
    local HDR_PATH="${objdir}/arch/arm64/boot/usr"
    make -j"$(nproc --all)" "${MAKE_ARGS[@]}" \
         INSTALL_HDR_PATH="$HDR_PATH" headers_install || return "$?"
    find "$HDR_PATH" -type f \( -name ".install" -o -name "..install.cmd" \) -delete
    echo -e "${LGR}Compiled and cleaned headers in: $HDR_PATH${NC}"
}

compile() {
    echo -e "${LGR}########### Compiling kernel ############${NC}"
    local TEMP_LOG=$(mktemp)
    set -o pipefail
    make -j"$(nproc --all)" "${MAKE_ARGS[@]}" Image.gz-dtb 2>&1 | tee "$TEMP_LOG"

    local exit_status=$?
    set +o pipefail

    # A successful make must also produce the requested flashable image.
    if [ "$exit_status" -eq 0 ] && [ ! -s "$KERNEL_IMAGE" ]; then
        echo "Error: Image.gz-dtb was not generated: $KERNEL_IMAGE" | tee -a "$TEMP_LOG"
        exit_status=1
    fi

    if [ $exit_status -ne 0 ]; then
        [ $exit_status -eq 130 ] && exit 130
        echo -e "${RED}Error: Compilation failed! Generating log...${NC}"
        mv "$TEMP_LOG" "$LOG_FILE"
        echo -e "${YLW}Build log saved locally: $LOG_FILE${NC}"
        exit $exit_status
    else
        rm -f "$TEMP_LOG"
        echo -e "${LGR}Kernel compiled successfully!${NC}"
    fi
}

compile_dtbs() {
    echo -e "${LGR}########### Compiling DTBs only ############${NC}"
    local TEMP_LOG=$(mktemp)
    set -o pipefail
    make -j"$(nproc --all)" "${MAKE_ARGS[@]}" dtbs 2>&1 | tee "$TEMP_LOG"

    local exit_status=$?
    set +o pipefail

    if [ $exit_status -ne 0 ]; then
        [ $exit_status -eq 130 ] && exit 130
        echo -e "${RED}Error: DTB compilation failed! Generating log...${NC}"
        mv "$TEMP_LOG" "$LOG_FILE"
        exit $exit_status
    else
        rm -f "$TEMP_LOG"
        echo -e "${LGR}DTBs compiled successfully!${NC}"
    fi

    echo -e "${YLW}### Generated DTBs:${NC}"
    find "${objdir}/arch/arm64/boot/dts" -name "*.dtb" | sort
}

completion() {
    if [[ -s "$KERNEL_IMAGE" ]]; then
        echo -e "${LGR}######### Packaging AnyKernel3 #########${NC}"
        rm -rf "$anykernel"
        git clone --depth=1 --single-branch https://github.com/SHANDZIN/AnyKernel3.git -b "$DEVICE" "$anykernel" || return "$?"
        cp -f "$KERNEL_IMAGE" "$anykernel/" || return "$?"
        cd "$anykernel" || exit 1
        zip -r9 AnyKernel.zip * -x .git README.md \*placeholder || return "$?"
        cp AnyKernel.zip "$kernel_dir/$zip_name" || return "$?"
        cd "$kernel_dir" || exit 1
        rm -rf "$anykernel"

        echo -e "${LGR}#############################################${NC}"
        echo -e "${LGR}####### Kernel packaged successfully! #######${NC}"
        echo -e "${LGR}#############################################${NC}"

        # Generation SHA256
        echo -e "${YLW}Generating SHA256 checksum...${NC}"
        SHA256=$(sha256sum "$zip_name" | awk '{print $1}')
        echo -e "${YLW}SHA256 Checksum: ${NC}${SHA256}"

        # Upload to Gofile if user selected YES before build
        if [ "$DO_UPLOAD" = true ]; then
            echo -e "${YLW}Checking Gofile status...${NC}"
            SERVER=$(curl -s https://api.gofile.io/servers | jq -r '.data.servers[0].name // "store1"')
            echo -e "${YLW}Uploading ZIP to ${SERVER}...${NC}"
            RESPONSE=$(curl -# -L -F "file=@$zip_name" "https://${SERVER}.gofile.io/contents/uploadfile")

            # Validation
            if echo "$RESPONSE" | jq -e '.status == "ok"' >/dev/null 2>&1; then
                DOWNLOAD_LINK=$(echo "$RESPONSE" | jq -r '.data.downloadPage')
                echo -e "${LGR}Download Link: ${NC}${DOWNLOAD_LINK}"
            else
                echo -e "${RED}Upload failed!${NC}"
            fi
        else
            echo -e "${YLW}Gofile upload skipped as requested.${NC}"
        fi
    fi
}

# Execution
check_deps
setup_toolchain

DO_UPLOAD=false
COMPILE_HDR=false

if [ "$ZIP_FLAG" = true ]; then
    # Prompt for Gofile upload
    echo -ne "${YLW}Do you want to upload the final ZIP to Gofile after compilation? (y/N): ${NC}"
    read -r UPLOAD_CHOICE
    case "$UPLOAD_CHOICE" in
        [yY][eE][sS]|[yY])
            DO_UPLOAD=true
            echo -e "${LGR}Upload enabled for this build.${NC}"
            ;;
        *)
            echo -e "${YLW}Upload disabled. The ZIP will only be generated locally.${NC}"
            ;;
    esac
else
    # Prompt for compile headers (skipped in DTBs-only mode)
    if [ "$DTBS_FLAG" != true ]; then
    echo -ne "${YLW}Do you want to compile headers? (y/N): ${NC}"
    read -r HDR_CHOICE
    case "$HDR_CHOICE" in
        [yY][eE][sS]|[yY])
            COMPILE_HDR=true
            echo -e "${LGR}Header compilation enabled.${NC}"
            ;;
        *)
            echo -e "${YLW}Header compilation skipped.${NC}"
            ;;
    esac
    fi
fi


SECONDS=0

# DTBs only: preserve output files and reuse the existing configuration.
if [ "$DTBS_FLAG" = true ]; then
    if [ -f "${objdir}/.config" ]; then
        echo -e "${YLW}Reusing the existing configuration in: ${objdir}${NC}"
        make -s "${MAKE_ARGS[@]}" olddefconfig || exit "$?"
    else
        make_defconfig
    fi
    compile_dtbs
    DIFF=$SECONDS
    BUILD_TIME="$((DIFF / 60)) minute(s) and $((DIFF % 60)) second(s)"
    echo -e "\n${LGR}-------------------------------------------------------"
    echo -e "DTBs completed successfully in: $BUILD_TIME"
    echo -e "The generated files are located in: ${objdir}/arch/arm64/boot/dts/"
    echo -e "-------------------------------------------------------${NC}"
    cd "${kernel_dir}" || exit 1
    exit 0
fi

clean_all
make_defconfig
compile

# Time build
DIFF=$SECONDS
BUILD_TIME="$((DIFF / 60)) minute(s) and $((DIFF % 60)) second(s)"

# compile() has verified that Image.gz-dtb exists and is not empty.
if [ "$ZIP_FLAG" = true ]; then
    completion || exit "$?"
else
    echo -e "\n${YLW}Info: Flag -z not detected. Compilation finished without generating ZIP.${NC}"
    if [ "$COMPILE_HDR" = true ]; then
        compile_headers || exit "$?"
    fi
fi

echo -e "The generated image is located at: $KERNEL_IMAGE"
echo -e "\n${LGR}-------------------------------------------------------"
echo -e "Completed successfully in: $BUILD_TIME"
echo -e "-------------------------------------------------------${NC}"

cd "${kernel_dir}" || exit 1
