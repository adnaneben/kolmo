# Draft for google/oss-fuzz: projects/kolmo/Dockerfile. Not submitted (see packaging/oss-fuzz/README.md).
FROM gcr.io/oss-fuzz-base/base-builder
RUN apt-get update && apt-get install -y make zip
RUN git clone --depth 1 https://github.com/adnaneben/kolmo kolmo
WORKDIR kolmo
COPY build.sh $SRC/
