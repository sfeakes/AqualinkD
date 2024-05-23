
#####################################
#
# Build container for build or buildx
#
# Enable multi platform
# docker buildx create --use --platform=linux/arm64,linux/arm/v7,linux/amd64 --name multi-platform-builder
# docker buildx inspect --bootstrap
#
# Build
# docker buildx build --platform=linux/amd64,linux/arm/v7,linux/arm64 .
# adding --progress=plain helps with debug
#
# Clean the build env and start again
# docker buildx prune
#
# Another clean method
# docker system prune
#
#####################################

FROM --platform=$BUILDPLATFORM gcc:12-bookworm AS aqualinkd-build

ARG TARGETPLATFORM
ARG BUILDPLATFORM
ARG BUILDVARIANT
ARG BUILDOS
ARG BUILDARCH
ARG TARGETOS
ARG TARGETARCH

# Print all buildx variables 
RUN echo "Build Platform $BUILDPLATFORM"
RUN echo "Target Platform $TARGETPLATFORM"
RUN echo "Build Veriant $BUILDVARIANT"
RUN echo "Build OS $BUILDOS"
RUN echo "Build Arch $BUILDARCH"
RUN echo "Tagert OS $TARGETOS"
RUN echo "Target Arch $TARGETARCH"


# Install libsystemd dev
RUN apt-get update && \
    apt-get -y install libsystemd-dev


# Get a aqualinkd into container
RUN mkdir /home/AqualinkD
WORKDIR /home/AqualinkD

# Use local
#COPY . /home/AqualinkD

# Use remote latest release version
RUN curl -sL $(curl -s https://api.github.com/repos/sfeakes/AqualinkD/releases/latest | grep "tarball_url" | cut -d'"' -f4) | tar xz --strip-components=1

# Use remote latest
#WORKDIR /home/
#RUN git clone https://api.github.com/repos/sfeakes/AqualinkD/releases/latest

WORKDIR /home/AqualinkD

RUN export AQUALINKD_VERSION=$(cat version.h | grep AQUALINKD_VERSION | cut -d'"' -f2);echo $AQUALINKD_VERSION;
RUN echo "AqualinkD Version $AQUALINKD_VERSION"

# Make AqualinkD
run make clean
RUN make container

#####################################
#
# Runtime container
#
#####################################

FROM debian:bookworm-slim AS aqualinkd

#VOLUME ["/aqualinkd"]
ARG AQUALINKD_VERSION

#ARG TARGETARCH

RUN apt-get update \
  && apt-get install -y cron curl
 
RUN apt-get clean

# Set cron to read local.d
RUN sed -i '/EXTRA_OPTS=.-l./s/^#//g' /etc/default/cron

# Add Open Container Initiative (OCI) annotations.
# See: https://github.com/opencontainers/image-spec/blob/main/annotations.md

LABEL org.opencontainers.image.title="AqualinkD"
LABEL org.opencontainers.image.url="https://hub.docker.com/repository/docker/sfeakes/aqualinkd/general"
LABEL org.opencontainers.image.source="https://github.com/sfeakes/AqualinkD"
LABEL org.opencontainers.image.documentation="https://github.com/sfeakes/AqualinkD"
LABEL org.opencontainers.image.version=$AQUALINKD_VERSION

COPY --from=aqualinkd-build /home/AqualinkD/release/aqualinkd /usr/local/bin/aqualinkd                        
COPY --from=aqualinkd-build /home/AqualinkD/release/serial_logger /usr/local/bin/serial_logger
COPY --from=aqualinkd-build /home/AqualinkD/web/ /var/www/aqualinkd/
COPY --from=aqualinkd-build /home/AqualinkD/release/aqualinkd.conf /etc/aqualinkd.conf

COPY ./extras/aqualinkd-docker.cmd /usr/local/bin/aqualinkd-docker

CMD ["sh", "-c", "/usr/local/bin/aqualinkd-docker"]

