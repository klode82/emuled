# emuled in Docker. Costruzione:   docker build -t emuled .
# Uso:  docker run -d --name emuled -v emuled-data:/data -p 127.0.0.1:8765:8765 -p 55662:55662 -p 55672:55672/udp emuled
# Il token dell'API e' in /data/.emuled/profile/emuled.token (docker exec emuled cat /data/.emuled/profile/emuled.token).
FROM debian:12 AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake ninja-build pkg-config flex bison python3 \
    libcrypto++-dev libminiupnpc-dev zlib1g-dev libcurl4-openssl-dev nlohmann-json3-dev nodejs npm ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN python3 tools/fix_case.py && (cd web && npm ci --no-audit --no-fund && npm run build) \
 && cmake -S . -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release && cmake --build build --target emuled \
 && DESTDIR=/out cmake --install build

FROM debian:12-slim
RUN apt-get update && apt-get install -y --no-install-recommends libcrypto++8 libminiupnpc17 zlib1g libcurl4 ca-certificates && rm -rf /var/lib/apt/lists/* \
 && useradd -m -d /data emule
COPY --from=build /out/usr/ /usr/
USER emule
ENV HOME=/data EMULED_API_BIND=0.0.0.0
VOLUME /data
EXPOSE 8765 55662/tcp 55672/udp
ENTRYPOINT ["emuled"]
