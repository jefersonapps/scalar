# Bookworm keeps the glibc floor at 2.36; Python is used only during packaging.
FROM python:3.12-bookworm
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config curl file patchelf \
    desktop-file-utils ca-certificates squashfs-tools git imagemagick \
    qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
    qt6-pdf-dev qt6-svg-dev qt6-tools-dev-tools qmake6 \
    libqt6sql6-sqlite qt6-image-formats-plugins \
    qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtquick-dialogs \
    qml6-module-qt-labs-folderlistmodel qml6-module-qtquick-shapes \
    qml6-module-qtquick-templates qml6-module-qtquick-window \
    qml6-module-qtqml qml6-module-qtqml-workerscript \
    libwebkit2gtk-4.1-dev libgirepository1.0-dev librsvg2-dev \
    glib-networking bubblewrap xdg-dbus-proxy \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    xvfb xauth dbus-x11 fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
