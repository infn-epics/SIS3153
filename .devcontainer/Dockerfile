FROM baltig.infn.it:4567/epics-containers/infn-epics-ioc:devel

USER root

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libmemcached-dev \
        libusb-dev \
    && rm -rf /var/lib/apt/lists/*
