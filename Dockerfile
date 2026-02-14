FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    qmake6 \
    cmake \
    git

WORKDIR /app

COPY . .

RUN qmake6 RGS_WebBackend.pro
RUN make -j$(nproc)

EXPOSE 8080

CMD ["./RGS_WebBackend"]
