FROM kdeorg/qt6-sdk:6.6

WORKDIR /app

COPY . .

RUN qmake RGS_WebBackend.pro
RUN make -j$(nproc)

EXPOSE 8080

CMD ["./RGS_WebBackend"]
