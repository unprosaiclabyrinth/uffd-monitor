#!/bin/bash

# Set variables
LIGHTTPD_VERSION="1.4.74"  # Change this to the desired version
LIGHTTPD_URL="https://download.lighttpd.net/lighttpd/releases-1.4.x/lighttpd-$LIGHTTPD_VERSION.tar.gz"
TARBALL="lighttpd-$LIGHTTPD_VERSION.tar.gz"
CUSTOM_CONFIG_FILE="lighttpd-custom.conf"

HTML_DIR="www-html"
HTML_FILE="$HTML_DIR/index.html"

# Get the current system username and group name
SYSTEM_USERNAME=$(id -un)
SYSTEM_GROUPNAME=$(id -gn)

# Function to install necessary packages
install_packages() {
    # Install necessary packages
    sudo apt-get update -y
    sudo apt-get install -y build-essential
    #sudo apt-get install -y build-essential libpcre3-dev libssl-dev zlib1g-dev
}

# Function to build Lighttpd
build_lighttpd() {
    # Check if the tarball is already downloaded
    if [ -f $TARBALL ]; then
        echo "$TARBALL already exists, skipping download."
    else
        # Download Lighttpd source code
        wget $LIGHTTPD_URL -O $TARBALL
    fi

    # Extract the tarball
    tar -xzf $TARBALL

    # Navigate to the source directory
    pushd lighttpd-$LIGHTTPD_VERSION

    # Compile Lighttpd
    ./configure --without-pcre2 --without-bzip2 --without-zlib
    make -j4

    # Go back to the previous directory
    popd
}

# Function to create custom configuration file
create_custom_config() {
    # Create a custom configuration file
    cat <<EOL > $CUSTOM_CONFIG_FILE
##
## Basic lighttpd configuration.
##
server.document-root        = "$(pwd)/$HTML_DIR"
server.port                 = 8888
server.username             = "$SYSTEM_USERNAME"
server.groupname            = "$SYSTEM_GROUPNAME"
index-file.names            = ( "index.php", "index.html", "index.lighttpd.html" )

##
## mimetype mapping.
##
mimetype.assign             = (
  ".html"         =>      "text/html",
  ".htm"          =>      "text/html",
  ".pdf"          =>      "application/pdf",
  ".jpg"          =>      "image/jpeg",
  ".jpeg"         =>      "image/jpeg",
  ".png"          =>      "image/png",
)
EOL
}

# Function to create HTML directory and file
create_html_dir() {
    # Create the local html directory and write a simple html file
    mkdir -p $HTML_DIR
    cat <<EOF > $HTML_FILE
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Lighttpd Server</title>
</head>
<body>
    <h1>Welcome to Lighttpd Server!</h1>
    <p>This is a simple HTML file served by Lighttpd.</p>
</body>
</html>
EOF

    # Change ownership of the html directory
    sudo chown -R $SYSTEM_USERNAME:$SYSTEM_GROUPNAME $HTML_DIR
}

# Function to run Lighttpd
run_lighttpd() {
    # Run Lighttpd with the custom configuration and save the PID
    ./lighttpd-$LIGHTTPD_VERSION/src/lighttpd -f $CUSTOM_CONFIG_FILE -D

    echo "Lighttpd is running with the custom configuration file: $CUSTOM_CONFIG_FILE"
    echo "Access the server at http://localhost:8888"
}

# Function to run Lighttpd if already built
run_lighttpd_only() {
    if [ -d "lighttpd-$LIGHTTPD_VERSION" ] && [ -x "lighttpd-$LIGHTTPD_VERSION/src/lighttpd" ]; then
        start_lighttpd
    else
        echo "Lighttpd is not built. Please build it first using the 'build' option."
        exit 1
    fi
}

# Function to gracefully kill Lighttpd
function kill_lighttpd {
    if pgrep lighttpd > /dev/null; then
        echo "Stopping Lighttpd..."
        sudo kill -9 $(pidof lighttpd)
        echo "Lighttpd stopped."
    else
        echo "Lighttpd is not running."
    fi
}

# Function to clean generated files
clean_files() {
    echo "Cleaning generated files..."
    rm -rf $CUSTOM_CONFIG_FILE $HTML_DIR lighttpd-$LIGHTTPD_VERSION $TARBALL
    echo "Cleaned generated files."
}

# Display usage information
usage() {
    echo "Usage: $0 {build|run|kill|clean}"
    exit 1
}

# Parse command-line arguments
case "$1" in
    build)
        build_lighttpd
        create_custom_config
        create_html_dir
        ;;
    run)
        run_lighttpd
        ;;
    kill)
        kill_lighttpd
        ;;
    clean)
        clean_files
        ;;
    *)
        usage
        ;;
esac
