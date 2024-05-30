#####################################
#
# Build container
# The most basic build for aqualinkd latest version
#####################################

FROM debian:bookworm AS aqualinkd-build

#VOLUME ["/aqualinkd-build"]

RUN apt-get update && \
    apt-get -y install build-essential libsystemd-dev

# Seup working dir
RUN mkdir /home/AqualinkD
WORKDIR /home/AqualinkD

# Get latest release
RUN curl -sL $(curl -s https://api.github.com/repos/sfeakes/AqualinkD/releases/latest | grep "tarball_url" | cut -d'"' -f4) | tar xz --strip-components=1   

# Build aqualinkd
RUN make clean && \
    make container

#####################################
#
# Runtime container
#
#####################################

FROM debian:bookworm-slim AS aqualinkd

#ARG AQUALINKD_VERSION

RUN apt-get update && \
    apt-get install -y cron curl && \
    apt-get clean

# Set cron to read local.d
RUN sed -i '/EXTRA_OPTS=.-l./s/^#//g' /etc/default/cron

# Add Open Container Initiative (OCI) annotations.
# See: https://github.com/opencontainers/image-spec/blob/main/annotations.md

LABEL org.opencontainers.image.title="AqualinkD"
LABEL org.opencontainers.image.url="https://hub.docker.com/repository/docker/sfeakes/aqualinkd/general"
LABEL org.opencontainers.image.source="https://github.com/sfeakes/AqualinkD"
LABEL org.opencontainers.image.documentation="https://github.com/sfeakes/AqualinkD"
#LABEL org.opencontainers.image.version=$AQUALINKD_VERSION


COPY --from=aqualinkd-build /home/AqualinkD/release/aqualinkd /usr/local/bin/aqualinkd                        
COPY --from=aqualinkd-build /home/AqualinkD/release/serial_logger /usr/local/bin/serial_logger
COPY --from=aqualinkd-build /home/AqualinkD/web/ /var/www/aqualinkd/
COPY --from=aqualinkd-build /home/AqualinkD/release/aqualinkd.conf /etc/aqualinkd.conf
#COPY --from=aqualinkd-build /home/AqualinkD/docker/aqualinkd-docker.cmd /usr/local/bin/aqualinkd-docker
RUN curl -s -o /usr/local/bin/aqualinkd-docker https://raw.githubusercontent.com/sfeakes/AqualinkD/master/extras/aqualinkd-docker.cmd

CMD ["sh", "-c", "/usr/local/bin/aqualinkd-docker"]
