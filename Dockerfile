FROM ghcr.io/qtproject/qt:6.6.2

WORKDIR /app

COPY . .

RUN qmake RGS_WebBackend.pro
RUN make -j$(nproc)

EXPOSE 8080

CMD ["./RGS_WebBackend"]
