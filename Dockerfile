FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    ninja-build \
    python3 \
    libgl1-mesa-dev \
    libssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Qt 6.6 (minimal)
RUN wget https://download.qt.io/archive/qt/6.6/6.6.2/single/qt-everywhere-src-6.6.2.tar.xz \
    && tar -xf qt-everywhere-src-6.6.2.tar.xz \
    && cd qt-everywhere-src-6.6.2 \
    && ./configure -prefix /opt/qt6 -release -opensource -confirm-license -nomake examples -nomake tests \
    && cmake --build . --parallel \
    && cmake --install .

ENV PATH="/opt/qt6/bin:${PATH}"

WORKDIR /app
COPY . .

RUN qmake RGS_WebBackend.pro
RUN make -j$(nproc)

EXPOSE 8080

CMD ["./RGS_WebBackend"]
