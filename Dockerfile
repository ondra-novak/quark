FROM ubuntu:xenial
RUN apt-get update && apt-get install -y git cmake make g++ couchapp
RUN cd ~ && git clone https://github.com/ondra-novak/quark.git && cd quark && git submodule update --init
WORKDIR /root/quark
RUN ./release.sh
RUN make all
RUN make DESTDIR=inst install

FROM ubuntu:xenial
RUN apt-get update && apt-get install -y couchdb xinetd curl
# DEVEL only
RUN apt-get install -y man mc vim net-tools telnet
# because of quark-bot
RUN apt-get install -y wget
COPY --from=0 /root/quark/inst/etc /etc/
COPY --from=0 /root/quark/inst/usr /usr/
ADD https://github.com/just-containers/s6-overlay/releases/download/v1.21.0.2/s6-overlay-amd64.tar.gz /tmp/
RUN tar xzf /tmp/s6-overlay-amd64.tar.gz -C /
ENTRYPOINT ["/init"]
#CMD ["/bin/bash"]