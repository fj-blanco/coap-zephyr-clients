#!/bin/bash

dtls_backend=""
skip_clone=false
build_dir="$(pwd)/libcoap/build"
install_dir=$build_dir
libcoap_dir="$(pwd)/libcoap"
groups_spec=false
libcoap_version="develop" # Develop branch
algorithm=""

# Parse command line arguments
for arg in "$@"
do
    case $arg in
        wolfssl)
            dtls_backend="wolfssl"
            ;;
        --skip-clone)
            skip_clone=true
            ;;
        --groups-spec=*)
            groups_spec=true
            algorithm="${arg#*=}"
            ;;
        --sigalgs-spec)
            sigalgs_spec=true
            ;;
        --install-dir=*)
            install_dir="${arg#*=}"
            echo "Install dir: $install_dir"
            ;;
    esac
done

echo "---------------------------------"
# print install dir
echo "Install dir: $install_dir"
echo "---------------------------------"

clean_coap_build() {
    if [ -d "./libcoap" ]; then
        echo "Cleaning existing libcoap directory..."
        cd libcoap
        make clean
        ./autogen.sh --clean
        sudo make uninstall
        cd ..
    else
        echo "libcoap directory does not exist, skipping clean..."
    fi
}

# Clean existing libcoap build
clean_coap_build

if [ "$skip_clone" = false ]; then
    sudo rm -rf ./libcoap
    git clone https://github.com/obgm/libcoap
    cd libcoap
    git checkout $libcoap_version
else
    cd libcoap
fi

# Configure based on dtls_backend flag
if [ "$dtls_backend" == "wolfssl" ]; then
    echo ".........."
    echo "Configuring with wolfSSL as DTLS backend"
    echo ".........."
    echo "Using autogen.sh...."
    ./autogen.sh

    if [ "$groups_spec" = true ]; then
        if [ "$install_dir" == "default" ]; then
            CPPFLAGS="-DCOAP_WOLFSSL_GROUPS=\"\\\"$algorithm\\\"\" -DDTLS_V1_3_ONLY=1" \
            ./configure --enable-dtls --with-wolfssl --disable-manpages --disable-doxygen
        else
            CPPFLAGS="-DCOAP_WOLFSSL_GROUPS=\"\\\"$algorithm\\\"\"" \
            ./configure --enable-dtls --with-wolfssl --disable-manpages --disable-doxygen --prefix=$install_dir
        fi
    elif [ "$sigalgs_spec" = true ]; then
        if [ "$install_dir" == "default" ]; then
            CPPFLAGS="-DCOAP_WOLFSSL_SIGALGS=\"\\\"DILITHIUM_LEVEL3\\\"\"" \
            ./configure --enable-dtls --with-wolfssl --disable-manpages --disable-doxygen
        else
            CPPFLAGS="-DCOAP_WOLFSSL_SIGALGS=\"\\\"DILITHIUM_LEVEL3\\\"\"" \
            ./configure --enable-dtls --with-wolfssl --disable-manpages --disable-doxygen --prefix=$install_dir
        fi
    else
        if [ "$install_dir" == "default" ]; then
            CPPFLAGS="-DCOAP_WOLFSSL_GROUPS=\\\"\\\" -DDTLS_V1_3_ONLY=1" \
            ./configure --enable-dtls --with-wolfssl --disable-manpages --disable-doxygen
        else
            echo "Installing in custom directory"
            CPPFLAGS="-DCOAP_WOLFSSL_GROUPS=\\\"\\\" -DDTLS_V1_3_ONLY=1" \
            ./configure --enable-dtls --with-wolfssl --disable-manpages --disable-doxygen --prefix=$install_dir
        fi
    fi
else
    echo ".........."
    echo "Configuring with OpenSSL as DTLS backend"
    echo ".........."
    # Generate build scripts
    ./autogen.sh
    if [ "$groups_spec" = true ]; then
        CPPFLAGS="-DCOAP_OPENSSL_GROUPS=\"\\\"$algorithm\\\"\"" \
        ./configure --enable-dtls --with-openssl --disable-manpages --disable-doxygen --prefix=$install_dir
    else
        ./configure --enable-dtls --with-openssl --disable-manpages --disable-doxygen --prefix=$install_dir
    fi
fi

# Build and install
make -j$(nproc)
sudo make install