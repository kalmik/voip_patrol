
FROM alpine:3.8

RUN echo "building VoIP Patrol" \
	&& apk update && apk add git cmake g++ cmake make curl-dev alsa-lib-dev

ADD ./pjsua /pjsua

RUN  cd pjsua && ./configure && make dep && make && make install

ADD . /voip_patrol

RUN     cd /voip_patrol \
	&& cmake CMakeLists.txt && make
