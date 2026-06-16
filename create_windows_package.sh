#!/bin/bash

# Create Windows package
PACKAGE_NAME="trino-odbc-windows-x64"
PACKAGE_DIR="/root/trino-odbc-driver/${PACKAGE_NAME}"

echo "Creating Windows package..."

# Clean up existing package
rm -rf "$PACKAGE_DIR"

# Create directory structure
mkdir -p "$PACKAGE_DIR/bin"
mkdir -p "$PACKAGE_DIR/etc"
mkdir -p "$PACKAGE_DIR/doc"

# Copy configuration files
cp /root/trino-odbc-driver/windows/odbcinst.ini "$PACKAGE_DIR/etc/"
cp /root/trino-odbc-driver/windows/odbc.ini "$PACKAGE_DIR/etc/"
cp /root/trino-odbc-driver/windows/install.bat "$PACKAGE_DIR/"
cp /root/trino-odbc-driver/windows/install.reg "$PACKAGE_DIR/"

# Copy README
cp /root/trino-odbc-driver/windows/README.md "$PACKAGE_DIR/doc/"
cp /root/trino-odbc-driver/BUILD_WINDOWS.md "$PACKAGE_DIR/doc/"

echo "Package structure created at: $PACKAGE_DIR"
echo ""
echo "Contents:"
ls -la "$PACKAGE_DIR"
ls -la "$PACKAGE_DIR/etc"
ls -la "$PACKAGE_DIR/doc"

# Create ZIP archive
cd /root/trino-odbc-driver
zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME"

echo ""
echo "Package archive created: ${PACKAGE_NAME}.zip"
