
FROM alpine:3.8

RUN echo "building VoIP Patrol" \
	&& apk update && apk add git cmake g++ cmake make curl-dev alsa-lib-dev

ADD ./pjsua /pjsua

RUN  cd pjsua && ./configure CFLAGS="-DPJSUA_MAX_CONF_PORTS=4096 -DPJSUA_MAX_ACC=100" && make dep && make && make install

ADD . /voip_patrol

RUN     cd /voip_patrol \
	&& cmake CMakeLists.txt && make
