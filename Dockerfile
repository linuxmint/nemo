FROM linuxmintd/mint21-amd64

WORKDIR /src

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update

RUN apt-get install -y --no-install-recommends \
    build-essential \
    meson ninja-build pkg-config \
    python3 python3-gi \
    gobject-introspection \
    gtk-doc-tools \
    intltool itstool \
    libatk1.0-dev \
    libcinnamon-desktop-dev \
    libexempi-dev \
    libexif-dev \
    libgail-3-dev \
    libgirepository1.0-dev \
    libglib2.0-dev \
    libgsf-1-dev \
    libgtk-3-dev \
    libgtk-layer-shell-dev \
    libjson-glib-dev \
    libpango1.0-dev \
    libx11-dev \
    libxapp-dev \
    libxext-dev \
    libxrender-dev \
    libwayland-dev \
    && rm -rf /var/lib/apt/lists/*
